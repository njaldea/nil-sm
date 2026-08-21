#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: state constructor called once on SM creation, destructor on SM destruction
TEST(sm_feature_state_lifetime, state_constructor_called_once)
{
    struct LeafDiscard
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<LeafDiscard>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafDiscard>)).Times(1);
    TestSM<LeafDiscard> sm(nullptr, &mock);

    {
        EXPECT_CALL(mock, on_event_called(type_id<LeafDiscard>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<LeafDiscard>)).Times(1);
}

// Test: transition destroys source state and creates target state
TEST(sm_feature_state_lifetime, transition_destroys_previous_and_creates_new_instance)
{
    struct Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
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

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Source>)).Times(1);
    TestSM<Source> sm(nullptr, &mock);

    // Transition: source processes event, exits; target is created
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<Target>)).Times(1);
        sm.post(e1{});
    }

    // Target reacts on second event
    {
        EXPECT_CALL(mock, on_event_called(type_id<Target>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Target>)).Times(1);
}

// Test: orthogonal regions are each constructed and destructed once
TEST(sm_feature_state_lifetime, orthogonal_region_destruction)
{
    struct R1
    {
        // no events
    };

    struct R2
    {
        // no events
    };

    struct Root
    {
        using regions = nil::xalt::tlist<R1, R2>;
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: parent destruction destroys all children in order
TEST(sm_feature_state_lifetime, parent_destruction_destroys_all_children)
{
    struct GrandChild
    {
        // no events
    };

    struct Child
    {
        using regions = nil::xalt::tlist<GrandChild>;
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Child>;
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<GrandChild>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<GrandChild>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    EXPECT_CALL(mock, on_exit_called(type_id<GrandChild>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}
