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
        Event //
        >;
    using on_enter_t = std::variant<Unhandled, NOOP, Event>;
    using on_exit_t = std::variant<Unhandled, NOOP, Event>;
    using on_regions_finalized_t = std::variant<Unhandled, NOOP, Terminate, Transit, Event>;

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

        virtual on_event_t on_event(const Event& e) = 0;

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

        void push_emit(Event e)
        {
            emit.push(e);
        }

        void push_defer(Event e)
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
        std::queue<Event> emit;
        std::queue<Event> defer;
    };

    // A Region owns its active state and deferred events. Leaving a state moves
    // deferred events back to the shared queue so they can be handled later.
    struct Region
    {
        std::size_t index;
        Queues* queues = nullptr;
        std::unique_ptr<IState> active_state;
        std::vector<Event> deferred;
        bool terminated = false;

        Region(std::size_t init_index, Queues* init_qs, std::unique_ptr<IState> init_active_state)
            : index(init_index)
            , queues(init_qs)
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
                queues->push_defer(event);
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
    struct transit_targets_from_action<sm::Transit<U>>
    {
        using type = nil::xalt::tlist<U>;
    };

    template <>
    struct transit_targets_from_action<Terminate>
    {
        using type = nil::xalt::tlist<Fin>;
    };

    template <typename... R>
    struct transit_targets_from_action<std::variant<R...>>
    {
        using type = typename nil::xalt::tlist<>::template join<
            typename transit_targets_from_action<R>::type...>;
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
            using type = typename nil::xalt::tlist<>::template join<
                typename transit_targets_from_action<event_result_t<E>>::type...>;
        };

        template <typename CaptureList>
        struct collect_capture_targets;

        template <typename... E>
        struct collect_capture_targets<nil::xalt::tlist<E...>>
        {
            using type = typename nil::xalt::tlist<>::template join<
                typename transit_targets_from_action<capture_result_t<E>>::type...>;
        };

    public:
        using type = typename nil::xalt::tlist<>::template join<
            typename collect_targets<typename api_t::events_t>::type,
            typename collect_capture_targets<typename api_t::captures_t>::type,
            typename transit_targets_from_action<decltype(api_t::on_regions_finalized(
                std::declval<state_t&>(),
                static_cast<api_context_t*>(nullptr)
            ))>::type>::dedupe;
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
        static constexpr auto already_seen = nil::xalt::tlist<Seen...>::template contains<Head>;

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

    template <template <typename...> typename API, typename Parent, typename States>
    struct state_maker;

    // This table is one static instance for each API, Parent, and reachable
    // state-list combination. Graphs with the same list reuse the table.
    template <template <typename...> typename API, typename Parent, typename... State>
    struct state_maker<API, Parent, nil::xalt::tlist<State...>>
    {
        using maker_t = std::unique_ptr<
            IState> (*)(Parent*, Queues*, Contexts*, std::size_t, std::size_t, const Metadata*);

        template <typename Candidate>
        static std::unique_ptr<IState> make(
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            std::size_t region,
            std::size_t state,
            const Metadata* parent_metadata
        )
        {
            return std::make_unique<::nil::sm::State<API, Candidate>>(
                parent,
                qs,
                contexts,
                region,
                state,
                parent_metadata
            );
        }

        static constexpr auto table = std::array<maker_t, sizeof...(State)>{&make<State>...};
    };

    template <template <typename...> typename API, typename InitialState>
    struct region_reachability_graph
    {
        // This graph describes one region. Composite states have one graph per
        // region because each region can have a different reachable state set.
        using states = typename reachable_state_set<API, nil::xalt::tlist<InitialState>>::type;

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
            const auto state = index_of(target);
            if (state == state_ids.size())
            {
                return {};
            }

            return state_maker<API, Parent, states>::table[state](
                parent,
                qs,
                contexts,
                region,
                state,
                parent_metadata
            );
        }
    };

    template <template <typename...> typename API, typename Parent, typename Regions>
    struct region_maker;

    // Regions are heterogeneous at compile time, so this table erases their
    // individual graph types behind one runtime function-pointer signature.
    template <template <typename...> typename API, typename Parent, typename... Region>
    struct region_maker<API, Parent, nil::xalt::tlist<Region...>>
    {
        using maker_t = std::unique_ptr<
            IState> (*)(std::size_t, const void*, Parent*, Queues*, Contexts*, const Metadata*);

        template <std::size_t I>
        static std::unique_ptr<IState> make(
            std::size_t region,
            const void* target,
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            const Metadata* parent_metadata
        )
        {
            using region_t = typename nil::xalt::tlist<Region...>::template at<I>;
            using graph_t = region_reachability_graph<API, region_t>;
            return graph_t::template make<Parent>(
                region,
                target,
                parent,
                qs,
                contexts,
                parent_metadata
            );
        }

    private:
        template <std::size_t... I>
        static consteval auto make_table(std::index_sequence<I...> /* indices */)
        {
            return std::array<maker_t, sizeof...(I)>{&make<I>...};
        }

    public:
        static constexpr auto table = make_table(std::make_index_sequence<sizeof...(Region)>{});
    };

    template <template <typename...> typename API, typename Regions>
    struct region_dispatcher;

    template <template <typename...> typename API, typename... Region>
    struct region_dispatcher<API, nil::xalt::tlist<Region...>>
    {
    public:
        template <typename Parent>
        static std::unique_ptr<IState> make(
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            std::size_t region,
            const Metadata* parent_metadata,
            const void* target
        )
        {
            using maker_t = region_maker<API, Parent, nil::xalt::tlist<Region...>>;
            if (region >= maker_t::table.size())
            {
                return {};
            }

            return maker_t::table[region](region, target, parent, qs, contexts, parent_metadata);
        }
    };

    template <typename MakeTransitState>
    void apply_region_runtime_action(
        const Event& e,
        on_event_t action,
        Region& region,
        const MakeTransitState& make_transit_state
    )
    {
        std::visit(
            [&]<typename Action>(Action& r)
            {
                if constexpr (std::is_same_v<Action, Event>)
                {
                    region.queues->push_emit(r);
                }
                else if constexpr (std::is_same_v<Action, Defer>)
                {
                    region.deferred.push_back(e.clone());
                }
                else if constexpr (std::is_same_v<Action, Transit>)
                {
                    region.transit_out();
                    region.active_state = make_transit_state(region.index, r.target);
                }
                else if constexpr (std::is_same_v<Action, Terminate>)
                {
                    region.transit_out();
                    region.active_state = make_transit_state(region.index, nil::xalt::type_id<Fin>);
                    region.terminated = true;
                }
            },
            action
        );
    }

    template <typename O, typename R>
    static O to_runtime_action_as(R r)
    {
        if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            return std::visit([](auto& v) { return to_runtime_action_as<O>(std::move(v)); }, r);
        }
        else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::Transit>)
        {
            return O{Transit{.target = nil::xalt::type_id<typename R::type>}};
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
        template <typename API, typename Event>
        static on_event_t invoke(
            typename API::state_t& state,
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
        template <typename API, typename Event>
        static on_event_t invoke(
            typename API::state_t& state,
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

    template <typename S, typename Events, typename Policy>
    struct action_dispatcher;

    template <typename S, typename... E, typename Policy>
    struct action_dispatcher<S, nil::xalt::tlist<E...>, Policy>
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
        static on_event_t invoke(state_t& state_value, const EV& event, api_context_t* api_contexts)
        {
            return Policy::template invoke<api_t>(state_value, event, api_contexts);
        }

        template <typename EV>
        static on_event_t call(state_t& state_value, const void* event, void* api_contexts)
        {
            return invoke<EV>(
                state_value,
                *static_cast<const EV*>(event),
                static_cast<api_context_t*>(api_contexts)
            );
        }

        static constexpr auto handlers = std::array<event_handler, sizeof...(E)>{
            event_handler{.id = nil::xalt::type_id<E>, .invoke = &call<E>}...
        };

    public:
        static on_event_t dispatch(const Event& event, state_t& state, api_context_t* api_contexts)
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

    template <typename S, typename Events>
    using event_dispatcher = action_dispatcher<S, Events, event_dispatch_policy>;

    template <typename S, typename Captures>
    using capture_dispatcher = action_dispatcher<S, Captures, capture_dispatch_policy>;
}
