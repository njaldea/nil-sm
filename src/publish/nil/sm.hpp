#pragma once

#include <nil/xalt/checks.hpp>
#include <nil/xalt/coalesce.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/tlist.hpp>
#include <nil/xalt/typed.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <queue>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nil::sm
{
    struct state_metadata final
    {
        std::size_t state = 0;
        std::size_t region = 0;
        std::size_t subregions = 1;
        std::string_view name;
        const state_metadata* parent = nullptr;
    };
}

namespace nil::sm::detail
{
    template <typename T>
    std::string_view type_name()
    {
        if constexpr (requires() { T::name; })
        {
            return T::name;
        }
        else
        {
            return nil::xalt::str_short_base_name_sv<T>;
        }
    }

    template <typename T>
    void deleter(void* v)
    {
        delete static_cast<T*>(v); // NOLINT
    }

    template <typename T>
    void* cloner(void* v)
    {
        return new T(*static_cast<T*>(v)); // NOLINT
    }

    class Queues;
    struct Contexts;
    struct IState;

    struct EvRegionsFinalized final
    {
        // This is the state instance owned by State<T>
        const void* target = nullptr;
    };

    struct Transit final
    {
        const void* target = nullptr;
    };

    struct Emit final
    {
        const void* id = nullptr;
        void (*deleter)(void*) = nullptr;
        void* (*cloner)(void*) = nullptr;
        void* data = nullptr;
    };
}

namespace nil::sm
{
    struct fin final
    {
        static constexpr auto name = "[**]";
    };

    struct root final
    {
    };

    struct Unhandled final
    {
    };

    struct Terminate final
    {
    };

    struct Forward final
    {
    };

    struct Defer final
    {
    };

    struct Discard final
    {
    };

    struct NOOP final
    {
    };

    template <typename T>
    struct Transit final
    {
        using type = T;
    };

    template <typename T>
    struct Emit final
    {
        static_assert(
            std::copy_constructible<T>,
            "Events emitted through Emit must be copy constructible."
        );

        template <typename... Args>
        explicit Emit(Args&&... args)
            : id(nil::xalt::type_id<T>)
            , deleter(&detail::deleter<T>)
            , cloner(&detail::cloner<T>)
            , data(new T{std::forward<Args>(args)...})
        {
        }

        Emit(Emit&& o) noexcept
            : id(o.id)
            , deleter(o.deleter)
            , cloner(o.cloner)
            , data(std::exchange(o.data, nullptr))
        {
        }

        Emit& operator=(Emit&& o) noexcept
        {
            if (this != &o)
            {
                if (data != nullptr)
                {
                    deleter(data);
                }

                id = o.id;
                deleter = o.deleter;
                cloner = o.cloner;
                data = std::exchange(o.data, nullptr);
            }
            return *this;
        }

        Emit(const Emit& o) = delete;
        Emit& operator=(const Emit& o) = delete;

        ~Emit()
        {
            if (data != nullptr)
            {
                deleter(data);
            }
        }

    private:
        using type = T;
        const void* id = nullptr;
        void (*deleter)(void*) = nullptr;
        void* (*cloner)(void*) = nullptr;
        void* data = nullptr;

        template <template <typename...> typename API, typename U>
        friend class State;
    };

    template <template <typename...> typename API, typename T>
    class State;
}

namespace nil::sm::detail
{
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

        template <typename EventList>
        struct collect_targets;

        template <typename... E>
        struct collect_targets<nil::xalt::tlist<E...>>
        {
            using type =
                typename nil::xalt::tlist<>::template join<typename transit_targets_from_action<
                    std::remove_cvref_t<event_result_t<E>>>::type...>;
        };

