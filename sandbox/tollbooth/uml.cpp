// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include "jobs.hpp"

#include <nil/sm/uml.hpp>

#include <iostream>

int main()
{
    using machine = nil::sm::SM<nil::sm::api::Coalesce<toll::tracing_api>, toll::booth>;
    nil::sm::puml<machine> diagram;
    std::cout << diagram.root << std::flush;
    return 0;
}
