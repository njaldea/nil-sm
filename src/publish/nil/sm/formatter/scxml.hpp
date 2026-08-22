#pragma once

#include "../state.hpp"
#include "detail.hpp"
#include "ir.hpp"

#include <ostream>

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// TODO: unique identifier is not used per state
// need to find a way for states to have separate
// id and display text

namespace nil::sm::formatter::scxml
{
    struct RegionContext
    {
        std::string initial_id;
        std::string final_id;
    };

    // Recursively collect display-name-to-ID mappings
    inline void collect_id_mappings(
        const std::vector<ir::Node>& region,
        std::unordered_map<std::string, std::string>& id_map
    )
    {
        for (const auto& node : region)
        {
            id_map[node.id] = !node.display_name.empty() ? node.display_name : node.id;

            for (const auto& child_region : node.regions)
            {
                collect_id_mappings(child_region, id_map);
            }
        }
    }

    inline std::string resolve_id(
        const std::string& raw_id,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        auto it = id_map.find(raw_id);
        if (it != id_map.end())
        {
            return it->second;
        }
        return raw_id;
    }

    inline bool is_final_node(const ir::Node& node)
    {
        return node.display_name == "[**]";
    }

    inline bool is_initial_node(const ir::Node& node)
    {
        return node.is_initial;
    }

    // Final target is the sibling [**] pseudostate in the same region.
    inline std::string find_final_id(const std::vector<ir::Node>& siblings)
    {
        for (const auto& node : siblings)
        {
            if (is_final_node(node))
            {
                return node.id;
            }
        }
        return {};
    }

    // Find the region's initial state from the explicit is_initial flag.
    inline std::string initial_id_of(const std::vector<ir::Node>& region)
    {
        for (const auto& node : region)
        {
            if (is_initial_node(node))
            {
                return node.id;
            }
        }
        return "";
    }

    inline RegionContext make_region_context(const std::vector<ir::Node>& region)
    {
        return RegionContext{
            .initial_id = initial_id_of(region),
            .final_id = find_final_id(region)
        };
    }

    inline bool is_final_target(std::string_view raw_target_id, std::string_view final_id)
    {
        return raw_target_id == "[*]" || (!final_id.empty() && raw_target_id == final_id);
    }

    inline std::string resolve_transition_target(
        const ir::transit::Info& transition,
        const RegionContext& context,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        if (is_final_target(target_id(transition), context.final_id))
        {
            return context.final_id;
        }
        return resolve_id(target_id(transition), id_map);
    }

    // Render entry/exit and event action executable content (raise-based, no <script> needed)
    inline void render_actions(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::action::Info>& actions,
        const std::string& state_id
    )
    {
        for (const auto& action : actions)
        {
            std::visit(
                [&]<typename T>(const T& info)
                {
                    if constexpr (std::is_same_v<T, ir::action::Entry>)
                    {
                        if (info.response == ir::response::EEntry::emit)
                        {
                            indent(os, depth) << "<onentry>\n";
                            indent(os, depth + 1)
                                << "<raise event=\"" << state_id << ".onentry.emit\"/>\n";
                            indent(os, depth) << "</onentry>\n";
                        }
                    }
                    else if constexpr (std::is_same_v<T, ir::action::Exit>)
                    {
                        if (info.response == ir::response::EExit::emit)
                        {
                            indent(os, depth) << "<onexit>\n";
                            indent(os, depth + 1)
                                << "<raise event=\"" << state_id << ".onexit.emit\"/>\n";
                            indent(os, depth) << "</onexit>\n";
                        }
                    }
                    else if constexpr (std::is_same_v<T, ir::action::Event>)
                    {
                        indent(os, depth) << "<transition event=\"" << info.event_name << "\">\n";
                        if (info.response == ir::response::EEvent::emit)
                        {
                            indent(os, depth + 1) << "<raise event=\"" << state_id << ".emit\"/>\n";
                        }
                        else
                        {
                            indent(os, depth + 1)
                                << "<!-- Response: " << action_name(info.response) << " -->\n";
                        }
                        indent(os, depth) << "</transition>\n";
                    }
                    else if constexpr (std::is_same_v<T, ir::action::Capture>)
                    {
                        indent(os, depth) << "<transition event=\"" << info.event_name << "\">\n";
                        indent(os, depth + 1) << "<!-- captured before regions -->\n";
                        if (info.response == ir::response::EEvent::emit)
                        {
                            indent(os, depth + 1) << "<raise event=\"" << state_id << ".emit\"/>\n";
                        }
                        else
                        {
                            indent(os, depth + 1)
                                << "<!-- Response: " << action_name(info.response) << " -->\n";
                        }
                        indent(os, depth) << "</transition>\n";
                    }
                    else if constexpr (std::is_same_v<T, ir::action::RegionsFinalized>)
                    {
                        indent(os, depth)
                            << "<transition event=\"done.state." << state_id << "\">\n";
                        if (info.response == ir::response::ERegionsFinalized::emit)
                        {
                            indent(os, depth + 1)
                                << "<raise event=\"" << state_id << ".regions_finalized.emit\"/>\n";
                        }
                        else
                        {
                            indent(os, depth + 1)
                                << "<!-- Response: " << action_name(info.response) << " -->\n";
                        }
                        indent(os, depth) << "</transition>\n";
                    }
                },
                action
            );
        }
    }

