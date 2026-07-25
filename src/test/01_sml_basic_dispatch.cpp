#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

using namespace sml_test;

TEST(sml_feature_basic_dispatch, leaf_event_forwarded)
{
    struct case_tag
    {
    };

    using leaf = regular_leaf<case_tag, 1>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<leaf>(activity);
    expect_react<leaf>(activity, reaction_choice::forward);
    expect_exit<leaf>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<leaf>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_basic_dispatch, leaf_event_discarded)
{
    struct case_tag
    {
    };

    using leaf = regular_leaf<case_tag, 1>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<leaf>(activity);
    expect_react<leaf>(activity, reaction_choice::discard);
    expect_exit<leaf>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<leaf>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_basic_dispatch, leaf_event_unhandled)
{
    struct case_tag
    {
    };

    using leaf = unhandled_leaf<case_tag, 1>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<leaf>(activity);
    expect_exit<leaf>(activity);
    expect_no_react<leaf>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<leaf>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_basic_dispatch, unrelated_event_not_matched)
{
    struct case_tag
    {
    };

    using root = regular_node_state<case_tag, 1>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
        send(sm, e2{});
    }
}

TEST(sml_feature_basic_dispatch, leaf_event_transitions)
{
    struct case_tag
    {
    };

    using target = regular_leaf<case_tag, 2>;
    using source = transit_leaf_state<case_tag, 1, target>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<source>(activity);
    expect_react<source>(activity, reaction_choice::transit);
    expect_exit<source>(activity);
    expect_enter<target>(activity);
    expect_react<target>(activity, reaction_choice::discard);
    expect_exit<target>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<source>> sm{};
        send(sm, e1{});
        send(sm, e1{});
    }
}
