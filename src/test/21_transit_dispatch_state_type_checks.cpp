#include <nil/sm.hpp>

#include <gtest/gtest.h>

#include <type_traits>

namespace
{
    template <typename Initial>
    using dispatch_for = nil::sm::detail::region_state_factory<
        nil::sm::api::Default<Initial>,
        typename nil::sm::detail::region_reachability_graph<nil::sm::api::Default, Initial>::
            states>;

    template <typename Dispatch, typename Target>
    consteval bool dispatch_contains_target_id()
    {
        return Dispatch::targets::template any_of<std::is_same, Target>;
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

    static_assert(std::is_same_v<dispatch_only_a::targets, nil::xalt::tlist<only_a>>);
    static_assert(dispatch_only_a::targets::size == 1);
    static_assert(dispatch_contains_target_id<dispatch_only_a, only_a>());

    // Case 2: A -> B
    struct ab_b;

    struct ab_a
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::Transit<ab_b>{};
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

    static_assert(std::is_same_v<dispatch_ab_a::targets, nil::xalt::tlist<ab_a, ab_b>>);
    static_assert(dispatch_ab_a::targets::size == 2);
    static_assert(dispatch_contains_target_id<dispatch_ab_a, ab_a>());
    static_assert(dispatch_contains_target_id<dispatch_ab_a, ab_b>());
    static_assert(std::is_same_v<dispatch_ab_b::targets, nil::xalt::tlist<ab_b>>);
    static_assert(dispatch_contains_target_id<dispatch_ab_b, ab_b>());

    // Case 3: A -> B -> C -> A
    struct cycle_b;
    struct cycle_c;

    struct cycle_a
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::Transit<cycle_b>{};
        }
    };

    struct cycle_b
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::Transit<cycle_c>{};
        }
    };

    struct cycle_c
    {
        using events = nil::xalt::tlist<event>;

        static auto on_event(const event& /* event */)
        {
            return nil::sm::Transit<cycle_a>{};
        }
    };

    using dispatch_cycle_a = dispatch_for<cycle_a>;
    using dispatch_cycle_b = dispatch_for<cycle_b>;
    using dispatch_cycle_c = dispatch_for<cycle_c>;

    static_assert(std::is_same_v<
                  dispatch_cycle_a::targets,
                  nil::xalt::tlist<cycle_a, cycle_b, cycle_c>>);
    static_assert(std::is_same_v<
                  dispatch_cycle_b::targets,
                  nil::xalt::tlist<cycle_b, cycle_c, cycle_a>>);
    static_assert(std::is_same_v<
                  dispatch_cycle_c::targets,
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
}

TEST(sm_feature_transit_dispatch_state, type_checks_compile)
{
    SUCCEED();
}
