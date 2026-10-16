#pragma once

#include <nil/sm/ir.hpp>

#include <cstdint>
#include <span>

namespace nil::sm::viz
{
    // Routes are described in viewer.cpp.
    int run(
        const nil::sm::ir::Model& model,
        std::span<const nil::sm::ir::Prop> root_props = {},
        std::uint16_t port = 1101
    );
}
