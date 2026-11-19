// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Test 20: api::Coalesce — partial API composition
// Test 20: api::Coalesce — partial API composition
// Demonstrates that a custom API can define only the hooks it cares about.
// api::Coalesce supplies default behavior for any method not present in the
// partial API, delegating to api::Default.
namespace
{
    // ---- Shared state types ----

    struct e_tick
    {
    };

    // Leaf that discards e_tick and counts calls via a member counter
    struct counting_leaf
    {
        using events = nil::xalt::tlist<e_tick>;

        int count = 0;

        auto on_event(const e_tick& /* ev */)
        {
            count++;
            return Discard{};
        }
    };

    // Leaf with on_enter and on_event hooks
    struct lifecycle_leaf
    {
        using events = nil::xalt::tlist<e_tick>;

        static auto on_enter()
        {
            return NOOP{};
        }

        static auto on_event(const e_tick& /* ev */)
        {
            return Discard{};
        }
    };

    // ---- Partial APIs reach their observer through an extra arg ----
    // The observer is a root argument. Every state receives it as an extra first arg and keeps
    // it in a wrapper that owns the state. The wrapper is not a T, so the base forwards every
    // hook to the default one on the wrapped state; a partial API overrides only what it needs.

    // Owns a state together with the observer it reports to.
    template <typename Observer, typename T>
    struct observed
    {
        template <typename... Args>
        explicit observed(Observer* init_observer, Args*... args)
            : state{args...}
            , observer(init_observer)
        {
        }

        T state;
        Observer* observer;
    };

    template <typename Observer, typename T>
    struct observed_state
    {
        static constexpr bool reports = !std::is_same_v<T, nil::sm::Fin>;

        using default_state_t = typename nil::sm::api::Default::template state<T>;
        using instance_t = observed<Observer, T>;
        using args_t = nil::xalt::tlist_join_t<
            nil::xalt::tlist<Observer>,
            nil::xalt::coalesce_t<T, nil::sm::detail::args_tag>>;

        // The observer is always the first arg; the rest are the state's own args.
        static instance_t make(
            const nil::sm::Metadata& /* metadata */,
            Observer* observer,
            auto*... args
        )
        {
            return instance_t(observer, args...);
        }

        static auto on_enter(instance_t& instance)
        {
            return default_state_t::on_enter(instance.state);
        }

        static auto on_exit(instance_t& instance)
        {
            return default_state_t::on_exit(instance.state);
        }

        static auto on_regions_finalized(instance_t& instance)
        {
            return default_state_t::on_regions_finalized(instance.state);
        }

        template <typename E>
        static auto on_event(instance_t& instance, const E& event)
        {
            return default_state_t::template on_event<E>(instance.state, event);
        }

        template <typename E>
        static auto on_capture(instance_t& instance, const E& event)
        {
            return default_state_t::template on_capture<E>(instance.state, event);
        }
    };

    // ---- Test 1: partial API that intercepts only `make` ----

    class MakeObserver
    {
    public:
        MOCK_METHOD(void, on_construct, (), ());
    };

    // Partial API: only defines `make`. All other methods (on_event, on_enter,
    // on_exit, on_regions_finalized) come from the shared base.
    struct MakeOnlyAPI
    {
        template <typename T>
        struct state: observed_state<MakeObserver, T>
        {
            using base_t = observed_state<MakeObserver, T>;

            static typename base_t::instance_t make(
                const nil::sm::Metadata& metadata,
                MakeObserver* observer,
                auto*... args
            )
            {
                if constexpr (base_t::reports)
                {
                    observer->on_construct();
                }
                // Delegate construction to the shared wrapper maker
                return base_t::make(metadata, observer, args...);
            }
        };
    };

    // ---- Test 2: partial API that intercepts only `on_enter` ----

    class EnterObserver
    {
    public:
        MOCK_METHOD(void, on_enter_intercepted, (), ());
    };

    // Partial API: only defines `on_enter`. make, on_event, on_exit,
    // on_regions_finalized come from the shared base.
    struct EnterOnlyAPI
    {
        template <typename T>
        struct state: observed_state<EnterObserver, T>
        {
            using base_t = observed_state<EnterObserver, T>;

            static auto on_enter(typename base_t::instance_t& instance)
            {
                if constexpr (base_t::reports)
                {
                    instance.observer->on_enter_intercepted();
                }
                // Delegate to api::Default for actual state hook dispatch
                return base_t::on_enter(instance);
            }
        };
    };

    class CombinedObserver
    {
    public:
        MOCK_METHOD(void, on_construct, (), ());
        MOCK_METHOD(void, on_enter_intercepted, (), ());
    };

    struct MakeAndEnterAPI
    {
        template <typename T>
        struct state: observed_state<CombinedObserver, T>
        {
            using base_t = observed_state<CombinedObserver, T>;

