#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: child transition applies when parent does not consume the forwarded event
TEST(sm_feature_composite_single_region, child_transition_applies_when_parent_no_transition)
{
    struct Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<Target>{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Source>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Source>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // e1: Source transits → Source exits, Target created+entered
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<Target>)).Times(1);
        sm.post(e1{});
    }

    // e1: Target forwards → Root handles → Discard
    {
        EXPECT_CALL(mock, on_event_called(type_id<Target>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: composite receives forwarded event from deeply nested child
TEST(sm_feature_composite_single_region, composite_receives_child_forward)
{
    struct Leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Mid
    {
        using regions = nil::xalt::tlist<Leaf>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Mid>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Mid>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Mid>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Leaf>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Leaf>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    {
        EXPECT_CALL(mock, on_event_called(type_id<Leaf>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Mid>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Leaf>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Mid>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: parent transition occurs after child forwards
TEST(sm_feature_composite_single_region, parent_transition_after_child_forward)
{
    struct ParentTarget
    {
        // leaf with no events
    };

    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<ParentTarget>{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        // Root transits: children exit first, then root exits
        EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
        // ParentTarget is created and entered
        EXPECT_CALL(mock, on_make_called(type_id<ParentTarget>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<ParentTarget>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<ParentTarget>)).Times(1);
}