    inline void render_node(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context,
        const std::unordered_map<std::string, std::string>& id_map
    );

    inline void render_transitions(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::transit::Info>& transitions,
        const RegionContext& context,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        for (const auto& transition : transitions)
        {
            const auto target = resolve_transition_target(transition, context, id_map);
            const auto ev = event_name(transition);

            if (is_capture(transition))
            {
                indent(os, depth) << "<!-- captured before regions -->\n";
            }
            indent(os, depth) << "<transition event=\"" << ev << "\" target=\"" << target
                              << "\"/>\n";
        }
    }

    // Render every child of a region, substituting a <final> element for the [**] pseudostate
    // instead of rendering it as an ordinary state.
    inline void render_children(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::Node>& region,
        const RegionContext& context,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        for (const auto& child : region)
        {
            if (is_final_node(child))
            {
                if (!context.final_id.empty())
                {
                    indent(os, depth) << "<final id=\"" << context.final_id << "\"/>\n";
                }
                continue;
            }
            render_node(os, depth, child, context, id_map);
        }
    }

    inline void render_compound_state(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context,
        const std::string& state_id,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        const bool is_composite = !node.regions.empty();

        std::string initial_attr;
        RegionContext child_context;
        if (is_composite)
        {
            child_context = make_region_context(node.regions.front());
            if (!child_context.initial_id.empty())
            {
                initial_attr = " initial=\"" + resolve_id(child_context.initial_id, id_map) + "\"";
            }
        }

        indent(os, depth) << "<state id=\"" << state_id << "\"" << initial_attr << ">\n";

        render_actions(os, depth + 1, node.actions, state_id);
        render_transitions(os, depth + 1, node.transitions, context, id_map);

        if (is_composite)
        {
            render_children(os, depth + 1, node.regions.front(), child_context, id_map);
        }

        indent(os, depth) << "</state>\n";
    }

    // Every region is always wrapped in its own compound state so its completion id is
    // well-defined and never a dangling reference.
    inline void render_parallel_region(
        std::ostream& os,
        std::size_t depth,
        std::size_t index,
        const std::string& parent_state_id,
        const std::vector<ir::Node>& reg,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        if (reg.empty())
        {
            return;
        }

        indent(os, depth) << "<!-- Region " << index << " -->\n";

        const std::string reg_state_id = std::to_string(index) + "_" + parent_state_id;
        const auto context = make_region_context(reg);
        std::string initial_attr;
        if (!context.initial_id.empty())
        {
            initial_attr = " initial=\"" + resolve_id(context.initial_id, id_map) + "\"";
        }

        indent(os, depth) << "<state id=\"" << reg_state_id << "\"" << initial_attr << ">\n";
        render_children(os, depth + 1, reg, context, id_map);
        indent(os, depth) << "</state>\n";
    }

    inline void render_parallel_state(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context,
        const std::string& state_id,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        indent(os, depth) << "<parallel id=\"" << state_id << "\">\n";

        render_actions(os, depth + 1, node.actions, state_id);
        render_transitions(os, depth + 1, node.transitions, context, id_map);

        for (std::size_t r = 0; r < node.regions.size(); ++r)
        {
            render_parallel_region(os, depth + 1, r, state_id, node.regions[r], id_map);
        }

        indent(os, depth) << "</parallel>\n";
    }

    inline void render_node(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        const bool is_parallel = node.regions.size() > 1;
        const std::string state_id = resolve_id(node.id, id_map);

        if (is_parallel)
        {
            render_parallel_state(os, depth, node, context, state_id, id_map);
        }
        else
        {
            render_compound_state(os, depth, node, context, state_id, id_map);
        }
    }

    inline std::ostream& render(std::ostream& os, const ir::Model& model)
    {
        std::unordered_map<std::string, std::string> id_map;
        collect_id_mappings(model.roots, id_map);

        os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << R"(<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0")";

        const auto root_context = make_region_context(model.roots);
        if (!root_context.initial_id.empty())
        {
            os << " initial=\"" << resolve_id(root_context.initial_id, id_map) << "\">\n";
        }
        else
        {
            os << ">\n";
        }

        for (const auto& root : model.roots)
        {
            if (is_final_node(root))
            {
                continue;
            }
            render_node(os, 1, root, root_context, id_map);
        }

        if (!root_context.final_id.empty())
        {
            indent(os, 1) << "<final id=\"" << root_context.final_id << "\"/>\n";
        }

        os << "</scxml>\n";
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct scxml;

    template <template <typename...> typename API, typename T>
    struct scxml<SM<API, T>>
    {
        friend std::ostream& operator<<(std::ostream& os, const scxml<SM<API, T>>& /* doc */)
        {
            return formatter::scxml::render(os, formatter::detail::build_ir<API, T>());
        }
    };
}
