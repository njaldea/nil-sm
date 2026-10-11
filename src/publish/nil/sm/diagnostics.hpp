// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "index.hpp"
#include "ir.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace nil::sm::detail
{
    // direct_parent<T> needs the immediate parent to expose T; other args look through ancestors.
    constexpr bool diagnostic_satisfied(
        const ir::Arg& requirement,
        const std::vector<ir::Prop>& ancestor_props,
        const ir::Node* parent
    )
    {
        const auto provides = [&](const std::vector<ir::Prop>& props)
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
        const std::vector<ir::Prop>& ancestor_props,
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
    constexpr bool validate(const ir::Model& model, const std::vector<ir::Prop>& root_props = {})
    {
        return detail::nodes_satisfied(model, model.roots, root_props, nullptr);
    }

    // Not consteval: a barrier's definition is built by its out-of-line ir() at runtime.
    template <typename T, typename API, typename... RootProps>
    constexpr bool validate()
    {
        return validate(ir::build<T, API>(), ir::make_root_props<RootProps...>());
    }

    // One unmet requirement. States that miss the same thing the same way are merged into one.
    struct Issue
    {
        ir::Arg dependency;
        // Barriers entered to reach the requirement, outermost first.
        std::vector<std::string_view> barrier_path;
        // The first state it was found on.
        std::string_view state;
        // Reported on the parent that has to expose `dependency` for a child's direct_parent<T>.
        bool as_parent = false;
    };

    struct ChildReport
    {
        std::string_view id;
        std::vector<Issue> issues;
        // Unmet requirements below this state; they stay out of sight while it is collapsed.
        std::size_t subtree_unmet = 0;
    };

    // The details behind validate(), reported one level at a time. Holds a reference to `model`.
    class Diagnostics final
    {
    public:
        Diagnostics(const ir::Model& init_model, std::vector<ir::Prop> init_root_props)
            : model(&init_model)
            , index(init_model)
            , root_props(std::move(init_root_props))
        {
        }

        const Index& lookup() const
        {
            return index;
        }

        // Reports for every state in the regions of the state at `occurrence`. An occurrence is
        // the state ids joined by "::", each barrier host followed by a state of its body, because
        // a body is judged by where it is hosted. "0" is the root. nullopt when it names no state.
        std::optional<std::vector<ChildReport>> children(std::string_view occurrence) const
        {
            const std::scoped_lock lock(mutex);
            const auto level = resolve(occurrence);
            if (!level)
            {
                return std::nullopt;
            }

            std::vector<ChildReport> reports;
            for (const auto* region : level->regions)
            {
                for (const auto& child : *region)
                {
                    auto issues = node_issues(child, level->props, level->parent);
                    append(issues, parent_issues(child));
                    reports.push_back(ChildReport{
                        child.id,
                        unique(std::move(issues)),
                        subtree_unmet(child, *level),
                    });
                }
            }
            return reports;
        }

        // What the whole root tree leaves unmet; direct_parent issues are reported by their states.
        const std::vector<Issue>& root_issues() const
        {
            const std::scoped_lock lock(mutex);
            if (!cached_root_issues)
            {
                std::vector<Issue> issues;
                for (auto& issue : collect(model->roots, root_props, nullptr))
                {
                    if (issue.dependency.is_direct_parent)
                    {
                        continue;
                    }
                    if (!issue.barrier_path.empty())
                    {
                        // the unnamed root has no barrier of its own to skip
                        issue.barrier_path.insert(issue.barrier_path.begin(), std::string_view());
                    }
                    issues.push_back(std::move(issue));
                }
                cached_root_issues = std::move(issues);
            }
            return *cached_root_issues;
        }

        // What a barrier body leaves for its host to provide. nullopt for an unknown barrier.
        std::optional<std::vector<Issue>> barrier_requirements(const void* barrier_id) const
        {
            const std::scoped_lock lock(mutex);
            const auto* barrier
                = barrier_id == nullptr ? nullptr : ir::find_barrier(*model, barrier_id);
            if (barrier == nullptr)
            {
                return std::nullopt;
            }
            return collect(barrier->roots, {}, nullptr);
        }

    private:
        using Props = std::vector<ir::Prop>;

        // The regions of one state, and what their states can see.
        struct Level
        {
            std::vector<const std::vector<ir::Node>*> regions;
            Props props;
            const ir::Node* parent = nullptr;
            // The barrier hosts the level sits under; it keeps equal ids of different hosts apart.
            std::string prefix;
        };

        static void append(std::vector<Issue>& target, std::vector<Issue> source)
        {
            std::move(source.begin(), source.end(), std::back_inserter(target));
        }

        // Issues that differ only by the state they were found on are one cause, so they merge.
        static std::vector<Issue> unique(std::vector<Issue> issues)
        {
            using Key = std::tuple<const void*, bool, bool, std::vector<std::string_view>>;
            std::set<Key> seen;
            std::vector<Issue> result;
            for (auto& issue : issues)
            {
                if (seen.emplace(
                            issue.dependency.type_id,
                            issue.dependency.is_direct_parent,
                            issue.as_parent,
                            issue.barrier_path
                    )
                        .second)
                {
                    result.push_back(std::move(issue));
                }
            }
            return result;
        }

        // The node's own args that nothing provides, plus whatever its barrier body cannot get.
        std::vector<Issue> node_issues(
            const ir::Node& node,
            const Props& props,
            const ir::Node* parent
        ) const
        {
            std::vector<Issue> issues;
            for (const auto& dependency : node.required_args)
            {
                if (!detail::diagnostic_satisfied(dependency, props, parent))
                {
                    issues.push_back(Issue{dependency, {}, node.display_name, false});
                }
            }

            const auto* barrier
                = node.barrier_id == nullptr ? nullptr : ir::find_barrier(*model, node.barrier_id);
            if (barrier != nullptr)
            {
                for (auto& issue : collect(barrier->roots, props, nullptr))
                {
                    issue.barrier_path.insert(issue.barrier_path.begin(), barrier->name);
                    issues.push_back(std::move(issue));
                }
            }
            return unique(std::move(issues));
        }

        // What `node` would have to expose for the direct_parent<T> args of its children.
        static std::vector<Issue> parent_issues(const ir::Node& node)
        {
            std::vector<Issue> issues;
            for (const auto& region : node.regions)
            {
                for (const auto& child : region)
                {
                    for (const auto& dependency : child.required_args)
                    {
                        const auto provided = std::any_of(
                            node.provided_props.begin(),
                            node.provided_props.end(),
                            [&](const auto& prop) { return prop.type_id == dependency.type_id; }
                        );
                        if (dependency.is_direct_parent && !provided)
                        {
                            issues.push_back(Issue{dependency, {}, node.display_name, true});
                        }
                    }
                }
            }
            return unique(std::move(issues));
        }

        std::vector<Issue> collect(
            const std::vector<ir::Node>& nodes,
            const Props& props,
            const ir::Node* parent
        ) const
        {
            std::vector<Issue> issues;
            for (const auto& node : nodes)
            {
                append(issues, node_issues(node, props, parent));
                if (!node.regions.empty())
                {
                    auto inner = props;
                    inner.insert(
                        inner.end(),
                        node.provided_props.begin(),
                        node.provided_props.end()
                    );
                    for (const auto& region : node.regions)
                    {
                        append(issues, collect(region, inner, &node));
                    }
                }
            }
            return unique(std::move(issues));
        }

        std::size_t subtree_unmet(const ir::Node& child, const Level& level) const
        {
            if (child.regions.empty())
            {
                return 0;
            }

            auto key = level.prefix;
            key += '|';
            key += child.id;
            if (const auto it = cached_subtree.find(key); it != cached_subtree.end())
            {
                return it->second;
            }

            auto inner = level.props;
            inner.insert(inner.end(), child.provided_props.begin(), child.provided_props.end());
            std::vector<Issue> issues;
            for (const auto& region : child.regions)
            {
                append(issues, collect(region, inner, &child));
            }
            const auto count = unique(std::move(issues)).size();
            cached_subtree.emplace(std::move(key), count);
            return count;
        }

        std::optional<Level> resolve(std::string_view occurrence) const
        {
            if (occurrence == "0")
            {
                return Level{{&model->roots}, root_props, nullptr, {}};
            }

            auto base = root_props;
            const void* scope = nullptr;
            std::string prefix;
            const ir::Node* owner = nullptr;
            Props visible;

            for (std::size_t begin = 0; begin <= occurrence.size();)
            {
                const auto end = std::min(occurrence.find("::", begin), occurrence.size());
                const auto segment = occurrence.substr(begin, end - begin);
                const auto* entry = index.find(scope, segment);
                if (entry == nullptr)
                {
                    return std::nullopt;
                }

                // what the ancestors within this scope expose
                visible = base;
                for (const auto* ancestor = entry->parent; ancestor != nullptr;
                     ancestor = index.find(scope, ancestor->id)->parent)
                {
                    visible.insert(
                        visible.end(),
                        ancestor->provided_props.begin(),
                        ancestor->provided_props.end()
                    );
                }

                if (end < occurrence.size())
                {
                    // a host in the middle of the chain leads into its barrier body
                    if (entry->node->barrier_id == nullptr)
                    {
                        return std::nullopt;
                    }
                    base = visible;
                    scope = entry->node->barrier_id;
                    prefix += segment;
                    prefix += "::";
                }
                else
                {
                    owner = entry->node;
                }
                begin = end + 2;
            }

            if (owner->barrier_id != nullptr)
            {
                const auto* barrier = ir::find_barrier(*model, owner->barrier_id);
                if (barrier == nullptr)
                {
                    return std::nullopt;
                }
                prefix += owner->id;
                prefix += "::";
                // a barrier looks props up through its host, but never reaches direct_parent
                return Level{{&barrier->roots}, std::move(visible), nullptr, std::move(prefix)};
            }

            Level level{{}, std::move(visible), owner, std::move(prefix)};
            level.props.insert(
                level.props.end(),
                owner->provided_props.begin(),
                owner->provided_props.end()
            );
            for (const auto& region : owner->regions)
            {
                level.regions.push_back(&region);
            }
            return level;
        }

        const ir::Model* model;
        Index index;
        Props root_props;
        mutable std::mutex mutex;
        mutable std::optional<std::vector<Issue>> cached_root_issues;
        mutable std::map<std::string, std::size_t> cached_subtree;
    };
}
