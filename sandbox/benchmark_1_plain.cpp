// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include "benchmark_sm.hpp"

int main()
{
    nil::sm::DefaultSM<benchmark::root> sm;
    benchmark::run_benchmark_cycle(sm);
    return 0;
}
