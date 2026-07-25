#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// Test: one branch discards (parent skipped), other branch forwards (parent + root called)
TEST(sm_feature_nested_composite, one_branch_discards_other_forwards_asymmetric)
{
    struct Branch1Leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Branch2Leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Branch1
    {
        using regions = nil::xalt::tlist<Branch1Leaf>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Branch2
    {
        using regions = nil::xalt::tlist<Branch2Leaf>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Branch1, Branch2>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch2Leaf>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch2Leaf>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // Branch1Leaf discards → Branch1 not called (discard from child suppresses parent)
    // Branch2Leaf forwards → Branch2 called → Branch2 forwards → Root called
    {
        EXPECT_CALL(mock, on_event_called(type_id<Branch1Leaf>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Branch2Leaf>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Branch2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Branch2Leaf>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: both nested branches discard → no forward reaches root; root not called
TEST(sm_feature_nested_composite, both_branches_discard_parent_skipped)
{
    struct Branch1Leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Branch2Leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Branch1
    {
        using regions = nil::xalt::tlist<Branch1Leaf>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Branch2
    {
        using regions = nil::xalt::tlist<Branch2Leaf>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Branch1, Branch2>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch2Leaf>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch2Leaf>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // Both leaves discard → branch parents skipped → root skipped
    {
        EXPECT_CALL(mock, on_event_called(type_id<Branch1Leaf>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Branch2Leaf>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Branch2Leaf>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: both nested branches forward → both branch parents called → root called
TEST(sm_feature_nested_composite, both_branches_forward_parent_handles)
{
    struct Branch1Leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Branch2Leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Branch1
    {
        using regions = nil::xalt::tlist<Branch1Leaf>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Branch2
    {
        using regions = nil::xalt::tlist<Branch2Leaf>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Forward{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Branch1, Branch2>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Branch2Leaf>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Branch2Leaf>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // Both leaves forward → both branch parents called → root called
    {
        EXPECT_CALL(mock, on_event_called(type_id<Branch1Leaf>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Branch1>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Branch2Leaf>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Branch2>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Branch2Leaf>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch2>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch1Leaf>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Branch1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}
