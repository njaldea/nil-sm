// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <nil/sm/ir.hpp>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>

namespace nil::sm::format
{
    std::ostream& indent(std::ostream& os, std::size_t depth);

    // "Emit(Name)" when the emitted event is known, otherwise the plain label.
    std::string with_emitted(std::string_view label, const std::string& emitted);

    std::string_view action_name(ir::response::EEntry response);
    std::string_view action_name(ir::response::EExit response);
    std::string_view action_name(ir::response::ERegionsFinalized response);
    std::string_view action_name(ir::response::EEvent response);
}
