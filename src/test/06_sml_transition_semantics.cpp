#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

#include <variant>

using namespace sml_test;

namespace
{
    template <typename Tag>
    struct self_transit_leaf: traced<self_transit_leaf<Tag>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Discard, nil::sml::Transit<self_transit_leaf<Tag>>>
        {
            switch (note_react<self_transit_leaf<Tag>>())
            {
                case reaction_choice::transit:
                    return nil::sml::Transit<self_transit_leaf<Tag>>();
                case reaction_choice::discard:
                    return nil::sml::Discard{};
                case reaction_choice::forward:
                    return nil::sml::Discard{};
            }

            return nil::sml::Discard{};
        }
    };

    template <typename Tag, typename Target>
    struct forward_or_transit: traced<forward_or_transit<Tag, Target>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Forward, nil::sml::Discard, nil::sml::Transit<Target>>
        {
            switch (note_react<forward_or_transit<Tag, Target>>())
            {
                case reaction_choice::forward:
                    return nil::sml::Forward{};
                case reaction_choice::discard:
                    return nil::sml::Discard{};
                case reaction_choice::transit:
                    return nil::sml::Transit<Target>();
            }

            return nil::sml::Discard{};
        }
    };

    template <typename Tag, typename Target>
    struct discard_or_transit: traced<discard_or_transit<Tag, Target>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Discard, nil::sml::Transit<Target>>
        {
            switch (note_react<discard_or_transit<Tag, Target>>())
            {
                case reaction_choice::discard:
                    return nil::sml::Discard{};
                case reaction_choice::transit:
                    return nil::sml::Transit<Target>();
                case reaction_choice::forward:
                    return nil::sml::Discard{};
            }

            return nil::sml::Discard{};
        }
    };

    template <typename Case, int Index>
    using self_transit_state = self_transit_leaf<state_tag<Case, Index>>;

    template <typename Case, int Index, typename Target>
    using forward_or_transit_state = forward_or_transit<state_tag<Case, Index>, Target>;

    template <typename Case, int Index, typename Target>
    using discard_or_transit_state = discard_or_transit<state_tag<Case, Index>, Target>;

    template <typename Case>
    struct sibling_transition_case
    {
        using sibling = regular_leaf<Case, 3>;
        using source = transit_leaf_state<Case, 2, sibling>;
        using root = no_event_node_state<Case, 1, source>;
    };

    template <typename Case>
    struct parent_child_transition_case
    {
        using sibling_child = regular_leaf<Case, 4>;
        using source_child = transit_leaf_state<Case, 3, sibling_child>;
        using mid = no_event_node_state<Case, 2, source_child>;
        using root = no_event_node_state<Case, 1, mid>;
    };

    template <typename Case>
    struct chained_transition_case
    {
        using c = regular_leaf<Case, 3>;
        using b = transit_leaf_state<Case, 2, c>;
        using a = transit_leaf_state<Case, 1, b>;
    };

    template <typename Case>
    struct forwarded_transition_case
    {
        using target = regular_leaf<Case, 3>;
        using source = forward_or_transit_state<Case, 2, target>;
        using root = regular_node_state<Case, 1, source>;
    };

    template <typename Case>
    struct discarded_transition_case
    {
        using target = regular_leaf<Case, 3>;
        using source = discard_or_transit_state<Case, 2, target>;
        using root = regular_node_state<Case, 1, source>;
    };

    template <typename... Regions>
    void run_e1(nil::sml::SM<nil::xalt::tlist<Regions...>>& sm)
    {
        send(sm, e1{});
    }
}

TEST(sml_feature_transition_semantics, self_transition_reconstructs_state)
{
    struct case_tag
    {
    };

    using self_state = self_transit_state<case_tag, 1>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<self_state>(activity);
    expect_react<self_state>(activity, reaction_choice::transit);
    expect_exit<self_state>(activity);
    expect_enter<self_state>(activity);
    expect_react<self_state>(activity, reaction_choice::discard);
    expect_exit<self_state>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<self_state>> sm{};
        send(sm, e1{});
        send(sm, e1{});
    }
}

