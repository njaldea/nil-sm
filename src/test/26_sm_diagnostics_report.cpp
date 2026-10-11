// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm/barrier.hpp>
#include <nil/sm/diagnostics.hpp>
#include <nil/sm/format/json.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    struct dependency
    {
    };

    struct parent_base
    {
    };

    struct needs_dependency
    {
        using args = nil::xalt::tlist<dependency>;

        explicit needs_dependency(dependency* /* value */)
        {
        }
    };

    struct provider_root
    {
        dependency value;
        using props = nil::xalt::tlist<nil::sm::prop<dependency, &provider_root::value>>;
        using regions = nil::xalt::tlist<needs_dependency>;
    };

    struct missing_root
    {
        using regions = nil::xalt::tlist<needs_dependency>;
    };

    struct wrapper
    {
        using regions = nil::xalt::tlist<needs_dependency>;
    };

    struct deep_root
    {
        using regions = nil::xalt::tlist<wrapper>;
    };

    struct also_needs_dependency
    {
        using args = nil::xalt::tlist<dependency>;

        explicit also_needs_dependency(dependency* /* value */)
        {
        }
    };

    struct two_consumers_root
    {
        using regions = nil::xalt::tlist<needs_dependency, also_needs_dependency>;
    };

    struct base_child
    {
        using args = nil::xalt::tlist<nil::sm::direct_parent<parent_base>>;
    };

    struct plain_parent: parent_base
    {
        using regions = nil::xalt::tlist<base_child>;
    };

    struct exposing_parent: parent_base
    {
        using props = nil::xalt::tlist<nil::sm::as_parent<parent_base>>;
        using regions = nil::xalt::tlist<base_child>;
    };

    using body_api = nil::sm::api::Default<>;

    NIL_SM_BARRIER_DECLARE(shared_barrier_state, body_api);
    NIL_SM_BARRIER_DEFINE(shared_barrier_state, needs_dependency);
    using shared_barrier = nil::sm::barrier::State<nil::sm::Terminate, shared_barrier_state>;

    struct supplying_parent
    {
        dependency value;
        using props = nil::xalt::tlist<nil::sm::prop<dependency, &supplying_parent::value>>;
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct plain_host_parent
    {
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct host_root
    {
        using regions = nil::xalt::tlist<supplying_parent, plain_host_parent>;
    };

    std::size_t count_of(const std::string& text, const std::string& part)
    {
        auto count = std::size_t{0};
        for (auto at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
        {
            ++count;
        }
        return count;
    }

    const nil::sm::ir::Node& first_child(const nil::sm::ir::Node& node)
    {
        return node.regions.front().front();
    }
}

TEST(sm_feature_diagnostics_report, shallow_level_omits_regions_and_counts_them)
{
    const auto model = nil::sm::ir::build<provider_root>();
    std::ostringstream output;

    nil::sm::format::json::render_level(
        output,
        std::span<const std::vector<nil::sm::ir::Node>>(&model.roots, 1)
    );

    const auto text = output.str();
    EXPECT_EQ(count_of(text, "\"regions\""), 1U);
    EXPECT_NE(text.find("\"region_sizes\":[1]"), std::string::npos);
    EXPECT_EQ(text.find("needs_dependency"), std::string::npos);
}

TEST(sm_feature_diagnostics_report, shallow_level_of_a_state_lists_its_regions)
{
    const auto model = nil::sm::ir::build<provider_root>();
    std::ostringstream output;

    nil::sm::format::json::render_level(output, model.roots.front().regions);

    const auto text = output.str();
    EXPECT_NE(text.find("needs_dependency"), std::string::npos);
    EXPECT_NE(text.find("\"region_sizes\":[]"), std::string::npos);
}

TEST(sm_feature_diagnostics_report, index_lists_barriers_and_root_props)
{
    const auto model = nil::sm::ir::build<host_root>();
    std::ostringstream output;

    nil::sm::format::json::write_index(output, model, nil::sm::ir::make_root_props<dependency>());

    const auto text = output.str();
    EXPECT_NE(text.find("\"barriers\":[{\"id\":\""), std::string::npos);
    EXPECT_NE(text.find("\"name\":\"shared_barrier_state\""), std::string::npos);
    EXPECT_NE(text.find("\"root_props\":[{\"type_id\":"), std::string::npos);
}

TEST(sm_feature_diagnostics_report, reports_a_missing_arg_on_the_state_that_needs_it)
{
    const auto model = nil::sm::ir::build<missing_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    const auto reports = diagnostics.children(model.roots.front().id);

    ASSERT_TRUE(reports.has_value());
    ASSERT_EQ(reports->size(), 1U);
    const auto& report = reports->front();
    EXPECT_EQ(report.id, first_child(model.roots.front()).id);
    ASSERT_EQ(report.issues.size(), 1U);
    EXPECT_EQ(report.issues.front().dependency.type_id, nil::xalt::type_id<dependency>);
    EXPECT_TRUE(report.issues.front().barrier_path.empty());
    EXPECT_FALSE(report.issues.front().as_parent);
}

TEST(sm_feature_diagnostics_report, reports_nothing_when_an_ancestor_provides_the_arg)
{
    const auto model = nil::sm::ir::build<provider_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    const auto reports = diagnostics.children(model.roots.front().id);

    ASSERT_TRUE(reports.has_value());
    ASSERT_EQ(reports->size(), 1U);
    EXPECT_TRUE(reports->front().issues.empty());
}

TEST(sm_feature_diagnostics_report, root_props_satisfy_top_level_requirements)
{
    const auto model = nil::sm::ir::build<missing_root>();
    const auto diagnostics
        = nil::sm::Diagnostics(model, nil::sm::ir::make_root_props<dependency>());

    const auto reports = diagnostics.children(model.roots.front().id);

    ASSERT_TRUE(reports.has_value());
    ASSERT_EQ(reports->size(), 1U);
    EXPECT_TRUE(reports->front().issues.empty());
}

TEST(sm_feature_diagnostics_report, counts_unmet_requirements_hidden_below_a_state)
{
    const auto model = nil::sm::ir::build<deep_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    {
        const auto reports = diagnostics.children("0");

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 1U);
        EXPECT_TRUE(reports->front().issues.empty());
        EXPECT_EQ(reports->front().subtree_unmet, 1U);
    }

    {
        const auto reports = diagnostics.children(model.roots.front().id);

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 1U);
        EXPECT_EQ(reports->front().subtree_unmet, 1U);
    }
}

