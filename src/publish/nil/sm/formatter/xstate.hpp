#pragma once

#include "../state.hpp"
#include "detail.hpp"
#include "ir.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nil::sm::formatter::xstate
{
    inline void render_node(std::ostream& os, std::size_t depth, const ir::Node& node);

    inline std::string format_target(std::string_view target_id)
    {
        if (target_id == "[*]")
        {
            return "done"; // Match the final state key name below
        }
        return "#" + std::string(target_id);
    }

    // Check only this node's own transitions (not descendants) for Terminate ([*])
    inline bool own_has_terminate(const ir::Node& node)
    {
        for (const auto& tx : node.transitions)
        {
            if (target_id(tx) == "[*]")
            {
                return true;
            }
        }
        return false;
    }

    inline void render_region_states(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::Node>& region
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
    inline void render_node(std::ostream& os, std::size_t depth, const ir::Node& node)
    {
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
    inline std::ostream& render(std::ostream& os, const ir::Model& model)
    {
        os << "{\n";
        indent(os, 1) << "\"id\": \"SM\",\n";
        if (!model.roots.empty())
        {
            const auto& root = model.roots[0];
            indent(os, 1) << R"("initial": ")" << root.display_name << "\",\n";
            indent(os, 1) << "\"states\": {\n";
            render_node(os, 2, root);
            // XState resolves a relative target like "done" against the transitioning node's
            // *parent*, so root's own terminate transition needs "done" as root's sibling here.
            if (own_has_terminate(root))
            {
                os << ",\n";
                indent(os, 2) << "\"done\": {\n";
                indent(os, 3) << "\"type\": \"final\"\n";
                indent(os, 2) << "}\n";
            }
            else
            {
                os << "\n";
            }
            indent(os, 1) << "}\n";
        }
        os << "}\n";
        return os;
    }
}

namespace nil::sm
{
    template <typename SM>
    struct xstate;

    template <template <typename...> typename API, typename T>
    struct xstate<SM<API, T>>
    {
        friend std::ostream& operator<<(std::ostream& os, const xstate<SM<API, T>>& /* d */)
        {
            return formatter::xstate::render(os, formatter::detail::build_ir<API, T>());
        }
    };
}
