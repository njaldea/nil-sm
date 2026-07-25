#include <nil/sm.hpp>

#include <gtest/gtest.h>

#include <variant>

// Test 08: reaction validation - static concept checks
// These are compile-time assertions; the runtime test just confirms compilation succeeds.

namespace
{
    struct e1
    {
    };

    struct target
    {
    };

    struct returns_forward
    {
        static auto on_event(const e1& /* event */)
        {
            return nil::sm::Forward{};
        }
    };

    struct returns_discard
    {
        static auto on_event(const e1& /* event */)
        {
            return nil::sm::Discard{};
        }
    };

    struct returns_transit
    {
        static auto on_event(const e1& /* event */)
        {
            return nil::sm::Transit<target>();
        }
    };

    struct returns_variant_fd
    {
        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sm::Forward, nil::sm::Discard>
        {
            return nil::sm::Discard{};
        }
    };

    struct returns_variant_dt
    {
        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sm::Discard, nil::sm::Transit<target>>
        {
            return nil::sm::Discard{};
        }
    };

    struct returns_invalid_int
    {
        static auto on_event(const e1& /* event */)
        {
            return 42;
        }
    };

    struct returns_unhandled
    {
        static auto on_event(const e1& /* event */)
        {
            return nil::sm::Unhandled{};
        }
    };

    struct returns_variant_invalid
    {
        static auto on_event(const e1& /* event */) -> std::variant<nil::sm::Forward, int>
        {
            return nil::sm::Forward{};
        }
    };

    static_assert(nil::sm::concepts::has_on_event<returns_forward, e1>);
    static_assert(nil::sm::concepts::has_on_event<returns_discard, e1>);
    static_assert(nil::sm::concepts::has_on_event<returns_transit, e1>);
    static_assert(nil::sm::concepts::has_on_event<returns_variant_fd, e1>);
    static_assert(nil::sm::concepts::has_on_event<returns_variant_dt, e1>);

    static_assert(!nil::sm::concepts::has_on_event<returns_invalid_int, e1>);
    static_assert(!nil::sm::concepts::has_on_event<returns_unhandled, e1>);
    static_assert(!nil::sm::concepts::has_on_event<returns_variant_invalid, e1>);
}

TEST(sm_feature_reaction_validation, static_concept_checks_compile)
{
    SUCCEED();
}
