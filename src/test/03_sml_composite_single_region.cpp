#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

#include <variant>

using namespace sml_test;

namespace
{
    template <typename Tag, typename Target, typename Child>
    struct parent_transit_or_discard: traced<parent_transit_or_discard<Tag, Target, Child>>
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Discard, nil::sml::Transit<Target>>
        {
            switch (note_react<parent_transit_or_discard<Tag, Target, Child>>())
            {
                case reaction_choice::transit:
                    return nil::sml::Transit<Target>();
                case reaction_choice::forward:
                case reaction_choice::discard:
                    return nil::sml::Discard{};
            }

            return nil::sml::Discard{};
        }
    };
}

TEST(sml_feature_composite_single_region, child_transition_applies_when_parent_no_transition)
{
    struct case_tag
    {
    };

    using target = regular_leaf<case_tag, 3>;
    using source = transit_leaf_state<case_tag, 2, target>;
    using root = regular_node_state<case_tag, 1, source>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<source>(activity);
    expect_react<source>(activity, reaction_choice::transit);
    expect_exit<source>(activity);
    expect_enter<target>(activity);
    expect_react<target>(activity, reaction_choice::forward);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<target>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
        send(sm, e1{});
    }
}

TEST(sml_feature_composite_single_region, composite_receives_child_forward)
{
    struct case_tag
    {
    };

    using leaf = regular_leaf<case_tag, 3>;
    using mid = regular_node_state<case_tag, 2, leaf>;
    using root = regular_node_state<case_tag, 1, mid>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<mid>(activity);
    expect_enter<leaf>(activity);
    expect_react<leaf>(activity, reaction_choice::forward);
    expect_react<mid>(activity, reaction_choice::forward);
    expect_react<root>(activity, reaction_choice::discard);
    expect_exit<leaf>(activity);
    expect_exit<mid>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_composite_single_region, parent_transition_after_child_forward)
{
    struct case_tag
    {
    };

    using child = regular_leaf<case_tag, 2>;
    using parent_target = regular_leaf<case_tag, 3>;
    using root = parent_transit_or_discard<state_tag<case_tag, 1>, parent_target, child>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<child>(activity);
    expect_react<child>(activity, reaction_choice::forward);
    expect_react<root>(activity, reaction_choice::transit);
    expect_exit<child>(activity);
    expect_exit<root>(activity);
    expect_enter<parent_target>(activity);
    expect_exit<parent_target>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        send(sm, e1{});
    }
}
