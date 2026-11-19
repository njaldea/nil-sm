// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../ir.hpp"
#include "../validate.hpp"
#include "index.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace nil::sm
{
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

    namespace detail
    {
        class DiagnosticsImpl;
    }

    // The details behind validate(), reported one level at a time. Holds a reference to `model`.
    // Defined in the nil-sm-format library.
    class Diagnostics final
    {
    public:
        Diagnostics(const ir::Model& init_model, std::vector<ir::Prop> init_root_props);
        ~Diagnostics();

        Diagnostics(const Diagnostics&) = delete;
        Diagnostics& operator=(const Diagnostics&) = delete;
        Diagnostics(Diagnostics&&) = delete;
        Diagnostics& operator=(Diagnostics&&) = delete;

        const Index& lookup() const;

        // Reports for every state in the regions of the state at `occurrence`. An occurrence is
        // the state ids joined by "::", each barrier host followed by a state of its body, because
        // a body is judged by where it is hosted. "0" is the root. nullopt when it names no state.
        std::optional<std::vector<ChildReport>> children(std::string_view occurrence) const;

        // What the whole root tree leaves unmet; direct_parent issues are reported by their states.
        const std::vector<Issue>& root_issues() const;

        // What a barrier body leaves for its host to provide. nullopt for an unknown barrier.
        std::optional<std::vector<Issue>> barrier_requirements(const void* barrier_id) const;

    private:
        std::unique_ptr<detail::DiagnosticsImpl> impl;
    };
}
