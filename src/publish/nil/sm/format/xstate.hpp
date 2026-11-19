// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../ir.hpp"
#include "diagram.hpp"

#include <ostream>
#include <span>

namespace nil::sm::format::xstate
{
    // Defined in the nil-sm-format library.
    void render(std::ostream& os, std::span<const ir::Node> roots);
}

namespace nil::sm
{
    template <typename SM>
    using xstate = format::diagram<SM, &format::xstate::render>;
}
