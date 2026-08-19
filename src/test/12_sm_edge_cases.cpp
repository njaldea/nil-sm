#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

namespace
{
    template <typename T>
    using EdgeCaseTestAPI = nil::sm::api::Default<T, testing::StrictMock<StateMock>, void>;

    // Wrapper to create SM with StateMock context
    template <typename... Regions>
    using EdgeCaseSM = nil::sm::SM<EdgeCaseTestAPI, Regions...>;

    template <typename Tag>
    struct discard_on_e1
    {
        using events = nil::xalt::tlist<e1>;

        StateMock* mock = nullptr;

        explicit discard_on_e1(auto* /* parent */, testing::StrictMock<StateMock>* m)
            : mock(m)
        {
        }

        auto on_event(const e1& /* event */) const
        {
            mock->on_state_event(type_id<discard_on_e1<Tag>>, type_id<e1>);
            return Discard{};
        }
    };

    template <typename Tag>
    struct no_events_region
    {
    };

    template <typename Tag>
    struct empty_regions_root
    {
        using regions = nil::xalt::tlist<>;
        using events = nil::xalt::tlist<e1>;

        StateMock* mock = nullptr;

        explicit empty_regions_root(auto* /* parent */, testing::StrictMock<StateMock>* m)
            : mock(m)
        {
        }

        auto on_event(const e1& /* event */) const
        {
            mock->on_state_event(type_id<empty_regions_root<Tag>>, type_id<e1>);
            return Discard{};
        }
    };

    template <typename Tag, typename... Regions>
    struct root_with_regions
    {
        using regions = nil::xalt::tlist<Regions...>;
    };

    template <typename Tag, typename Child>
    struct chain_node
    {
        using regions = nil::xalt::tlist<Child>;
    };

    template <typename Tag>
    struct reentrant_state
    {
        using events = nil::xalt::tlist<e1, e2>;

        StateMock* mock = nullptr;

        explicit reentrant_state(auto* /* parent */, testing::StrictMock<StateMock>* m)
            : mock(m)
        {
        }

        auto on_event(const e1& /* event */) const
        {
            mock->on_state_event(type_id<reentrant_state<Tag>>, type_id<e1>);
            return Emit<e2>();
        }

        auto on_event(const e2& /* event */) const
        {
            mock->on_state_event(type_id<reentrant_state<Tag>>, type_id<e2>);
            return Discard{};
        }
    };

    template <typename Tag>
    struct reentrant_chain_state
    {
        using events = nil::xalt::tlist<e1, e2, e3>;

        StateMock* mock = nullptr;

        explicit reentrant_chain_state(auto* /* parent */, testing::StrictMock<StateMock>* m)
            : mock(m)
        {
        }

        auto on_event(const e1& /* event */) const
        {
            mock->on_state_event(type_id<reentrant_chain_state<Tag>>, type_id<e1>);
            return Emit<e2>();
        }

        auto on_event(const e2& /* event */) const
        {
            mock->on_state_event(type_id<reentrant_chain_state<Tag>>, type_id<e2>);
            return Emit<e3>();
        }

        auto on_event(const e3& /* event */) const
        {
            mock->on_state_event(type_id<reentrant_chain_state<Tag>>, type_id<e3>);
            return Discard{};
        }
    };

    template <typename Tag>
    struct terminate_on_e1
    {
        using events = nil::xalt::tlist<e1, e2>;

        StateMock* mock = nullptr;

        explicit terminate_on_e1(auto* /* parent */, testing::StrictMock<StateMock>* m)
            : mock(m)
        {
        }

        auto on_event(const e1& /* event */) const
        {
            mock->on_state_event(type_id<terminate_on_e1<Tag>>, type_id<e1>);
            return Terminate{};
        }

        auto on_event(const e2& /* event */) const
        {
            mock->on_state_event(type_id<terminate_on_e1<Tag>>, type_id<e2>);
            return Discard{};
        }
    };

    template <typename Tag, typename Child>
    struct parent_with_child_termination
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1, e2>;

        StateMock* mock = nullptr;

        explicit parent_with_child_termination(
            auto* /* parent */,
            testing::StrictMock<StateMock>* m
        )
            : mock(m)
        {
        }

        auto on_event(const e1& /* event */) const
        {
            mock->on_state_event(type_id<parent_with_child_termination<Tag, Child>>, type_id<e1>);
            return Discard{};
        }

        auto on_event(const e2& /* event */) const
        {
            mock->on_state_event(type_id<parent_with_child_termination<Tag, Child>>, type_id<e2>);
            return Discard{};
        }
    };
}

