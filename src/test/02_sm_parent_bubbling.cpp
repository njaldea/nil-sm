#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: parent handles forwarded event from child
TEST(sm_feature_parent_bubbling, parent_handles_forwarded_event)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
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

    // Child forwards → Parent handles and discards (stops bubbling here)
    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: parent forwards forwarded event from child upward to grandparent
TEST(sm_feature_parent_bubbling, parent_forwards_forwarded_event)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Grandparent
    {
        using regions = nil::xalt::tlist<Parent>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Grandparent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Grandparent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Grandparent> sm(nullptr, &mock);

    // Child forwards → Parent forwards → Grandparent handles
    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Grandparent>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Grandparent>)).Times(1);
}

// Test: parent is skipped when child discards the event
TEST(sm_feature_parent_bubbling, parent_skips_discarded_child_event)
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
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
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

    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        // Parent on_event NOT called (child discarded)
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: parent handles event when child has no handler (unhandled)
TEST(sm_feature_parent_bubbling, parent_handles_unhandled_child_event)
{
    struct Child
    {
        // no events
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
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

    // Child has no e1 handler (Unhandled) → Parent handles e1 directly (no Child on_event call)
    {
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: parent on_event is NOT called when child terminates (Terminate != Forward);
//       on_regions_finalized IS triggered on parent once the child region is nulled
TEST(sm_feature_parent_bubbling, child_terminate_does_not_bubble_to_parent)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Terminate{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
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

    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        // Parent on_event NOT called (Terminate does not forward to parent)
        EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
        // All regions null → EvRegionsFinalized emitted → on_regions_finalized called
        EXPECT_CALL(mock, on_regions_finalized_called(type_id<Parent>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: no parent handler when child is unhandled and parent has no events
TEST(sm_feature_parent_bubbling, no_parent_handler_on_unhandled_event)
{
    struct Child
    {
        // no events
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        // no events
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // No on_event_called for either state
    {
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: parent transits to a new state after receiving a forwarded event from child;
//       the old child region is destroyed along with the parent, then NewState is created
TEST(sm_feature_parent_bubbling, parent_transits_after_child_forward)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct NewState
    {
        // leaf with no events
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<NewState>{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // Child forwards → Parent transits → Child+Parent destroyed, NewState created
    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<NewState>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<NewState>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<NewState>)).Times(1);
}

// Test: parent terminates itself after receiving a forwarded event from child;
//       child is destroyed inside parent's destructor, SM destructor has nothing left to clean up
TEST(sm_feature_parent_bubbling, parent_terminates_after_child_forward)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Terminate{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // Child forwards → Parent terminates → Child destroyed inside Parent's destructor
    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
        // SM destructor: Parent's region is already null, nothing more to exit
        sm.post(e1{});
    }
}

// Test: parent emits a new event after receiving a forwarded event from child;
//       the emitted event is enqueued and dispatched in the next SM cycle
TEST(sm_feature_parent_bubbling, parent_emits_after_child_forward)
{
    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            return Emit<e2>{};
        }

        static auto on_event(const e2& /* event */)
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

    // e1: Child forwards → Parent emits e2 (enqueued, returns Discard)
    // e2: Child has no e2 handler (Unhandled) → Parent handles e2 directly
    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e2>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

//       behavior is identical to having no events at all — parent handles directly
TEST(sm_feature_parent_bubbling, parent_handles_event_unregistered_in_child)
{
    struct Child
    {
        using events = nil::xalt::tlist<e2>; // handles e2, not e1

        static auto on_event(const e2& /* event */)
        {
            return Discard{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
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

    // e1 is not in Child's events list → Unhandled → Parent handles directly (no Child on_event
    // call)
    {
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: child transit does not bubble to parent (Transit != Forward);
//       child region is replaced by the target state, parent on_event is never called
TEST(sm_feature_parent_bubbling, child_transit_does_not_bubble_to_parent)
{
    struct ChildTarget
    {
        // leaf with no events
    };

    struct Child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<ChildTarget>{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
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

    // Child transits → Parent on_event NOT called (Transit does not forward)
    {
        EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<ChildTarget>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<ChildTarget>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<ChildTarget>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}