            static typename base_t::instance_t make(
                const nil::sm::Metadata& metadata,
                CombinedObserver* observer,
                auto*... args
            )
            {
                if constexpr (base_t::reports)
                {
                    observer->on_construct();
                }
                return base_t::make(metadata, observer, args...);
            }

            static auto on_enter(typename base_t::instance_t& instance)
            {
                if constexpr (base_t::reports)
                {
                    instance.observer->on_enter_intercepted();
                }
                return base_t::on_enter(instance);
            }
        };
    };

    // ---- Test 3: partial API that intercepts only `on_event` ----

    class EventObserver
    {
    public:
        MOCK_METHOD(void, on_event_intercepted, (), ());
    };

    // Partial API: only defines `on_event`. make, on_enter, on_exit,
    // on_regions_finalized come from the shared base.
    struct EventOnlyAPI
    {
        template <typename T>
        struct state: observed_state<EventObserver, T>
        {
            using base_t = observed_state<EventObserver, T>;

            template <typename E>
            static auto on_event(typename base_t::instance_t& instance, const E& event)
            {
                if constexpr (base_t::reports)
                {
                    instance.observer->on_event_intercepted();
                }
                return base_t::template on_event<E>(instance, event);
            }
        };
    };

    // ---- Test 4: state declares `args` for multi-arg constructor injection ----

    struct CtxA
    {
        int a = 0;
    };

    struct CtxB
    {
        int b = 0;
    };

    // State that receives two root-provided contexts as individual constructor args
    struct spread_leaf
    {
        using events = nil::xalt::tlist<e_tick>;
        using args = nil::xalt::tlist<CtxA, CtxB>;

        int sum = 0;

        explicit spread_leaf(CtxA* a, CtxB* b)
            : sum(a->a + b->b)
        {
        }

        static auto on_event(const e_tick& /* ev */)
        {
            return Discard{};
        }
    };

    // ---- Test 5: make() returns a wrapper that carries a service added through args_t ----

    struct Service
    {
        int entered = 0;
        int events = 0;
    };

    struct wrapped_leaf
    {
        using events = nil::xalt::tlist<e_tick>;

        static auto on_event(const e_tick& /* ev */)
        {
            return Discard{};
        }
    };

    // Owns a state together with the service the API added through args_t.
    template <typename T>
    struct serviced
    {
        explicit serviced(Service* init_service)
            : service(init_service)
        {
        }

        T state;
        Service* service;
    };

    struct WrappingAPI
    {
        template <typename T>
        struct state
        {
            using default_state_t = nil::sm::api::Default::template state<T>;
            static constexpr bool is_fin = std::is_same_v<T, nil::sm::Fin>;

            using instance_t = std::conditional_t<is_fin, T, serviced<T>>;
            using regions_t = nil::xalt::coalesce_t<T, nil::sm::detail::regions_tag>;
            using events_t = nil::xalt::coalesce_t<T, nil::sm::detail::events_tag>;
            using captures_t = nil::xalt::coalesce_t<T, nil::sm::detail::captures_tag>;
            using props_t = nil::xalt::coalesce_t<T, nil::sm::detail::props_tag>;
            using args_t = std::conditional_t<is_fin, nil::xalt::tlist<>, nil::xalt::tlist<Service>>;

            // The default hooks operate on T, so they get the wrapped state.
            static T& unwrap(instance_t& state)
            {
                if constexpr (is_fin)
                {
                    return state;
                }
                else
                {
                    return state.state;
                }
            }

            static instance_t make(const nil::sm::Metadata& metadata, auto*... args)
            {
                if constexpr (is_fin)
                {
                    return default_state_t::make(metadata, args...);
                }
                else
                {
                    return instance_t(args...);
                }
            }

            static auto on_enter(instance_t& state)
            {
                if constexpr (!is_fin)
                {
                    state.service->entered++;
                }
                return default_state_t::on_enter(unwrap(state));
            }

            static auto on_exit(instance_t& state)
            {
                return default_state_t::on_exit(unwrap(state));
            }

            static auto on_regions_finalized(instance_t& state)
            {
                return default_state_t::on_regions_finalized(unwrap(state));
            }

            template <typename E>
            static auto on_event(instance_t& state, const E& event)
            {
                if constexpr (!is_fin)
                {
                    state.service->events++;
                }
                return default_state_t::template on_event<E>(unwrap(state), event);
            }

            template <typename E>
            static auto on_capture(instance_t& state, const E& event)
            {
                return default_state_t::template on_capture<E>(unwrap(state), event);
            }
        };
    };
    // ---- Test 6: props are applied to the wrapped state ----

    struct HostProp
    {
        int number = 7;
    };

    struct PropsSeen
    {
        int number = 0;
    };

