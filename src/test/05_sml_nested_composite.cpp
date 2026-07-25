#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

using namespace sml_test;

TEST(sml_feature_nested_composite, deep_orthogonal_nested_branches)
{
    struct case_tag
    {
    };

    using branch1_leaf = regular_leaf<case_tag, 4>;
    using branch2_leaf = regular_leaf<case_tag, 5>;
    using branch1 = regular_node_state<case_tag, 2, branch1_leaf>;
    using branch2 = regular_node_state<case_tag, 3, branch2_leaf>;
    using root = regular_node_state<case_tag, 1, branch1, branch2>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<branch1>(activity);
    expect_enter<branch1_leaf>(activity);
    expect_enter<branch2>(activity);
    expect_enter<branch2_leaf>(activity);
    expect_react<branch1_leaf>(activity, reaction_choice::discard);
    expect_react<branch2_leaf>(activity, reaction_choice::forward);
    expect_react<branch2>(activity, reaction_choice::forward);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<branch2_leaf>(activity);
    expect_exit<branch2>(activity);
    expect_exit<branch1_leaf>(activity);
    expect_exit<branch1>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}