    public:
        using type = typename nil::xalt::tlist<>::template join<
            typename collect_targets<typename api_t::events_t>::type,
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
            const state_metadata* parent_metadata
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
                IState> (*ctor)(Parent*, Queues*, Contexts*, std::size_t, std::size_t, const state_metadata*)
                = nullptr;
        };

        template <typename Parent, typename Candidate>
        static std::unique_ptr<IState> create(
            Parent* parent,
            Queues* qs,
            Contexts* contexts,
            std::size_t region,
            std::size_t state,
            const state_metadata* parent_metadata
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
            const state_metadata* parent_metadata
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
            const state_metadata* parent_metadata
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
            const state_metadata* parent_metadata,
            std::index_sequence<I...> /* indices */
        )
        {
            using maker_t = std::unique_ptr<
                IState> (*)(std::size_t, const void*, Parent*, Queues*, Contexts*, const state_metadata*);

            static constexpr auto table
                = std::array<maker_t, sizeof...(I)>{&make_for_region<I, Parent>...};

            if (region >= table.size())
            {
                return {};
            }

            return table[region](region, target, parent, qs, contexts, parent_metadata);
        }
    };

    template <typename RegionsArray, typename PushDefer>
    void flush_deferred_events_for_region(
        RegionsArray& regions,
        std::size_t region_idx,
        PushDefer&& push_defer
    )
    {
        auto& deferred = regions[region_idx].deferred;
        for (auto& event : deferred)
        {
            std::forward<PushDefer>(push_defer)(event);
        }
        deferred.clear();
    }

    template <
        typename R,
        typename RegionsArray,
        typename MakeTransitState,
        typename MakeTerminatedState,
        typename PushEmit,
        typename PushDefer>
    void apply_region_runtime_action(
        std::size_t i,
        R& r,
        const Emit& e,
        RegionsArray& regions,
        MakeTransitState&& make_transit_state,
        MakeTerminatedState&& make_terminated_state,
        PushEmit&& push_emit,
        PushDefer&& push_defer
    )
    {
        if constexpr (std::is_same_v<R, Transit>)
        {
            regions[i].active_state.reset();
            regions[i].active_state
                = std::forward<MakeTransitState>(make_transit_state)(i, r.target);
            flush_deferred_events_for_region(regions, i, std::forward<PushDefer>(push_defer));
        }
        else if constexpr (std::is_same_v<R, Emit>)
        {
            std::forward<PushEmit>(push_emit)(r);
        }
        else if constexpr (std::is_same_v<R, Terminate>)
        {
            regions[i].active_state.reset();
            regions[i].active_state = std::forward<MakeTerminatedState>(make_terminated_state)(i);
            regions[i].terminated = true;
            flush_deferred_events_for_region(regions, i, std::forward<PushDefer>(push_defer));
        }
        else if constexpr (std::is_same_v<R, Defer>)
        {
            regions[i].deferred.push_back(Emit{
                .id = e.id,
                .deleter = e.deleter,
                .cloner = e.cloner,
                .data = e.cloner(e.data),
            });
        }
    }
}

namespace nil::sm::concepts
{
    template <typename T>
    concept is_allowed_to_use_for_on_event         //
        = std::is_same_v<T, Terminate>             //
        || std::is_same_v<T, Forward>              //
        || std::is_same_v<T, Defer>                //
        || std::is_same_v<T, Discard>              //
        || nil::xalt::is_of_template_v<T, Transit> //
        || nil::xalt::is_of_template_v<T, Emit>;

    template <typename T>
    struct is_allowed_to_use_for_react_as_predicate final
    {
        static constexpr bool value = is_allowed_to_use_for_on_event<T>;
    };

    template <typename T>
    concept is_allowed_to_use_for_on_event_result
        = is_allowed_to_use_for_on_event<std::remove_cvref_t<T>>
        || (nil::xalt::is_of_template_v<std::remove_cvref_t<T>, std::variant>
            && nil::xalt::to_tlist_t<std::remove_cvref_t<T>>::template all_of<
                is_allowed_to_use_for_react_as_predicate>);

    template <typename T, typename E>
    concept has_on_event = requires(T t, E event) {
        { t.on_event(event) } -> is_allowed_to_use_for_on_event_result;
    };

    template <typename T>
    concept is_allowed_to_use_for_lifecycle_hook
        = std::is_same_v<T, NOOP> || nil::xalt::is_of_template_v<T, Emit>;

    template <typename T>
    struct is_allowed_to_use_for_lifecycle_hook_as_predicate final
    {
        static constexpr bool value = is_allowed_to_use_for_lifecycle_hook<T>;
    };