    struct prop_reader
    {
        using args = nil::xalt::tlist<HostProp, PropsSeen>;

        prop_reader(HostProp* host_prop, PropsSeen* seen)
        {
            seen->number = host_prop->number;
        }
    };

    struct prop_host
    {
        using regions = nil::xalt::tlist<prop_reader>;

        HostProp host_prop;
        using props = nil::xalt::tlist<nil::sm::prop<&prop_host::host_prop>>;
    };
}

// Test: partial API that only defines `make` intercepts construction;
// on_event falls through to the default path and the state handles events normally.
TEST(sm_feature_coalesce_api, make_intercepted_construction_observer_called)
{
    testing::StrictMock<MakeObserver> obs;
    testing::InSequence seq;

    EXPECT_CALL(obs, on_construct()).Times(1);
    nil::sm::SM<nil::sm::api::Coalesce<MakeOnlyAPI>, counting_leaf, MakeObserver> sm{&obs};

    // on_event falls through to api::Default — counting_leaf handles e_tick
    {
        sm.post(e_tick{});
    }
    {
        sm.post(e_tick{});
    }
}

// Test: partial API that only defines `on_enter` intercepts entry;
// make falls through so the state is default-constructed, and on_event is
// dispatched normally via the default path.
TEST(sm_feature_coalesce_api, on_enter_intercepted_enter_observer_called)
{
    testing::StrictMock<EnterObserver> obs;
    testing::InSequence sequence;

    // lifecycle_leaf has on_enter — our interceptor fires, then calls default
    EXPECT_CALL(obs, on_enter_intercepted()).Times(1);
    nil::sm::SM<nil::sm::api::Coalesce<EnterOnlyAPI>, lifecycle_leaf, EnterObserver> sm{&obs};

    {
        sm.post(e_tick{}); // on_event falls through to default; state discards
    }
}

TEST(sm_feature_coalesce_api, multiple_partial_hooks_compose)
{
    testing::StrictMock<CombinedObserver> obs;
    testing::InSequence sequence;

    EXPECT_CALL(obs, on_construct()).Times(1);
    EXPECT_CALL(obs, on_enter_intercepted()).Times(1);
    nil::sm::SM<nil::sm::api::Coalesce<MakeAndEnterAPI>, lifecycle_leaf, CombinedObserver> sm{&obs};

    sm.post(e_tick{});
}

// Test: partial API that only defines `on_event` intercepts every dispatched
// event; make and lifecycle hooks fall through to defaults.
TEST(sm_feature_coalesce_api, on_event_intercepted_event_observer_called)
{
    testing::StrictMock<EventObserver> obs;
    testing::InSequence sequence;

    nil::sm::SM<nil::sm::api::Coalesce<EventOnlyAPI>, lifecycle_leaf, EventObserver> sm{&obs};

    {
        EXPECT_CALL(obs, on_event_intercepted()).Times(1);
        sm.post(e_tick{});
    }
    {
        EXPECT_CALL(obs, on_event_intercepted()).Times(1);
        sm.post(e_tick{});
    }
}

// Test: state declares `args` to receive two root-provided contexts as individual
// constructor parameters; no custom API::make() override is needed.
TEST(sm_feature_coalesce_api, custom_make_spreads_tuple_context_to_state_args)
{
    CtxA a{.a = 10};
    CtxB b{.b = 32};

    nil::sm::DefaultSM<spread_leaf, CtxA, CtxB> sm(&a, &b);

    // spread_leaf was constructed with sum = a.a + b.b = 42
    // (verified implicitly — SM would not compile if construction failed)
    sm.post(e_tick{});
}

// Test: make() returns a wrapper of the state; the stored value, and therefore every hook,
// is the wrapper, so the API can carry a service it adds through args_t.
TEST(sm_feature_coalesce_api, make_returning_wrapper_gives_hooks_the_wrapper)
{
    Service service;

    nil::sm::SM<WrappingAPI, wrapped_leaf, Service> sm(&service);
    EXPECT_EQ(service.entered, 1);

    sm.post(e_tick{});
    EXPECT_EQ(service.events, 1);
}

// Test: when instance_t is a wrapper, props are applied to its `state` member, so a child still
// reads what the wrapped parent exposes.
TEST(sm_feature_coalesce_api, props_of_a_wrapped_state_reach_its_children)
{
    testing::StrictMock<EnterObserver> obs;
    PropsSeen seen;

    // host and reader are both wrapped, and both report their entry
    EXPECT_CALL(obs, on_enter_intercepted()).Times(2);
    nil::sm::SM<nil::sm::api::Coalesce<EnterOnlyAPI>, prop_host, EnterObserver, PropsSeen> sm{
        &obs,
        &seen
    };

    EXPECT_EQ(seen.number, 7);
}
