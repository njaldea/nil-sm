#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: capture discards the event before it ever reaches the region
TEST(sm_feature_capture_handling, capture_discards_without_dispatching_to_region)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using captures = nil::xalt::tlist<e1>;

        static auto on_capture(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // Capture handles the event → Child::on_event is never called
    {
        EXPECT_CALL(mock, on_capture_called(type_id<Parent>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: an event not declared in `captures` never reaches on_capture and falls
// through to the normal region + own event flow
TEST(sm_feature_capture_handling, undeclared_capture_falls_through_to_region)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using captures = nil::xalt::tlist<e2>;

        static auto on_capture(const e2& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // e1 isn't in Parent's captures list → on_capture is skipped, Child::on_event runs
    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: capture returning Forward also falls through to the region (Forward means
// "propagate to children" for capture, the opposite of its meaning for on_event)
TEST(sm_feature_capture_handling, capture_forward_dispatches_to_region_instead_of_bubbling)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using captures = nil::xalt::tlist<e1>;

        static auto on_capture(const e1& /* event */)
        {
            return Forward{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // Capture forwards → reaches Child (not bubbled up, Parent has no events list)
    {
        EXPECT_CALL(mock, on_capture_called(type_id<Parent>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: capture transits the state directly; the region never sees the event
TEST(sm_feature_capture_handling, capture_transit_replaces_state_without_region_reacting)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Target
    {
    };

    struct Source
    {
        using regions = nil::xalt::tlist<Child>;
        using captures = nil::xalt::tlist<e1>;

        static auto on_capture(const e1& /* event */)
        {
            return Transit<Target>{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Source> sm(nullptr, &mock);

    // Capture transits Source → Target; Child::on_event never runs
    {
        EXPECT_CALL(mock, on_capture_called(type_id<Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<Target>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Target>)).Times(1);
}

// Test: parent capture takes priority over a nested child's own capture
TEST(sm_feature_capture_handling, parent_capture_preempts_nested_child_capture)
{
    struct Child
    {
        using captures = nil::xalt::tlist<e1>;

        static auto on_capture(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using captures = nil::xalt::tlist<e1>;

        static auto on_capture(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // Parent's capture handles the event → Child::on_capture is never invoked
    {
        EXPECT_CALL(mock, on_capture_called(type_id<Parent>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}