TEST(sm_feature_diagnostics_report, merges_states_that_miss_the_same_thing)
{
    const auto model = nil::sm::ir::build<two_consumers_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    {
        const auto& issues = diagnostics.root_issues();

        ASSERT_EQ(issues.size(), 1U);
    }

    {
        const auto reports = diagnostics.children("0");

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 1U);
        EXPECT_EQ(reports->front().subtree_unmet, 1U);
    }

    {
        // each state still reports its own
        const auto reports = diagnostics.children(model.roots.front().id);

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 2U);
        ASSERT_EQ(reports->front().issues.size(), 1U);
        ASSERT_EQ(reports->back().issues.size(), 1U);
    }
}

TEST(sm_feature_diagnostics_report, judges_a_barrier_body_by_where_it_is_hosted)
{
    const auto model = nil::sm::ir::build<host_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});
    const auto& supplying = model.roots.front().regions.front().front();
    const auto& plain = model.roots.front().regions.back().front();
    const auto& supplied_host = first_child(supplying);
    const auto& plain_host = first_child(plain);

    {
        const auto reports = diagnostics.children(supplied_host.id);

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 1U);
        EXPECT_TRUE(reports->front().issues.empty());
    }

    {
        const auto reports = diagnostics.children(plain_host.id);

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 1U);
        ASSERT_EQ(reports->front().issues.size(), 1U);
        EXPECT_EQ(reports->front().issues.front().dependency.type_id, nil::xalt::type_id<dependency>);
    }
}

TEST(sm_feature_diagnostics_report, a_host_carries_what_its_barrier_body_lacks)
{
    const auto model = nil::sm::ir::build<host_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});
    const auto& supplying = model.roots.front().regions.front().front();
    const auto& plain = model.roots.front().regions.back().front();

    {
        const auto reports = diagnostics.children(supplying.id);

        ASSERT_TRUE(reports.has_value());
        // the host and the termination its barrier leads to
        ASSERT_EQ(reports->size(), 2U);
        EXPECT_TRUE(reports->front().issues.empty());
    }

    {
        const auto reports = diagnostics.children(plain.id);

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 2U);
        ASSERT_EQ(reports->front().issues.size(), 1U);
        const auto& issue = reports->front().issues.front();
        ASSERT_EQ(issue.barrier_path.size(), 1U);
        EXPECT_EQ(issue.barrier_path.front(), "shared_barrier_state");
    }
}

