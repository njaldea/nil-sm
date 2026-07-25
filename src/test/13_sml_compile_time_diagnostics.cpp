#include <nil/sml.hpp>

#include <gtest/gtest.h>

#include <type_traits>
#include <variant>

namespace
{
    struct e1
    {
    };

    struct missing_react_for_declared_event
    {
        using events = nil::xalt::tlist<e1>;
    };

    struct returns_unsupported_type
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return 123;
        }
    };

    struct returns_variant_with_unsupported_type
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */) -> std::variant<nil::sml::Forward, int>
        {
            return nil::sml::Forward{};
        }
    };

    struct returns_unhandled_directly
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::detail::Unhandled{};
        }
    };

    struct no_default_ctor
    {
        using events = nil::xalt::tlist<e1>;

        explicit no_default_ctor(int /*x*/)
        {
        }

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Discard{};
        }
    };

    struct transit_target_no_default_ctor
    {
        transit_target_no_default_ctor() = delete;

        explicit transit_target_no_default_ctor(int /*x*/)
        {
        }
    };

    struct transit_to_invalid_target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Transit<transit_target_no_default_ctor>();
        }
    };

    struct overloads_are_legal
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Discard{};
        }

        static auto on_event(e1&& /* event */)
        {
            return nil::sml::Forward{};
        }
    };

    static_assert(!nil::sml::concepts::has_on_event<missing_react_for_declared_event, e1>);
    static_assert(!nil::sml::concepts::has_on_event<returns_unsupported_type, e1>);
    static_assert(!nil::sml::concepts::has_on_event<returns_variant_with_unsupported_type, e1>);
    static_assert(!nil::sml::concepts::has_on_event<returns_unhandled_directly, e1>);

    static_assert(!std::is_default_constructible_v<no_default_ctor>);

    // Transit target validity is not diagnosed by has_on_event; it is validated later in template
    // instantiation paths.
    static_assert(nil::sml::concepts::has_on_event<transit_to_invalid_target, e1>);

    // Overloads are currently legal in this runtime.
    static_assert(nil::sml::concepts::has_on_event<overloads_are_legal, e1>);
}

TEST(sml_feature_compile_time_diagnostics, static_checks_compile)
{
    SUCCEED();
}
