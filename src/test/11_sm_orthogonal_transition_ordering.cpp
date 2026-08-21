#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: child transition plus parent discard - child transitions, parent discards
TEST(sm_feature_orthogonal_transition_ordering, child_transition_plus_parent_discard)
{
    struct R1Target
    {
        // no events
    };

    struct R1Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<R1Target>{};
        }
    };

    struct R2
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<R1Source, R2>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // R1 transits, R2 forwards → parent called
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        // Apply transits: R1 exits, R1Target created; R2 exits (no transition)
        EXPECT_CALL(mock, on_exit_called(type_id<R1Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<R1Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<R1Target>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}

// Test: child transition plus parent forward - child transitions, parent forwards to grandparent
TEST(sm_feature_orthogonal_transition_ordering, child_transition_plus_parent_forward)
{
    struct R1Target
    {
        // no events
    };

    struct R1Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<R1Target>{};
        }
    };

    struct R2
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<R1Source, R2>;
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
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Grandparent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Grandparent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Grandparent> sm(nullptr, &mock);

    {
        // R1Source transits, R2 forwards → Parent called → Parent forwards
        EXPECT_CALL(mock, on_event_called(type_id<R1Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        // Sub-transits applied before forwarding: R1Source→R1Target
        EXPECT_CALL(mock, on_exit_called(type_id<R1Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<R1Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<R1Target>)).Times(1);
        // Grandparent receives forwarded event and discards
        EXPECT_CALL(mock, on_event_called(type_id<Grandparent>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    // SM destructor: regions exit in reverse order
    EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Grandparent>)).Times(1);
}

// Test: child transition plus parent transition - parent cancels child's pending transit
TEST(sm_feature_orthogonal_transition_ordering, child_transition_plus_parent_transition)
{
    struct R1Target
    {
        // no events
    };

    struct ParentTarget
    {
        // no events
    };

    struct R1Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<R1Target>{};
        }
    };

    struct R2
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<R1Source, R2>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<ParentTarget>{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    {
        EXPECT_CALL(mock, on_event_called(type_id<R1Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        // Parent transits: all children exit in reverse, then parent exits
        EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<R1Source>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
        // ParentTarget is created (R1Target is never created since parent cancels)
        EXPECT_CALL(mock, on_make_called(type_id<ParentTarget>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<ParentTarget>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<ParentTarget>)).Times(1);
}

// Test: multiple child transitions in orthogonal regions
TEST(sm_feature_orthogonal_transition_ordering, multiple_child_transitions)
{
    struct R1Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct R2Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct R1Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<R1Target>{};
        }
    };

    struct R2Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<R2Target>{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<R1Source, R2Source>;
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2Source>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // e1: both transit
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<R1Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<R1Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<R1Target>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<R2Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<R2Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<R2Target>)).Times(1);
        sm.post(e1{});
    }

    // e1: both discard
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1Target>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2Target>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<R2Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: nested child transitions within sibling regions
TEST(sm_feature_orthogonal_transition_ordering, nested_child_transitions)
{
    struct LeafTarget
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct LeafSource
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<LeafTarget>{};
        }
    };

    struct Branch
    {
        using regions = nil::xalt::tlist<LeafSource>;
        // no events
    };

    struct Sibling
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Top
    {
        using regions = nil::xalt::tlist<Branch, Sibling>;
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Top>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Top>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<LeafSource>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafSource>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Sibling>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Sibling>)).Times(1);
    TestSM<Top> sm(nullptr, &mock);

    // e1: leaf transits, sibling discards
    {
        EXPECT_CALL(mock, on_event_called(type_id<LeafSource>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<LeafSource>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<LeafTarget>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<LeafTarget>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Sibling>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    // e1: leaf target discards, sibling discards
    {
        EXPECT_CALL(mock, on_event_called(type_id<LeafTarget>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Sibling>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Sibling>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<LeafTarget>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Top>)).Times(1);
}

// Test: sibling regions transition independently
TEST(sm_feature_orthogonal_transition_ordering, sibling_transitions_independent)
{
    struct R1Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct R2Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct R1Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<R1Target>{};
        }
    };

    struct R2Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<R2Target>{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<R1Source, R2Source>;
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2Source>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // e1: both transit
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<R1Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<R1Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<R1Target>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<R2Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<R2Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<R2Target>)).Times(1);
        sm.post(e1{});
    }

    // e1: R1Target forwards, R2Target discards
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1Target>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2Target>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<R2Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: parent transition cancels pending child transitions
TEST(sm_feature_orthogonal_transition_ordering, parent_transition_cancels_pending_child_transitions)
{
    struct T1Target
    {
        // no events
    };

    struct T2Target
    {
        // no events
    };

    struct ParentTarget
    {
        // no events
    };

    struct T1Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<T1Target>{};
        }
    };

    struct T2Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<T2Target>{};
        }
    };

    struct Fwd
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<T1Source, T2Source, Fwd>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<ParentTarget>{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<T1Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<T1Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<T2Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<T2Source>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Fwd>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Fwd>)).Times(1);
    TestSM<Parent> sm(nullptr, &mock);

    // e1: t1 transits, t2 transits, fwd forwards → parent transits
    {
        EXPECT_CALL(mock, on_event_called(type_id<T1Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<T2Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Fwd>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
        // Parent transits: children exit in reverse order
        EXPECT_CALL(mock, on_exit_called(type_id<Fwd>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<T2Source>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<T1Source>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
        // ParentTarget created (T1Target and T2Target never created)
        EXPECT_CALL(mock, on_make_called(type_id<ParentTarget>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<ParentTarget>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<ParentTarget>)).Times(1);
}
