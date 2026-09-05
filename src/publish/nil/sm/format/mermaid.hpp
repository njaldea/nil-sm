#pragma once

#include "../ir.hpp"
#include "diagram.hpp"
#include "utils.hpp"

#include <format>

namespace nil::sm::format::mermaid
{
    inline std::string action_label(const ir::action::Info& action)
    {
        return std::visit(
            [](const auto& info) -> std::string
            {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, ir::action::Entry>)
                {
                    return std::format("on Enter / {}", action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::action::Exit>)
                {
                    return std::format("on Exit / {}", action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::action::RegionsFinalized>)
                {
                    return std::format(
                        "on {} / {}",
                        reserved::ev_regions_finalized,
                        action_name(info.response)
                    );
                }
                else if constexpr (std::is_same_v<T, ir::action::Capture>)
                {
                    return std::format(
                        "on {} [c] / {}",
                        info.event_name,
                        action_name(info.response)
                    );
                }
                else
                {
                    return std::format("on {} / {}", info.event_name, action_name(info.response));
                }
            },
            action
        );
    }

    inline std::string title_label(const ir::Node& node)
    {
        auto label = std::string(node.display_name);
        if (node.actions.empty())
        {
            return label;
        }

        for (const auto& action : node.actions)
        {
            label += "<br/>";
            label += action_label(action);
        }
        return label;
    }

    inline void render_annotations(std::ostream& os, std::size_t depth, const ir::Node& node)
    {
        if (node.is_initial)
        {
            indent(os, depth) << "[*] --> " << node.id << "\n";
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

        if (!node.regions.empty())
        {
            // Composite states cannot carry separate `stateId : on ...` description lines,
            // so actions are embedded in the title label instead.
            indent(os, depth) << "state \"" << title_label(node) << "\" as " << node.id << " {\n";
            for (auto region_idx = std::size_t{0}; region_idx < node.regions.size(); ++region_idx)
            {
                render_region(os, depth + 1, node.regions[region_idx]);
                if (region_idx + 1 < node.regions.size())
                {
                    indent(os, depth + 1) << "--\n";
                }
            }
            indent(os, depth) << "}\n";
        }
        else
        {
            indent(os, depth) << "state \"" << title_label(node) << "\" as " << node.id << "\n";
        }

        render_annotations(os, depth, node);
    }

    inline void render(std::ostream& os, std::span<const ir::Node> roots)
    {
        os << "stateDiagram-v2\n";
        render_region(os, 0, roots);
    }
}

namespace nil::sm
{
    template <typename SM>
    using mermaid = format::diagram<SM, &format::mermaid::render>;
}
