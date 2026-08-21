#pragma once

#include "concepts.hpp"
#include "structs.hpp"

#include <nil/xalt/coalesce.hpp>
#include <nil/xalt/tlist.hpp>
#include <nil/xalt/typed.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <queue>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nil::sm::detail
{
    NIL_XALT_COALESCE_TAG(regions, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(events, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(captures, nil::xalt::tlist<>);

    using on_event_t = std::variant<
        Terminate,
        Forward,
        Discard,
        Unhandled,
        Defer,
        Transit,
        Emit //
        >;
    using on_enter_t = std::variant<Unhandled, NOOP, Emit>;
    using on_exit_t = std::variant<Unhandled, NOOP, Emit>;
    using on_regions_finalized_t = std::variant<Unhandled, NOOP, Terminate, Transit, Emit>;

    struct IState
    {
        explicit IState(Metadata init_metadata)
            : metadata(init_metadata)
        {
        }

        IState(IState&&) = delete;
        IState(const IState&) = delete;
        IState& operator=(IState&&) = delete;
        IState& operator=(const IState&) = delete;
        virtual ~IState() = default;

        virtual on_event_t on_event(const Emit& e) = 0;

        const Metadata metadata;
    };

    class Queues final
    {
    public:
        Queues() = default;
        Queues(Queues&&) = delete;
        Queues(const Queues&) = delete;
        Queues& operator=(Queues&&) = delete;
        Queues& operator=(const Queues&) = delete;

        void push_emit(Emit e)
        {
            emit.push(e);
        }

        void push_defer(Emit e)
        {
            defer.push(e);
        }

        void flush(IState& state)
        {
            while (!defer.empty())
            {
                auto emitted = defer.front();
                defer.pop();
                state.on_event(emitted);
                emitted.deleter(emitted.data);
            }

            while (!emit.empty())
            {
                auto emitted = emit.front();
                emit.pop();
                state.on_event(emitted);
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
        std::queue<Emit> emit;
        std::queue<Emit> defer;
    };

    struct Region
    {
        Queues* qs = nullptr;
        std::unique_ptr<IState> active_state;
        std::vector<Emit> deferred;
        bool terminated = false;

        Region(Queues* init_qs, std::unique_ptr<IState> init_active_state)
            : qs(init_qs)
            , active_state(std::move(init_active_state))
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
                qs->push_defer(event);
            }
            deferred.clear();
        }

        ~Region()
        {
            transit_out();
        }
    };

    struct Contexts
    {
        void* state;
        void* api;
    };

    template <typename R>
    struct transit_targets_from_action
    {
        using type = nil::xalt::tlist<>;
    };

    template <typename U>
    struct transit_targets_from_action<::nil::sm::Transit<U>>
    {
        using type = nil::xalt::tlist<U>;
    };

    template <typename... R>
    struct transit_targets_from_action<std::variant<R...>>
    {
        using type = typename nil::xalt::tlist<>::template join<
            typename transit_targets_from_action<std::remove_cvref_t<R>>::type...>;
    };

    template <typename APIState>
    struct state_transit_targets;

    template <template <typename...> typename API, typename StateT, typename... Rest>
    struct state_transit_targets<API<StateT, Rest...>>
    {
    private:
        using api_t = API<StateT, Rest...>;
        using state_t = typename api_t::state_t;
        using api_context_t = typename api_t::api_context_t;

        template <typename E>
        using event_result_t = decltype(api_t::template on_event<E>(
            std::declval<state_t&>(),
            std::declval<const E&>(),
            static_cast<api_context_t*>(nullptr)
        ));

        template <typename E>
        using capture_result_t = decltype(api_t::template on_capture<E>(
            std::declval<state_t&>(),
            std::declval<const E&>(),
            static_cast<api_context_t*>(nullptr)
        ));

        template <typename EventList>
        struct collect_targets;

        template <typename... E>
        struct collect_targets<nil::xalt::tlist<E...>>
        {
            using type =
                typename nil::xalt::tlist<>::template join<typename transit_targets_from_action<
                    std::remove_cvref_t<event_result_t<E>>>::type...>;
        };

        template <typename CaptureList>
        struct collect_capture_targets;

        template <typename... E>
        struct collect_capture_targets<nil::xalt::tlist<E...>>
        {
            using type =
                typename nil::xalt::tlist<>::template join<typename transit_targets_from_action<
                    std::remove_cvref_t<capture_result_t<E>>>::type...>;
        };

    public:
        using type = typename nil::xalt::tlist<>::template join<
            typename collect_targets<typename api_t::events_t>::type,
            typename collect_capture_targets<typename api_t::captures_t>::type,
            typename transit_targets_from_action<
                std::remove_cvref_t<decltype(api_t::on_regions_finalized(
                    std::declval<state_t&>(),
                    static_cast<api_context_t*>(nullptr)
                ))>>::type>::dedupe;
    };

    template <template <typename...> typename API, typename Pending, typename Seen>
    struct reachable_state_set_impl;

    template <template <typename...> typename API, typename Seen>
    struct reachable_state_set_impl<API, nil::xalt::tlist<>, Seen>
    {
        using type = Seen;
    };

    template <
        template <typename...>
        typename API,
        typename Head,
        typename... Tail,
        typename... Seen>
    struct reachable_state_set_impl<API, nil::xalt::tlist<Head, Tail...>, nil::xalt::tlist<Seen...>>
    {
        static constexpr auto already_seen
            = nil::xalt::tlist<Seen...>::template any_of<std::is_same, Head>;

        using next_pending_raw = std::conditional_t<
            already_seen,
            nil::xalt::tlist<Tail...>,
            typename nil::xalt::tlist<Tail...>::template join<
                typename state_transit_targets<API<Head>>::type>>;

        using next_seen = std::
            conditional_t<already_seen, nil::xalt::tlist<Seen...>, nil::xalt::tlist<Seen..., Head>>;

        using type =
            typename reachable_state_set_impl<API, typename next_pending_raw::dedupe, next_seen>::
                type;
    };

    template <template <typename...> typename API, typename SeedStates>
    struct reachable_state_set
    {
        using type = typename reachable_state_set_impl<
            API,
            typename SeedStates::dedupe,
            nil::xalt::tlist<>>::type;
    };

    template <template <typename...> typename API, typename InitialState>
    struct region_reachability_graph
    {
        using states = typename reachable_state_set<API, nil::xalt::tlist<InitialState>>::type;
    };

    template <typename APIState, typename TargetStates>
    struct region_state_factory;

    template <
        template <typename...>
        typename API,
        typename StateT,
        typename... Rest,
        typename TargetStates>
    struct region_state_factory<API<StateT, Rest...>, TargetStates>
    {
    public:
        using targets = TargetStates;

        template <typename Parent>
        static std::unique_ptr<IState> make(
            std::size_t region,
            const void* target,
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            const Metadata* parent_metadata
        )
        {
            static constexpr auto table = make_table<Parent>(targets{});

            for (auto state = std::size_t{0}; state < table.size(); ++state)
            {
                const auto& it = table[state];
                if (it.id == target)
                {
                    return it.ctor(parent, qs, contexts, region, state, parent_metadata);
                }
            }

            return {};
        }

    private:
        template <typename U>
        using state_api_t = API<U, Rest...>;

        template <typename Parent>
        struct entry
        {
            const void* id = nullptr;
            std::unique_ptr<
                IState> (*ctor)(Parent*, Queues*, Contexts*, std::size_t, std::size_t, const Metadata*)
                = nullptr;
        };

        template <typename Parent, typename Candidate>
        static std::unique_ptr<IState> create(
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            std::size_t region,
            std::size_t state,
            const Metadata* parent_metadata
        )
        {
            return std::make_unique<State<state_api_t, Candidate>>(
                parent,
                qs,
                contexts,
                region,
                state,
                parent_metadata
            );
        }

        template <typename Parent, typename... Target>
        static consteval auto make_table(nil::xalt::tlist<Target...> /* targets */)
        {
            using E = entry<Parent>;
            return std::array<E, sizeof...(Target)>{
                E{nil::xalt::type_id<Target>, &create<Parent, Target>}...
            };
        }
    };

    template <template <typename...> typename API, typename Regions>
    struct region_dispatcher;

    template <template <typename...> typename API, typename... Region>
    struct region_dispatcher<API, nil::xalt::tlist<Region...>>
    {
    public:
        template <typename Parent>
        static std::unique_ptr<IState> make(
            std::size_t region,
            const void* target,
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            const Metadata* parent_metadata
        )
        {
            if constexpr (sizeof...(Region) > 0)
            {
                return make_impl(
                    region,
                    target,
                    parent,
                    qs,
                    contexts,
                    parent_metadata,
                    std::make_index_sequence<sizeof...(Region)>{}
                );
            }

            return {};
        }

    private:
        using regions_t = nil::xalt::tlist<Region...>;

        template <std::size_t I, typename Parent>
        static std::unique_ptr<IState> make_for_region(
            std::size_t region,
            const void* target,
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            const Metadata* parent_metadata
        )
        {
            using region_t = typename regions_t::template at<I>;
            using dispatch_t = region_state_factory<
                API<region_t>,
                typename region_reachability_graph<API, region_t>::states>;
            return dispatch_t::template make<Parent>(
                region,
                target,
                parent,
                qs,
                contexts,
                parent_metadata
            );
        }

        template <typename Parent, std::size_t... I>
        static std::unique_ptr<IState> make_impl(
            std::size_t region,
            const void* target,
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            const Metadata* parent_metadata,
            std::index_sequence<I...> /* indices */
        )
        {
            using maker_t = std::unique_ptr<
                IState> (*)(std::size_t, const void*, Parent*, Queues*, Contexts*, const Metadata*);

            static constexpr auto table
                = std::array<maker_t, sizeof...(I)>{&make_for_region<I, Parent>...};

            if (region >= table.size())
            {
                return {};
            }

            return table[region](region, target, parent, qs, contexts, parent_metadata);
        }
    };

    template <typename Action, typename MakeTransitState, typename MakeTerminatedState>
    void apply_region_runtime_action(
        std::size_t i,
        Action& r,
        const Emit& e,
        Region& region,
        const MakeTransitState& make_transit_state,
        const MakeTerminatedState& make_terminated_state
    )
    {
        if constexpr (std::is_same_v<Action, Transit>)
        {
            region.transit_out();
            region.active_state = make_transit_state(i, r.target);
        }
        else if constexpr (std::is_same_v<Action, Emit>)
        {
            region.qs->push_emit(r);
        }
        else if constexpr (std::is_same_v<Action, Terminate>)
        {
            region.transit_out();
            region.active_state = make_terminated_state(i);
            region.terminated = true;
        }
        else if constexpr (std::is_same_v<Action, Defer>)
        {
            region.deferred.push_back(e.clone());
        }
    }

    template <typename O, typename R>
    static O to_runtime_action_as(R& r)
    {
        if constexpr (nil::xalt::is_of_template_v<std::remove_cvref_t<R>, std::variant>)
        {
            return std::visit([]<typename V>(V& v) { return to_runtime_action_as<O>(v); }, r);
        }
        else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::Transit>)
        {
            return O{detail::Transit{.target = nil::xalt::type_id<typename R::type>}};
        }
        else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::Emit>)
        {
            return O{detail::Emit(std::move(r))};
        }
        else
        {
            return O{r};
        }
    }

    template <typename S, typename E>
    struct event_dispatcher;

    template <typename S, typename... E>
    struct event_dispatcher<S, nil::xalt::tlist<E...>>
    {
    private:
        using api_t = typename S::api_t;
        using state_t = typename api_t::state_t;
        using api_context_t = typename api_t::api_context_t;

        struct event_handler
        {
            const void* id = nullptr;
            on_event_t (*invoke)(state_t&, const void*, void*) = nullptr;
        };

        template <typename EV>
        static on_event_t call(state_t& state_value, const void* event, void* api_contexts)
        {
            static_assert(
                requires() {
                    {
                        api_t::template on_event<EV>(
                            state_value,
                            *static_cast<const EV*>(event),
                            static_cast<api_context_t*>(api_contexts)
                        )
                    } -> concepts::is_allowed_to_use_for_on_event_result;
                },
                "API must expose on_event<Event>(state_value, event, contexts...) with an "
                "allowed "
                "return type"
            );
            auto result = api_t::template on_event<EV>(
                state_value,
                *static_cast<const EV*>(event),
                static_cast<api_context_t*>(api_contexts)
            );
            return detail::to_runtime_action_as<on_event_t>(result);
        }

        static constexpr auto handlers = std::array<event_handler, sizeof...(E)>{
            event_handler{.id = nil::xalt::type_id<E>, .invoke = &call<E>}...
        };

    public:
        static on_event_t dispatch(const Emit& event, state_t& state, api_context_t* api_contexts)
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

    template <typename S, typename E>
    struct capture_dispatcher;

    template <typename S, typename... E>
    struct capture_dispatcher<S, nil::xalt::tlist<E...>>
    {
    private:
        using api_t = typename S::api_t;
        using state_t = typename api_t::state_t;
        using api_context_t = typename api_t::api_context_t;

        struct capture_handler
        {
            const void* id = nullptr;
            on_event_t (*invoke)(state_t&, const void*, void*) = nullptr;
        };

        template <typename EV>
        static on_event_t call(state_t& state_value, const void* event, void* api_contexts)
        {
            static_assert(
                requires() {
                    {
                        api_t::template on_capture<EV>(
                            state_value,
                            *static_cast<const EV*>(event),
                            static_cast<api_context_t*>(api_contexts)
                        )
                    } -> concepts::is_allowed_to_use_for_on_event_result;
                },
                "API must expose on_capture<Event>(state_value, event, contexts...) with an "
                "allowed return type"
            );
            auto result = api_t::template on_capture<EV>(
                state_value,
                *static_cast<const EV*>(event),
                static_cast<api_context_t*>(api_contexts)
            );
            return detail::to_runtime_action_as<on_event_t>(result);
        }

        static constexpr auto handlers = std::array<capture_handler, sizeof...(E)>{
            capture_handler{.id = nil::xalt::type_id<E>, .invoke = &call<E>}...
        };

    public:
        static on_event_t dispatch(const Emit& event, state_t& state, api_context_t* api_contexts)
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
}
