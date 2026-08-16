#pragma once

#include "detail.hpp"
#include "ir.hpp"

namespace nil::sm::formatter::puml
{
    inline void render_action(
        std::ostream& os,
        std::size_t depth,
        std::string_view node_id,
        const ir::action_info& action
    )
    {
        std::visit(
            [&](const auto& info)
            {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, ir::entry_action_info>)
                {
                    indent(os, depth)
                        << node_id << " : on Enter / " << action_name(info.response) << "\n";
                }
                else if constexpr (std::is_same_v<T, ir::exit_action_info>)
                {
                    indent(os, depth)
                        << node_id << " : on Exit / " << action_name(info.response) << "\n";
                }
                else if constexpr (std::is_same_v<T, ir::regions_finalized_action_info>)
                {
                    indent(os, depth)
                        << node_id << " : on [**] / " << action_name(info.response) << "\n";
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

    inline void render_annotations(std::ostream& os, std::size_t depth, const ir::state_node& node)
    {
        for (const auto& action : node.actions)
        {
            render_action(os, depth, node.id, action);
        }
        for (const auto& transition : node.transitions)
        {
            indent(os, depth) << transition.source_id << " --> " << transition.target_id << " : "
                              << transition.event_name << "\n";
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
            indent(os, depth) << "state " << node.id << " as \"" << node.display_name << "\" {\n";
            for (auto region_idx = std::size_t{0}; region_idx < node.regions.size(); ++region_idx)
            {
                render_region(os, depth + 1, node.regions[region_idx], true);
                if (region_idx + 1 < node.regions.size())
                {
                    indent(os, depth + 1) << "||\n";
                }
            }
            indent(os, depth) << "}\n";
        }
        else
        {
            indent(os, depth) << "state " << node.id << " as \"" << node.display_name << "\"\n";
        }

        render_annotations(os, depth, node);
    }

    inline std::ostream& render(std::ostream& os, const ir::model& model)
    {
        os << "@startuml\n"
              "skin rose\n"
              "skinparam linetype ortho\n";

        render_region(os, 0, model.roots, false);

        os << "@enduml\n";
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct puml;

    template <template <typename...> typename API, typename... T>
    struct puml<SM<API, T...>>
    {
        friend std::ostream& operator<<(std::ostream& os, const puml<SM<API, T...>>& /* uml */)
        {
            return formatter::puml::render(os, formatter::detail::build_ir<API, T...>());
        }
    };
}
