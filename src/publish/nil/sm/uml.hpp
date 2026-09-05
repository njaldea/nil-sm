#pragma once

#include "format/diagram.hpp" // IWYU pragma: export
#include "format/dot.hpp"     // IWYU pragma: export
#include "format/mermaid.hpp" // IWYU pragma: export
#include "format/puml.hpp"    // IWYU pragma: export
#include "format/scxml.hpp"   // IWYU pragma: export
#include "format/xstate.hpp"  // IWYU pragma: export

namespace nil::sm
{
    template <typename SM>
    using puml_diagram = format::diagram<SM, &format::puml::render>;

    template <typename SM>
    using mermaid_diagram = format::diagram<SM, &format::mermaid::render>;

    template <typename SM>
    using dot_diagram = format::diagram<SM, &format::dot::render>;

    template <typename SM>
    using scxml_diagram = format::diagram<SM, &format::scxml::render>;

    template <typename SM>
    using xstate_diagram = format::diagram<SM, &format::xstate::render>;
}
