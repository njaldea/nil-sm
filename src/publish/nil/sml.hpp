#pragma once

#include <nil/xalt/checks.hpp>
#include <nil/xalt/coalesce.hpp>
#include <nil/xalt/tlist.hpp>
#include <nil/xalt/typed.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <queue>
#include <type_traits>
#include <variant>

namespace nil::sml
{
    namespace detail
    {
        struct IState;

        struct Event final
        {
            const void* id = nullptr;
            const void* event = nullptr;
        };

        struct EvRegionsComplete
        {
            // This is the state instance owned by State<T>
            const void* target = nullptr;
        };

        struct Unhandled final
        {
        };

        struct Transit final
        {
            std::unique_ptr<sml::detail::IState> (*to)(void*, void*, void*, void*);
        };

        struct Emit final
        {
            const void* id = nullptr;
            std::unique_ptr<void, void (*)(void*)> data;
        };
    }

    struct Terminate final
    {
    };

    struct Forward final
    {
    };

    struct Discard final
    {
    };

    struct NOOP final
    {
    };

    template <typename T>
    struct Transit
    {
        using type = T;
    };

    template <typename T>
    struct Emit
    {
        using type = T;
        const void* id = nullptr;
        std::unique_ptr<void, void (*)(void*)> data;

        template <typename... Args>
        explicit Emit(Args&&... args)
            : id(nil::xalt::type_id<T>)
            , data(
                  new T(std::forward<Args>(args)...),
                  // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
                  [](void* v) { delete static_cast<T*>(v); }
              )
        {
        }

        operator detail::Emit() && // NOLINT
        {
            return detail::Emit{id, std::move(data)};
        }
    };

    using on_event_t = std::variant<
        Terminate,
        Forward,
        Discard,
        detail::Transit,
        detail::Emit,
        detail::Unhandled //
        >;
    using on_enter_t = std::variant<NOOP, detail::Emit>;
    using on_exit_t = std::variant<NOOP, detail::Emit>;
    using on_regions_complete_t = std::variant<NOOP, detail::Transit, detail::Emit>;

    namespace concepts
    {
        template <typename T>
        concept has_events = requires() { typename T::events; };

        template <typename T>
        concept has_regions = requires() { typename T::regions; };

        template <typename T>
        concept is_allowed_to_use_for_on_event         //
            = std::is_same_v<T, Terminate>             //
            || std::is_same_v<T, Forward>              //
            || std::is_same_v<T, Discard>              //
            || nil::xalt::is_of_template_v<T, Transit> //
            || nil::xalt::is_of_template_v<T, Emit>;

        template <typename T>
        struct is_allowed_to_use_for_react_as_predicate
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
        struct is_allowed_to_use_for_lifecycle_hook_as_predicate
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
        concept is_allowed_to_use_for_on_regions_complete //
            = std::is_same_v<T, NOOP>                     //
            || std::is_same_v<T, Terminate>               //
            || nil::xalt::is_of_template_v<T, Transit>    //
            || nil::xalt::is_of_template_v<T, Emit>;

        template <typename T>
        struct is_allowed_to_use_for_on_regions_complete_as_predicate
        {
            static constexpr bool value = is_allowed_to_use_for_on_regions_complete<T>;
        };

        template <typename T>
        concept has_on_regions_complete = requires(T t) {
            { t.on_regions_complete() } -> is_allowed_to_use_for_on_regions_complete;
        } || requires(T t) {
            requires nil::xalt::is_of_template_v<decltype(t.on_regions_complete()), std::variant>;
            requires nil::xalt::to_tlist_t<decltype(t.on_regions_complete()
            )>::template all_of<is_allowed_to_use_for_on_regions_complete_as_predicate>;
        };
    }

    namespace detail
    {
        NIL_XALT_COALESCE_TAG(regions, nil::xalt::tlist<>);
        NIL_XALT_COALESCE_TAG(events, nil::xalt::tlist<>);

        struct IState
        {
            IState() = default;
            IState(IState&&) = delete;
            IState(const IState&) = delete;
            IState& operator=(IState&&) = delete;
            IState& operator=(const IState&) = delete;
            virtual ~IState() = default;

