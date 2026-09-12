// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include "benchmark_sm.hpp"

static_assert(nil::sm::validate<nil::sm::api::Default<>::type, benchmark::root>());

int main()
{
    nil::sm::DefaultSM<benchmark::root> sm;
    benchmark::run_benchmark_cycle(sm);
    return 0;
}
