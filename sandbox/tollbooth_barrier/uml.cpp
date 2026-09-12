// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

// PlantUML for the barrier-based toll booth. Each job's barrier state exposes a static ir(),
// so its graph is spliced in as a nested region even though its concrete API/root types live
// in a separate library.

#include "slot.hpp"

#include <nil/sm/uml.hpp>

#include <iostream>

int main()
{
    using machine = nil::sm::SM<nil::sm::api::Coalesce<toll::tracing_api>, toll::bslot::booth>;
    nil::sm::puml<machine> diagram;
    std::cout << diagram.root << std::flush;

    for (const auto& child : diagram.barriers)
    {
        std::cout << child << std::flush;
    }
    return 0;
}
