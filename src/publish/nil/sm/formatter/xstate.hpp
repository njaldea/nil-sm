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
    struct RegionContext
    {
        std::string initial_key;
        std::string final_id;
    };

    inline void render_node(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context
    );

    inline bool is_initial_node(const ir::Node& node)
    {
        return node.is_initial;
    }

    // A [**] pseudostate node marks where [*] transitions in its region terminate into
    inline bool is_final_node(const ir::Node& node)
    {
        return node.display_name == "[**]";
    }

    inline const ir::Node* find_node_by_id(const std::vector<ir::Node>& region, std::string_view id)
    {
        for (const auto& node : region)
        {
            if (node.id == id)
            {
                return &node;
            }
        }
        return nullptr;
    }

    inline RegionContext make_region_context(const std::vector<ir::Node>& region)
    {
        auto context = RegionContext{};

        for (const auto& node : region)
        {
            if (is_final_node(node))
            {
                context.final_id = node.id;
                break;
            }
        }

        for (const auto& node : region)
        {
            if (is_initial_node(node))
            {
                context.initial_key = is_final_node(node) ? "done" : node.display_name;
                return context;
            }
        }

        return context;
    }

    inline std::string transition_target_key(
        std::string_view raw_target,
        const RegionContext& context
    )
    {
        if (raw_target == "[*]" || (!context.final_id.empty() && raw_target == context.final_id))
        {
            return "done";
        }
        return "#" + std::string(raw_target);
    }

    inline void render_region_states(
        std::ostream& os,
        std::size_t depth,
        const std::vector<ir::Node>& region,
        const RegionContext& context
    )
    {
        auto first = true;
        auto rendered_final = false;
        for (const auto& node : region)
        {
            if (is_final_node(node) && rendered_final)
            {
                continue;
            }

            if (!first)
            {
                os << ",\n";
            }
            render_node(os, depth, node, context);
            rendered_final = rendered_final || is_final_node(node);
            first = false;
        }
        os << "\n";
    }

    // NOLINTNEXTLINE
    inline void render_node(
        std::ostream& os,
        std::size_t depth,
        const ir::Node& node,
        const RegionContext& context
    )
    {
        if (is_final_node(node))
        {
            // The [**] pseudostate renders as XState's own final-state marker
            indent(os, depth) << "\"done\": {\n";
            indent(os, depth + 1) << "\"type\": \"final\"\n";
            indent(os, depth) << "}";
            return;
        }

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
            auto rendered = std::size_t{0};
            for (const auto& tx : node.transitions)
            {
                if (event_name(tx).empty())
                {
                    continue;
                }

                if (rendered > 0)
                {
                    os << ",\n";
                }
                indent(os, depth + 2) << "\"" << event_name(tx) << "\": \""
                                      << transition_target_key(target_id(tx), context) << "\"";
                ++rendered;
            }
            os << "\n";
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
                const auto child_context = make_region_context(node.regions[0]);
                if (!child_context.initial_key.empty())
                {
                    indent(os, depth + 1)
                        << R"("initial": ")" << child_context.initial_key << "\",\n";
                }
                indent(os, depth + 1) << "\"states\": {\n";
                render_region_states(os, depth + 2, node.regions[0], child_context);
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
                        const auto reg_context = make_region_context(node.regions[r]);
                        if (!reg_context.initial_key.empty())
                        {
                            indent(os, depth + 3)
                                << R"("initial": ")" << reg_context.initial_key << "\",\n";
                        }
                        indent(os, depth + 3) << "\"states\": {\n";
                        render_region_states(os, depth + 4, node.regions[r], reg_context);
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
            const auto root_context = make_region_context(model.roots);
            if (!root_context.initial_key.empty())
            {
                indent(os, 1) << R"("initial": ")" << root_context.initial_key << "\",\n";
            }

            indent(os, 1) << "\"states\": {\n";
            render_region_states(os, 2, model.roots, root_context);
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