TEST(sml_feature_transition_semantics, transition_to_sibling_state)
{
    struct case_tag
    {
    };

    using cfg = sibling_transition_case<case_tag>;
    using root = cfg::root;
    using source = cfg::source;
    using sibling = cfg::sibling;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<source>(activity);
    expect_react<source>(activity, reaction_choice::transit);
    expect_exit<source>(activity);
    expect_enter<sibling>(activity);
    expect_react<sibling>(activity, reaction_choice::discard);
    expect_exit<sibling>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        run_e1(sm);
        run_e1(sm);
    }
}

TEST(sml_feature_transition_semantics, transition_to_parents_child)
{
    struct case_tag
    {
    };

    using cfg = parent_child_transition_case<case_tag>;
    using root = cfg::root;
    using mid = cfg::mid;
    using source_child = cfg::source_child;
    using sibling_child = cfg::sibling_child;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<mid>(activity);
    expect_enter<source_child>(activity);
    expect_react<source_child>(activity, reaction_choice::transit);
    expect_exit<source_child>(activity);
    expect_enter<sibling_child>(activity);
    expect_react<sibling_child>(activity, reaction_choice::discard);
    expect_exit<sibling_child>(activity);
    expect_exit<mid>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        run_e1(sm);
        run_e1(sm);
    }
}

TEST(sml_feature_transition_semantics, transition_destroys_previous_state)
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
    expect_exit<target>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<source>> sm{};
        run_e1(sm);
    }
}

TEST(sml_feature_transition_semantics, multiple_consecutive_transitions)
{
    struct case_tag
    {
    };

    using cfg = chained_transition_case<case_tag>;
    using a = cfg::a;
    using b = cfg::b;
    using c = cfg::c;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<a>(activity);
    expect_react<a>(activity, reaction_choice::transit);
    expect_exit<a>(activity);
    expect_enter<b>(activity);
    expect_react<b>(activity, reaction_choice::transit);
    expect_exit<b>(activity);
    expect_enter<c>(activity);
    expect_react<c>(activity, reaction_choice::discard);
    expect_exit<c>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<a>> sm{};
        run_e1(sm);
        run_e1(sm);
        run_e1(sm);
    }
}

TEST(sml_feature_transition_semantics, transition_after_forwarded_event)
{
    struct case_tag
    {
    };

    using cfg = forwarded_transition_case<case_tag>;
    using root = cfg::root;
    using source = cfg::source;
    using target = cfg::target;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<source>(activity);
    expect_react<source>(activity, reaction_choice::forward);
    expect_react<root>(activity, reaction_choice::discard);
    expect_react<source>(activity, reaction_choice::transit);
    expect_exit<source>(activity);
    expect_enter<target>(activity);
    expect_react<target>(activity, reaction_choice::discard);
    expect_exit<target>(activity);
    expect_exit<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        run_e1(sm);
        run_e1(sm);
        run_e1(sm);
    }
}

TEST(sml_feature_transition_semantics, transition_after_discarded_event)
{
    struct case_tag
    {
    };

    using cfg = discarded_transition_case<case_tag>;
    using root = cfg::root;
    using source = cfg::source;
    using target = cfg::target;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<root>(activity);
    expect_enter<source>(activity);
    expect_react<source>(activity, reaction_choice::discard);
    expect_react<source>(activity, reaction_choice::transit);
    expect_exit<source>(activity);
    expect_enter<target>(activity);
    expect_react<target>(activity, reaction_choice::discard);
    expect_exit<target>(activity);
    expect_exit<root>(activity);
    expect_no_react<root>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        run_e1(sm);
        run_e1(sm);
        run_e1(sm);
    }
}