    template <typename T>
    concept has_on_enter = requires(T t) {
        { t.on_enter() } -> is_allowed_to_use_for_lifecycle_hook;
    } || requires(T t) {
        requires nil::xalt::is_of_template_v<decltype(t.on_enter()), std::variant>;
        requires nil::xalt::to_tlist_t<decltype(t.on_enter()
        )>::template all_of<is_allowed_to_use_for_lifecycle_hook_as_predicate>;
    };

    template <typename T>
    concept has_on_exit = requires(T t) {
        { t.on_exit() } -> is_allowed_to_use_for_lifecycle_hook;
    } || requires(T t) {
        requires nil::xalt::is_of_template_v<decltype(t.on_exit()), std::variant>;
        requires nil::xalt::to_tlist_t<decltype(t.on_exit()
        )>::template all_of<is_allowed_to_use_for_lifecycle_hook_as_predicate>;
    };

    template <typename T>
    concept is_allowed_to_use_for_on_regions_finalized //
        = std::is_same_v<T, NOOP>                      //
        || std::is_same_v<T, Terminate>                //
        || nil::xalt::is_of_template_v<T, Transit>     //
        || nil::xalt::is_of_template_v<T, Emit>;

    template <typename T>
    struct is_allowed_to_use_for_on_regions_finalized_as_predicate final
    {
        static constexpr bool value = is_allowed_to_use_for_on_regions_finalized<T>;
    };

    template <typename T>
    concept has_on_regions_finalized = requires(T t) {
        { t.on_regions_finalized() } -> is_allowed_to_use_for_on_regions_finalized;
    } || requires(T t) {
        requires nil::xalt::is_of_template_v<decltype(t.on_regions_finalized()), std::variant>;
        requires nil::xalt::to_tlist_t<decltype(t.on_regions_finalized()
        )>::template all_of<is_allowed_to_use_for_on_regions_finalized_as_predicate>;
    };
}

namespace nil::sm::detail
{
    NIL_XALT_COALESCE_TAG(regions, nil::xalt::tlist<>);
    NIL_XALT_COALESCE_TAG(events, nil::xalt::tlist<>);

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
        explicit IState(state_metadata init_metadata)
            : metadata(init_metadata)
        {
        }

        IState(IState&&) = delete;
        IState(const IState&) = delete;
        IState& operator=(IState&&) = delete;
        IState& operator=(const IState&) = delete;
        virtual ~IState() = default;

        virtual on_event_t on_event(const Emit& e) = 0;

        const state_metadata metadata;
    };

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
            return S::template to_runtime_action_as<on_event_t>(result);
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
        std::unique_ptr<IState> active_state;
        std::vector<Emit> deferred;
        bool terminated = false;
    };

    struct Contexts
    {
        void* state;
        void* api;
    };
}

namespace nil::sm
{
    template <template <typename...> typename API, typename T>
    class State: public detail::IState
    {
    public:
        using api_t = API<T>;
        using self_t = State<API, T>;
        using metadata_t = state_metadata;

    private:
        using state_t = typename api_t::state_t;
        using regions_t = typename api_t::regions_t;
        using events_t = typename api_t::events_t;
        using event_dispatch_t = detail::event_dispatcher<self_t, events_t>;
        using state_context_t = typename api_t::state_context_t;
        using api_context_t = typename api_t::api_context_t;
        using on_event_results_t = std::array<detail::on_event_t, regions_t::size>;

        using region_dispatcher_t = detail::region_dispatcher<API, regions_t>;

        struct sub_state_scan_t
        {
            bool handle = false;
            bool forward = false;
            on_event_results_t results = {};
        };

        template <typename... R, std::size_t... I>
        static std::array<detail::Region, regions_t::size> init_regions(
            [[maybe_unused]] self_t* self,
            [[maybe_unused]] detail::Queues* qs,
            [[maybe_unused]] detail::Contexts* contexts,
            nil::xalt::tlist<R...> /* regions */,
            std::index_sequence<I...> /* region indices */
        )
        {
            auto on_enter_result = self->on_enter();
            if (std::holds_alternative<detail::Emit>(on_enter_result))
            {
                qs->push_emit(std::get<detail::Emit>(on_enter_result));
            }

            return std::array<detail::Region, regions_t::size>{detail::Region{
                std::make_unique<State<API, R>>(
                    std::addressof(self->current_state),
                    qs,
                    contexts,
                    I,
                    0,
                    std::addressof(self->metadata)
                ),
                {},
                false
            }...};
        }

