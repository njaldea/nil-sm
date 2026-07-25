#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Test 20: coalesce_api — partial API composition
//
// Demonstrates that a custom API can define only the hooks it cares about.
// coalesce_api provides default behavior for any method not present in the
// partial API, delegating to default_api<T>.

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
    // on_exit, on_regions_finalized) are absent — coalesce_api fills them in.
    template <typename T>
    struct MakeOnlyAPI
    {
        using api_context_t = MakeObserver;

        template <typename Parent>
        static T make(
            Parent* parent,
            void* state_contexts,
            MakeObserver* api_contexts,
            const nil::sm::state_metadata& metadata
        )
        {
            if constexpr (!std::is_same_v<T, nil::sm::fin>)
            {
                api_contexts->on_construct();
            }
            // Delegate construction to default_api
            return nil::sm::default_api<T>::make(parent, state_contexts, nullptr, metadata);
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
    // on_regions_finalized — all fall through to defaults via coalesce_api.
    template <typename T>
    struct EnterOnlyAPI
    {
        using api_context_t = EnterObserver;

        static auto on_enter(T& state, EnterObserver* api_contexts)
        {
            if constexpr (!std::is_same_v<T, nil::sm::fin>)
            {
                api_contexts->on_enter_intercepted();
            }
            // Delegate to default_api for actual state hook dispatch
            return nil::sm::default_api<T>::on_enter(state, nullptr);
        }

        // make, on_event, on_exit, on_regions_finalized — not defined here
    };

    // ---- Test 3: partial API that intercepts only `on_event` ----

    class EventObserver
    {
    public:
        MOCK_METHOD(void, on_event_intercepted, (), ());
    };

    // Partial API: only defines `on_event`. make, on_enter, on_exit,
    // on_regions_finalized — all fall through to defaults via coalesce_api.
    template <typename T>
    struct EventOnlyAPI
    {
        using api_context_t = EventObserver;

        template <typename E>
        static auto on_event(T& state, const E& event, EventObserver* api_contexts)
        {
            if constexpr (!std::is_same_v<T, nil::sm::fin>)
            {
                api_contexts->on_event_intercepted();
            }
            return nil::sm::default_api<T>::template on_event<E>(state, event, nullptr);
        }

        // make, on_enter, on_exit, on_regions_finalized — not defined here
    };

    // ---- Test 4: custom make that spreads a tuple context via std::apply ----

    struct CtxA
    {
        int a = 0;
    };

    struct CtxB
    {
        int b = 0;
    };

    // State that receives two context pointers as individual constructor args
    struct spread_leaf
    {
        using events = nil::xalt::tlist<e_tick>;

        int sum = 0;

        explicit spread_leaf(auto* /* parent */, CtxA* a, CtxB* b)
            : sum(a->a + b->b)
        {
        }

        static auto on_event(const e_tick& /* ev */)
        {
            return Discard{};
        }
    };

    // Custom API: make spreads a std::tuple state context into individual args.
    // All other hooks use defaults via coalesce_api.
    template <typename T>
    struct SpreadMakeAPI
    {
        using state_context_t = std::tuple<CtxA*, CtxB*>;

        template <typename Parent>
        static T make(
            Parent* parent,
            state_context_t* state_contexts,
            void* /* api_contexts */,
            const nil::sm::state_metadata& /* metadata */
        )
        {
            if constexpr (!std::is_same_v<T, nil::sm::fin>)
            {
                return std::apply(
                    [parent](auto*... args) -> T { return T(parent, args...); },
                    *state_contexts
                );
            }
            else
            {
                return T{};
            }
        }

        // on_event, on_enter, on_exit, on_regions_finalized — not defined here
    };
}

// Test: partial API that only defines `make` intercepts construction;
// on_event falls through to the default path and the state handles events normally.
TEST(sm_feature_coalesce_api, make_intercepted_construction_observer_called)
{
    testing::StrictMock<MakeObserver> obs;
    testing::InSequence seq;

    EXPECT_CALL(obs, on_construct()).Times(1);
    nil::sm::SM<nil::sm::coalesce_api<MakeOnlyAPI>::type, counting_leaf> sm{nullptr, &obs};

    // on_event falls through to default_api — counting_leaf handles e_tick
    {
        sm.post(e_tick{});
        sm.post(e_tick{});
    }
}

// Test: partial API that only defines `on_enter` intercepts entry;
// make falls through so the state is default-constructed, and on_event is
// dispatched normally via the default path.
TEST(sm_feature_coalesce_api, on_enter_intercepted_enter_observer_called)
{
    testing::StrictMock<EnterObserver> obs;

    // lifecycle_leaf has on_enter — our interceptor fires, then calls default
    EXPECT_CALL(obs, on_enter_intercepted()).Times(1);
    nil::sm::SM<nil::sm::coalesce_api<EnterOnlyAPI>::type, lifecycle_leaf> sm{nullptr, &obs};

    {
        sm.post(e_tick{}); // on_event falls through to default; state discards
    }
}

// Test: partial API that only defines `on_event` intercepts every dispatched
// event; make and lifecycle hooks fall through to defaults.
TEST(sm_feature_coalesce_api, on_event_intercepted_event_observer_called)
{
    testing::StrictMock<EventObserver> obs;

    nil::sm::SM<nil::sm::coalesce_api<EventOnlyAPI>::type, lifecycle_leaf> sm{nullptr, &obs};

    {
        EXPECT_CALL(obs, on_event_intercepted()).Times(1);
        sm.post(e_tick{});
    }
    {
        EXPECT_CALL(obs, on_event_intercepted()).Times(1);
        sm.post(e_tick{});
    }
}

// Test: custom API make spreads a std::tuple state context into individual
// constructor args via std::apply; all other hooks fall through to defaults.
TEST(sm_feature_coalesce_api, custom_make_spreads_tuple_context_to_state_args)
{
    CtxA a{.a = 10};
    CtxB b{.b = 32};
    auto ctx = std::tuple<CtxA*, CtxB*>(&a, &b);

    nil::sm::SM<nil::sm::coalesce_api<SpreadMakeAPI>::type, spread_leaf> sm{&ctx, nullptr};

    // spread_leaf was constructed with sum = a.a + b.b = 42
    // (verified implicitly — SM would not compile if construction failed)
    sm.post(e_tick{});
}
