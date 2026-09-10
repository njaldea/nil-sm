// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace
{
    template <typename Initial>
    using dispatch_for = nil::sm::detail::
        region_reachability_graph<nil::sm::api::Default<>::template type, Initial>;

    template <typename Dispatch, typename Target>
    consteval bool dispatch_contains_target_id()
    {
        return Dispatch::states::template any_of<std::is_same, Target>;
    }

    struct event
    {
    };

    // Case 1: A only
    struct only_a
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::Discard{};
        }
    };

    using dispatch_only_a = dispatch_for<only_a>;

    static_assert(std::is_same_v<dispatch_only_a::states, nil::xalt::tlist<only_a>>);
    static_assert(dispatch_only_a::states::size == 1);
    static_assert(dispatch_only_a::index_of<only_a>() == 0);
    static_assert(dispatch_only_a::index_of(nullptr) == dispatch_only_a::states::size);
    static_assert(dispatch_contains_target_id<dispatch_only_a, only_a>());

    // Case 2: A -> B
    struct ab_b;

    struct ab_a
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::TransitTo<ab_b>{};
        }
    };

    struct ab_b
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::Discard{};
        }
    };

    using dispatch_ab_a = dispatch_for<ab_a>;
    using dispatch_ab_b = dispatch_for<ab_b>;

    static_assert(std::is_same_v<dispatch_ab_a::states, nil::xalt::tlist<ab_a, ab_b>>);
    static_assert(dispatch_ab_a::states::size == 2);
    static_assert(dispatch_ab_a::index_of<ab_b>() == 1);
    static_assert(dispatch_ab_a::index_of(nil::xalt::type_id<ab_b>) == 1);
    static_assert(dispatch_contains_target_id<dispatch_ab_a, ab_a>());
    static_assert(dispatch_contains_target_id<dispatch_ab_a, ab_b>());
    static_assert(std::is_same_v<dispatch_ab_b::states, nil::xalt::tlist<ab_b>>);
    static_assert(dispatch_contains_target_id<dispatch_ab_b, ab_b>());

    // Case 3: A -> B -> C -> A
    struct cycle_b;
    struct cycle_c;

    struct cycle_a
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::TransitTo<cycle_b>{};
        }
    };

    struct cycle_b
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::TransitTo<cycle_c>{};
        }
    };

    struct cycle_c
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::TransitTo<cycle_a>{};
        }
    };

    using dispatch_cycle_a = dispatch_for<cycle_a>;
    using dispatch_cycle_b = dispatch_for<cycle_b>;
    using dispatch_cycle_c = dispatch_for<cycle_c>;

    static_assert(std::is_same_v<
                  dispatch_cycle_a::states,
                  nil::xalt::tlist<cycle_a, cycle_b, cycle_c>>);
    static_assert(std::is_same_v<
                  dispatch_cycle_b::states,
                  nil::xalt::tlist<cycle_b, cycle_c, cycle_a>>);
    static_assert(std::is_same_v<
                  dispatch_cycle_c::states,
                  nil::xalt::tlist<cycle_c, cycle_a, cycle_b>>);

    static_assert(dispatch_contains_target_id<dispatch_cycle_a, cycle_a>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_a, cycle_b>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_a, cycle_c>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_b, cycle_a>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_b, cycle_c>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_b, cycle_b>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_c, cycle_a>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_c, cycle_b>());
    static_assert(dispatch_contains_target_id<dispatch_cycle_c, cycle_c>());

    // Case 4: Explicit siblings bypass transition discovery.
    struct explicit_sibling_c
    {
    };

    struct explicit_sibling_b
    {
    };

    struct explicit_sibling_a
    {
    };
}

template <>
struct nil::sm::siblings<explicit_sibling_a>
{
    using type = nil::xalt::tlist<explicit_sibling_c, explicit_sibling_b>;
};

namespace transit_dispatch_state_test
{
    using dispatch_explicit_siblings = dispatch_for<explicit_sibling_a>;

    static_assert(std::is_same_v<
                  dispatch_explicit_siblings::states,
                  nil::xalt::tlist<explicit_sibling_a, explicit_sibling_c, explicit_sibling_b>>);
    static_assert(dispatch_explicit_siblings::index_of<explicit_sibling_a>() == 0);
    static_assert(dispatch_explicit_siblings::index_of<explicit_sibling_c>() == 1);
    static_assert(dispatch_explicit_siblings::index_of<explicit_sibling_b>() == 2);
}

TEST(sm_feature_transit_dispatch_state, type_checks_compile)
{
    SUCCEED();
}
