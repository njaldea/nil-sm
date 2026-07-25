#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

#include <variant>

using namespace sml_test;

namespace
{
    template <typename Tag, typename Target>
    struct transit_or_regular_leaf: traced<transit_or_regular_leaf<Tag, Target>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Forward, nil::sml::Discard, nil::sml::Transit<Target>>
        {
            switch (note_react<transit_or_regular_leaf<Tag, Target>>())
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

    template <typename Tag, typename ParentTarget, typename... Regions>
    struct parent_transit_or_regular
        : traced<parent_transit_or_regular<Tag, ParentTarget, Regions...>>
    {
        using regions = nil::xalt::tlist<Regions...>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Forward, nil::sml::Discard, nil::sml::Transit<ParentTarget>>
        {
            switch (note_react<parent_transit_or_regular<Tag, ParentTarget, Regions...>>())
            {
                case reaction_choice::forward:
                    return nil::sml::Forward{};
                case reaction_choice::discard:
                    return nil::sml::Discard{};
                case reaction_choice::transit:
                    return nil::sml::Transit<ParentTarget>();
            }

            return nil::sml::Discard{};
        }
    };

    template <typename Case>
    struct parent_child_case
    {
        using r1_target = regular_leaf<Case, 4>;
        using r1_source = transit_or_regular_leaf<state_tag<Case, 2>, r1_target>;
        using r2 = regular_leaf<Case, 3>;
        using parent_target = regular_leaf<Case, 5>;
        using parent = parent_transit_or_regular<state_tag<Case, 1>, parent_target, r1_source, r2>;
    };

    template <typename Case>
    struct dual_child_case
    {
        using r1_target = regular_leaf<Case, 3>;
        using r2_target = regular_leaf<Case, 4>;
        using r1_source = transit_or_regular_leaf<state_tag<Case, 1>, r1_target>;
        using r2_source = transit_or_regular_leaf<state_tag<Case, 2>, r2_target>;
    };

    template <typename Case>
    struct nested_child_case
    {
        using leaf_target = regular_leaf<Case, 4>;
        using leaf_source = transit_or_regular_leaf<state_tag<Case, 3>, leaf_target>;
        using branch = no_event_node_state<Case, 1, leaf_source>;
        using sibling = regular_leaf<Case, 2>;
    };

    template <typename Case>
    struct parent_cancel_case
    {
        using t1_target = regular_leaf<Case, 5>;
        using t2_target = regular_leaf<Case, 6>;
        using t1_source = transit_or_regular_leaf<state_tag<Case, 2>, t1_target>;
        using t2_source = transit_or_regular_leaf<state_tag<Case, 3>, t2_target>;
        using fwd = regular_leaf<Case, 4>;
        using parent_target = regular_leaf<Case, 7>;
        using parent = parent_transit_or_regular<
            state_tag<Case, 1>,
            parent_target,
            t1_source,
            t2_source,
            fwd>;
    };

    template <typename... Regions>
    void run_e1(nil::sml::SM<nil::xalt::tlist<Regions...>>& sm)
    {
        send(sm, e1{});
    }
}

TEST(sml_feature_orthogonal_transition_ordering, child_transition_plus_parent_discard)
{
    struct case_tag
    {
    };

    using cfg = parent_child_case<case_tag>;
    using r1_source = cfg::r1_source;
    using r1_target = cfg::r1_target;
    using r2 = cfg::r2;
    using parent = cfg::parent;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<parent>(activity);
    expect_enter<r1_source>(activity);
    expect_enter<r2>(activity);
    expect_react<r1_source>(activity, reaction_choice::transit);
    expect_react<r2>(activity, reaction_choice::forward);
    expect_react<parent>(activity, reaction_choice::discard);
    expect_exit<r1_source>(activity);
    expect_enter<r1_target>(activity);
    expect_exit<r2>(activity);
    expect_exit<r1_target>(activity);
    expect_exit<parent>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<parent>> sm{};
        run_e1(sm);
    }
}

TEST(sml_feature_orthogonal_transition_ordering, child_transition_plus_parent_forward)
{
    struct case_tag
    {
    };

    using cfg = parent_child_case<case_tag>;
    using r1_source = cfg::r1_source;
    using r1_target = cfg::r1_target;
    using r2 = cfg::r2;
    using parent = cfg::parent;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<parent>(activity);
    expect_enter<r1_source>(activity);
    expect_enter<r2>(activity);
    expect_react<r1_source>(activity, reaction_choice::transit);
    expect_react<r2>(activity, reaction_choice::forward);
    expect_react<parent>(activity, reaction_choice::forward);
    expect_exit<r1_source>(activity);
    expect_enter<r1_target>(activity);
    expect_exit<r2>(activity);
    expect_exit<r1_target>(activity);
    expect_exit<parent>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<parent>> sm{};
        run_e1(sm);
    }
}

