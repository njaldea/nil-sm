#pragma once

#include "../tollbooth/common.hpp"

#include <nil/sm.hpp>

#include <memory>

// Each factory is compiled in its own translation unit, so a job's template instantiation cost
// is paid once by its own library instead of by every consumer.
namespace toll::libs
{
    std::unique_ptr<nil::sm::ISM> make_startup(booth_context* state, trace_context* trace);
    std::unique_ptr<nil::sm::ISM> make_collection(booth_context* state, trace_context* trace);
    std::unique_ptr<nil::sm::ISM> make_shift(booth_context* state, trace_context* trace);
    std::unique_ptr<nil::sm::ISM> make_maintenance(booth_context* state, trace_context* trace);
}
