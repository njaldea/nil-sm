// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "ir.hpp"

#include <algorithm>
#include <vector>

namespace nil::sm::detail
{
    // direct_parent<T> needs the immediate parent to expose T; other args look through ancestors.
    constexpr bool diagnostic_satisfied(
        const ir::Dependency& requirement,
        const std::vector<ir::Dependency>& ancestor_props,
        const ir::Node* parent
    )
    {
        const auto provides = [&](const std::vector<ir::Dependency>& props)
        {
            return std::any_of(
                props.begin(),
                props.end(),
                [&](const auto& property) { return property.type_id == requirement.type_id; }
            );
        };

        return requirement.is_direct_parent ? parent != nullptr && provides(parent->provided_props)
                                            : provides(ancestor_props);
    }

    // A barrier looks props up through its host, but never reaches direct_parent.
    // NOLINTNEXTLINE
    constexpr bool nodes_satisfied(
        const ir::Model& model,
        const std::vector<ir::Node>& nodes,
        const std::vector<ir::Dependency>& ancestor_props,
        const ir::Node* parent
    )
    {
        return std::all_of(
            nodes.begin(),
            nodes.end(),
            [&](const ir::Node& node)
            {
                const auto own_satisfied = std::all_of(
                    node.required_args.begin(),
                    node.required_args.end(),
                    [&](const auto& requirement)
                    { return diagnostic_satisfied(requirement, ancestor_props, parent); }
                );
                if (!own_satisfied)
                {
                    return false;
                }

                auto descendant_props = ancestor_props;
                descendant_props.insert(
                    descendant_props.end(),
                    node.provided_props.begin(),
                    node.provided_props.end()
                );
                const auto regions_satisfied = std::all_of(
                    node.regions.begin(),
                    node.regions.end(),
                    [&](const auto& region)
                    { return nodes_satisfied(model, region, descendant_props, &node); }
                );
                if (!regions_satisfied)
                {
                    return false;
                }

                const auto* barrier = ir::find_barrier(model, node.barrier_id);
                return node.barrier_id == nullptr || barrier == nullptr
                    || nodes_satisfied(model, barrier->roots, ancestor_props, nullptr);
            }
        );
    }
}

namespace nil::sm
{
    // True when every state's args are provided by an ancestor prop or by a root arg.
    // Use the viz sandbox to see which state is missing what.
    constexpr bool validate(
        const ir::Model& model,
        const std::vector<ir::Dependency>& root_props = {}
    )
    {
        return detail::nodes_satisfied(model, model.roots, root_props, nullptr);
    }

    // Not consteval: a barrier's definition is built by its out-of-line ir() at runtime.
    template <typename T, typename API, typename... RootProps>
    constexpr bool validate()
    {
        return validate(ir::build<T, API>(), ir::make_root_props<RootProps...>());
    }
}
