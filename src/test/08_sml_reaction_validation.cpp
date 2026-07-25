#include <nil/sml.hpp>

#include <gtest/gtest.h>

#include <variant>

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
            return nil::sml::Forward{};
        }
    };

    struct returns_discard
    {
        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Discard{};
        }
    };

    struct returns_transit
    {
        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Transit<target>();
        }
    };

    struct returns_variant_fd
    {
        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Forward, nil::sml::Discard>
        {
            return nil::sml::Discard{};
        }
    };

    struct returns_variant_dt
    {
        static auto on_event(const e1& /* event */)
            -> std::variant<nil::sml::Discard, nil::sml::Transit<target>>
        {
            return nil::sml::Discard{};
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
            return nil::sml::detail::Unhandled{};
        }
    };

    struct returns_variant_invalid
    {
        static auto on_event(const e1& /* event */) -> std::variant<nil::sml::Forward, int>
        {
            return nil::sml::Forward{};
        }
    };

    static_assert(nil::sml::concepts::has_on_event<returns_forward, e1>);
    static_assert(nil::sml::concepts::has_on_event<returns_discard, e1>);
    static_assert(nil::sml::concepts::has_on_event<returns_transit, e1>);
    static_assert(nil::sml::concepts::has_on_event<returns_variant_fd, e1>);
    static_assert(nil::sml::concepts::has_on_event<returns_variant_dt, e1>);

    static_assert(!nil::sml::concepts::has_on_event<returns_invalid_int, e1>);
    static_assert(!nil::sml::concepts::has_on_event<returns_unhandled, e1>);
    static_assert(!nil::sml::concepts::has_on_event<returns_variant_invalid, e1>);
}

TEST(sml_feature_reaction_validation, static_concept_checks_compile)
{
    SUCCEED();
}
