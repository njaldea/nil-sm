#pragma once

#include "detail.hpp"
#include "ir.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nil::sm::formatter::dot
{
    inline std::ostream& indent(std::ostream& os, std::size_t depth)
    {
        for (std::size_t i = 0; i < depth; ++i)
        {
            os << "    ";
        }

        return os;
    }

    inline std::string_view action_name(ir::entry_response response)
    {
        switch (response)
        {
            case ir::entry_response::noop:
                return "NOOP";
            case ir::entry_response::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::exit_response response)
    {
        switch (response)
        {
            case ir::exit_response::noop:
                return "NOOP";
            case ir::exit_response::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::regions_finalized_response response)
    {
        switch (response)
        {
            case ir::regions_finalized_response::noop:
                return "NOOP";
            case ir::regions_finalized_response::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::event_response response)
    {
        switch (response)
        {
            case ir::event_response::discard:
                return "Discard";
            case ir::event_response::forward:
                return "Forward";
            case ir::event_response::defer:
                return "Defer";
            case ir::event_response::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string format_action(const ir::action_info& action)
    {
        std::string result;
        std::visit(
            [&](const auto& info)
            {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, ir::entry_action_info>)
                {
                    result = "on Enter / " + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::exit_action_info>)
                {
                    result = "on Exit / " + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::regions_finalized_action_info>)
                {
                    result = "on [**] / " + std::string(action_name(info.response));
                }
                else
                {
                    result
                        = "on " + info.event_name + " / " + std::string(action_name(info.response));
                }
            },
            action
        );
        return result;
    }

    inline void render_node(std::ostream& os, std::size_t depth, const ir::state_node& node)
    {
        if (!node.regions.empty())
        {
            // Composite state represented as a Graphviz cluster subgraph
            indent(os, depth) << "subgraph cluster_" << node.id << " {\n";
            indent(os, depth + 1) << "label=\"" << node.display_name << "\";\n";
            indent(os, depth + 1) << "compound=true;\n";
            indent(os, depth + 1) << "style=\"rounded\";\n";
            indent(os, depth + 1) << "color=black;\n";

            // State header/action record node inside the cluster container
            indent(os, depth + 1) << node.id << " [shape=record";
            if (node.actions.empty())
            {
                indent(os, depth + 2) << ", style=\"invis\"];\n";
            }
            else
            {
                os << ", label=\"{ actions ";
                for (const auto& action : node.actions)
                {
                    os << "|" << format_action(action);
                }
                os << "}\"];\n";
            }

            // Render regions as internal clusters
            for (std::size_t r_idx = 0; r_idx < node.regions.size(); ++r_idx)
            {
                std::string reg_cluster_id = "cluster_" + node.id + "_reg_" + std::to_string(r_idx);
                indent(os, depth + 1) << "subgraph " << reg_cluster_id << " {\n";
                indent(os, depth + 2) << "label=\"\";\n";
                indent(os, depth + 2) << "style=\"invis\";\n";

                const auto& region = node.regions[r_idx];
                if (!region.empty())
                {
                    std::string init_id = node.id + "_reg_" + std::to_string(r_idx) + "_init";
                    std::string term_id = node.id + "_reg_" + std::to_string(r_idx) + "_term";

                    // Region initial pseudo-state
                    indent(os, depth + 2) << init_id << " [shape=point, label=\"\"];\n";
                    // Local region termination node
                    indent(os, depth + 2)
                        << term_id << " [shape=doublecircle, label=\"\", width=0.2, height=0.2];\n";

                    for (const auto& sub_node : region)
                    {
                        render_node(os, depth + 2, sub_node);
                    }

                    indent(os, depth + 2) << init_id << " -> " << region.front().id
                                          << "[lhead=\"cluster_" << region.front().id << "\"];\n";
                }
                indent(os, depth + 1) << "}\n";
            }

            indent(os, depth) << "}\n";
        }
        else
        {
            // Leaf state node with record shape for actions
            indent(os, depth) << node.id << " [shape=record, label=\"{" << node.display_name;
            for (const auto& action : node.actions)
            {
                os << "|" << format_action(action);
            }
            os << "}\"];\n";
        }
    }

    inline void render_transitions(
        std::ostream& os,
        std::size_t depth,
        const ir::state_node& node,
        std::string_view current_term_node
    )
    {
        for (const auto& transition : node.transitions)
        {
            indent(os, depth) << transition.source_id << " -> ";

            if (transition.target_id == "[*]")
            {
                os << current_term_node;
            }
            else
            {
                os << transition.target_id;
            }

            os << " [label=\"" << transition.event_name << "\"";

            // Use ltail with compound graphs when transitioning from composite state
            if (!node.regions.empty())
            {
                os << ", ltail=cluster_" << node.id;
            }
            os << "];\n";
        }

        // Recurse into nested regions, passing down each region's unique local termination node ID
        for (std::size_t r_idx = 0; r_idx < node.regions.size(); ++r_idx)
        {
            std::string reg_term_id = node.id + "_reg_" + std::to_string(r_idx) + "_term";
            for (const auto& sub_node : node.regions[r_idx])
            {
                render_transitions(os, depth, sub_node, reg_term_id);
            }
        }
    }

    inline std::ostream& render(std::ostream& os, const ir::model& model)
    {
        os << "digraph sm {\n"
              "    compound=true;\n"
              "    node [shape=record, fontname=\"Helvetica\", fontsize=10];\n"
              "    edge [fontname=\"Helvetica\", fontsize=9];\n\n";

        // Render root states and hierarchy
        for (const auto& node : model.roots)
        {
            render_node(os, 1, node);
        }

        os << "\n    // Transitions\n";
        for (const auto& node : model.roots)
        {
            render_transitions(os, 1, node, "root_term");
        }

        os << "}\n";
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct dot;

    template <template <typename...> typename API, typename... T>
    struct dot<SM<API, T...>>
    {
        friend std::ostream& operator<<(std::ostream& os, const dot<SM<API, T...>>& /* d */)
        {
            return formatter::dot::render(os, formatter::detail::build_ir<API, T...>());
        }
    };
}
