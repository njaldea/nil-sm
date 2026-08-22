#pragma once

#include "../state.hpp"
#include "detail.hpp"
#include "ir.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nil::sm::formatter::dot
{
    struct RegionContext
    {
        std::string init_id;
        std::string term_id;
        std::string final_id;
        bool needs_term = false;
    };

    inline bool is_final_node(const ir::Node& node)
    {
        return node.display_name == "[**]";
    }

    inline std::string format_action(const ir::action::Info& action)
    {
        std::string result;
        std::visit(
            [&](const auto& info)
            {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, ir::action::Entry>)
                {
                    result = "on Enter / " + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::action::Exit>)
                {
                    result = "on Exit / " + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::action::RegionsFinalized>)
                {
                    result = "on [**] / " + std::string(action_name(info.response));
                }
                else if constexpr (std::is_same_v<T, ir::action::Capture>)
                {
                    result = "on " + info.event_name + " [c] / "
                        + std::string(action_name(info.response));
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

    inline std::string initial_id_of(const std::vector<ir::Node>& region)
    {
        for (const auto& node : region)
        {
            if (node.is_initial)
            {
                return node.id;
            }
        }
        return "";
    }

    inline std::string final_id_of(const std::vector<ir::Node>& region)
    {
        for (const auto& node : region)
        {
            if (is_final_node(node))
            {
                return node.id;
            }
        }
        return "";
    }

    inline bool is_termination_target(std::string_view target_id_value, std::string_view final_id)
    {
        return target_id_value == "[*]" || (!final_id.empty() && target_id_value == final_id);
    }

    inline RegionContext make_region_context(
        std::string_view prefix,
        const std::vector<ir::Node>& region
    )
    {
        auto context = RegionContext{
            .init_id = std::string(prefix) + "_init",
            .term_id = std::string(prefix) + "_term",
            .final_id = final_id_of(region),
            .needs_term = false,
        };

        for (const auto& node : region)
        {
            if (is_final_node(node))
            {
                continue;
            }

            for (const auto& transition : node.transitions)
            {
                if (is_termination_target(target_id(transition), context.final_id))
                {
                    context.needs_term = true;
                    return context;
                }
            }
        }

        return context;
    }

    inline void render_node(std::ostream& os, std::size_t depth, const ir::Node& node)
    {
        if (is_final_node(node))
        {
            return;
        }

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
                    const auto context
                        = make_region_context(node.id + "_reg_" + std::to_string(r_idx), region);
                    const auto initial_id = initial_id_of(region);

                    if (!initial_id.empty())
                    {
                        // Region initial pseudo-state
                        indent(os, depth + 2) << context.init_id << " [shape=point, label=\"\"];\n";
                        indent(os, depth + 2) << context.init_id << " -> " << initial_id
                                              << "[lhead=\"cluster_" << initial_id << "\"];\n";
                    }
                    if (context.needs_term)
                    {
                        // Local region termination node
                        indent(os, depth + 2)
                            << context.term_id
                            << " [shape=doublecircle, label=\"\", width=0.2, height=0.2];\n";
                    }

                    for (const auto& sub_node : region)
                    {
                        render_node(os, depth + 2, sub_node);
                    }
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
        const ir::Node& node,
        const RegionContext& context
    )
    {
        if (is_final_node(node))
        {
            return;
        }

        for (const auto& transition : node.transitions)
        {
            indent(os, depth) << node.id << " -> ";

            if (is_termination_target(target_id(transition), context.final_id)
                && context.needs_term)
            {
                os << context.term_id;
            }
            else
            {
                os << target_id(transition);
            }

            if (event_name(transition).empty())
            {
                os << ";\n";
            }
            else
            {
                os << " [label=\"" << event_name(transition)
                   << (is_capture(transition) ? " [c]" : "") << "\"";

                // Use ltail with compound graphs when transitioning from composite state
                if (!node.regions.empty())
                {
                    os << ", ltail=cluster_" << node.id;
                }
                os << "];\n";
            }
        }

        // Recurse into nested regions, passing down each region's unique local termination node ID
        for (std::size_t r_idx = 0; r_idx < node.regions.size(); ++r_idx)
        {
            const auto child_context = make_region_context(
                node.id + "_reg_" + std::to_string(r_idx),
                node.regions[r_idx]
            );
            for (const auto& sub_node : node.regions[r_idx])
            {
                render_transitions(os, depth, sub_node, child_context);
            }
        }
    }

    inline std::ostream& render(std::ostream& os, const ir::Model& model)
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

        const auto root_context = make_region_context("root", model.roots);
        const auto root_initial_id = initial_id_of(model.roots);

        if (!root_initial_id.empty())
        {
            indent(os, 1) << "root_init [shape=point, label=\"\"];\n";
            indent(os, 1) << "root_init -> " << root_initial_id << ";\n";
        }

        if (root_context.needs_term)
        {
            indent(os, 1) << "root_term [shape=doublecircle, label=\"\", width=0.2, height=0.2];\n";
        }

        os << "\n    // Transitions\n";
        for (const auto& node : model.roots)
        {
            render_transitions(os, 1, node, root_context);
        }

        os << "}\n";
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct dot;

    template <template <typename...> typename API, typename T>
    struct dot<SM<API, T>>
    {
        friend std::ostream& operator<<(std::ostream& os, const dot<SM<API, T>>& /* d */)
        {
            return formatter::dot::render(os, formatter::detail::build_ir<API, T>());
        }
    };
}
