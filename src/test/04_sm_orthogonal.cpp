#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: all orthogonal regions discard, parent is skipped
TEST(sm_feature_orthogonal, all_discard_parent_skipped)
{
    struct R1
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct R2
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<R1, R2>;
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
    EXPECT_CALL(mock, on_make_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // Both discard → parent NOT called
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: one region forwards, one discards → parent handles
TEST(sm_feature_orthogonal, forward_discard_parent_handles)
{
    struct R1
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct R2
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<R1, R2>;
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
    EXPECT_CALL(mock, on_make_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // R1 forwards → parent called
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: all regions unhandled → parent handles
TEST(sm_feature_orthogonal, all_unhandled_parent_handles)
{
    struct U1
    {
        // no events
    };

    struct U2
    {
        // no events
    };

    struct Root
    {
        using regions = nil::xalt::tlist<U1, U2>;
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
    EXPECT_CALL(mock, on_make_called(type_id<U1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<U1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<U2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<U2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // All unhandled → parent called
    {
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<U2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<U1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: all regions unhandled, parent has no events → nothing happens
TEST(sm_feature_orthogonal, no_parent_all_unhandled)
{
    struct U1
    {
        // no events
    };

    struct U2
    {
        // no events
    };

    struct Root
    {
        using regions = nil::xalt::tlist<U1, U2>;
        // no events
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<U1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<U1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<U2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<U2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // No on_event calls (parent has no events)
    {
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<U2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<U1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: both regions forward → any Forward triggers parent
TEST(sm_feature_orthogonal, both_regions_forward_parent_handles)
{
    struct R1
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
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

    struct Root
    {
        using regions = nil::xalt::tlist<R1, R2>;
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
    EXPECT_CALL(mock, on_make_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // Both forward → parent called
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: one region forwards, one is unhandled → Forward alone triggers parent
TEST(sm_feature_orthogonal, one_forward_one_unhandled_parent_handles)
{
    struct R1
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct R2
    {
        // no events → Unhandled
    };

    struct Root
    {
        using regions = nil::xalt::tlist<R1, R2>;
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
    EXPECT_CALL(mock, on_make_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // R1 forwards (R2 unhandled) → parent called
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: both regions terminate → all regions null → on_regions_finalized triggered on parent
TEST(sm_feature_orthogonal, both_regions_terminate_triggers_regions_complete)
{
    struct R1
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Terminate{};
        }
    };

    struct R2
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Terminate{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<R1, R2>;
        // no events
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<R2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<R2>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // Both terminate → regions nulled in order → all null → EvRegionsFinalized
    {
        EXPECT_CALL(mock, on_event_called(type_id<R1>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<R2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<R1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<R2>)).Times(1);
        EXPECT_CALL(mock, on_regions_finalized_called(type_id<Root>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}
