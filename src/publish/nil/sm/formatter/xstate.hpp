#pragma once

#include "detail.hpp"
#include "ir.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nil::sm::formatter::xstate
{
    inline void render_node(std::ostream& os, std::size_t depth, const ir::state_node& node);

    inline std::string format_target(std::string_view target_id)
    {
        if (target_id == "[*]")
        {
            return "done"; // Match the final state key name below
        }
        return "#" + std::string(target_id);
    }

    inline void render_region_states(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::state_node>& region
    )
    {
        for (const auto& r : region)
        {
            render_node(os, depth, r);
            os << ",\n";
        }

        // Inject XState final state for region termination matching [*]
        indent(os, depth) << "\"done\": {\n";
        indent(os, depth + 1) << "\"type\": \"final\"\n";
        indent(os, depth) << "}\n";
    }

    // NOLINTNEXTLINE
    inline void render_node(std::ostream& os, std::size_t depth, const ir::state_node& node)
    {
        indent(os, depth) << "\"" << node.display_name << "\": {\n";
        indent(os, depth + 1) << R"("id": ")" << node.id << "\",\n";

        // Collect entry and exit actions
        bool has_entry = false;
        bool has_exit = false;
        for (const auto& action : node.actions)
        {
            std::visit(
                [&](const auto& info)
                {
                    using T = std::decay_t<decltype(info)>;
                    if constexpr (std::is_same_v<T, ir::entry_action_info>)
                    {
                        has_entry = true;
                    }
                    else if constexpr (std::is_same_v<T, ir::exit_action_info>)
                    {
                        has_exit = true;
                    }
                },
                action
            );
        }

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
                        if constexpr (std::is_same_v<T, ir::entry_action_info>)
                        {
                            if (!first_entry)
                            {
                                os << ",\n";
                            }
                            indent(os, depth + 2)
                                << "\""
                                << (info.response == ir::entry_response::emit ? "Emit" : "NOOP")
                                << "\"";
                            first_entry = false;
                        }
                    },
                    action
                );
            }
            os << "\n";
            indent(os, depth + 1) << "],\n";
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
                        if constexpr (std::is_same_v<T, ir::exit_action_info>)
                        {
                            if (!first_exit)
                            {
                                os << ",\n";
                            }
                            indent(os, depth + 2)
                                << "\""
                                << (info.response == ir::exit_response::emit ? "Emit" : "NOOP")
                                << "\"";
                            first_exit = false;
                        }
                    },
                    action
                );
            }
            os << "\n";
            indent(os, depth + 1) << "],\n";
        }

        // Map event transitions ("on" block)
        if (!node.transitions.empty())
        {
            indent(os, depth + 1) << "\"on\": {\n";
            for (std::size_t i = 0; i < node.transitions.size(); ++i)
            {
                const auto& tx = node.transitions[i];
                indent(os, depth + 2)
                    << "\"" << event_name(tx) << "\": \"" << format_target(target_id(tx)) << "\"";
                if (i + 1 < node.transitions.size())
                {
                    os << ",";
                }
                os << "\n";
            }
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
                indent(os, depth + 1)
                    << R"("initial": ")" << node.regions[0].front().display_name << "\",\n";
                indent(os, depth + 1) << "\"states\": {\n";
                render_region_states(os, depth + 2, node.regions[0]);
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
                        indent(os, depth + 3)
                            << R"("initial": ")" << node.regions[r].front().display_name << "\",\n";
                        indent(os, depth + 3) << "\"states\": {\n";
                        render_region_states(os, depth + 4, node.regions[r]);
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

    // NOLINTNEXTLINE
    inline std::ostream& render(std::ostream& os, const ir::model& model)
    {
        os << "{\n";
        os << "  \"id\": \"SM\",\n";
        if (!model.roots.empty())
        {
            const auto& root = model.roots[0];
            // Unwrap the top-level container state and promote its inner regions/states directly
            if (!root.regions.empty())
            {
                if (root.regions.size() == 1)
                {
                    if (!root.regions[0].empty())
                    {
                        os << R"(  "initial": ")" << root.regions[0].front().display_name
                           << "\",\n";
                    }
                    os << "  \"states\": {\n";
                    render_region_states(os, 2, root.regions[0]);
                    os << "  }\n";
                }
                else
                {
                    os << "  \"type\": \"parallel\",\n";
                    os << "  \"states\": {\n";
                    for (std::size_t r = 0; r < root.regions.size(); ++r)
                    {
                        indent(os, 2) << "\"region_" << r << "\": {\n";
                        if (!root.regions[r].empty())
                        {
                            indent(os, 3) << R"("initial": ")"
                                          << root.regions[r].front().display_name << "\",\n";
                            indent(os, 3) << "\"states\": {\n";
                            render_region_states(os, 4, root.regions[r]);
                            indent(os, 3) << "}\n";
                        }
                        indent(os, 2) << "}";
                        if (r + 1 < root.regions.size())
                        {
                            os << ",";
                        }
                        os << "\n";
                    }
                    os << "  }\n";
                }
            }
            else
            {
                os << R"(  "initial": ")" << root.display_name << "\",\n";
                os << "  \"states\": {\n";
                render_node(os, 2, root);
                os << "\n  }\n";
            }
        }
        os << "}\n";
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct xstate;

    template <template <typename...> typename API, typename... T>
    struct xstate<SM<API, T...>>
    {
        friend std::ostream& operator<<(std::ostream& os, const xstate<SM<API, T...>>& /* d */)
        {
            return formatter::xstate::render(os, formatter::detail::build_ir<API, T...>());
        }
    };
}