TEST(sm_feature_diagnostics_report, reports_both_sides_of_an_unmet_direct_parent)
{
    const auto model = nil::sm::ir::build<plain_parent>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    {
        const auto reports = diagnostics.children("0");

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 1U);
        ASSERT_EQ(reports->front().issues.size(), 1U);
        EXPECT_TRUE(reports->front().issues.front().as_parent);
        EXPECT_TRUE(reports->front().issues.front().dependency.is_direct_parent);
    }

    {
        const auto reports = diagnostics.children(model.roots.front().id);

        ASSERT_TRUE(reports.has_value());
        ASSERT_EQ(reports->size(), 1U);
        ASSERT_EQ(reports->front().issues.size(), 1U);
        EXPECT_FALSE(reports->front().issues.front().as_parent);
        EXPECT_TRUE(reports->front().issues.front().dependency.is_direct_parent);
    }
}

TEST(sm_feature_diagnostics_report, an_exposing_parent_leaves_direct_parent_satisfied)
{
    const auto model = nil::sm::ir::build<exposing_parent>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    const auto at_top = diagnostics.children("0");
    const auto below = diagnostics.children(model.roots.front().id);

    ASSERT_TRUE(at_top.has_value());
    ASSERT_TRUE(below.has_value());
    EXPECT_TRUE(at_top->front().issues.empty());
    EXPECT_TRUE(below->front().issues.empty());
}

TEST(sm_feature_diagnostics_report, rejects_occurrences_that_name_no_state)
{
    const auto model = nil::sm::ir::build<missing_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    EXPECT_FALSE(diagnostics.children("").has_value());
    EXPECT_FALSE(diagnostics.children("ST_0000000000000000").has_value());
    // only a barrier host can be followed by a state of its body
    EXPECT_FALSE(
        diagnostics.children(model.roots.front().id + "::" + model.roots.front().id).has_value()
    );
}

TEST(sm_feature_diagnostics_report, root_issues_agree_with_validate)
{
    const auto missing = nil::sm::ir::build<missing_root>();
    const auto provided = nil::sm::ir::build<provider_root>();
    const auto hosted = nil::sm::ir::build<host_root>();
    const auto direct_parent = nil::sm::ir::build<plain_parent>();

    EXPECT_FALSE(nil::sm::validate(missing));
    EXPECT_FALSE(nil::sm::Diagnostics(missing, {}).root_issues().empty());
    EXPECT_TRUE(nil::sm::validate(provided));
    EXPECT_TRUE(nil::sm::Diagnostics(provided, {}).root_issues().empty());
    EXPECT_FALSE(nil::sm::validate(hosted));
    EXPECT_FALSE(nil::sm::Diagnostics(hosted, {}).root_issues().empty());
    // an unmet direct_parent is reported by its states, not by the root
    EXPECT_FALSE(nil::sm::validate(direct_parent));
    EXPECT_TRUE(nil::sm::Diagnostics(direct_parent, {}).root_issues().empty());
}

TEST(sm_feature_diagnostics_report, barrier_requirements_list_what_the_body_leaves_to_its_host)
{
    const auto model = nil::sm::ir::build<host_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});

    const auto known = diagnostics.barrier_requirements(shared_barrier_state::id);
    const auto unknown = diagnostics.barrier_requirements(&model);

    ASSERT_TRUE(known.has_value());
    ASSERT_EQ(known->size(), 1U);
    EXPECT_EQ(known->front().dependency.type_id, nil::xalt::type_id<dependency>);
    EXPECT_FALSE(unknown.has_value());
    EXPECT_FALSE(diagnostics.barrier_requirements(nullptr).has_value());
}

TEST(sm_feature_diagnostics_report, reports_are_written_as_json)
{
    const auto model = nil::sm::ir::build<missing_root>();
    const auto diagnostics = nil::sm::Diagnostics(model, {});
    const auto reports = diagnostics.children(model.roots.front().id);
    ASSERT_TRUE(reports.has_value());
    std::ostringstream output;

    nil::sm::format::json::render_reports(output, *reports);

    const auto text = output.str();
    EXPECT_NE(
        text.find("\"unsatisfied_args\":[{\"dependency\":{\"type_id\":\""),
        std::string::npos
    );
    EXPECT_NE(text.find("\"as_parent\":false"), std::string::npos);
    EXPECT_NE(text.find("\"subtree_unmet\":0"), std::string::npos);
}
