// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm/barrier.hpp>
#include <nil/sm/diagnostics.hpp>

#include <gtest/gtest.h>

#include <sstream>

namespace
{
    struct go
    {
    };

    struct A
    {
    };

    struct B
    {
    };

    struct needs_a
    {
        static constexpr auto name = "state 2";
        using args = nil::xalt::tlist<A>;

        explicit needs_a(A* /* dependency */)
        {
        }
    };

    struct needs_b
    {
        static constexpr auto name = "state 3";
        using args = nil::xalt::tlist<B>;

        explicit needs_b(B* /* dependency */)
        {
        }
    };

    using barrier_api = nil::sm::api::Default<>;

    struct two_levels_ok_root
    {
        static constexpr auto name = "state 1";
        A value;
        using props = nil::xalt::tlist<nil::sm::prop<A, &two_levels_ok_root::value>>;
        using regions = nil::xalt::tlist<needs_a>;
    };

    struct two_levels_missing_a_root
    {
        static constexpr auto name = "state 1";
        using regions = nil::xalt::tlist<needs_a>;
    };

    NIL_SM_BARRIER_DECLARE(two_levels_barrier_state, barrier_api, "barrier 2");
    NIL_SM_BARRIER_DEFINE(two_levels_barrier_state, needs_a);

    using two_levels_barrier
        = nil::sm::barrier::State<nil::sm::Terminate, two_levels_barrier_state>;

    struct two_levels_barrier_ok_root
    {
        static constexpr auto name = "state 1";
        A value;
        using props = nil::xalt::tlist<nil::sm::prop<A, &two_levels_barrier_ok_root::value>>;
        using regions = nil::xalt::tlist<two_levels_barrier>;
    };

    struct two_levels_barrier_missing_a_root
    {
        static constexpr auto name = "state 1";
        using regions = nil::xalt::tlist<two_levels_barrier>;
    };

    struct third_level
    {
        static constexpr auto name = "state 3";
    };

    struct second_level_missing_a
    {
        static constexpr auto name = "state 2";
        using args = nil::xalt::tlist<A>;
        using regions = nil::xalt::tlist<third_level>;

        explicit second_level_missing_a(A* /* dependency */)
        {
        }
    };

    struct three_levels_second_missing_a_root
    {
        static constexpr auto name = "state 1";
        using regions = nil::xalt::tlist<second_level_missing_a>;
    };

    NIL_SM_BARRIER_DECLARE(third_barrier_needs_b_state, barrier_api, "barrier 3");
    NIL_SM_BARRIER_DEFINE(third_barrier_needs_b_state, needs_b);

    using third_barrier_needs_b
        = nil::sm::barrier::State<nil::sm::Terminate, third_barrier_needs_b_state>;

    struct second_barrier_third_barrier_root
    {
        static constexpr auto name = "state 2";
        using regions = nil::xalt::tlist<third_barrier_needs_b>;
    };

    NIL_SM_BARRIER_DECLARE(second_barrier_third_barrier_state, barrier_api, "barrier 2");
    NIL_SM_BARRIER_DEFINE(second_barrier_third_barrier_state, second_barrier_third_barrier_root);

    using second_barrier_third_barrier
        = nil::sm::barrier::State<nil::sm::Terminate, second_barrier_third_barrier_state>;

    struct three_levels_third_barrier_missing_b_root
    {
        static constexpr auto name = "state 1";
        using regions = nil::xalt::tlist<second_barrier_third_barrier>;
    };

    struct second_barrier_missing_a_root
    {
        static constexpr auto name = "state 2";
        using args = nil::xalt::tlist<A>;
        using regions = nil::xalt::tlist<third_level>;

        explicit second_barrier_missing_a_root(A* /* dependency */)
        {
        }
    };

    NIL_SM_BARRIER_DECLARE(second_barrier_missing_a_state, barrier_api, "barrier 2");
    NIL_SM_BARRIER_DEFINE(second_barrier_missing_a_state, second_barrier_missing_a_root);

    using second_barrier_missing_a
        = nil::sm::barrier::State<nil::sm::Terminate, second_barrier_missing_a_state>;

    struct three_levels_second_barrier_missing_a_root
    {
        static constexpr auto name = "state 1";
        using regions = nil::xalt::tlist<second_barrier_missing_a>;
    };

    struct second_barrier_missing_a_third_barrier_root
    {
        static constexpr auto name = "state 2";
        using args = nil::xalt::tlist<A>;
        using regions = nil::xalt::tlist<third_barrier_needs_b>;

        explicit second_barrier_missing_a_third_barrier_root(A* /* dependency */)
        {
        }
    };

    NIL_SM_BARRIER_DECLARE(second_barrier_missing_a_and_third_b_state, barrier_api, "barrier 2");
    NIL_SM_BARRIER_DEFINE(
        second_barrier_missing_a_and_third_b_state,
        second_barrier_missing_a_third_barrier_root
    );

