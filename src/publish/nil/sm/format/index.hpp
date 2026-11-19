// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../ir.hpp"

#include <memory>
#include <string_view>
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
    // Defined in the nil-sm-format library.
    class Index final
    {
    public:
        struct Entry
        {
            const ir::Node* node = nullptr;
            // nullptr for a node at the top of its scope
            const ir::Node* parent = nullptr;
        };

        explicit Index(const ir::Model& init_model);
        ~Index();

        Index(const Index&) = delete;
        Index& operator=(const Index&) = delete;
        Index(Index&&) = delete;
        Index& operator=(Index&&) = delete;

        // nullptr when the scope is neither the root nor a known barrier.
        const std::vector<ir::Node>* roots(const void* scope) const;

        const Entry* find(const void* scope, std::string_view id) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
