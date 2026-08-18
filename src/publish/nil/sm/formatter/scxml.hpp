#pragma once

#include "detail.hpp"
#include "ir.hpp"

#include <algorithm>
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
    inline std::string sanitize_event(const std::string& ev)
    {
        if (ev == "[*]" || ev == "[**]" || ev.empty())
        {
            return "*";
        }
        return ev;
    }

    // Recursively collect display-name-to-ID mappings
    inline void collect_id_mappings(
        const std::vector<ir::state_node>& region,
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

    // Check only this node's own transitions (not descendants) for Terminate ([*])
    inline bool own_has_terminate(const ir::state_node& node)
    {
        return std::ranges::any_of(
            node.transitions,
            [](const auto& t) { return target_id(t) == "[*]"; }
        );
    }

    // Check if a leaf anywhere in this subtree terminates into current_final_id. A composite's
    // own transitions are excluded (they resolve to their own dedicated own-final id instead),
    // and a parallel node breaks the chain entirely since each of its regions mints its own
    // fresh final id rather than forwarding current_final_id to its descendants.
    inline bool leaf_descendant_has_terminate(const ir::state_node& node)
    {
        if (node.regions.empty())
        {
            return own_has_terminate(node);
        }
        if (node.regions.size() > 1)
        {
            return false;
        }
        return std::ranges::any_of(
            node.regions.front(),
            [](const auto& child) { return leaf_descendant_has_terminate(child); }
        );
    }

    inline bool region_has_terminate(const std::vector<ir::state_node>& region)
    {
        return std::ranges::any_of(
            region,
            [](const auto& node) { return leaf_descendant_has_terminate(node); }
        );
    }

    // Render entry/exit and event action executable content (raise-based, no <script> needed)
    inline void render_actions(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::action_info>& actions,
        const std::string& state_id
    )
    {
        for (const auto& action : actions)
        {
            std::visit(
                [&]<typename T>(const T& info)
                {
                    if constexpr (std::is_same_v<T, ir::entry_action_info>)
                    {
                        if (info.response == ir::entry_response::emit)
                        {
                            indent(os, depth) << "<onentry>\n";
                            indent(os, depth + 1)
                                << "<raise event=\"" << state_id << ".onentry.emit\"/>\n";
                            indent(os, depth) << "</onentry>\n";
                        }
                    }
                    else if constexpr (std::is_same_v<T, ir::exit_action_info>)
                    {
                        if (info.response == ir::exit_response::emit)
                        {
                            indent(os, depth) << "<onexit>\n";
                            indent(os, depth + 1)
                                << "<raise event=\"" << state_id << ".onexit.emit\"/>\n";
                            indent(os, depth) << "</onexit>\n";
                        }
                    }
                    else if constexpr (std::is_same_v<T, ir::event_action_info>)
                    {
                        indent(os, depth)
                            << "<transition event=\"" << sanitize_event(info.event_name) << "\">\n";
                        if (info.response == ir::event_response::emit)
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
                    else if constexpr (std::is_same_v<T, ir::capture_action_info>)
                    {
                        indent(os, depth)
                            << "<transition event=\"" << sanitize_event(info.event_name) << "\">\n";
                        indent(os, depth + 1) << "<!-- captured before regions -->\n";
                        if (info.response == ir::event_response::emit)
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
                    else if constexpr (std::is_same_v<T, ir::regions_finalized_action_info>)
                    {
                        indent(os, depth)
                            << "<transition event=\"done.state." << state_id << "\">\n";
                        if (info.response == ir::regions_finalized_response::emit)
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
        const ir::state_node& node,
        const std::string& current_final_id,
        const std::string& own_final_id,
        const std::unordered_map<std::string, std::string>& id_map
    );

    // Render a child within its parent's region: only composite children need a dedicated
    // own-final id (leaves always resolve their own transitions to current_final_id directly).
    inline void render_child(
        std::ostream& os,
        std::size_t depth,
        const ir::state_node& child,
        const std::string& parent_current_final_id,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        const bool child_is_composite = !child.regions.empty();
        const std::string child_own_final_id
            = child_is_composite ? (child.id + "_final") : std::string{};
        render_node(os, depth, child, parent_current_final_id, child_own_final_id, id_map);
        if (child_is_composite && own_has_terminate(child))
        {
            indent(os, depth) << "<final id=\"" << child_own_final_id << "\"/>\n";
        }
    }

    inline void render_compound_state(
        std::ostream& os,
        std::size_t depth,
        const ir::state_node& node,
        const std::string& current_final_id,
        const std::string& self_target,
        const std::string& state_id,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        const bool is_composite = !node.regions.empty();

        std::string initial_attr;
        if (is_composite && !node.regions.front().empty())
        {
            initial_attr
                = " initial=\"" + resolve_id(node.regions.front().front().id, id_map) + "\"";
        }

        indent(os, depth) << "<state id=\"" << state_id << "\"" << initial_attr << ">\n";

        render_actions(os, depth + 1, node.actions, state_id);

        // Outgoing transitions defined on this state target self_target
        for (const auto& transition : node.transitions)
        {
            const bool is_target_final = (target_id(transition) == "[*]");
            const std::string target
                = is_target_final ? self_target : resolve_id(target_id(transition), id_map);
            const std::string ev = sanitize_event(event_name(transition));

            if (is_capture(transition))
            {
                indent(os, depth + 1) << "<!-- captured before regions -->\n";
            }
            indent(os, depth + 1) << "<transition event=\"" << ev << "\" target=\"" << target
                                  << "\"/>\n";
        }

        if (is_composite)
        {
            for (const auto& child : node.regions.front())
            {
                render_child(os, depth + 1, child, current_final_id, id_map);
            }
        }

        // Render final tag inside this compound state if a leaf descendant terminates into it
        if (is_composite && leaf_descendant_has_terminate(node))
        {
            indent(os, depth + 1) << "<final id=\"" << current_final_id << "\"/>\n";
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
        const std::vector<ir::state_node>& reg,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        if (reg.empty())
        {
            return;
        }

        indent(os, depth) << "<!-- Region " << index << " -->\n";

        const std::string reg_state_id = std::to_string(index) + "_" + parent_state_id;
        // Keyed off reg_state_id (not reg.front().id) so it never collides with a sole
        // composite child's own_final_id, which is keyed off the child's own id instead.
        const std::string reg_final_id = reg_state_id + "_final";
        const bool reg_has_term = region_has_terminate(reg);

        indent(os, depth) << "<state id=\"" << reg_state_id << "\">\n";

        for (const auto& child : reg)
        {
            render_child(os, depth + 1, child, reg_final_id, id_map);
        }

        // A sole composite child already declares reg_final_id itself (as its own
        // current_final_id); only declare it here when nothing else will.
        const bool sole_child_owns_final = (reg.size() == 1) && !reg.front().regions.empty();
        if (reg_has_term && !sole_child_owns_final)
        {
            indent(os, depth + 1) << "<final id=\"" << reg_final_id << "\"/>\n";
        }

        indent(os, depth) << "</state>\n";
    }

    inline void render_parallel_state(
        std::ostream& os,
        std::size_t depth,
        const ir::state_node& node,
        const std::string& self_target,
        const std::string& state_id,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        indent(os, depth) << "<parallel id=\"" << state_id << "\">\n";

        render_actions(os, depth + 1, node.actions, state_id);

        // Outgoing transitions on the parallel state target self_target
        for (const auto& transition : node.transitions)
        {
            const bool is_target_final = (target_id(transition) == "[*]");
            const std::string target
                = is_target_final ? self_target : resolve_id(target_id(transition), id_map);
            const std::string ev = sanitize_event(event_name(transition));

            if (is_capture(transition))
            {
                indent(os, depth + 1) << "<!-- captured before regions -->\n";
            }
            indent(os, depth + 1) << "<transition event=\"" << ev << "\" target=\"" << target
                                  << "\"/>\n";
        }

        for (std::size_t r = 0; r < node.regions.size(); ++r)
        {
            render_parallel_region(os, depth + 1, r, state_id, node.regions[r], id_map);
        }

        indent(os, depth) << "</parallel>\n";
    }

    inline void render_node(
        std::ostream& os,
        std::size_t depth,
        const ir::state_node& node,
        const std::string& current_final_id,
        const std::string& own_final_id,
        const std::unordered_map<std::string, std::string>& id_map
    )
    {
        const bool is_parallel = node.regions.size() > 1;
        const bool is_composite = !node.regions.empty();
        const std::string state_id = resolve_id(node.id, id_map);
        // A composite's own transitions target its dedicated own-final id (declared by its
        // parent as a sibling); a leaf has no descendants, so it keeps using current_final_id.
        const std::string& self_target = is_composite ? own_final_id : current_final_id;

        if (is_parallel)
        {
            render_parallel_state(os, depth, node, self_target, state_id, id_map);
        }
        else
        {
            render_compound_state(os, depth, node, current_final_id, self_target, state_id, id_map);
        }
    }

    inline std::ostream& render(std::ostream& os, const ir::model& model)
    {
        std::unordered_map<std::string, std::string> id_map;
        collect_id_mappings(model.roots, id_map);

        os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << R"(<scxml xmlns="http://www.w3.org/2005/07/scxml" version="1.0")";

        // model.roots is always a single synthetic "SM" wrapper with one region holding the
        // actual top-level state; it never has actions/transitions of its own, so unwrap it
        // and expose that state directly as <scxml>'s top-level content.

        const auto& top = model.roots.front().regions.front().front();
        const std::string root_final_id = top.id + "_final";
        const std::string root_own_final_id = top.id + "_own_final";

        os << " initial=\"" << resolve_id(top.id, id_map) << "\">\n";
        render_node(os, 1, top, root_final_id, root_own_final_id, id_map);
        if (own_has_terminate(top))
        {
            indent(os, 1) << "<final id=\"" << root_own_final_id << "\"/>\n";
        }

        os << "</scxml>\n";
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct scxml;

    template <template <typename...> typename API, typename... T>
    struct scxml<SM<API, T...>>
    {
        friend std::ostream& operator<<(std::ostream& os, const scxml<SM<API, T...>>& /* doc */)
        {
            return formatter::scxml::render(os, formatter::detail::build_ir<API, T...>());
        }
    };
}
