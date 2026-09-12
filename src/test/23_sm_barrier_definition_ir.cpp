// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm/barrier.hpp>
#include <nil/sm/uml.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <sstream>
#include <string>

namespace
{
    struct child_state
    {
    };

    using child_api = nil::sm::api::Default<>;

    NIL_SM_BARRIER_DECLARE(shared_barrier_state, child_api);
    NIL_SM_BARRIER_DEFINE(shared_barrier_state, child_state);
    using shared_barrier = nil::sm::barrier::State<nil::sm::Terminate, shared_barrier_state>;

    struct parent_a
    {
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct parent_b
    {
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct root
    {
        using regions = nil::xalt::tlist<parent_a, parent_b>;
    };

    using machine = nil::sm::DefaultSM<root>;

    struct lifecycle_child
    {
    };

    NIL_SM_BARRIER_DECLARE(lifecycle_barrier_state, child_api);
    NIL_SM_BARRIER_DEFINE(lifecycle_barrier_state, lifecycle_child);
    using lifecycle_barrier = nil::sm::barrier::State<nil::sm::Terminate, lifecycle_barrier_state>;

    struct lifecycle_root
    {
        using regions = nil::xalt::tlist<lifecycle_barrier>;
    };

    std::size_t count_barrier_references(
        const std::vector<nil::sm::ir::Node>& nodes,
        const void* barrier_id
    )
    {
        auto count = std::size_t{0};
        for (const auto& node : nodes)
        {
            count += node.barrier_id == barrier_id ? 1 : 0;
            for (const auto& region : node.regions)
            {
                count += count_barrier_references(region, barrier_id);
            }
        }
        return count;
    }

}

TEST(BarrierDefinitionIr, BarrierUsesBarrierStateAsLeafRegion)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>, lifecycle_root>();

    ASSERT_EQ(model.roots.size(), 1U);
    ASSERT_EQ(model.roots.front().regions.size(), 1U);

    const auto& region = model.roots.front().regions.front();
    const auto barrier_it = std::find_if(
        region.begin(),
        region.end(),
        [](const auto& node) { return node.is_barrier; }
    );
    ASSERT_NE(barrier_it, region.end());

    const auto& barrier = *barrier_it;
    EXPECT_TRUE(barrier.is_barrier);
    EXPECT_TRUE(barrier.regions.empty());
    EXPECT_EQ(barrier.barrier_id, lifecycle_barrier_state::id);
}

TEST(BarrierDefinitionIr, StoresRepeatedBarrierStateOnceAndReferencesIt)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>, root>();
    const void* const barrier_id = shared_barrier_state::id;

    ASSERT_EQ(model.barriers.size(), 1U);
    EXPECT_EQ(model.barriers.front().id, barrier_id);
    EXPECT_EQ(model.barriers.front().name, "shared_barrier_state");
    EXPECT_EQ(count_barrier_references(model.roots, barrier_id), 2U);
    EXPECT_NE(nil::sm::ir::find_barrier<shared_barrier_state>(model), nullptr);
    ASSERT_EQ(model.barriers.front().roots.size(), 1U);
    EXPECT_EQ(model.barriers.front().roots.front().display_name, "child_state");
}

TEST(BarrierDefinitionIr, BarrierDefinitionIdsIgnoreHostMetadata)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>, root>();
    const auto local_barrier = shared_barrier_state::ir(nullptr);

    ASSERT_EQ(model.barriers.size(), 1U);
    ASSERT_EQ(model.barriers.front().roots.size(), local_barrier.roots.size());
    EXPECT_EQ(model.barriers.front().roots.front().id, local_barrier.roots.front().id);
}

TEST(BarrierDefinitionIr, RendersRootAndSelectedBarrierSeparately)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>, root>();

    std::ostringstream root_output;
    nil::sm::format::puml::render(root_output, model.roots);
    EXPECT_EQ(root_output.str().find("barrier-ref"), std::string::npos);
    EXPECT_EQ(root_output.str().find("[/]"), std::string::npos);
    EXPECT_NE(root_output.str().find("shared_barrier_state"), std::string::npos);
    EXPECT_NE(root_output.str().find("parent_a"), std::string::npos);

    nil::sm::puml<machine> diagram;
    std::ostringstream barrier_output;
    for (const auto& barrier : diagram.barriers)
    {
        barrier_output << barrier;
    }
    EXPECT_NE(barrier_output.str().find("child_state"), std::string::npos);
    EXPECT_EQ(barrier_output.str().find("parent_a"), std::string::npos);
}