    using second_barrier_missing_a_and_third_b
        = nil::sm::barrier::State<nil::sm::Terminate, second_barrier_missing_a_and_third_b_state>;

    struct three_levels_second_missing_a_third_missing_b_root
    {
        static constexpr auto name = "state 1";
        using regions = nil::xalt::tlist<second_barrier_missing_a_and_third_b>;
    };

    struct state_3_barrier_root
    {
        static constexpr auto name = "state 3";
        using args = nil::xalt::tlist<B>;

        explicit state_3_barrier_root(B* /* dependency */)
        {
        }
    };

    NIL_SM_BARRIER_DECLARE(state_3_barrier_state, barrier_api, "barrier 3");
    NIL_SM_BARRIER_DEFINE(state_3_barrier_state, state_3_barrier_root);

    using state_3_barrier = nil::sm::barrier::State<nil::sm::Terminate, state_3_barrier_state>;

    struct state_2_barrier_root
    {
        static constexpr auto name = "state 2";
        using regions = nil::xalt::tlist<state_3_barrier>;
    };

    NIL_SM_BARRIER_DECLARE(state_2_barrier_state, barrier_api, "barrier 2");
    NIL_SM_BARRIER_DEFINE(state_2_barrier_state, state_2_barrier_root);

    using state_2_barrier = nil::sm::barrier::State<nil::sm::Terminate, state_2_barrier_state>;

    struct state_1b
    {
        static constexpr auto name = "state 1b";
        using regions = nil::xalt::tlist<state_2_barrier>;
    };

    struct state_1a
    {
        static constexpr auto name = "state 1a";
        using events = nil::xalt::tlist<go>;
        using regions = nil::xalt::tlist<state_2_barrier>;

        static auto on_event(const go& /* event */)
        {
            return nil::sm::TransitTo<state_1b>{};
        }
    };

    using transitioned_to_nested_barrier = state_1a;
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_are_satisfied)
{
    EXPECT_TRUE((nil::sm::validate<two_levels_ok_root, barrier_api>()));
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_second_state_missing_a_fails)
{
    EXPECT_FALSE((nil::sm::validate<two_levels_missing_a_root, barrier_api>()));
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_second_barrier_is_satisfied)
{
    EXPECT_TRUE((nil::sm::validate<two_levels_barrier_ok_root, barrier_api>()));
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_second_barrier_missing_a_fails)
{
    EXPECT_FALSE((nil::sm::validate<two_levels_barrier_missing_a_root, barrier_api>()));
}

TEST(sm_feature_barrier_dependency_diagnostics, root_args_satisfy_barrier_requirements)
{
    EXPECT_TRUE((nil::sm::validate<two_levels_barrier_missing_a_root, barrier_api, A>()));
    EXPECT_FALSE((nil::sm::validate<two_levels_barrier_missing_a_root, barrier_api, B>()));
}

TEST(sm_feature_barrier_dependency_diagnostics, three_levels_second_state_missing_a_fails)
{
    EXPECT_FALSE((nil::sm::validate<three_levels_second_missing_a_root, barrier_api>()));
}

TEST(
    sm_feature_barrier_dependency_diagnostics,
    three_levels_second_and_third_barriers_third_missing_b_fails
)
{
    EXPECT_FALSE((nil::sm::validate<three_levels_third_barrier_missing_b_root, barrier_api>()));
    EXPECT_TRUE((nil::sm::validate<three_levels_third_barrier_missing_b_root, barrier_api, B>()));
}

TEST(sm_feature_barrier_dependency_diagnostics, three_levels_second_barrier_missing_a_fails)
{
    EXPECT_FALSE((nil::sm::validate<three_levels_second_barrier_missing_a_root, barrier_api>()));
}

TEST(
    sm_feature_barrier_dependency_diagnostics,
    three_levels_second_barrier_missing_a_and_third_barrier_missing_b_fail
)
{
    EXPECT_FALSE(
        (nil::sm::validate<three_levels_second_missing_a_third_missing_b_root, barrier_api>())
    );
    EXPECT_FALSE(
        (nil::sm::validate<three_levels_second_missing_a_third_missing_b_root, barrier_api, A>())
    );
    EXPECT_TRUE(
        (nil::sm::validate<three_levels_second_missing_a_third_missing_b_root, barrier_api, A, B>())
    );
}

TEST(sm_feature_barrier_dependency_diagnostics, transition_reaches_nested_barrier_levels)
{
    const auto model = nil::sm::ir::build<transitioned_to_nested_barrier, barrier_api>();

    ASSERT_EQ(model.barriers.size(), 2U);
    EXPECT_EQ(model.barriers[0].name, "barrier 2");
    EXPECT_EQ(model.barriers[1].name, "barrier 3");
    EXPECT_FALSE(nil::sm::validate(model));
    EXPECT_TRUE(nil::sm::validate(model, nil::sm::ir::make_root_props<B>()));
}
