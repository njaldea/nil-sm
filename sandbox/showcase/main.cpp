#include "../viz/viewer.hpp"
#include "showcase.hpp"

#include <nil/sm.hpp>
#include <nil/sm/diagnostics.hpp>

#include <iostream>

namespace viz_demo::showcase
{
    NIL_SM_BARRIER_DEFINE(calibration, cal_probe);
    NIL_SM_BARRIER_DEFINE(diagnostics, diag_start);
}

int main()
{
    const auto valid = nil::sm::validate<viz_demo::showcase::plant, nil::sm::api::Default<>>();
    std::cout << "showcase: " << (valid ? "valid" : "unmet args") << '\n';
    const auto model = nil::sm::ir::build<viz_demo::showcase::plant>();
    return sandbox::viz::run(model);
}
