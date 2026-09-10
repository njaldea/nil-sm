// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

// Internal state-machine building blocks:
// - runtime state, region, queue, and action handling
// - compile-time reachability analysis and state construction tables
// - runtime region and event/capture dispatch
//
// Reachable states are kept in a stable type-list order. The state ID and
// state-maker tables use that same order, so a state ID can select its maker
// by index without storing the ID twice.

#include "concepts.hpp"
#include "structs.hpp"

#include <nil/xalt/coalesce.hpp>
#include <nil/xalt/tlist.hpp>
#include <nil/xalt/typed.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <queue>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nil::sm::detail
{
    struct api_tag final
    {
    };

    template <template <typename> typename API>
    using api_context_t = typename API<api_tag>::api_context_t;

    NIL_XALT_COALESCE_TAG(regions, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(events, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(captures, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(args, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(props, nil::xalt::tlist<>);

    using on_event_t = std::variant<Forward, Discard, Unhandled, Defer, TransitTo, Event>;
    using on_enter_t = std::variant<Unhandled, NOOP, Event>;
    using on_exit_t = std::variant<Unhandled, NOOP, Event>;
    using on_regions_finalized_t = std::variant<Unhandled, NOOP, TransitTo, Event>;

    template <typename T>
    Metadata make_metadata(
        std::size_t region,
        std::size_t state,
        std::size_t subregions,
        const Metadata* parent_metadata
    )
    {
        return Metadata{
            .state = state,
            .region = region,
            .subregions = subregions,
            .depth = parent_metadata == nullptr ? 0 : parent_metadata->depth + 1,
            .is_final = std::is_same_v<T, Fin>,
            .is_barrier = nil::xalt::is_of_template_v<T, barrier::State>,
            .name = type_name<T>(),
            .parent = parent_metadata
        };
    }

    // Fixed sentinel State<API,T>::get() matches on for any direct_parent<T> request,
    // regardless of what T the caller asked for.
    struct direct_parent_marker final
    {
    };

    struct IState
    {
        explicit IState(IState* init_parent, Metadata init_metadata)
            : parent(init_parent)
            , metadata(init_metadata)
        {
        }

        IState(IState&&) = delete;
        IState(const IState&) = delete;
        IState& operator=(IState&&) = delete;
        IState& operator=(const IState&) = delete;
        virtual ~IState() = default;

        virtual on_event_t on_event(const Event& e) = 0;

        // Looks up a value owned by this state or one of its ancestors, by type id.
        virtual void* get(const void* requested_id) = 0;

        IState* parent = nullptr;
        const Metadata metadata;
    };

    // Owns the pointers SM/barrier::SM are constructed with; sits above the top-level
    // region as its parent IState so descendants can reach them through get().
    template <typename... T>
    struct RootState final: IState
    {
        explicit RootState(T*... init_values)
            : IState(nullptr, Metadata{})
            , values(init_values...)
        {
        }

        on_event_t on_event(const Event& /* e */) override
        {
            return Unhandled();
        }

        void* get(const void* requested_id) override
        {
            return std::apply(
                [&]<typename... U>(U*... ptrs) -> void*
                {
                    void* result = nullptr;
                    (void)((requested_id == nil::xalt::type_id<U>
                                ? (result = static_cast<void*>(ptrs), true)
                                : false)
                           || ...);
                    return result;
                },
                values
            );
        }

    private:
        std::tuple<T*...> values;
    };

    class Queues final
    {
    public:
        Queues() = default;
        Queues(Queues&&) = delete;
        Queues(const Queues&) = delete;
        Queues& operator=(Queues&&) = delete;
        Queues& operator=(const Queues&) = delete;

        void push_emit(Event e)
        {
            emit.push(e);
        }

        void push_defer(Event e)
        {
            defer.push(e);
        }

        // The consumer is re-invoked per event so the caller can re-read the active state,
        // which a queued event is allowed to replace.
        template <typename Consume>
        void flush(Consume consume)
        {
            while (!defer.empty())
            {
                auto emitted = defer.front();
                defer.pop();
                consume(emitted);
                emitted.deleter(emitted.data);
            }

            while (!emit.empty())
            {
                auto emitted = emit.front();
                emit.pop();
                consume(emitted);
                emitted.deleter(emitted.data);
            }
        }

        ~Queues()
        {
            while (!defer.empty())
            {
                auto emitted = defer.front();
                defer.pop();
                emitted.deleter(emitted.data);
            }

            while (!emit.empty())
            {
                auto emitted = emit.front();
                emit.pop();
                emitted.deleter(emitted.data);
            }
        }

    private:
        std::queue<Event> emit;
        std::queue<Event> defer;
    };

    struct Region
    {
        std::size_t index;
        IState* parent = nullptr;
        const Metadata* parent_metadata = nullptr;

        Queues* queues = nullptr;
        void* api_contexts = nullptr;
        std::unique_ptr<IState> active_state;
        std::vector<Event> deferred;
        bool terminated = false;

        template <template <typename> typename API, typename R>
        struct tag
        {
        };

        template <template <typename> typename API, typename R>
        Region(
            tag<API, R> /* tag */,
            std::size_t init_index,
            IState* init_parent,
            Queues* init_queues,
            void* init_api_contexts,
            const Metadata* init_parent_metadata
        )
            : index(init_index)
            , parent(init_parent)
            , parent_metadata(init_parent_metadata)
            , queues(init_queues)
            , api_contexts(init_api_contexts)
            , active_state(std::make_unique<State<API, R>>(
                  init_parent,
                  init_queues,
                  static_cast<typename API<R>::api_context_t*>(init_api_contexts),
                  make_metadata<R>(init_index, 0, API<R>::regions_t::size, parent_metadata)
              ))
        {
        }

        Region(Region&&) = default;
        Region& operator=(Region&&) = default;
        Region(const Region&) = delete;
        Region& operator=(const Region&) = delete;

        void transit_out()
        {
            active_state.reset();
            for (auto& event : deferred)
            {
                queues->push_defer(event);
            }
            deferred.clear();
        }

        template <typename RegionDispatcher>
        action_t consume_action(const Event& e, on_event_t action)
        {
            return std::visit(
                [&]<typename Action>(Action& r) -> action_t
                {
                    if constexpr (std::is_same_v<Action, Event>)
                    {
                        queues->push_emit(r);
                        return Discard();
                    }
                    else if constexpr (std::is_same_v<Action, Defer>)
                    {
                        deferred.push_back(e.clone());
                        return Discard();
                    }
                    else if constexpr (std::is_same_v<Action, TransitTo>)
                    {
                        if (r.defer)
                        {
                            deferred.push_back(e.clone());
                        }

                        transit_out();
                        active_state = RegionDispatcher::make(
                            parent,
                            queues,
                            static_cast<typename RegionDispatcher::api_context_t*>(api_contexts),
                            index,
                            parent_metadata,
                            r.target
                        );

                        if (r.target == nil::xalt::type_id<Fin>)
                        {
                            terminated = true;
                        }

                        return Discard();
                    }
                    else if constexpr (std::is_same_v<Action, Forward>)
                    {
                        return Forward();
                    }
                    else if constexpr (std::is_same_v<Action, Unhandled>)
                    {
                        return Unhandled();
                    }
                    else
                    {
                        return Discard();
                    }
                },
                action
            );
        }

        ~Region()
        {
            transit_out();
        }
    };

    template <typename R>
    struct transit_targets_from_action
    {
        using type = nil::xalt::tlist<>;
    };

    template <typename U>
    struct transit_targets_from_action<sm::TransitTo<U>>
    {
        using type = nil::xalt::tlist<U>;
    };

    template <typename U>
    struct transit_targets_from_action<sm::DeferTo<U>>
    {
        using type = nil::xalt::tlist<U>;
    };

    template <typename... R>
    struct transit_targets_from_action<std::variant<R...>>
    {
        using type = nil::xalt::tlist_join_t<typename transit_targets_from_action<R>::type...>;
    };

    template <typename APIState>
    struct state_transit_targets;

    template <template <typename...> typename API, typename StateT, typename... Rest>
    struct state_transit_targets<API<StateT, Rest...>>
    {
    private:
        using api_t = API<StateT, Rest...>;
        using api_context_t = typename api_t::api_context_t;

        template <typename E>
        using event_result_t = decltype(api_t::template on_event<E>(
            std::declval<StateT&>(),
            std::declval<const E&>(),
            static_cast<api_context_t*>(nullptr)
        ));

        template <typename E>
        using capture_result_t = decltype(api_t::template on_capture<E>(
            std::declval<StateT&>(),
            std::declval<const E&>(),
            static_cast<api_context_t*>(nullptr)
        ));

        template <typename EventList>
        struct collect_targets;

        template <typename... E>
        struct collect_targets<nil::xalt::tlist<E...>>
        {
            using type = nil::xalt::tlist_join_t<
                typename transit_targets_from_action<event_result_t<E>>::type...>;
        };

        template <typename CaptureList>
        struct collect_capture_targets;

        template <typename... E>
        struct collect_capture_targets<nil::xalt::tlist<E...>>
        {
            using type = nil::xalt::tlist_join_t<
                typename transit_targets_from_action<capture_result_t<E>>::type...>;
        };

    public:
        using type = nil::xalt::tlist_dedupe_t<nil::xalt::tlist_join_t<
            typename collect_targets<typename api_t::events_t>::type,
            typename collect_capture_targets<typename api_t::captures_t>::type,
            typename transit_targets_from_action<decltype(api_t::on_regions_finalized(
                std::declval<StateT&>(),
                static_cast<api_context_t*>(nullptr)
            ))>::type>>;
    };

    template <template <typename> typename API, typename Pending, typename Seen>
    struct reachable_state_set_impl;

    template <template <typename> typename API, typename Seen>
    struct reachable_state_set_impl<API, nil::xalt::tlist<>, Seen>
    {
        using type = Seen;
    };

    template <template <typename> typename API, typename InitialState, typename Siblings>
    struct region_state_set;

    template <
        template <typename...>
        typename API,
        bool AlreadySeen,
        typename Head,
        typename PendingTail,
        typename Seen>
    struct reachable_state_step;

    template <
        template <typename...>
        typename API,
        typename Head,
        typename... Tail,
        typename... Seen>
    struct reachable_state_step<
        API,
        true,
        Head,
        nil::xalt::tlist<Tail...>,
        nil::xalt::tlist<Seen...>>
    {
        using next_pending = nil::xalt::tlist<Tail...>;
        using next_seen = nil::xalt::tlist<Seen...>;
    };

    template <
        template <typename...>
        typename API,
        typename Head,
        typename... Tail,
        typename... Seen>
    struct reachable_state_step<
        API,
        false,
        Head,
        nil::xalt::tlist<Tail...>,
        nil::xalt::tlist<Seen...>>
    {
        using next_pending = nil::xalt::tlist_join_t<
            nil::xalt::tlist<Tail...>,
            typename state_transit_targets<API<Head>>::type>;
        using next_seen = nil::xalt::tlist<Seen..., Head>;
    };

    template <template <typename> typename API, typename Head, typename... Tail, typename... Seen>
    struct reachable_state_set_impl<API, nil::xalt::tlist<Head, Tail...>, nil::xalt::tlist<Seen...>>
    {
        static constexpr auto already_seen = nil::xalt::tlist<Seen...>::template contains<Head>;

        using step = reachable_state_step<
            API,
            already_seen,
            Head,
            nil::xalt::tlist<Tail...>,
            nil::xalt::tlist<Seen...>>;

        using type = typename reachable_state_set_impl<
            API,
            typename step::next_pending,
            typename step::next_seen>::type;
    };

    template <template <typename> typename API, typename SeedStates>
    struct reachable_state_set
    {
        using type = typename reachable_state_set_impl<API, SeedStates, nil::xalt::tlist<>>::type;
    };

    template <template <typename> typename API, typename InitialState, typename... Sibling>
    struct region_state_set<API, InitialState, nil::xalt::tlist<Sibling...>>
    {
        using type = nil::xalt::tlist<InitialState, Sibling...>;
    };

    template <template <typename> typename API, typename InitialState>
    struct region_state_set<API, InitialState, void>
    {
        using type = typename reachable_state_set<API, nil::xalt::tlist<InitialState>>::type;
    };

    template <template <typename> typename API, typename States>
    struct state_maker;

    // This table is one static instance for each API and reachable state-list
    // combination. Graphs with the same list reuse the table.
    template <template <typename> typename API, typename... State>
    struct state_maker<API, nil::xalt::tlist<State...>>
    {
        using api_context_t = detail::api_context_t<API>;
        using maker_t = std::unique_ptr<
            IState> (*)(IState*, Queues*, api_context_t*, std::size_t, std::size_t, const Metadata*);

        template <typename Candidate>
        static std::unique_ptr<IState> make(
            IState* parent,
            Queues* queues,
            api_context_t* api_contexts,
            std::size_t region,
            std::size_t state,
            const Metadata* parent_metadata
        )
        {
            return std::make_unique<::nil::sm::State<API, Candidate>>(
                parent,
                queues,
                api_contexts,
                make_metadata<Candidate>(
                    region,
                    state,
                    API<Candidate>::regions_t::size,
                    parent_metadata
                )
            );
        }

        static maker_t get_maker(std::size_t index)
        {
            static constexpr auto table = std::array<maker_t, sizeof...(State)>{&make<State>...};
            return table[index];
        }
    };

    template <template <typename> typename API, typename InitialState>
    struct region_reachability_graph
    {
        using api_context_t = typename API<InitialState>::api_context_t;
        // This graph describes one region. Composite states have one graph per
        // region because each region can have a different reachable state set.
        using states = typename region_state_set<
            API,
            InitialState,
            typename nil::sm::siblings<InitialState>::type>::type;

        static constexpr auto state_ids = []<typename... States>(nil::xalt::tlist<States...>)
        { return std::array{nil::xalt::type_id<States>...}; }(states{});

        template <typename Target>
        static consteval std::size_t index_of()
        {
            return index_of(nil::xalt::type_id<Target>);
        }

        static constexpr std::size_t index_of(const void* target)
        {
            for (std::size_t state = 0; state < state_ids.size(); ++state)
            {
                if (state_ids[state] == target)
                {
                    return state;
                }
            }

            return state_ids.size();
        }

        static std::unique_ptr<IState> make(
            std::size_t region,
            const void* target,
            IState* parent,
            Queues* queues,
            api_context_t* api_contexts,
            const Metadata* parent_metadata
        )
        {
            const auto state = index_of(target);
            if (state == state_ids.size())
            {
                return {};
            }

            return state_maker<API, states>::get_maker(state)(
                parent,
                queues,
                api_contexts,
                region,
                state,
                parent_metadata
            );
        }
    };

    template <template <typename> typename API, typename Regions>
    struct region_dispatcher;

    // Merges what used to be a separate region_maker: builds the per-index
    // reachable-state table and adds the Fin fallback in one template.
    template <template <typename> typename API, typename... Region>
    struct region_dispatcher<API, nil::xalt::tlist<Region...>>
    {
    public:
        using api_context_t = detail::api_context_t<API>;

    private:
        using maker_t = std::unique_ptr<
            IState> (*)(std::size_t, const void*, IState*, Queues*, api_context_t*, const Metadata*);

        template <std::size_t I>
        static std::unique_ptr<IState> make_indexed(
            std::size_t region,
            const void* target,
            IState* parent,
            Queues* queues,
            api_context_t* api_contexts,
            const Metadata* parent_metadata
        )
        {
            using region_t = typename nil::xalt::tlist<Region...>::template at<I>;
            using graph_t = region_reachability_graph<API, region_t>;
            return graph_t::make(region, target, parent, queues, api_contexts, parent_metadata);
        }

        template <std::size_t... I>
        static consteval auto make_table(std::index_sequence<I...> /* indices */)
        {
            return std::array<maker_t, sizeof...(I)>{&make_indexed<I>...};
        }

        static constexpr auto table = make_table(std::make_index_sequence<sizeof...(Region)>{});

    public:
        static std::unique_ptr<IState> make(
            IState* parent,
            Queues* queues,
            api_context_t* api_contexts,
            std::size_t region,
            const Metadata* parent_metadata,
            const void* target
        )
        {
            if (nil::xalt::type_id<Fin> == target)
            {
                return std::make_unique<::nil::sm::State<API, Fin>>(
                    parent,
                    queues,
                    api_contexts,
                    make_metadata<Fin>(
                        region,
                        Fin::state_index,
                        API<Fin>::regions_t::size,
                        parent_metadata
                    )
                );
            }

            if (region < table.size())
            {
                return table[region](region, target, parent, queues, api_contexts, parent_metadata);
            }

            return {};
        }
    };

    template <typename O, typename R>
    O to_runtime_action_as(R r)
    {
        if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            return std::visit([](auto& v) { return to_runtime_action_as<O>(std::move(v)); }, r);
        }
        else if constexpr (std::is_same_v<R, ::nil::sm::Terminate>)
        {
            return O{TransitTo{.defer = false, .target = nil::xalt::type_id<Fin>}};
        }
        else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::TransitTo>)
        {
            return O{TransitTo{.defer = false, .target = nil::xalt::type_id<typename R::type>}};
        }
        else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::DeferTo>)
        {
            return O{TransitTo{.defer = true, .target = nil::xalt::type_id<typename R::type>}};
        }
        else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::Emit>)
        {
            return O{Event(std::move(r))};
        }
        else
        {
            return O{r};
        }
    }

    struct event_dispatch_policy
    {
        template <typename API, typename StateT, typename Event>
        static on_event_t invoke(
            StateT& state,
            const Event& event,
            typename API::api_context_t* context
        )
        {
            static_assert(
                requires { API::template on_event<Event>(state, event, context); }
                    && concepts::is_allowed_to_use_for_on_event_result<
                        decltype(API::template on_event<Event>(state, event, context))>,
                "API must expose on_event<Event>(state, event, contexts...) with an allowed "
                "return type"
            );
            return to_runtime_action_as<on_event_t>(
                API::template on_event<Event>(state, event, context)
            );
        }
    };

    // Event and capture dispatch share the same table and lookup machinery;
    // only the API hook selected by the policy differs.
    struct capture_dispatch_policy
    {
        template <typename API, typename StateT, typename Event>
        static on_event_t invoke(
            StateT& state,
            const Event& event,
            typename API::api_context_t* context
        )
        {
            static_assert(
                requires { API::template on_capture<Event>(state, event, context); }
                    && concepts::is_allowed_to_use_for_on_event_result<
                        decltype(API::template on_capture<Event>(state, event, context))>,
                "API must expose on_capture<Event>(state, event, contexts...) with an allowed "
                "return type"
            );
            return to_runtime_action_as<on_event_t>(
                API::template on_capture<Event>(state, event, context)
            );
        }
    };

    template <typename S, typename T, typename Events, typename Policy>
    struct action_dispatcher;

    template <typename S, typename T, typename... E, typename Policy>
    struct action_dispatcher<S, T, nil::xalt::tlist<E...>, Policy>
    {
    private:
        using api_t = typename S::api_t;
        using api_context_t = typename api_t::api_context_t;

        struct event_handler
        {
            const void* id = nullptr;
            on_event_t (*invoke)(T&, const void*, void*) = nullptr;
        };

        template <typename EV>
        static on_event_t call(T& state_value, const void* event, void* api_contexts)
        {
            return Policy::template invoke<api_t, T>(
                state_value,
                *static_cast<const EV*>(event),
                static_cast<api_context_t*>(api_contexts)
            );
        }

        static constexpr auto handlers = std::array<event_handler, sizeof...(E)>{
            event_handler{.id = nil::xalt::type_id<E>, .invoke = &call<E>}...
        };

    public:
        static on_event_t dispatch(const Event& event, T& state, api_context_t* api_contexts)
        {
            for (const auto& handler : handlers)
            {
                if (handler.id == event.id)
                {
                    return handler.invoke(state, event.data, api_contexts);
                }
            }

            return Unhandled();
        }
    };

    template <typename S, typename T, typename Events>
    using event_dispatcher = action_dispatcher<S, T, Events, event_dispatch_policy>;

    template <typename S, typename T, typename Captures>
    using capture_dispatcher = action_dispatcher<S, T, Captures, capture_dispatch_policy>;
}
