#include "00_sml_test_support.hpp"

#include <gtest/gtest.h>

#include <vector>

using namespace sml_test;

namespace
{
    struct out_a
    {
        int value = 0;

        explicit out_a(int v)
            : value(v)
        {
        }
    };

    struct out_b
    {
        int value = 0;

        explicit out_b(int v)
            : value(v)
        {
        }
    };

    struct observed_emit
    {
        const void* id = nullptr;
        int value = 0;
    };

    template <typename Tag>
    struct emit_sink
    {
        using events = nil::xalt::tlist<out_a, out_b>;
        static inline std::vector<observed_emit> seen;

        static void reset()
        {
            seen.clear();
        }

        static auto on_event(const out_a& event)
        {
            seen.push_back(observed_emit{.id = nil::xalt::type_id<out_a>, .value = event.value});
            return nil::sml::Discard{};
        }

        static auto on_event(const out_b& event)
        {
            seen.push_back(observed_emit{.id = nil::xalt::type_id<out_b>, .value = event.value});
            return nil::sml::Discard{};
        }
    };

    template <typename Tag>
    struct emit_leaf: traced<emit_leaf<Tag>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Emit<out_a>(11);
        }
    };

    template <typename Tag>
    struct emit_or_forward_leaf: traced<emit_or_forward_leaf<Tag>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Emit<out_a>, nil::sml::Forward>
        {
            switch (note_react<emit_or_forward_leaf<Tag>>())
            {
                case reaction_choice::forward:
                    return nil::sml::Forward{};
                case reaction_choice::discard:
                case reaction_choice::transit:
                    return nil::sml::Emit<out_a>(21);
            }

            return nil::sml::Emit<out_a>(21);
        }
    };

    template <typename Tag>
    struct emit_or_discard_parent: traced<emit_or_discard_parent<Tag>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Emit<out_b>, nil::sml::Discard>
        {
            switch (note_react<emit_or_discard_parent<Tag>>())
            {
                case reaction_choice::discard:
                    return nil::sml::Discard{};
                case reaction_choice::forward:
                case reaction_choice::transit:
                    return nil::sml::Emit<out_b>(22);
            }

            return nil::sml::Emit<out_b>(22);
        }
    };

    template <typename Tag>
    struct emit_sequence_leaf: traced<emit_sequence_leaf<Tag>>
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Emit<out_a>(31);
        }

        static auto on_event(const e2& /* event */)
        {
            return nil::sml::Emit<out_b>(32);
        }
    };

    template <typename Tag, typename Child>
    struct emit_or_discard_parent_with_child: emit_or_discard_parent<Tag>
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1>;
    };

    template <typename Case>
    struct emit_forward_case
    {
        using child = emit_or_forward_leaf<state_tag<Case, 2>>;
        using parent = regular_node_state<Case, 1, child>;
    };

    template <typename Case>
    struct emit_parent_case
    {
        using child = unhandled_leaf<Case, 2>;
        using parent_base = emit_or_discard_parent<state_tag<Case, 1>>;
        using parent = emit_or_discard_parent_with_child<state_tag<Case, 1>, child>;
    };

    template <typename Case>
    struct emit_orthogonal_case
    {
        using r1 = emit_leaf<state_tag<Case, 1>>;
        using r2 = emit_leaf<state_tag<Case, 2>>;
    };

    template <typename... Regions>
    void run_e1(nil::sml::SM<nil::xalt::tlist<Regions...>>& sm)
    {
        send(sm, e1{});
    }

    template <typename Sink>
    void expect_sink_single_emit(const void* id, int value)
    {
        ASSERT_EQ(Sink::seen.size(), 1U);
        EXPECT_EQ(Sink::seen[0].id, id);
        EXPECT_EQ(Sink::seen[0].value, value);
    }

    template <typename Sink>
    void expect_sink_two_emits(const void* id0, int value0, const void* id1, int value1)
    {
        ASSERT_EQ(Sink::seen.size(), 2U);
        EXPECT_EQ(Sink::seen[0].id, id0);
        EXPECT_EQ(Sink::seen[1].id, id1);
        EXPECT_EQ(Sink::seen[0].value, value0);
        EXPECT_EQ(Sink::seen[1].value, value1);
    }

    template <typename Sink>
    void expect_sink_three_emits(
        const void* id0,
        int value0,
        const void* id1,
        int value1,
        const void* id2,
        int value2
    )
    {
        ASSERT_EQ(Sink::seen.size(), 3U);
        EXPECT_EQ(Sink::seen[0].id, id0);
        EXPECT_EQ(Sink::seen[1].id, id1);
        EXPECT_EQ(Sink::seen[2].id, id2);
        EXPECT_EQ(Sink::seen[0].value, value0);
        EXPECT_EQ(Sink::seen[1].value, value1);
        EXPECT_EQ(Sink::seen[2].value, value2);
    }
}

