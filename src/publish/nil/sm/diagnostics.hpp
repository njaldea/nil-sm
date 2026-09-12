// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "ir.hpp"

#include <algorithm>
#include <vector>

namespace nil::sm::detail
{
    constexpr bool diagnostic_satisfied(
        const ir::Dependency& requirement,
        const std::vector<ir::Dependency>& ancestor_props,
        bool has_parent
    )
    {
        return requirement.is_direct_parent
            ? has_parent
            : std::any_of(
                  ancestor_props.begin(),
                  ancestor_props.end(),
                  [&](const auto& property) { return property.type_id == requirement.type_id; }
              );
    }

    constexpr void append_diagnostic_requirement(
        std::vector<ir::UnsatisfiedArgument>& requirements,
        ir::UnsatisfiedArgument requirement
    )
    {
        if (!std::any_of(
                requirements.begin(),
                requirements.end(),
                [&](const auto& existing)
                {
                    return existing.dependency.type_id == requirement.dependency.type_id
                        && existing.dependency.is_direct_parent
                        == requirement.dependency.is_direct_parent
                        && existing.barrier_path == requirement.barrier_path;
                }
            ))
        {
            requirements.push_back(std::move(requirement));
        }
    }

    // NOLINTNEXTLINE
    constexpr void validate_nodes(
        std::vector<ir::Node>& nodes,
        const std::vector<ir::Dependency>& ancestor_props,
        bool has_parent,
        std::vector<std::string_view> host_path,
        ir::Model& model
    )
    {
        for (auto& node : nodes)
        {
            host_path.push_back(node.display_name);
            for (const auto& requirement : node.required_args)
            {
                if (!diagnostic_satisfied(requirement, ancestor_props, has_parent))
                {
                    node.has_unsatisfied_args = true;
                    model.has_unsatisfied_args = true;
                    append_diagnostic_requirement(
                        model.unsatisfied_args,
                        ir::UnsatisfiedArgument{requirement, {}}
                    );
                }
            }

            auto descendant_props = ancestor_props;
            descendant_props.insert(
                descendant_props.end(),
                node.provided_props.begin(),
                node.provided_props.end()
            );
            for (auto& region : node.regions)
            {
                validate_nodes(region, descendant_props, true, host_path, model);
            }

            if (node.barrier_id != nullptr)
            {
                for (const auto& barrier : model.barriers)
                {
                    if (barrier.id == node.barrier_id)
                    {
                        for (const auto& requirement : barrier.unsatisfied_args)
                        {
                            if (!diagnostic_satisfied(
                                    requirement.dependency,
                                    ancestor_props,
                                    false
                                ))
                            {
                                node.has_unsatisfied_args = true;
                                model.has_unsatisfied_args = true;
                                append_diagnostic_requirement(model.unsatisfied_args, requirement);
                                model.barrier_errors.push_back(ir::BarrierError{
                                    requirement.dependency,
                                    requirement.barrier_path,
                                    host_path
                                });
                            }
                        }
                        break;
                    }
                }
            }

            host_path.pop_back();
        }
    }
}

namespace nil::sm
{
    constexpr bool validate(ir::Model& model)
    {
        detail::validate_nodes(model.roots, {}, false, {}, model);
        return !model.has_unsatisfied_args;
    }

    template <typename API, typename T>
    consteval bool validate()
    {
        auto model = ir::build<API, T>();
        return validate(model);
    }
}
