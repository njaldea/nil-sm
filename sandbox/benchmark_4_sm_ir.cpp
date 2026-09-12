// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include "benchmark_sm.hpp"

#include <nil/sm/ir.hpp>

int main()
{
    nil::sm::DefaultSM<benchmark::root> sm;
    benchmark::run_benchmark_cycle(sm);

    auto model = nil::sm::ir::build<nil::sm::api::Default<>::type, benchmark::root>();
    (void)model;
    return 0;
}
