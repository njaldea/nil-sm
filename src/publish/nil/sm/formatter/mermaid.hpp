#pragma once

#include "detail.hpp"
#include "ir.hpp"

namespace nil::sm::formatter::mermaid
{
    inline std::string action_label(const ir::action_info& action)
    {
        return std::visit(
            [](const auto& info) -> std::string
            {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, ir::entry_action_info>)
                {
                    return std::string("on Enter / ") + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::exit_action_info>)
                {
                    return std::string("on Exit / ") + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::regions_finalized_action_info>)
                {
                    return std::string("on [**] / ") + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::capture_action_info>)
                {
                    return "on " + info.event_name + " [c] / "
                        + std::string(action_name(info.response));
                }
                else
                {
                    return "on " + info.event_name + " / "
                        + std::string(action_name(info.response));
                }
            },
            action
        );
    }

    inline std::string title_label(const ir::state_node& node)
    {
        if (node.actions.empty())
        {
            return node.display_name;
        }

        auto label = std::string(node.display_name);
        for (const auto& action : node.actions)
        {
            label += "<br/>";
            label += action_label(action);
        }
        return label;
    }

    inline void render_annotations(std::ostream& os, std::size_t depth, const ir::state_node& node)
    {
        for (const auto& transition : node.transitions)
        {
            indent(os, depth) << source_id(transition) << " --> " << target_id(transition) << " : "
                              << event_name(transition) << (is_capture(transition) ? " [c]" : "")
                              << "\n";
        }
    }

    inline void render_node(std::ostream& os, std::size_t depth, const ir::state_node& node);

    inline void render_region(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::state_node>& region,
        bool nested
    )
    {
        for (const auto& node : region)
        {
            render_node(os, depth, node);
        }

        if (!region.empty() && nested)
        {
            indent(os, depth) << "[*] --> " << region.front().id << "\n";
        }
    }

    inline void render_node(std::ostream& os, std::size_t depth, const ir::state_node& node)
    {
        if (!node.regions.empty())
        {
            // Composite states cannot carry separate `stateId : on ...` description lines,
            // so actions are embedded in the title label instead.
            indent(os, depth) << "state \"" << title_label(node) << "\" as " << node.id << " {\n";
            for (auto region_idx = std::size_t{0}; region_idx < node.regions.size(); ++region_idx)
            {
                render_region(os, depth + 1, node.regions[region_idx], true);
                if (region_idx + 1 < node.regions.size())
                {
                    indent(os, depth + 1) << "--\n";
                }
            }
            indent(os, depth) << "}\n";

            render_annotations(os, depth, node);
        }
        else
        {
            indent(os, depth) << "state \"" << title_label(node) << "\" as " << node.id << "\n";
            render_annotations(os, depth, node);
        }
    }

    inline std::ostream& render(std::ostream& os, const ir::model& model)
    {
        os << "stateDiagram-v2\n";
        render_region(os, 0, model.roots, false);
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct mermaid;

    template <template <typename...> typename API, typename... T>
    struct mermaid<SM<API, T...>>
    {
        friend std::ostream& operator<<(std::ostream& os, const mermaid<SM<API, T...>>& /* mmd */)
        {
            return formatter::mermaid::render(os, formatter::detail::build_ir<API, T...>());
        }
    };
}