TEST(sml_feature_emit_handling, emit_from_leaf_reaction)
{
    struct case_tag
    {
    };

    using leaf = emit_leaf<state_tag<case_tag, 1>>;
    using sink = emit_sink<state_tag<case_tag, 90>>;

    sink::reset();
    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<leaf>(activity);
    expect_exit<leaf>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<leaf, sink>> sm{};
        run_e1(sm);
    }

    expect_sink_single_emit<sink>(nil::xalt::type_id<out_a>, 11);
}

TEST(sml_feature_emit_handling, emit_during_forward_path)
{
    struct case_tag
    {
    };

    using cfg = emit_forward_case<case_tag>;
    using child = cfg::child;
    using parent = cfg::parent;
    using sink = emit_sink<state_tag<case_tag, 90>>;

    sink::reset();
    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<parent>(activity);
    expect_enter<child>(activity);
    expect_react<child>(activity, reaction_choice::forward);
    expect_react<parent>(activity, reaction_choice::discard);
    expect_react<child>(activity, reaction_choice::discard);
    expect_exit<child>(activity);
    expect_exit<parent>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<parent, sink>> sm{};
        run_e1(sm);
        run_e1(sm);
    }

    expect_sink_single_emit<sink>(nil::xalt::type_id<out_a>, 21);
}

TEST(sml_feature_emit_handling, emit_from_parent_reaction)
{
    struct case_tag
    {
    };

    using cfg = emit_parent_case<case_tag>;
    using child = cfg::child;
    using parent_base = cfg::parent_base;
    using parent = cfg::parent;
    using sink = emit_sink<state_tag<case_tag, 90>>;

    sink::reset();
    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<parent_base>(activity);
    expect_enter<child>(activity);
    expect_react<parent_base>(activity, reaction_choice::forward);
    expect_exit<child>(activity);
    expect_exit<parent_base>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<parent, sink>> sm{};
        run_e1(sm);
    }

    expect_sink_single_emit<sink>(nil::xalt::type_id<out_b>, 22);
}

TEST(sml_feature_emit_handling, emit_from_orthogonal_regions)
{
    struct case_tag
    {
    };

    using cfg = emit_orthogonal_case<case_tag>;
    using r1 = cfg::r1;
    using r2 = cfg::r2;
    using sink = emit_sink<state_tag<case_tag, 90>>;

    sink::reset();
    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<r1>(activity);
    expect_enter<r2>(activity);
    expect_exit<r2>(activity);
    expect_exit<r1>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<r1, r2, sink>> sm{};
        run_e1(sm);
    }

    expect_sink_two_emits<sink>(nil::xalt::type_id<out_a>, 11, nil::xalt::type_id<out_a>, 11);
}

TEST(sml_feature_emit_handling, multiple_emit_events_preserve_order)
{
    struct case_tag
    {
    };

    using state = emit_sequence_leaf<state_tag<case_tag, 1>>;
    using sink = emit_sink<state_tag<case_tag, 90>>;

    sink::reset();
    testing::StrictMock<activity_mock> activity;
    activity_scope scope(activity);

    testing::InSequence sequence;
    expect_enter<state>(activity);
    expect_exit<state>(activity);

    {
        nil::sml::SM<nil::xalt::tlist<state, sink>> sm{};
        send(sm, e1{});
        send(sm, e2{});
        send(sm, e1{});
    }

    expect_sink_three_emits<sink>(
        nil::xalt::type_id<out_a>,
        31,
        nil::xalt::type_id<out_b>,
        32,
        nil::xalt::type_id<out_a>,
        31
    );
}