TEST(sml_feature_orthogonal_transition_ordering, child_transition_plus_parent_transition)
{
    struct case_tag
    {
    };

    using cfg = parent_child_case<case_tag>;
    using r1_source = cfg::r1_source;
    using r1_target = cfg::r1_target;
    using r2 = cfg::r2;
    using parent_target = cfg::parent_target;
    using parent = cfg::parent;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<parent>(activity);
    expect_enter<r1_source>(activity);
    expect_enter<r2>(activity);
    expect_react<r1_source>(activity, reaction_choice::transit);
    expect_react<r2>(activity, reaction_choice::forward);
    expect_react<parent>(activity, reaction_choice::transit);
    expect_exit<r2>(activity);
    expect_exit<r1_source>(activity);
    expect_exit<parent>(activity);
    expect_enter<parent_target>(activity);
    expect_exit<parent_target>(activity);
    expect_no_react<r1_target>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<parent>> sm{};
        run_e1(sm);
    }
}

TEST(sml_feature_orthogonal_transition_ordering, multiple_child_transitions)
{
    struct case_tag
    {
    };

    using cfg = dual_child_case<case_tag>;
    using r1_source = cfg::r1_source;
    using r1_target = cfg::r1_target;
    using r2_source = cfg::r2_source;
    using r2_target = cfg::r2_target;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<r1_source>(activity);
    expect_enter<r2_source>(activity);
    expect_react<r1_source>(activity, reaction_choice::transit);
    expect_react<r2_source>(activity, reaction_choice::transit);
    expect_exit<r1_source>(activity);
    expect_enter<r1_target>(activity);
    expect_exit<r2_source>(activity);
    expect_enter<r2_target>(activity);
    expect_react<r1_target>(activity, reaction_choice::discard);
    expect_react<r2_target>(activity, reaction_choice::discard);
    expect_exit<r2_target>(activity);
    expect_exit<r1_target>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<r1_source, r2_source>> sm{};
        run_e1(sm);
        run_e1(sm);
    }
}

TEST(sml_feature_orthogonal_transition_ordering, nested_child_transitions)
{
    struct case_tag
    {
    };

    using cfg = nested_child_case<case_tag>;
    using branch = cfg::branch;
    using leaf_source = cfg::leaf_source;
    using leaf_target = cfg::leaf_target;
    using sibling = cfg::sibling;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<branch>(activity);
    expect_enter<leaf_source>(activity);
    expect_enter<sibling>(activity);
    expect_react<leaf_source>(activity, reaction_choice::transit);
    expect_exit<leaf_source>(activity);
    expect_enter<leaf_target>(activity);
    expect_react<sibling>(activity, reaction_choice::discard);
    expect_react<leaf_target>(activity, reaction_choice::discard);
    expect_react<sibling>(activity, reaction_choice::discard);
    expect_exit<sibling>(activity);
    expect_exit<leaf_target>(activity);
    expect_exit<branch>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<branch, sibling>> sm{};
        run_e1(sm);
        run_e1(sm);
    }
}

TEST(sml_feature_orthogonal_transition_ordering, sibling_transitions_independent)
{
    struct case_tag
    {
    };

    using cfg = dual_child_case<case_tag>;
    using r1_source = cfg::r1_source;
    using r1_target = cfg::r1_target;
    using r2_source = cfg::r2_source;
    using r2_target = cfg::r2_target;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<r1_source>(activity);
    expect_enter<r2_source>(activity);
    expect_react<r1_source>(activity, reaction_choice::transit);
    expect_react<r2_source>(activity, reaction_choice::transit);
    expect_exit<r1_source>(activity);
    expect_enter<r1_target>(activity);
    expect_exit<r2_source>(activity);
    expect_enter<r2_target>(activity);
    expect_react<r1_target>(activity, reaction_choice::forward);
    expect_react<r2_target>(activity, reaction_choice::discard);
    expect_exit<r2_target>(activity);
    expect_exit<r1_target>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<r1_source, r2_source>> sm{};
        run_e1(sm);
        run_e1(sm);
    }
}

TEST(
    sml_feature_orthogonal_transition_ordering,
    parent_transition_cancels_pending_child_transitions
)
{
    struct case_tag
    {
    };

    using cfg = parent_cancel_case<case_tag>;
    using fwd = cfg::fwd;
    using parent = cfg::parent;
    using parent_target = cfg::parent_target;
    using t1_source = cfg::t1_source;
    using t1_target = cfg::t1_target;
    using t2_source = cfg::t2_source;
    using t2_target = cfg::t2_target;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<parent>(activity);
    expect_enter<t1_source>(activity);
    expect_enter<t2_source>(activity);
    expect_enter<fwd>(activity);
    expect_react<t1_source>(activity, reaction_choice::transit);
    expect_react<t2_source>(activity, reaction_choice::transit);
    expect_react<fwd>(activity, reaction_choice::forward);
    expect_react<parent>(activity, reaction_choice::transit);
    expect_exit<fwd>(activity);
    expect_exit<t2_source>(activity);
    expect_exit<t1_source>(activity);
    expect_exit<parent>(activity);
    expect_enter<parent_target>(activity);
    expect_exit<parent_target>(activity);
    expect_no_react<t1_target>(activity);
    expect_no_react<t2_target>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<parent>> sm{};
        run_e1(sm);
    }
}