            virtual on_event_t on_event(Event e) = 0;
        };

        template <typename S, typename E, typename A>
        struct event_dispatcher;

        template <typename S, typename... E, typename... A>
        struct event_dispatcher<S, nil::xalt::tlist<E...>, nil::xalt::tlist<A...>>
        {
        private:
            using api_t = typename S::api_t;
            using state_t = typename api_t::state_t;

            struct event_handler
            {
                const void* id = nullptr;
                on_event_t (*invoke)(state_t&, const void*, std::tuple<A*...>*) = nullptr;
            };

            template <typename EV>
            static on_event_t call(
                state_t& state_value,
                const void* event,
                std::tuple<A*...>* api_contexts
            )
            {
                static_assert(
                    requires(state_t& state_value_ref, const EV& ev) {
                        {
                            api_t::template on_event<EV>(state_value_ref, ev)
                        } -> concepts::is_allowed_to_use_for_on_event_result;
                    },
                    "API must expose on_event<Event>(state_value, event) with an allowed "
                    "return type"
                );
                auto result = std::apply(
                    [&](A*... contexts) {
                        return api_t::template on_event<EV>(
                            state_value,
                            *static_cast<const EV*>(event),
                            contexts...
                        );
                    },
                    *api_contexts
                );
                return S::template to_runtime_action_as<on_event_t>(result);
            }

            static constexpr auto handlers = std::array<event_handler, sizeof...(E)>{
                event_handler{.id = nil::xalt::type_id<E>, .invoke = &call<E>}...
            };

        public:
            static on_event_t dispatch(Event event, state_t& state, std::tuple<A*...>* api_contexts)
            {
                for (const auto& handler : handlers)
                {
                    if (handler.id == event.id)
                    {
                        return handler.invoke(state, event.event, api_contexts);
                    }
                }

                return detail::Unhandled{};
            }
        };

        using queue_t = std::queue<detail::Emit>;

        template <
            typename T,
            typename StateContexts,
            typename APIContexts,
            template <typename>
            typename API>
        class State;

        template <
            typename T,
            typename... StateContexts,
            typename... APIContexts,
            template <typename>
            typename API>
        class State<T, nil::xalt::tlist<StateContexts...>, nil::xalt::tlist<APIContexts...>, API>
            : public IState
        {
        public:
            using api_t = API<T>;
            using self_t = State<
                T,
                nil::xalt::tlist<StateContexts...>,
                nil::xalt::tlist<APIContexts...>,
                API>;

        private:
            using state_t = typename api_t::state_t;
            using regions_t = typename api_t::regions_t;
            using events_t = typename api_t::events_t;
            using event_dispatch_t
                = detail::event_dispatcher<self_t, events_t, nil::xalt::tlist<APIContexts...>>;

            struct sub_state_scan_t
            {
                bool handle = false;
                bool forward = false;
                std::array<on_event_t, regions_t::size> results = {};
            };

            template <typename U>
            static std::unique_ptr<IState> make_state(void* p, void* q, void* s, void* a)
            {
                return std::make_unique<State<
                    U,
                    nil::xalt::tlist<StateContexts...>,
                    nil::xalt::tlist<APIContexts...>,
                    API>>(
                    static_cast<state_t*>(p),
                    static_cast<std::queue<detail::Emit>*>(q),
                    static_cast<std::tuple<StateContexts*...>*>(s),
                    static_cast<std::tuple<APIContexts*...>*>(a)
                );
            }

            template <typename... R>
            static std::array<std::unique_ptr<IState>, regions_t::size> init_regions(
                [[maybe_unused]] self_t* parent,
                [[maybe_unused]] queue_t* queue,
                [[maybe_unused]] std::tuple<StateContexts*...>* state_contexts,
                [[maybe_unused]] std::tuple<APIContexts*...>* api_contexts,
                nil::xalt::tlist<R...> /* regions */
            )
            {
                auto on_enter_result = parent->on_enter();
                if (std::holds_alternative<detail::Emit>(on_enter_result))
                {
                    queue->push(std::move(std::get<detail::Emit>(on_enter_result)));
                }

                return {make_state<R>(
                    std::addressof(parent->current_state),
                    queue,
                    state_contexts,
                    api_contexts
                )...};
            }

        public:
            template <typename Parent>
            explicit State(
                Parent* init_parent,
                queue_t* init_queue,
                std::tuple<StateContexts*...>* init_state_contexts,
                std::tuple<APIContexts*...>* init_api_contexts
            )
                : state_contexts(init_state_contexts)
                , api_contexts(init_api_contexts)
                , queue(init_queue)
                , current_state(std::apply(
                      [&](StateContexts*... contexts)
                      { return api_t::make(init_parent, contexts...); },
                      *init_state_contexts
                  ))
                , regions(init_regions(this, queue, state_contexts, api_contexts, regions_t()))
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
                    it->reset();
                }