TEST(BarrierDefinitionIr, PumlDiagramSelectsDefinition)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream output;
    for (const auto& barrier : diagram.barriers)
    {
        output << barrier;
    }

    EXPECT_NE(output.str().find("child_state"), std::string::npos);
    EXPECT_EQ(output.str().find("parent_a"), std::string::npos);
}

TEST(BarrierDefinitionIr, PumlObjectBuildsOnceAndExposesRootAndBarriers)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream root_output;
    root_output << diagram.root;

    auto barrier_count = std::size_t{0};
    std::ostringstream barrier_output;
    for (const auto& barrier : diagram.barriers)
    {
        ++barrier_count;
        barrier_output << barrier;
    }

    EXPECT_EQ(barrier_count, 1U);
    EXPECT_NE(root_output.str().find("shared_barrier_state"), std::string::npos);
    EXPECT_NE(barrier_output.str().find("child_state"), std::string::npos);
}

TEST(BarrierDefinitionIr, DiagramRenderLeavesBarriersCollapsed)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream output;

    output << diagram.root;

    const auto text = output.str();
    EXPECT_NE(text.find("parent_a"), std::string::npos);
    EXPECT_EQ(text.find("child_state"), std::string::npos);
}

TEST(BarrierDefinitionIr, DiagramViewsSupportStreamInsertion)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream output;

    output << diagram.root << '\n';
    for (const auto& barrier : diagram.barriers)
    {
        output << barrier << '\n';
    }

    EXPECT_NE(output.str().find("parent_a"), std::string::npos);
    EXPECT_NE(output.str().find("child_state"), std::string::npos);
}

TEST(BarrierDefinitionIr, GenericDiagramSupportsEveryFormatter)
{
    nil::sm::puml_diagram<machine> puml;
    nil::sm::mermaid_diagram<machine> mermaid;
    nil::sm::dot_diagram<machine> dot;
    nil::sm::scxml_diagram<machine> scxml;
    nil::sm::xstate_diagram<machine> xstate;

    const auto render = [](const auto& view)
    {
        std::ostringstream output;
        output << view;
        return output.str();
    };

    const auto puml_output = render(puml.root);
    const auto mermaid_output = render(mermaid.root);
    const auto dot_output = render(dot.root);
    const auto scxml_output = render(scxml.root);
    const auto xstate_output = render(xstate.root);

    for (const auto& barrier : mermaid.barriers)
    {
        std::ostringstream barrier_output;
        barrier_output << barrier;
        EXPECT_NE(barrier_output.str().find("child_state"), std::string::npos);
    }

    EXPECT_NE(mermaid_output.find("stateDiagram-v2"), std::string::npos);
    EXPECT_NE(dot_output.find("digraph sm"), std::string::npos);
    EXPECT_NE(scxml_output.find("<scxml"), std::string::npos);
    EXPECT_NE(xstate_output.find("\"states\""), std::string::npos);
    EXPECT_NE(puml_output.find("@startuml"), std::string::npos);
}

TEST(BarrierDefinitionIr, IteratesUnknownBarriersAndRendersEachDefinition)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>, root>();
    auto barrier_count = std::size_t{0};

    nil::sm::ir::for_each_barrier(
        model,
        [&](const auto& barrier)
        {
            ++barrier_count;
            EXPECT_EQ(barrier.id, shared_barrier_state::id);
        }
    );

    EXPECT_EQ(barrier_count, 1U);

    nil::sm::puml<machine> diagram;
    std::ostringstream output;
    for (const auto& barrier : diagram.barriers)
    {
        output << barrier;
    }
    const auto text = output.str();
    EXPECT_NE(text.find("child_state"), std::string::npos);
    EXPECT_EQ(std::count(text.begin(), text.end(), '@'), 2);
}
