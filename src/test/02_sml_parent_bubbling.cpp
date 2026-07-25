#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

using namespace sml_test;

TEST(sml_feature_parent_bubbling, parent_handles_forwarded_event)
{
    struct case_tag
    {
    };

    using child = regular_leaf<case_tag, 2>;
    using root = regular_node_state<case_tag, 1, child>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<child>(activity);
    expect_react<child>(activity, reaction_choice::forward);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<child>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_parent_bubbling, parent_forwards_forwarded_event)
{
    struct case_tag
    {
    };

    using child = regular_leaf<case_tag, 2>;
    using root = regular_node_state<case_tag, 1, child>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<child>(activity);
    expect_react<child>(activity, reaction_choice::forward);
    expect_react<root>(activity, reaction_choice::forward);
    expect_exit<child>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_parent_bubbling, parent_skips_discarded_child_event)
{
    struct case_tag
    {
    };

    using child = regular_leaf<case_tag, 2>;
    using root = regular_node_state<case_tag, 1, child>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<child>(activity);
    expect_react<child>(activity, reaction_choice::discard);
    expect_exit<child>(activity);
    expect_exit<root>(activity);
    expect_no_react<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_parent_bubbling, parent_handles_unhandled_child_event)
{
    struct case_tag
    {
    };

    using child = unhandled_leaf<case_tag, 2>;
    using root = regular_node_state<case_tag, 1, child>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<child>(activity);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<child>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_parent_bubbling, no_parent_handler_on_unhandled_event)
{
    struct case_tag
    {
    };

    using child = unhandled_leaf<case_tag, 2>;
    using root = no_event_node_state<case_tag, 1, child>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<child>(activity);
    expect_exit<child>(activity);
    expect_exit<root>(activity);
    expect_no_react<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}