    public:
        template <typename Parent>
        explicit State(
            Parent* init_parent,
            detail::Queues* init_qs,
            detail::Contexts* init_contexts,
            std::size_t init_region,
            std::size_t init_state,
            const metadata_t* init_parent_metadata
        )
            : detail::IState(state_metadata{
                  .state = init_state,
                  .region = init_region,
                  .subregions = regions_t::size,
                  .name = detail::type_name<T>(),
                  .parent = init_parent_metadata
              })
            , qs(init_qs)
            , contexts(init_contexts)
            , current_state(api_t::make(
                  init_parent,
                  static_cast<state_context_t*>(contexts->state),
                  static_cast<api_context_t*>(contexts->api),
                  this->metadata
              ))
            , regions(init_regions(
                  this,
                  qs,
                  contexts,
                  regions_t(),
                  std::make_index_sequence<regions_t::size>()
              ))
        {
        }

        State(State&&) = delete;
        State(const State&) = delete;
        State& operator=(State&&) = delete;
        State& operator=(const State&) = delete;

        ~State() override
        {
            for (auto it = regions.rbegin(); it != regions.rend(); ++it)
            {
                it->active_state.reset();
            }

            for (auto i = 0U; i < regions_t::size; ++i)
            {
                const auto index = i;
                detail::flush_deferred_events_for_region(
                    regions,
                    index,
                    [this](const detail::Emit& event) { qs->push_defer(event); }
                );
            }

            auto on_exit_result = on_exit();
            if (std::holds_alternative<detail::Emit>(on_exit_result))
            {
                qs->push_emit(std::get<detail::Emit>(on_exit_result));
            }
        }

        detail::on_event_t on_event(const detail::Emit& e) override
        {
            const auto is_regions_finalized_event
                = e.id == nil::xalt::type_id<detail::EvRegionsFinalized>;
            if (is_regions_finalized_event)
            {
                const auto* ev = static_cast<const detail::EvRegionsFinalized*>(e.data);
                if (ev->target == std::addressof(current_state))
                {
                    finalized = true;
                    return std::visit(
                        []<typename V>(V v) -> detail::on_event_t
                        {
                            if constexpr (std::is_same_v<NOOP, V>)
                            {
                                return Discard();
                            }
                            else
                            {
                                return v;
                            }
                        },
                        on_regions_finalized()
                    );
                }
            }

            auto sub_state = dispatch_to_regions(e);

            if (!is_regions_finalized_event && sub_state.handle)
            {
                auto this_result = event_dispatch_t::dispatch(
                    e,
                    current_state,
                    static_cast<api_context_t*>(contexts->api)
                );
                if (std::holds_alternative<Unhandled>(this_result))
                {
                    commit_region_results(e, sub_state.results);
                    if (sub_state.forward)
                    {
                        return Forward();
                    }

                    return Unhandled();
                }

                if (!std::holds_alternative<detail::Transit>(this_result))
                {
                    commit_region_results(e, sub_state.results);
                }

                if (std::holds_alternative<detail::Emit>(this_result))
                {
                    qs->push_emit(std::get<detail::Emit>(this_result));
                    return Discard();
                }

                return this_result;
            }

            commit_region_results(e, sub_state.results);
            return Discard();
        }

    private:
        detail::Queues* qs;
        detail::Contexts* contexts;
        state_t current_state;
        std::array<detail::Region, regions_t::size> regions;
        bool finalized = false;

        detail::on_enter_t on_enter()
        {
            auto result
                = api_t::on_enter(current_state, static_cast<api_context_t*>(contexts->api));
            return to_runtime_action_as<detail::on_enter_t>(result);
        }

        detail::on_exit_t on_exit()
        {
            auto result = api_t::on_exit(current_state, static_cast<api_context_t*>(contexts->api));
            return to_runtime_action_as<detail::on_exit_t>(result);
        }

