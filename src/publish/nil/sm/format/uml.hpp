// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "diagram.hpp" // IWYU pragma: export
#include "dot.hpp"     // IWYU pragma: export
#include "json.hpp"    // IWYU pragma: export
#include "mermaid.hpp" // IWYU pragma: export
#include "puml.hpp"    // IWYU pragma: export
#include "scxml.hpp"   // IWYU pragma: export
#include "xstate.hpp"  // IWYU pragma: export

namespace nil::sm
{
    template <typename SM>
    using puml_diagram = format::diagram<SM, &format::puml::render>;

    template <typename SM>
    using mermaid_diagram = format::diagram<SM, &format::mermaid::render>;

    template <typename SM>
    using dot_diagram = format::diagram<SM, &format::dot::render>;

    template <typename SM>
    using json_diagram = format::diagram<SM, &format::json::render>;

    template <typename SM>
    using scxml_diagram = format::diagram<SM, &format::scxml::render>;

    template <typename SM>
    using xstate_diagram = format::diagram<SM, &format::xstate::render>;
}