                auto on_exit_result = on_exit();
                if (std::holds_alternative<detail::Emit>(on_exit_result))
                {
                    queue->push(std::move(std::get<detail::Emit>(on_exit_result)));
                }
            }

            on_event_t on_event(Event e) override
            {
                const auto is_regions_complete_event
                    = e.id == nil::xalt::type_id<EvRegionsComplete>;
                if (is_regions_complete_event)
                {
                    const auto* ev = static_cast<const EvRegionsComplete*>(e.event);
                    if (ev->target == std::addressof(current_state))
                    {
                        completed = true;
                        return std::visit(
                            []<typename V>(V v) -> on_event_t
                            {
                                if constexpr (std::is_same_v<NOOP, V>)
                                {
                                    return Discard();
                                }
                                else
                                {
                                    return std::move(v);
                                }
                            },
                            on_regions_complete()
                        );
                    }
                }

                auto sub_state = scan_regions(e);

                if (!is_regions_complete_event && sub_state.handle)
                {
                    auto this_result = event_dispatch_t::dispatch(e, current_state, api_contexts);
                    if (std::holds_alternative<detail::Unhandled>(this_result))
                    {
                        apply_sub_transits(sub_state.results);
                        if (sub_state.forward)
                        {
                            return Forward{};
                        }

                        return detail::Unhandled{};
                    }

                    if (!std::holds_alternative<detail::Transit>(this_result))
                    {
                        apply_sub_transits(sub_state.results);
                    }

                    if (std::holds_alternative<detail::Emit>(this_result))
                    {
                        queue->push(std::move(std::get<detail::Emit>(this_result)));
                        return Discard{};
                    }

                    return this_result;
                }

                apply_sub_transits(sub_state.results);
                return Discard{};
            }

        private:
            std::tuple<StateContexts*...>* state_contexts;
            std::tuple<APIContexts*...>* api_contexts;

            queue_t* queue;
            state_t current_state;
            std::array<std::unique_ptr<IState>, regions_t::size> regions;
            bool completed = false;

            on_enter_t on_enter()
            {
                auto result = std::apply(
                    [&](APIContexts*... contexts)
                    { return api_t::on_enter(current_state, contexts...); },
                    *api_contexts
                );
                return to_runtime_action_as<on_enter_t>(result);
            }

            on_exit_t on_exit()
            {
                auto result = std::apply(
                    [&](APIContexts*... contexts)
                    { return api_t::on_exit(current_state, contexts...); },
                    *api_contexts
                );
                return to_runtime_action_as<on_exit_t>(result);
            }

            on_regions_complete_t on_regions_complete()
            {
                auto result = std::apply(
                    [&](APIContexts*... contexts)
                    { return api_t::on_regions_complete(current_state, contexts...); },
                    *api_contexts
                );
                return to_runtime_action_as<on_regions_complete_t>(result);
            }

            sub_state_scan_t scan_regions(Event e)
            {
                sub_state_scan_t scan = {};
                auto all_unhandled = true;
                for (auto i = 0U; i < regions_t::size; ++i)
                {
                    if (!regions[i])
                    {
                        // A null region represents a terminated node.
                        scan.results[i] = detail::Unhandled{};
                        continue;
                    }

                    scan.results[i] = regions[i]->on_event(e);
                    std::visit(
                        [&]<typename R>(const R& /* r */)
                        {
                            if constexpr (std::is_same_v<R, Forward>)
                            {
                                scan.forward = true;
                                all_unhandled = false;
                            }
                            else if constexpr (!std::is_same_v<R, detail::Unhandled>)
                            {
                                all_unhandled = false;
                            }
                        },
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                        scan.results[i]
                    );
                }

                scan.handle = scan.forward || all_unhandled;

                return scan;
            }