TEST(sm_feature_edge_cases, state_with_no_regions)
{
    struct tag_root
    {
    };

    using root = discard_on_e1<tag_root>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* root_id = type_id<root>;
    const void* e1_id = type_id<e1>;

    EdgeCaseSM<root> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(root_id, e1_id)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_edge_cases, state_with_no_events)
{
    struct tag_root
    {
    };

    struct tag_child
    {
    };

    using child = discard_on_e1<tag_child>;
    using root = root_with_regions<tag_root, child>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* child_id = type_id<child>;
    const void* e1_id = type_id<e1>;

    EdgeCaseSM<root> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(child_id, e1_id)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_edge_cases, composite_with_empty_regions)
{
    struct tag_root
    {
    };

    using root = empty_regions_root<tag_root>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* root_id = type_id<root>;
    const void* e1_id = type_id<e1>;

    EdgeCaseSM<root> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(root_id, e1_id)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_edge_cases, deep_hierarchy_ten_plus_levels)
{
    struct tag_leaf
    {
    };

    using leaf = discard_on_e1<tag_leaf>;
    using n10 = chain_node<struct tag10, leaf>;
    using n9 = chain_node<struct tag9, n10>;
    using n8 = chain_node<struct tag8, n9>;
    using n7 = chain_node<struct tag7, n8>;
    using n6 = chain_node<struct tag6, n7>;
    using n5 = chain_node<struct tag5, n6>;
    using n4 = chain_node<struct tag4, n5>;
    using n3 = chain_node<struct tag3, n4>;
    using n2 = chain_node<struct tag2, n3>;
    using n1 = chain_node<struct tag1, n2>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* leaf_id = type_id<leaf>;
    const void* e1_id = type_id<e1>;

    EdgeCaseSM<n1> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(leaf_id, e1_id)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_edge_cases, many_orthogonal_regions)
{
    using r0 = discard_on_e1<struct tag0>;
    using r1 = discard_on_e1<struct tag1>;
    using r2 = discard_on_e1<struct tag2>;
    using r3 = discard_on_e1<struct tag3>;
    using r4 = discard_on_e1<struct tag4>;
    using r5 = discard_on_e1<struct tag5>;
    using r6 = discard_on_e1<struct tag6>;
    using r7 = discard_on_e1<struct tag7>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* e1_id = type_id<e1>;

    EdgeCaseSM<r0, r1, r2, r3, r4, r5, r6, r7> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(testing::_, e1_id)).Times(8);
        sm.post(e1{});
    }
}

TEST(sm_feature_edge_cases, event_not_present_anywhere)
{
    using root = root_with_regions<struct root_tag, no_events_region<struct child_tag>>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;

    EdgeCaseSM<root> sm(&mock, {});
    {
        sm.post(e1{});
    }
    {
        sm.post(e2{});
    }

    // No events handled, so no mock calls expected
}

TEST(sm_feature_edge_cases, reentrant_event_emission)
{
    using state = reentrant_state<struct tag_state>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* state_id = type_id<state>;
    const void* e1_id = type_id<e1>;
    const void* e2_id = type_id<e2>;

    EdgeCaseSM<state> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(state_id, e1_id)).Times(1);
        EXPECT_CALL(mock, on_state_event(state_id, e2_id)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_edge_cases, reentrant_event_emission_chain)
{
    using state = reentrant_chain_state<struct tag_state_chain>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* state_id = type_id<state>;
    const void* e1_id = type_id<e1>;
    const void* e2_id = type_id<e2>;
    const void* e3_id = type_id<e3>;

    EdgeCaseSM<state> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(state_id, e1_id)).Times(1);
        EXPECT_CALL(mock, on_state_event(state_id, e2_id)).Times(1);
        EXPECT_CALL(mock, on_state_event(state_id, e3_id)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_edge_cases, terminate_stops_region)
{
    struct tag_state
    {
    };

    using state = terminate_on_e1<tag_state>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* state_id = type_id<state>;
    const void* e1_id = type_id<e1>;

    EdgeCaseSM<state> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(state_id, e1_id)).Times(1);
        // After terminate, e2 is not processed
        sm.post(e1{});
    }
    {
        sm.post(e2{});
    }
}

TEST(sm_feature_edge_cases, terminate_in_child_stops_only_child)
{
    struct tag_parent
    {
    };

    struct tag_child
    {
    };

    using child = terminate_on_e1<tag_child>;
    using parent = parent_with_child_termination<tag_parent, child>;

    testing::StrictMock<StateMock> mock;
    testing::InSequence sequence;
    const void* child_id = type_id<child>;
    const void* parent_id = type_id<parent>;
    const void* e1_id = type_id<e1>;
    const void* e2_id = type_id<e2>;

    EdgeCaseSM<parent> sm(&mock, {});
    {
        EXPECT_CALL(mock, on_state_event(child_id, e1_id)).Times(1);
        sm.post(e1{});
    }
    {
        EXPECT_CALL(mock, on_state_event(parent_id, e2_id)).Times(1);
        sm.post(e2{});
    }
}
