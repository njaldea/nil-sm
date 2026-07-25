#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: event matches first handler
TEST(sm_feature_event_matching, event_matches_first_handler)
{
    struct TwoEventState
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }

        static auto on_event(const e2& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<TwoEventState>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<TwoEventState>)).Times(1);
    TestSM<TwoEventState> sm({}, &mock);

    {
        EXPECT_CALL(mock, on_event_called(type_id<TwoEventState>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<TwoEventState>)).Times(1);
}

// Test: event matches last handler
TEST(sm_feature_event_matching, event_matches_last_handler)
{
    struct TwoEventState
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }

        static auto on_event(const e2& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<TwoEventState>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<TwoEventState>)).Times(1);
    TestSM<TwoEventState> sm({}, &mock);

    {
        EXPECT_CALL(mock, on_event_called(type_id<TwoEventState>, type_id<e2>)).Times(1);
        sm.post(e2{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<TwoEventState>)).Times(1);
}

// Test: event matches none of the handlers
TEST(sm_feature_event_matching, event_matches_none)
{
    struct TwoEventState
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }

        static auto on_event(const e2& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<TwoEventState>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<TwoEventState>)).Times(1);
    TestSM<TwoEventState> sm({}, &mock);

    {
        // e3 is not in events list → no on_event_called
        sm.post(e3{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<TwoEventState>)).Times(1);
}

// Test: state with explicit empty events list handles no events
TEST(sm_feature_event_matching, state_with_empty_event_list)
{
    struct ExplicitEmptyEvents
    {
        using events = nil::xalt::tlist<>;
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<ExplicitEmptyEvents>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<ExplicitEmptyEvents>)).Times(1);
    TestSM<ExplicitEmptyEvents> sm({}, &mock);

    {
        // No on_event_called (empty events list)
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<ExplicitEmptyEvents>)).Times(1);
}

// Test: state with no events member handles no events
TEST(sm_feature_event_matching, state_with_default_events_only)
{
    struct DefaultEventsState
    {
        // no events member
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<DefaultEventsState>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<DefaultEventsState>)).Times(1);
    TestSM<DefaultEventsState> sm({}, &mock);

    {
        // No on_event_called (no events member defaults to empty)
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<DefaultEventsState>)).Times(1);
}
