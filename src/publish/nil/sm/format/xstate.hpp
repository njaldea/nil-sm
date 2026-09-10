// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../ir.hpp"
#include "diagram.hpp"
#include "utils.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <variant>

namespace nil::sm::format::xstate
{
    struct RegionContext
    {
        std::string initial_key;
        std::string final_id;
    };

    inline void render_node(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context
    );

    // A [**] pseudostate node marks where [*] transitions in its region terminate into
    inline RegionContext make_region_context(std::span<const ir::Node> region)
    {
        auto context = RegionContext{};

        for (const auto& node : region)
        {
            if (node.is_final)
            {
                context.final_id = node.id;
                break;
            }
        }

        for (const auto& node : region)
        {
            if (node.is_initial)
            {
                context.initial_key = node.is_final ? "done" : node.display_name;
                return context;
            }
        }

        return context;
    }

    inline std::string transition_target_key(
        std::string_view raw_target,
        const RegionContext& context
    )
    {
        if (raw_target == reserved::termination_node
            || (!context.final_id.empty() && raw_target == context.final_id))
        {
            return "done";
        }
        return "#" + std::string(raw_target);
    }

    inline void render_region_states(
        std::ostream& os,
        std::size_t depth,
        std::span<const ir::Node> region,
        const RegionContext& context
    )
    {
        auto first = true;
        auto rendered_final = false;
        for (const auto& node : region)
        {
            if (node.is_final && rendered_final)
            {
                continue;
            }

            if (!first)
            {
                os << ",\n";
            }
            render_node(os, depth, node, context);
            rendered_final = rendered_final || node.is_final;
            first = false;
        }
        os << "\n";
    }

    // NOLINTNEXTLINE
    inline void render_node(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context
    )
    {
        if (node.is_final)
        {
            // The [**] pseudostate renders as XState's own final-state marker
            indent(os, depth) << "\"done\": {\n";
            indent(os, depth + 1) << "\"type\": \"final\"\n";
            indent(os, depth) << "}";
            return;
        }

        indent(os, depth) << "\"" << node.display_name << "\": {\n";

        // Collect entry and exit actions
        bool has_entry = false;
        bool has_exit = false;
        for (const auto& action : node.actions)
        {
            std::visit(
                [&](const auto& info)
                {
                    using T = std::decay_t<decltype(info)>;
                    if constexpr (std::is_same_v<T, ir::action::Entry>)
                    {
                        has_entry = true;
                    }
                    else if constexpr (std::is_same_v<T, ir::action::Exit>)
                    {
                        has_exit = true;
                    }
                },
                action
            );
        }

        const bool has_more
            = has_entry || has_exit || !node.transitions.empty() || !node.regions.empty();
        indent(os, depth + 1) << R"("id": ")" << node.id << "\"" << (has_more ? ",\n" : "\n");

        if (has_entry)
        {
            indent(os, depth + 1) << "\"entry\": [\n";
            bool first_entry = true;
            for (const auto& action : node.actions)
            {
                std::visit(
                    [&](const auto& info)
                    {
                        using T = std::decay_t<decltype(info)>;
                        if constexpr (std::is_same_v<T, ir::action::Entry>)
                        {
                            if (!first_entry)
                            {
                                os << ",\n";
                            }
                            indent(os, depth + 2)
                                << "\""
                                << (info.response == ir::response::EEntry::emit ? "Emit" : "NOOP")
                                << "\"";
                            first_entry = false;
                        }
                    },
                    action
                );
            }
            os << "\n";
            indent(os, depth + 1) << "]"
                                  << (has_exit || !node.transitions.empty() || !node.regions.empty()
                                          ? ",\n"
                                          : "\n");
        }

        if (has_exit)
        {
            indent(os, depth + 1) << "\"exit\": [\n";
            bool first_exit = true;
            for (const auto& action : node.actions)
            {
                std::visit(
                    [&](const auto& info)
                    {
                        using T = std::decay_t<decltype(info)>;
                        if constexpr (std::is_same_v<T, ir::action::Exit>)
                        {
                            if (!first_exit)
                            {
                                os << ",\n";
                            }
                            indent(os, depth + 2)
                                << "\""
                                << (info.response == ir::response::EExit::emit ? "Emit" : "NOOP")
                                << "\"";
                            first_exit = false;
                        }
                    },
                    action
                );
            }
            os << "\n";
            indent(os, depth + 1) << "]"
                                  << (!node.transitions.empty() || !node.regions.empty() ? ",\n"
                                                                                         : "\n");
        }

        // Map event transitions ("on" block)
        if (!node.transitions.empty())
        {
            indent(os, depth + 1) << "\"on\": {\n";
            auto rendered = std::size_t{0};
            for (const auto& tx : node.transitions)
            {
                if (ir::event_name(tx).empty())
                {
                    continue;
                }

                if (rendered > 0)
                {
                    os << ",\n";
                }
                indent(os, depth + 2) << "\"" << ir::event_name(tx) << "\": \""
                                      << transition_target_key(ir::target_id(tx), context) << "\"";
                ++rendered;
            }
            os << "\n";
            indent(os, depth + 1) << "}";
            if (!node.regions.empty())
            {
                os << ",";
            }
            os << "\n";
        }

        // Handle nested or parallel regions
        if (!node.regions.empty())
        {
            if (node.regions.size() == 1)
            {
                const auto child_context = make_region_context(node.regions[0]);
                if (!child_context.initial_key.empty())
                {
                    indent(os, depth + 1)
                        << R"("initial": ")" << child_context.initial_key << "\",\n";
                }
                indent(os, depth + 1) << "\"states\": {\n";
                render_region_states(os, depth + 2, node.regions[0], child_context);
                indent(os, depth + 1) << "}\n";
            }
            else
            {
                indent(os, depth + 1) << "\"type\": \"parallel\",\n";
                indent(os, depth + 1) << "\"states\": {\n";
                for (std::size_t r = 0; r < node.regions.size(); ++r)
                {
                    indent(os, depth + 2) << "\"region_" << r << "\": {\n";
                    if (!node.regions[r].empty())
                    {
                        const auto reg_context = make_region_context(node.regions[r]);
                        if (!reg_context.initial_key.empty())
                        {
                            indent(os, depth + 3)
                                << R"("initial": ")" << reg_context.initial_key << "\",\n";
                        }
                        indent(os, depth + 3) << "\"states\": {\n";
                        render_region_states(os, depth + 4, node.regions[r], reg_context);
                        indent(os, depth + 3) << "}\n";
                    }
                    indent(os, depth + 2) << "}";
                    if (r + 1 < node.regions.size())
                    {
                        os << ",";
                    }
                    os << "\n";
                }
                indent(os, depth + 1) << "}\n";
            }
        }

        indent(os, depth) << "}";
    }

    inline void render(std::ostream& os, std::span<const ir::Node> roots)
    {
        os << "{\n";
        indent(os, 1) << "\"id\": \"SM\",\n";
        if (!roots.empty())
        {
            const auto root_context = make_region_context(roots);
            if (!root_context.initial_key.empty())
            {
                indent(os, 1) << R"("initial": ")" << root_context.initial_key << "\",\n";
            }

            indent(os, 1) << "\"states\": {\n";
            render_region_states(os, 2, roots, root_context);
            indent(os, 1) << "}\n";
        }
        os << "}\n";
    }
}

namespace nil::sm
{
    template <typename SM>
    using xstate = format::diagram<SM, &format::xstate::render>;
}
