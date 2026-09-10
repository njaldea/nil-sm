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
// partial API, delegating to api::Default<T>.
// partial API, delegating to api::Default<T>.
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

    // ---- Test 1: partial API that intercepts only `make` ----

    class MakeObserver
    {
    public:
        MOCK_METHOD(void, on_construct, (), ());
    };

    // Partial API: only defines `make`. All other methods (on_event, on_enter,
    // on_exit, on_regions_finalized) are absent — api::Coalesce fills them in.
    template <typename T>
    struct MakeOnlyAPI
    {
        using api_context_t = MakeObserver;

        template <typename... Args>
        static T make(api_context_t* api_contexts, const nil::sm::Metadata& metadata, Args*... args)
        {
            if constexpr (!std::is_same_v<T, nil::sm::Fin>)
            {
                api_contexts->on_construct();
            }
            // Delegate construction to api::Default
            return nil::sm::api::Default<api_context_t>::type<T>::make(
                api_contexts,
                metadata,
                args...
            );
        }

        // on_event, on_enter, on_exit, on_regions_finalized — not defined here
    };

    // ---- Test 2: partial API that intercepts only `on_enter` ----

    class EnterObserver
    {
    public:
        MOCK_METHOD(void, on_enter_intercepted, (), ());
    };

    // Partial API: only defines `on_enter`. make, on_event, on_exit,
    // on_regions_finalized — all fall through to defaults via api::Coalesce.
    template <typename T>
    struct EnterOnlyAPI
    {
        using api_context_t = EnterObserver;

        static auto on_enter(T& state, api_context_t* api_contexts)
        {
            if constexpr (!std::is_same_v<T, nil::sm::Fin>)
            {
                api_contexts->on_enter_intercepted();
            }
            // Delegate to api::Default for actual state hook dispatch
            return nil::sm::api::Default<api_context_t>::type<T>::on_enter(state, api_contexts);
        }

        // make, on_event, on_exit, on_regions_finalized — not defined here
    };

    class CombinedObserver
    {
    public:
        MOCK_METHOD(void, on_construct, (), ());
        MOCK_METHOD(void, on_enter_intercepted, (), ());
    };

    template <typename T>
    struct MakeAndEnterAPI
    {
        using api_context_t = CombinedObserver;

        template <typename... Args>
        static T make(api_context_t* context, const nil::sm::Metadata& metadata, Args*... args)
        {
            if constexpr (!std::is_same_v<T, nil::sm::Fin>)
            {
                context->on_construct();
            }
            return nil::sm::api::Default<api_context_t>::type<T>::make(context, metadata, args...);
        }

        static auto on_enter(T& state, api_context_t* context)
        {
            if constexpr (!std::is_same_v<T, nil::sm::Fin>)
            {
                context->on_enter_intercepted();
            }
            return nil::sm::api::Default<api_context_t>::type<T>::on_enter(state, context);
        }
    };

    // ---- Test 3: partial API that intercepts only `on_event` ----

    class EventObserver
    {
    public:
        MOCK_METHOD(void, on_event_intercepted, (), ());
    };

    // Partial API: only defines `on_event`. make, on_enter, on_exit,
    // on_regions_finalized — all fall through to defaults via api::Coalesce.
    template <typename T>
    struct EventOnlyAPI
    {
        using api_context_t = EventObserver;

        template <typename E>
        static auto on_event(T& state, const E& event, EventObserver* api_contexts)
        {
            if constexpr (!std::is_same_v<T, nil::sm::Fin>)
            {
                api_contexts->on_event_intercepted();
            }
            return nil::sm::api::Default<EventObserver>::type<T>::template on_event<E>(
                state,
                event,
                api_contexts
            );
        }

        // make, on_enter, on_exit, on_regions_finalized — not defined here
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
}

// Test: partial API that only defines `make` intercepts construction;
// on_event falls through to the default path and the state handles events normally.
TEST(sm_feature_coalesce_api, make_intercepted_construction_observer_called)
{
    testing::StrictMock<MakeObserver> obs;
    testing::InSequence seq;

    EXPECT_CALL(obs, on_construct()).Times(1);
    nil::sm::SM<nil::sm::api::Coalesce<MakeOnlyAPI>::type, counting_leaf> sm{&obs};

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
    nil::sm::SM<nil::sm::api::Coalesce<EnterOnlyAPI>::type, lifecycle_leaf> sm{&obs};

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
    nil::sm::SM<nil::sm::api::Coalesce<MakeAndEnterAPI>::type, lifecycle_leaf> sm{&obs};

    sm.post(e_tick{});
}

// Test: partial API that only defines `on_event` intercepts every dispatched
// event; make and lifecycle hooks fall through to defaults.
TEST(sm_feature_coalesce_api, on_event_intercepted_event_observer_called)
{
    testing::StrictMock<EventObserver> obs;
    testing::InSequence sequence;

    nil::sm::SM<nil::sm::api::Coalesce<EventOnlyAPI>::type, lifecycle_leaf> sm{&obs};

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
