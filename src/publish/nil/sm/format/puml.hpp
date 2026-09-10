// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../ir.hpp"
#include "diagram.hpp"
#include "utils.hpp"

namespace nil::sm::format::puml
{
    inline void render_action(
        std::ostream& os,
        std::size_t depth,
        std::string_view node_id,
        const ir::action::Info& action
    )
    {
        std::visit(
            [&](const auto& info)
            {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, ir::action::Entry>)
                {
                    indent(os, depth)
                        << node_id << " : on Enter / " << action_name(info.response) << "\n";
                }
                else if constexpr (std::is_same_v<T, ir::action::Exit>)
                {
                    indent(os, depth)
                        << node_id << " : on Exit / " << action_name(info.response) << "\n";
                }
                else if constexpr (std::is_same_v<T, ir::action::RegionsFinalized>)
                {
                    indent(os, depth) << node_id << " : on " << reserved::ev_regions_finalized
                                      << " / " << action_name(info.response) << "\n";
                }
                else if constexpr (std::is_same_v<T, ir::action::Capture>)
                {
                    indent(os, depth) << node_id << " : on " << info.event_name << " [c] / "
                                      << action_name(info.response) << "\n";
                }
                else
                {
                    indent(os, depth) << node_id << " : on " << info.event_name << " / "
                                      << action_name(info.response) << "\n";
                }
            },
            action
        );
    }

    inline void render_annotations(std::ostream& os, std::size_t depth, const ir::Node& node)
    {
        if (node.is_initial)
        {
            indent(os, depth) << "[*] --> " << node.id << "\n";
        }

        for (const auto& action : node.actions)
        {
            render_action(os, depth, node.id, action);
        }

        for (const auto& transition : node.transitions)
        {
            const auto target = ir::target_id(transition) == reserved::termination_node
                ? std::string_view{"[*]"}
                : std::string_view{ir::target_id(transition)};
            indent(os, depth) << node.id << " --> " << target;
            if (!ir::event_name(transition).empty())
            {
                os << " : " << ir::event_name(transition)
                   << (ir::is_capture(transition) ? " [c]" : "");
            }
            os << "\n";
        }
    }

    inline void render_node(std::ostream& os, std::size_t depth, const ir::Node& node);

    inline void render_region(std::ostream& os, std::size_t depth, std::span<const ir::Node> region)
    {
        for (const auto& node : region)
        {
            render_node(os, depth, node);
        }
    }

    inline void render_node(std::ostream& os, std::size_t depth, const ir::Node& node)
    {
        if (node.is_final)
        {
            return;
        }

        if (node.barrier_id != nullptr)
        {
            indent(os, depth) << "state " << node.id << " as \"" << node.display_name << "\" "
                              << (node.has_unsatisfied_args ? "<<invalid-args>>" : "<<barrier>>")
                              << "\n";
            render_annotations(os, depth, node);
            return;
        }

        if (!node.regions.empty())
        {
            indent(os, depth) << "state " << node.id << " as \"" << node.display_name << "\""
                              << (node.has_unsatisfied_args
                                      ? " <<invalid-args>>"
                                      : (node.is_barrier ? " <<barrier>>" : ""))
                              << " {\n";
            for (auto region_idx = std::size_t{0}; region_idx < node.regions.size(); ++region_idx)
            {
                render_region(os, depth + 1, node.regions[region_idx]);
                if (region_idx + 1 < node.regions.size())
                {
                    indent(os, depth + 1) << "||\n";
                }
            }
            indent(os, depth) << "}\n";
        }
        else
        {
            indent(os, depth) << "state " << node.id << " as \"" << node.display_name << "\""
                              << (node.has_unsatisfied_args ? " <<invalid-args>>" : "") << "\n";
        }

        render_annotations(os, depth, node);
    }

    inline void render(std::ostream& os, std::span<const ir::Node> roots)
    {
        os << "@startuml\n"
              "skin rose\n"
              "skinparam linetype ortho\n"
              "skinparam state {\n"
              "    BackgroundColor<<barrier>> #EEE8FF\n"
              "    BorderColor<<barrier>> #7F5FBF\n"
              "    BorderStyle<<barrier>> dashed\n"
              "    BackgroundColor<<invalid-args>> #FFE2E2\n"
              "    BorderColor<<invalid-args>> #B22222\n"
              "}\n";

        render_region(os, 0, roots);

        os << "@enduml\n";
    }
}

namespace nil::sm
{
    template <typename SM>
    using puml = format::diagram<SM, &format::puml::render>;
}
