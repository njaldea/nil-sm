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

    template <typename T>
    auto build_validated()
    {
        auto model = nil::sm::ir::build<barrier_api, T>();
        nil::sm::validate(model);
        return model;
    }
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_are_satisfied)
{
    EXPECT_TRUE((nil::sm::validate<barrier_api, two_levels_ok_root>()));

    const auto model = build_validated<two_levels_ok_root>();

    EXPECT_FALSE(model.has_unsatisfied_args);
    EXPECT_TRUE(model.barrier_errors.empty());

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_TRUE(output.str().empty());
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_second_state_missing_a_fails)
{
    EXPECT_FALSE((nil::sm::validate<barrier_api, two_levels_missing_a_root>()));

    const auto model = build_validated<two_levels_missing_a_root>();

    EXPECT_TRUE(model.has_unsatisfied_args);
    EXPECT_TRUE(model.barrier_errors.empty());

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_EQ(output.str(), "root | A |\n");
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_second_barrier_is_satisfied)
{
    const auto model = build_validated<two_levels_barrier_ok_root>();

    EXPECT_FALSE(model.has_unsatisfied_args);
    EXPECT_TRUE(model.barrier_errors.empty());

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_TRUE(output.str().empty());
}

TEST(sm_feature_barrier_dependency_diagnostics, two_levels_second_barrier_missing_a_fails)
{
    const auto model = build_validated<two_levels_barrier_missing_a_root>();

    EXPECT_TRUE(model.has_unsatisfied_args);
    ASSERT_EQ(model.barrier_errors.size(), 1U);
    EXPECT_EQ(model.barrier_errors.front().dependency.type_id, nil::xalt::type_id<A>);

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_EQ(output.str(), "barrier 2 | A | state 1\n");
}

TEST(sm_feature_barrier_dependency_diagnostics, three_levels_second_state_missing_a_fails)
{
    const auto model = build_validated<three_levels_second_missing_a_root>();

    EXPECT_TRUE(model.has_unsatisfied_args);
    EXPECT_TRUE(model.barrier_errors.empty());

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_EQ(output.str(), "root | A |\n");
}

TEST(
    sm_feature_barrier_dependency_diagnostics,
    three_levels_second_and_third_barriers_third_missing_b_fails
)
{
    const auto model = build_validated<three_levels_third_barrier_missing_b_root>();

    EXPECT_TRUE(model.has_unsatisfied_args);
    ASSERT_EQ(model.barrier_errors.size(), 1U);
    EXPECT_EQ(model.barrier_errors.front().dependency.type_id, nil::xalt::type_id<B>);
    EXPECT_EQ(model.barrier_errors.front().barrier_path.size(), 2U);

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_EQ(output.str(), "barrier 3 | B | state 1 - barrier 2\n");
}

TEST(sm_feature_barrier_dependency_diagnostics, three_levels_second_barrier_missing_a_fails)
{
    const auto model = build_validated<three_levels_second_barrier_missing_a_root>();

    EXPECT_TRUE(model.has_unsatisfied_args);
    ASSERT_EQ(model.barrier_errors.size(), 1U);
    EXPECT_EQ(model.barrier_errors.front().dependency.type_id, nil::xalt::type_id<A>);
    EXPECT_EQ(model.barrier_errors.front().barrier_path.size(), 1U);

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_EQ(output.str(), "barrier 2 | A | state 1\n");
}

TEST(
    sm_feature_barrier_dependency_diagnostics,
    three_levels_second_barrier_missing_a_and_third_barrier_missing_b_fail
)
{
    const auto model = build_validated<three_levels_second_missing_a_third_missing_b_root>();

    EXPECT_TRUE(model.has_unsatisfied_args);
    ASSERT_EQ(model.barrier_errors.size(), 2U);
    EXPECT_EQ(model.barrier_errors[0].dependency.type_id, nil::xalt::type_id<A>);
    EXPECT_EQ(model.barrier_errors[1].dependency.type_id, nil::xalt::type_id<B>);

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);

    EXPECT_EQ(
        output.str(),
        "barrier 2 | A | state 1\n"
        "barrier 3 | B | state 1 - barrier 2\n"
    );
}

TEST(sm_feature_barrier_dependency_diagnostics, transition_reaches_nested_barrier_levels)
{
    const auto model = build_validated<transitioned_to_nested_barrier>();

    ASSERT_TRUE(model.has_unsatisfied_args);
    ASSERT_EQ(model.barriers.size(), 2U);
    ASSERT_EQ(model.barrier_errors.size(), 2U);
    EXPECT_EQ(model.barriers[0].name, "barrier 2");
    EXPECT_EQ(model.barriers[1].name, "barrier 3");

    std::ostringstream output;
    nil::sm::ir::print_errors(output, model);
    EXPECT_EQ(
        output.str(),
        "barrier 3 | B | state 1a - barrier 2\n"
        "barrier 3 | B | state 1b - barrier 2\n"
    );
}
