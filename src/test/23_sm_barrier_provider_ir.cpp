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

    template <typename T>
    using child_api = nil::sm::api::Default<>::type<T>;

    NIL_SM_BARRIER_DECLARE(shared_provider, child_api);
    NIL_SM_BARRIER_DEFINE(shared_provider, child_api, child_state);

    using shared_barrier = nil::sm::barrier::State<nil::sm::Terminate, shared_provider>;

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

    std::size_t count_provider_references(
        const std::vector<nil::sm::ir::Node>& nodes,
        const void* provider_id
    )
    {
        auto count = std::size_t{0};
        for (const auto& node : nodes)
        {
            count += node.provider_id == provider_id ? 1 : 0;
            for (const auto& region : node.regions)
            {
                count += count_provider_references(region, provider_id);
            }
        }
        return count;
    }
}

TEST(BarrierProviderIr, StoresRepeatedProviderOnceAndReferencesIt)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>::template type, root>();
    const void* const provider_id = nil::xalt::type_id<shared_provider>;

    ASSERT_EQ(model.providers.size(), 1U);
    EXPECT_EQ(model.providers.front().id, provider_id);
    EXPECT_EQ(model.providers.front().name, "shared_provider");
    EXPECT_EQ(count_provider_references(model.roots, provider_id), 2U);
    EXPECT_NE(nil::sm::ir::find_provider<shared_provider>(model), nullptr);
    ASSERT_EQ(model.providers.front().roots.size(), 1U);
    EXPECT_EQ(model.providers.front().roots.front().display_name, "child_state");
}

TEST(BarrierProviderIr, ProviderDefinitionIdsIgnoreHostMetadata)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>::template type, root>();
    const auto local_provider = shared_provider::ir(nullptr);

    ASSERT_EQ(model.providers.size(), 1U);
    ASSERT_EQ(model.providers.front().roots.size(), local_provider.roots.size());
    EXPECT_EQ(model.providers.front().roots.front().id, local_provider.roots.front().id);
}

TEST(BarrierProviderIr, RendersRootAndSelectedProviderSeparately)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>::template type, root>();

    std::ostringstream root_output;
    nil::sm::format::puml::render(root_output, model.roots);
    EXPECT_EQ(root_output.str().find("provider-ref"), std::string::npos);
    EXPECT_EQ(root_output.str().find("[/]"), std::string::npos);
    EXPECT_NE(root_output.str().find("shared_provider"), std::string::npos);
    EXPECT_NE(root_output.str().find("parent_a"), std::string::npos);

    nil::sm::puml<machine> diagram;
    std::ostringstream provider_output;
    for (const auto& provider : diagram.providers)
    {
        provider_output << provider;
    }
    EXPECT_NE(provider_output.str().find("child_state"), std::string::npos);
    EXPECT_EQ(provider_output.str().find("parent_a"), std::string::npos);
}

TEST(BarrierProviderIr, PumlDiagramSelectsDefinition)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream output;
    for (const auto& provider : diagram.providers)
    {
        output << provider;
    }

    EXPECT_NE(output.str().find("child_state"), std::string::npos);
    EXPECT_EQ(output.str().find("parent_a"), std::string::npos);
}

TEST(BarrierProviderIr, PumlObjectBuildsOnceAndExposesRootAndProviders)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream root_output;
    root_output << diagram.root;

    auto provider_count = std::size_t{0};
    std::ostringstream provider_output;
    for (const auto& provider : diagram.providers)
    {
        ++provider_count;
        provider_output << provider;
    }

    EXPECT_EQ(provider_count, 1U);
    EXPECT_NE(root_output.str().find("shared_provider"), std::string::npos);
    EXPECT_NE(provider_output.str().find("child_state"), std::string::npos);
}

TEST(BarrierProviderIr, DiagramRenderLeavesBarriersCollapsed)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream output;

    output << diagram.root;

    const auto text = output.str();
    EXPECT_NE(text.find("parent_a"), std::string::npos);
    EXPECT_EQ(text.find("child_state"), std::string::npos);
}

TEST(BarrierProviderIr, DiagramViewsSupportStreamInsertion)
{
    nil::sm::puml<machine> diagram;
    std::ostringstream output;

    output << diagram.root << '\n';
    for (const auto& provider : diagram.providers)
    {
        output << provider << '\n';
    }

    EXPECT_NE(output.str().find("parent_a"), std::string::npos);
    EXPECT_NE(output.str().find("child_state"), std::string::npos);
}

TEST(BarrierProviderIr, GenericDiagramSupportsEveryFormatter)
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

    for (const auto& provider : mermaid.providers)
    {
        std::ostringstream provider_output;
        provider_output << provider;
        EXPECT_NE(provider_output.str().find("child_state"), std::string::npos);
    }

    EXPECT_NE(mermaid_output.find("stateDiagram-v2"), std::string::npos);
    EXPECT_NE(dot_output.find("digraph sm"), std::string::npos);
    EXPECT_NE(scxml_output.find("<scxml"), std::string::npos);
    EXPECT_NE(xstate_output.find("\"states\""), std::string::npos);
    EXPECT_NE(puml_output.find("@startuml"), std::string::npos);
}

TEST(BarrierProviderIr, IteratesUnknownProvidersAndRendersEachDefinition)
{
    const auto model = nil::sm::ir::build<nil::sm::api::Default<>::template type, root>();
    auto provider_count = std::size_t{0};

    nil::sm::ir::for_each_provider(
        model,
        [&](const auto& provider)
        {
            ++provider_count;
            EXPECT_EQ(provider.id, nil::xalt::type_id<shared_provider>);
        }
    );

    EXPECT_EQ(provider_count, 1U);

    nil::sm::puml<machine> diagram;
    std::ostringstream output;
    for (const auto& provider : diagram.providers)
    {
        output << provider;
    }
    const auto text = output.str();
    EXPECT_NE(text.find("child_state"), std::string::npos);
    EXPECT_EQ(std::count(text.begin(), text.end(), '@'), 2);
}
