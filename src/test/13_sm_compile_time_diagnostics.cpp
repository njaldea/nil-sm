// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm.hpp>
#include <nil/sm/diagnostics.hpp>
#include <nil/sm/format/puml.hpp>

#include <gtest/gtest.h>

#include <sstream>
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

        static auto on_event(const e1& /* event */) -> std::variant<nil::sm::Forward, int>
        {
            return nil::sm::Forward{};
        }
    };

    struct returns_unhandled_directly
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sm::Unhandled{};
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
            return nil::sm::Discard{};
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
            return nil::sm::TransitTo<transit_target_no_default_ctor>();
        }
    };

    struct overloads_are_legal
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sm::Discard{};
        }

        static auto on_event(e1&& /* event */)
        {
            return nil::sm::Forward{};
        }
    };

    struct dependency
    {
    };

    struct dependency_consumer
    {
        using args = nil::xalt::tlist<dependency>;
    };

    struct dependency_provider
    {
        dependency value;
        using props = nil::xalt::tlist<nil::sm::prop<dependency, &dependency_provider::value>>;
        using regions = nil::xalt::tlist<dependency_consumer>;
    };

    struct missing_dependency_provider
    {
        using regions = nil::xalt::tlist<dependency_consumer>;
    };

    struct direct_parent_consumer
    {
        using args = nil::xalt::tlist<nil::sm::direct_parent<dependency_provider>>;
    };

    struct direct_parent_provider
    {
        using regions = nil::xalt::tlist<direct_parent_consumer>;
    };

    struct target_dependency_consumer;

    struct transition_to_dependency_consumer
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sm::TransitTo<target_dependency_consumer>{};
        }
    };

    struct target_dependency_consumer
    {
        using args = nil::xalt::tlist<dependency>;
    };

    struct transition_dependency_provider
    {
        dependency value;
        using props
            = nil::xalt::tlist<nil::sm::prop<dependency, &transition_dependency_provider::value>>;
        using regions = nil::xalt::tlist<transition_to_dependency_consumer>;
    };

    static_assert(!nil::sm::concepts::has_valid_on_event<missing_react_for_declared_event, e1>);
    static_assert(!nil::sm::concepts::has_valid_on_event<returns_unsupported_type, e1>);
    static_assert(!nil::sm::concepts::
                      has_valid_on_event<returns_variant_with_unsupported_type, e1>);
    static_assert(!nil::sm::concepts::has_valid_on_event<returns_unhandled_directly, e1>);

    static_assert(!std::is_default_constructible_v<no_default_ctor>);

    // TransitTo target validity is not diagnosed by has_on_event; it is validated later in template
    // instantiation paths.
    static_assert(nil::sm::concepts::has_valid_on_event<transit_to_invalid_target, e1>);

    // Overloads are currently legal in this runtime.
    static_assert(nil::sm::concepts::has_valid_on_event<overloads_are_legal, e1>);

    static_assert(nil::sm::validate<nil::sm::api::Default<>::type, dependency_provider>());
    static_assert(!nil::sm::validate<nil::sm::api::Default<>::type, missing_dependency_provider>());
}

TEST(sm_feature_compile_time_diagnostics, static_checks_compile)
{
    SUCCEED();
}

TEST(sm_feature_compile_time_diagnostics, ir_build_validates_ancestor_properties)
{
    auto valid_model = nil::sm::ir::build<nil::sm::api::Default<>::type, dependency_provider>();
    EXPECT_TRUE(nil::sm::validate(valid_model));

    auto missing_model
        = nil::sm::ir::build<nil::sm::api::Default<>::type, missing_dependency_provider>();
    EXPECT_FALSE(nil::sm::validate(missing_model));
    EXPECT_TRUE(missing_model.has_unsatisfied_args);
    EXPECT_TRUE(missing_model.roots.front().regions.front().front().has_unsatisfied_args);

    std::ostringstream puml;
    nil::sm::format::puml::render(puml, missing_model.roots);
    EXPECT_NE(puml.str().find("<<invalid-args>>"), std::string::npos);
    EXPECT_EQ(puml.str().find("ERROR:"), std::string::npos);
    EXPECT_NO_THROW((
        [] { (void)nil::sm::ir::build<nil::sm::api::Default<>::type, direct_parent_provider>(); }()
    ));
    EXPECT_NO_THROW((
        [] {
            (void
            )nil::sm::ir::build<nil::sm::api::Default<>::type, transition_dependency_provider>();
        }()
    ));
}