        detail::on_regions_finalized_t on_regions_finalized()
        {
            auto result = api_t::on_regions_finalized(
                current_state,
                static_cast<api_context_t*>(contexts->api)
            );
            return to_runtime_action_as<detail::on_regions_finalized_t>(result);
        }

        sub_state_scan_t dispatch_to_regions(const detail::Emit& e)
        {
            sub_state_scan_t scan = {};
            auto no_region_handled = true;
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                scan.results[i] = regions[i].active_state->on_event(e);
                std::visit(
                    [&]<typename R>(const R& /* r */)
                    {
                        if constexpr (std::is_same_v<R, Forward>)
                        {
                            scan.forward = true;
                            no_region_handled = false;
                        }
                        else if constexpr (!std::is_same_v<R, Unhandled>)
                        {
                            no_region_handled = false;
                        }
                    },
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    scan.results[i]
                );
            }

            scan.handle = scan.forward || no_region_handled;

            return scan;
        }

    public:
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
                return O{detail::Emit{
                    .id = r.id,
                    .deleter = r.deleter,
                    .cloner = r.cloner,
                    .data = std::exchange(r.data, nullptr)
                }};
            }
            else
            {
                return O{r};
            }
        }

    private:
        void commit_region_results(const detail::Emit& e, on_event_results_t& sub_state_result)
        {
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                std::visit(
                    [&]<typename R>(R& r)
                    {
                        detail::apply_region_runtime_action(
                            i,
                            r,
                            e,
                            regions,
                            [this](std::size_t idx, const void* target)
                            {
                                return region_dispatcher_t::template make(
                                    idx,
                                    target,
                                    std::addressof(current_state),
                                    qs,
                                    contexts,
                                    std::addressof(this->metadata)
                                );
                            },
                            [this](std::size_t idx)
                            {
                                return std::make_unique<State<API, fin>>(
                                    std::addressof(current_state),
                                    qs,
                                    contexts,
                                    idx,
                                    0,
                                    std::addressof(this->metadata)
                                );
                            },
                            [this](const detail::Emit& event) { qs->push_emit(event); },
                            [this](const detail::Emit& event) { qs->push_defer(event); }
                        );
                    },
                    sub_state_result[i]
                );
            }

            if (!finalized && !regions.empty()
                && std::all_of(
                    regions.begin(),
                    regions.end(),
                    [](const auto& r) { return r.terminated; }
                ))
            {
                auto r = Emit<detail::EvRegionsFinalized>(std::addressof(current_state));
                qs->push_emit( //
                    detail::Emit{
                        .id = r.id,
                        .deleter = r.deleter,
                        .cloner = r.cloner,
                        .data = std::exchange(r.data, nullptr)
                    }
                );
            }
        }
    };

    template <template <typename...> typename API, typename... Regions>
    class RootState: public detail::IState
    {
    public:
        static_assert(sizeof...(Regions) > 0);
        using metadata_t = state_metadata;

    private:
        using state_t = root;
        using regions_t = nil::xalt::tlist<Regions...>;
        using on_event_results_t = std::array<detail::on_event_t, regions_t::size>;
        using region_dispatcher_t = detail::region_dispatcher<API, regions_t>;

        template <std::size_t... I>
        static std::array<detail::Region, regions_t::size> init_regions(
            [[maybe_unused]] state_t* self,
            [[maybe_unused]] detail::Queues* qs,
            [[maybe_unused]] detail::Contexts* contexts,
            std::index_sequence<I...> /* region indices */
        )
        {
            return std::array<detail::Region, regions_t::size>{detail::Region{
                std::make_unique<State<API, Regions>>(self, qs, contexts, I, 0, nullptr),
                {},
                false
            }...};
        }

    public:
        explicit RootState(detail::Queues* init_qs, detail::Contexts* init_contexts)
            : detail::IState(state_metadata{
                  .state = 0,
                  .region = 0,
                  .subregions = regions_t::size,
                  .name = "[--]",
                  .parent = nullptr
              })
            , qs(init_qs)
            , contexts(init_contexts)
            , regions(init_regions(
                  &root_state,
                  qs,
                  contexts,
                  std::index_sequence_for<Regions...>() //
              ))
        {
        }

        RootState(RootState&&) = delete;
        RootState(const RootState&) = delete;
        RootState& operator=(RootState&&) = delete;
        RootState& operator=(const RootState&) = delete;

        ~RootState() override
        {
            for (auto it = regions.rbegin(); it != regions.rend(); ++it)
            {
                it->active_state.reset();
            }

            for (auto i = 0U; i < regions_t::size; ++i)
            {
                const auto index = i;
                detail::flush_deferred_events_for_region(
                    regions,
                    index,
                    [this](const detail::Emit& event) { qs->push_defer(event); }
                );
            }
        }

        detail::on_event_t on_event(const detail::Emit& e) override
        {
            on_event_results_t results;
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                results[i] = regions[i].active_state->on_event(e);
            }
            commit_region_results(e, results);
            return Discard();
        }

    private:
        detail::Queues* qs;
        detail::Contexts* contexts;
        state_t root_state;
        std::array<detail::Region, regions_t::size> regions;

        void commit_region_results(const detail::Emit& e, on_event_results_t& sub_state_result)
        {
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                std::visit(
                    [&]<typename R>(R& r)
                    {
                        detail::apply_region_runtime_action(
                            i,
                            r,
                            e,
                            regions,
                            [this](std::size_t idx, const void* target)
                            {
                                return region_dispatcher_t::template make(
                                    idx,
                                    target,
                                    std::addressof(root_state),
                                    qs,
                                    contexts,
                                    nullptr
                                );
                            },
                            [this](std::size_t idx) {
                                return std::make_unique<State<API, fin>>(
                                    &root_state,
                                    qs,
                                    contexts,
                                    idx,
                                    0,
                                    nullptr
                                );
                            },
                            [this](const detail::Emit& event) { qs->push_emit(event); },
                            [this](const detail::Emit& event) { qs->push_defer(event); }
                        );
                    },
                    sub_state_result[i]
                );
            }
        }
    };

    template <typename T, typename S = void, typename A = void>
    struct default_api final
    {
        using state_t = T;
        using state_context_t = S;
        using api_context_t = A;
        using regions_t = nil::xalt::coalesce_t<T, detail::regions_tag>;
        using events_t = nil::xalt::coalesce_t<T, detail::events_tag>;

        template <typename Parent>
        static state_t make(
            Parent* parent,
            state_context_t* state_contexts,
            api_context_t* /* api_contexts */,
            state_metadata /* metadata */
        )
        {
            if constexpr (requires() { T(parent, state_contexts); })
            {
                return T(parent, state_contexts);
            }
            else if constexpr (requires() { T(parent); })
            {
                return T(parent);
            }
            else
            {
                static_assert(
                    std::is_default_constructible_v<T>,
                    "State must be constructible from parent/contexts or "
                    "default-constructible"
                );
                return T();
            }
        }

        template <typename E>
        static auto on_event(
            state_t& state,
            const E& event,
            api_context_t* /* api_contexts */
        )
        {
            static_assert(concepts::has_on_event<state_t, E>);
            return state.on_event(event);
        }

        static auto on_enter(state_t& state, api_context_t* /* api_contexts */)
        {
            if constexpr (concepts::has_on_enter<state_t>)
            {
                return state.on_enter();
            }
            else
            {
                return Unhandled();
            }
        }

        static auto on_exit(state_t& state, api_context_t* /* api_contexts */)
        {
            if constexpr (concepts::has_on_exit<state_t>)
            {
                return state.on_exit();
            }
            else
            {
                return Unhandled();
            }
        }

        static auto on_regions_finalized(state_t& state, api_context_t* /* api_contexts */)
        {
            if constexpr (concepts::has_on_regions_finalized<state_t>)
            {
                return state.on_regions_finalized();
            }
            else
            {
                return Unhandled();
            }
        }
    };

    template <template <typename...> typename API>
    struct coalesce_api final
    {
        template <typename T>
        struct type final
        {
            using inner_t = T;
            NIL_XALT_COALESCE_TAG(state_context_t, void*);
            NIL_XALT_COALESCE_TAG(api_context_t, void*);

            using state_context_t = nil::xalt::coalesce_t<API<T>, state_context_t_tag>;
            using api_context_t = nil::xalt::coalesce_t<API<T>, api_context_t_tag>;

            using defaulter_t = default_api<inner_t, state_context_t, api_context_t>;
            NIL_XALT_COALESCE_TAG(state_t, defaulter_t::state_t);
            NIL_XALT_COALESCE_TAG(events_t, defaulter_t::events_t);
            NIL_XALT_COALESCE_TAG(regions_t, defaulter_t::regions_t);

            using state_t = nil::xalt::coalesce_t<T, state_t_tag>;
            using regions_t = nil::xalt::coalesce_t<T, regions_t_tag>;
            using events_t = nil::xalt::coalesce_t<T, events_t_tag>;

            template <typename Parent>
            static state_t make(
                Parent* parent,
                state_context_t* state_contexts,
                api_context_t* api_contexts,
                const state_metadata& metadata
            )
            {
                static constexpr auto api_has_make
                    = requires() { API<T>::make(parent, state_contexts, api_contexts, metadata); };

                if constexpr (api_has_make)
                {
                    return API<T>::make(parent, state_contexts, api_contexts, metadata);
                }
                else
                {
                    return defaulter_t::make(parent, state_contexts, api_contexts, metadata);
                }
            }

            template <typename E>
            static auto on_event(state_t& state, const E& event, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_event(state, event, api_contexts); })
                {
                    return API<T>::on_event(state, event, api_contexts);
                }
                else
                {
                    return defaulter_t::on_event(state, event, api_contexts);
                }
            }

            static auto on_enter(state_t& state, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_enter(state, api_contexts); })
                {
                    return API<T>::on_enter(state, api_contexts);
                }
                else
                {
                    return defaulter_t::on_enter(state, api_contexts);
                }
            }

            static auto on_exit(state_t& state, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_exit(state, api_contexts); })
                {
                    return API<T>::on_exit(state, api_contexts);
                }
                else
                {
                    return defaulter_t::on_exit(state, api_contexts);
                }
            }

            static auto on_regions_finalized(state_t& state, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_regions_finalized(state, api_contexts); })
                {
                    return API<T>::on_regions_finalized(state, api_contexts);
                }
                else
                {
                    return defaulter_t::on_regions_finalized(state, api_contexts);
                }
            }
        };
    };

    class ISM
    {
    public:
        ISM() = default;
        ISM(ISM&) = delete;
        ISM(const ISM&&) = delete;
        ISM& operator=(ISM&&) = delete;
        ISM& operator=(const ISM&) = delete;
        virtual ~ISM() noexcept = default;

        template <typename T>
        void post(T event)
        {
            post_impl(detail::Emit{
                .id = nil::xalt::type_id<T>,
                .deleter = &detail::deleter<T>,
                .cloner = &detail::cloner<T>,
                .data = &event
            });
        }

    private:
        virtual void post_impl(detail::Emit event) = 0;
    };

    template <template <typename...> typename API, typename... Regions>
    class SM final: public ISM
    {
        using api_t = API<struct ST>;
        using regions_t = typename api_t::regions_t;
        using state_context_t = typename api_t::state_context_t;
        using api_context_t = typename api_t::api_context_t;

    public:
        explicit SM(state_context_t* state_contexts, api_context_t* api_contexts)
            : contexts({.state = state_contexts, .api = api_contexts})
            , state(&queues, &contexts)
        {
            queues.flush(state);
        }

        ~SM() noexcept override = default;

        SM(const SM&) = delete;
        SM& operator=(const SM&) = delete;

        SM(SM&&) noexcept = delete;
        SM& operator=(SM&&) noexcept = delete;

    private:
        detail::Queues queues;
        detail::Contexts contexts;
        RootState<API, Regions...> state;

        void post_impl(detail::Emit event) override
        {
            state.on_event(event);
            queues.flush(state);
        }
    };

    template <typename... Regions>
    using DefaultSM = SM<default_api, Regions...>;

    template <template <typename...> typename API, typename... Regions>
    using CoalescedSM = SM<coalesce_api<API>::template type, Regions...>;
}
