// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm/format/index.hpp>

#include <string_view>
#include <unordered_map>
#include <vector>

namespace nil::sm
{
    using Entries = std::unordered_map<std::string_view, Index::Entry>;

    namespace
    {
        void add_region(Entries& entries, const std::vector<ir::Node>& nodes, const ir::Node* parent)
        {
            for (const auto& node : nodes)
            {
                entries.emplace(std::string_view(node.id), Index::Entry{&node, parent});
                for (const auto& region : node.regions)
                {
                    add_region(entries, region, &node);
                }
            }
        }
    }

    struct Index::Impl
    {
        explicit Impl(const ir::Model& init_model)
            : model(&init_model)
        {
            add_region(scopes[nullptr], model->roots, nullptr);
            for (const auto& barrier : model->barriers)
            {
                add_region(scopes[barrier.id], barrier.roots, nullptr);
            }
        }

        const ir::Model* model;
        std::unordered_map<const void*, Entries> scopes;
    };

    Index::Index(const ir::Model& init_model)
        : impl(std::make_unique<Impl>(init_model))
    {
    }

    Index::~Index() = default;

    const std::vector<ir::Node>* Index::roots(const void* scope) const
    {
        if (scope == nullptr)
        {
            return &impl->model->roots;
        }
        const auto* barrier = ir::find_barrier(*impl->model, scope);
        return barrier == nullptr ? nullptr : &barrier->roots;
    }

    const Index::Entry* Index::find(const void* scope, std::string_view id) const
    {
        const auto scope_it = impl->scopes.find(scope);
        if (scope_it == impl->scopes.end())
        {
            return nullptr;
        }
        const auto it = scope_it->second.find(id);
        return it == scope_it->second.end() ? nullptr : &it->second;
    }
}