        public:
            template <typename O, typename R>
            static O to_runtime_action_as(R& r)
            {
                if constexpr (nil::xalt::is_of_template_v<std::remove_cvref_t<R>, std::variant>)
                {
                    return std::visit(
                        []<typename V>(V& v) { return to_runtime_action_as<O>(v); },
                        r
                    );
                }
                else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sml::Transit>)
                {
                    return O{detail::Transit{.to = &make_state<typename R::type>}};
                }
                else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sml::Emit>)
                {
                    return O{detail::Emit{.id = r.id, .data = std::move(r.data)}};
                }
                else
                {
                    return O{r};
                }
            }

        private:
            void apply_sub_transits(std::array<on_event_t, regions_t::size>& sub_state_result)
            {
                for (auto i = 0U; i < regions_t::size; ++i)
                {
                    std::visit(
                        [&]<typename R>(R& r)
                        {
                            if constexpr (std::is_same_v<R, detail::Transit>)
                            {
                                regions[i].reset();
                                regions[i] = r.to(
                                    std::addressof(current_state),
                                    queue,
                                    state_contexts,
                                    api_contexts
                                );
                            }
                            else if constexpr (std::is_same_v<R, Terminate>)
                            {
                                regions[i].reset();
                            }
                            else if constexpr (std::is_same_v<R, detail::Emit>)
                            {
                                queue->push(std::move(r));
                            }
                        },
                        sub_state_result[i]
                    );
                }

                if (!completed && !regions.empty()
                    && std::all_of(
                        regions.begin(),
                        regions.end(),
                        [](const auto& r) { return r == nullptr; }
                    ))
                {
                    queue->push(sml::Emit<EvRegionsComplete>(std::addressof(current_state)));
                }
            }
        };

        template <typename T>
        struct extract_type;

        template <template <typename> typename T, typename U>
        struct extract_type<T<U>>
        {
            using type = U;
        };

        template <typename T>
        struct default_api
        {
            using state_t = T;
            using regions_t = nil::xalt::coalesce_t<T, regions_tag>;
            using events_t = nil::xalt::coalesce_t<T, events_tag>;

            template <typename Parent, typename... Contexts>
            static state_t make(Parent* parent, Contexts*... contexts)
            {
                if constexpr (std::is_constructible_v<T, Parent*, Contexts*...>)
                {
                    return T(parent, contexts...);
                }
                else
                {
                    static_assert(
                        std::is_default_constructible_v<T>,
                        "State must be constructible from parent/contexts or "
                        "default-constructible"
                    );
                    return T{};
                }
            }

            template <typename E>
            static auto on_event(state_t& state, const E& event, auto*... api_contexts)
            {
                if constexpr (concepts::has_on_event<state_t, E>)
                {
                    return state.on_event(event, api_contexts...);
                }
                else
                {
                    return Unhandled();
                }
            }

            static auto on_enter(state_t& state, auto*... api_contexts)
            {
                if constexpr (concepts::has_on_enter<state_t>)
                {
                    return state.on_enter(api_contexts...);
                }
                else
                {
                    return NOOP();
                }
            }

            static auto on_exit(state_t& state, auto*... api_contexts)
            {
                if constexpr (concepts::has_on_exit<state_t>)
                {
                    return state.on_exit(api_contexts...);
                }
                else
                {
                    return NOOP();
                }
            }

            static auto on_regions_complete(state_t& state, auto*... api_contexts)
            {
                if constexpr (concepts::has_on_regions_complete<state_t>)
                {
                    return state.on_regions_complete(api_contexts...);
                }
                else
                {
                    return NOOP();
                }
            }
        };
    }

    template <typename T>
    struct coalesce_api
    {
        using inner_t = typename detail::extract_type<T>::type;
        NIL_XALT_COALESCE_TAG(state_t, detail::default_api<inner_t>::state_t)
        NIL_XALT_COALESCE_TAG(events_t, detail::default_api<inner_t>::events_t)
        NIL_XALT_COALESCE_TAG(regions_t, detail::default_api<inner_t>::regions_t)

        using state_t = nil::xalt::coalesce_t<T, state_t_tag>;
        using regions_t = nil::xalt::coalesce_t<T, regions_t_tag>;
        using events_t = nil::xalt::coalesce_t<T, events_t_tag>;

        template <typename Parent, typename... Contexts>
        static state_t make(Parent* parent, Contexts*... contexts)
        {
            if constexpr (requires() { T::make(parent, contexts...); })
            {
                return T::make(parent, contexts...);
            }
            else
            {
                return detail::default_api<inner_t>::make(parent, contexts...);
            }
        }

        template <typename E>
        static auto on_event(state_t& state, const E& event, auto*... api_contexts)
        {
            if constexpr (requires() { T::on_event(state, event, api_contexts...); })
            {
                return T::on_event(state, event, api_contexts...);
            }
            else
            {
                return detail::default_api<inner_t>::on_event(state, event, api_contexts...);
            }
        }

        static auto on_enter(state_t& state, auto*... api_contexts)
        {
            if constexpr (requires() { T::on_enter(state, api_contexts...); })
            {
                return T::on_enter(state, api_contexts...);
            }
            else
            {
                return detail::default_api<inner_t>::on_enter(state, api_contexts...);
            }
        }

        static auto on_exit(state_t& state, auto*... api_contexts)
        {
            if constexpr (requires() { T::on_exit(state, api_contexts...); })
            {
                return T::on_exit(state, api_contexts...);
            }
            else
            {
                return detail::default_api<inner_t>::on_exit(state, api_contexts...);
            }
        }

        static auto on_regions_complete(state_t& state, auto*... api_contexts)
        {
            if constexpr (requires() { T::on_regions_complete(state, api_contexts...); })
            {
                return T::on_regions_complete(state, api_contexts...);
            }
            else
            {
                return detail::default_api<inner_t>::on_regions_complete(state, api_contexts...);
            }
        }
    };

    namespace detail
    {
        template <typename T>
        using coalesce_default_api = coalesce_api<default_api<T>>;
    }

    template <
        typename Regions,
        typename StateContexts = nil::xalt::tlist<>,
        typename APIContexts = nil::xalt::tlist<>,
        template <typename> typename API = detail::coalesce_default_api>
    class SM;

    template <
        template <typename>
        typename API,
        typename... Regions,
        typename... StateContexts,
        typename... APIContexts>
    class SM<
        nil::xalt::tlist<Regions...>,
        nil::xalt::tlist<StateContexts...>,
        nil::xalt::tlist<APIContexts...>,
        API>
    {
        struct RootState
        {
            using regions = nil::xalt::tlist<Regions...>;
        };

    public:
        explicit SM(
            std::tuple<StateContexts*...> init_state_contexts = {},
            std::tuple<APIContexts*...> init_api_contexts = {}
        )
            : state_contexts(init_state_contexts)
            , api_contexts(init_api_contexts)
            , state(this, &queue, &state_contexts, &api_contexts)
        {
        }

        template <typename T>
            requires(!std::is_same_v<std::remove_cvref_t<T>, detail::Event>)
        void process_event(T event)
        {
            state.on_event(detail::Event{.id = nil::xalt::type_id<T>, .event = &event});

            while (!queue.empty())
            {
                auto emitted = std::move(queue.front());
                queue.pop();
                state.on_event(detail::Event{.id = emitted.id, .event = emitted.data.get()});
            }
        }

    private:
        std::queue<detail::Emit> queue;
        std::tuple<StateContexts*...> state_contexts;
        std::tuple<APIContexts*...> api_contexts;
        detail::State<
            RootState,
            nil::xalt::tlist<StateContexts...>,
            nil::xalt::tlist<APIContexts...>,
            API>
            state;
    };
}
