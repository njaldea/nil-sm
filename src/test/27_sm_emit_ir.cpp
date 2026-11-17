// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm.hpp>
#include <nil/sm/format/json.hpp>
#include <nil/sm/format/puml.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

namespace
{
    using e_enter = e4;
    using e_exit = e3;
    using e_react = e2;
    using e_regions = ev<5>;
    using e_capture = ev<6>;

    struct emitting_leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_enter()
        {
            return Emit<e_enter>();
        }

        static auto on_exit()
        {
            return Emit<e_exit>();
        }

        static auto on_event(const e1& /* event */)
        {
            return Emit<e_react>();
        }
    };

    struct emitting_parent
    {
        using regions = nil::xalt::tlist<emitting_leaf>;
        using captures = nil::xalt::tlist<e1>;

        static auto on_capture(const e1& /* event */)
        {
            return Emit<e_capture>();
        }

        static auto on_regions_finalized()
        {
            return Emit<e_regions>();
        }
    };

    template <typename T>
    const T& find_action(const nil::sm::ir::Node& node)
    {
        for (const auto& action : node.actions)
        {
            if (const auto* found = std::get_if<T>(&action))
            {
                return *found;
            }
        }
        throw std::logic_error("action not found");
    }

    template <typename E>
    std::string name_of()
    {
        return std::string(nil::sm::detail::type_name<E>());
    }
}

TEST(sm_ir_emit, lifecycle_and_reaction_actions_carry_the_emitted_event_name)
{
    const auto model = nil::sm::ir::build<emitting_parent>();

    ASSERT_EQ(model.roots.size(), 1U);
    const auto& parent = model.roots.front();
    ASSERT_EQ(parent.regions.size(), 1U);
    ASSERT_EQ(parent.regions.front().size(), 1U);
    const auto& leaf = parent.regions.front().front();

    EXPECT_EQ(find_action<nil::sm::ir::action::Entry>(leaf).emitted_event_name, name_of<e_enter>());
    EXPECT_EQ(find_action<nil::sm::ir::action::Exit>(leaf).emitted_event_name, name_of<e_exit>());
    EXPECT_EQ(find_action<nil::sm::ir::action::Event>(leaf).emitted_event_name, name_of<e_react>());
    EXPECT_EQ(
        find_action<nil::sm::ir::action::RegionsFinalized>(parent).emitted_event_name,
        name_of<e_regions>()
    );
    EXPECT_EQ(
        find_action<nil::sm::ir::action::Capture>(parent).emitted_event_name,
        name_of<e_capture>()
    );
}

TEST(sm_ir_emit, non_emit_actions_leave_the_emitted_event_name_empty)
{
    struct noop_leaf
    {
        static auto on_enter()
        {
            return NOOP{};
        }
    };

    const auto model = nil::sm::ir::build<noop_leaf>();

    ASSERT_EQ(model.roots.size(), 1U);
    EXPECT_TRUE(
        find_action<nil::sm::ir::action::Entry>(model.roots.front()).emitted_event_name.empty()
    );
}

TEST(sm_ir_emit, puml_labels_name_the_emitted_event)
{
    const auto model = nil::sm::ir::build<emitting_parent>();
    std::ostringstream output;

    nil::sm::format::puml::render(output, model.roots);

    EXPECT_NE(output.str().find("on Enter / Emit(" + name_of<e_enter>() + ")"), std::string::npos);
    EXPECT_NE(output.str().find("on Exit / Emit(" + name_of<e_exit>() + ")"), std::string::npos);
    EXPECT_NE(output.str().find("/ Emit(" + name_of<e_react>() + ")"), std::string::npos);
}

TEST(sm_ir_emit, json_includes_the_emitted_event_field)
{
    const auto model = nil::sm::ir::build<emitting_parent>();
    std::ostringstream output;

    nil::sm::format::json::render(output, model.roots);

    EXPECT_NE(
        output.str().find("\"emitted_event\":\"" + name_of<e_enter>() + "\""),
        std::string::npos
    );
}
