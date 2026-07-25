#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

using namespace sml_test;

TEST(sml_feature_orthogonal, all_discard_parent_skipped)
{
    struct case_tag
    {
    };

    using r1 = regular_leaf<case_tag, 2>;
    using r2 = regular_leaf<case_tag, 3>;
    using root = regular_node_state<case_tag, 1, r1, r2>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<r1>(activity);
    expect_enter<r2>(activity);
    expect_react<r1>(activity, reaction_choice::discard);
    expect_react<r2>(activity, reaction_choice::discard);
    expect_exit<r2>(activity);
    expect_exit<r1>(activity);
    expect_exit<root>(activity);
    expect_no_react<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_orthogonal, forward_discard_parent_handles)
{
    struct case_tag
    {
    };

    using r1 = regular_leaf<case_tag, 2>;
    using r2 = regular_leaf<case_tag, 3>;
    using root = regular_node_state<case_tag, 1, r1, r2>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<r1>(activity);
    expect_enter<r2>(activity);
    expect_react<r1>(activity, reaction_choice::forward);
    expect_react<r2>(activity, reaction_choice::discard);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<r2>(activity);
    expect_exit<r1>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_orthogonal, all_unhandled_parent_handles)
{
    struct case_tag
    {
    };

    using u1 = unhandled_leaf<case_tag, 2>;
    using u2 = unhandled_leaf<case_tag, 3>;
    using root = regular_node_state<case_tag, 1, u1, u2>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<u1>(activity);
    expect_enter<u2>(activity);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<u2>(activity);
    expect_exit<u1>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_orthogonal, no_parent_all_unhandled)
{
    struct case_tag
    {
    };

    using u1 = unhandled_leaf<case_tag, 2>;
    using u2 = unhandled_leaf<case_tag, 3>;
    using root = no_event_node_state<case_tag, 1, u1, u2>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<u1>(activity);
    expect_enter<u2>(activity);
    expect_exit<u2>(activity);
    expect_exit<u1>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}
