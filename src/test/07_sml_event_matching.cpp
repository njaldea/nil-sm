#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

using namespace sml_test;

namespace
{
    struct e3
    {
    };

    template <typename Tag>
    struct two_event_state: traced<two_event_state<Tag>>
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */) -> regular_reaction
        {
            return react_regular_from_mock<two_event_state<Tag>>();
        }

        static auto on_event(const e2& /* event */) -> regular_reaction
        {
            return react_regular_from_mock<two_event_state<Tag>>();
        }
    };

    template <typename Tag>
    struct explicit_empty_events_state: traced<explicit_empty_events_state<Tag>>
    {
        using events = nil::xalt::tlist<>;
    };
}

TEST(sml_feature_event_matching, event_matches_first_handler)
{
    struct tag_state
    {
    };

    using state = two_event_state<tag_state>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<state>(activity);
    expect_react<state>(activity, reaction_choice::discard);
    expect_exit<state>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<state>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_event_matching, event_matches_last_handler)
{
    struct tag_state
    {
    };

    using state = two_event_state<tag_state>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<state>(activity);
    expect_react<state>(activity, reaction_choice::discard);
    expect_exit<state>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<state>> sm{};
        send(sm, e2{});
    }
}

TEST(sml_feature_event_matching, event_matches_none)
{
    struct tag_state
    {
    };

    using state = two_event_state<tag_state>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<state>(activity);
    expect_exit<state>(activity);
    expect_no_react<state>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<state>> sm{};
        send(sm, e3{});
    }
}

TEST(sml_feature_event_matching, state_with_empty_event_list)
{
    struct tag_empty
    {
    };

    using explicit_empty_events = explicit_empty_events_state<tag_empty>;

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<explicit_empty_events>(activity);
    expect_exit<explicit_empty_events>(activity);
    expect_no_react<explicit_empty_events>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<explicit_empty_events>> sm{};
        send(sm, e1{});
    }
}

TEST(sml_feature_event_matching, state_with_default_events_only)
{
    struct default_events_state: traced<default_events_state>
    {
    };

    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<default_events_state>(activity);
    expect_exit<default_events_state>(activity);
    expect_no_react<default_events_state>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<default_events_state>> sm{};
        send(sm, e1{});
    }
}
