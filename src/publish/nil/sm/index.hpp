// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "ir.hpp"

#include <string_view>
#include <unordered_map>
#include <vector>

namespace nil::sm
{
    // A flat id -> node table over the nested ir::Model, so a state can be reached, and its
    // ancestors climbed, without walking the tree. Diagnostics uses it to turn an occurrence
    // into a state; a viewer uses it to serve the regions of one state.
    //
    // Ids are unique per scope only: the root tree (nullptr) and each barrier body (its id) are
    // separate scopes, since a body keeps the same ids wherever it is hosted.
    // The entries point into the model, which must outlive the index and stay unchanged.
    class Index final
    {
    public:
        struct Entry
        {
            const ir::Node* node = nullptr;
            // nullptr for a node at the top of its scope
            const ir::Node* parent = nullptr;
        };

        explicit Index(const ir::Model& init_model)
            : model(&init_model)
        {
            add_region(scopes[nullptr], model->roots, nullptr);
            for (const auto& barrier : model->barriers)
            {
                add_region(scopes[barrier.id], barrier.roots, nullptr);
            }
        }

        // nullptr when the scope is neither the root nor a known barrier.
        const std::vector<ir::Node>* roots(const void* scope) const
        {
            if (scope == nullptr)
            {
                return &model->roots;
            }
            const auto* barrier = ir::find_barrier(*model, scope);
            return barrier == nullptr ? nullptr : &barrier->roots;
        }

        const Entry* find(const void* scope, std::string_view id) const
        {
            const auto scope_it = scopes.find(scope);
            if (scope_it == scopes.end())
            {
                return nullptr;
            }
            const auto it = scope_it->second.find(id);
            return it == scope_it->second.end() ? nullptr : &it->second;
        }

    private:
        using Entries = std::unordered_map<std::string_view, Entry>;

        static void add_region(
            Entries& entries,
            const std::vector<ir::Node>& nodes,
            const ir::Node* parent
        )
        {
            for (const auto& node : nodes)
            {
                entries.emplace(std::string_view(node.id), Entry{&node, parent});
                for (const auto& region : node.regions)
                {
                    add_region(entries, region, &node);
                }
            }
        }

        const ir::Model* model;
        std::unordered_map<const void*, Entries> scopes;
    };
}
