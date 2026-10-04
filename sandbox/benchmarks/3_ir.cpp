// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include "benchmark_sm.hpp"

#include <nil/sm/ir.hpp>

int main()
{
    auto model = nil::sm::ir::build<nil::sm::api::Default<>, benchmark::root>();
    (void)model;
    return 0;
}
