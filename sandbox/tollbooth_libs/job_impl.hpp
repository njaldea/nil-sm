#pragma once

#include "job_api.hpp"

#include "../tollbooth/jobs.hpp"

#include <nil/sm.hpp>

#include <memory>

namespace toll::libs
{
    template <typename Job>
    std::unique_ptr<nil::sm::ISM> make_job(booth_context* state, trace_context* trace)
    {
        using machine = nil::sm::SM<nil::sm::api::Coalesce<tracing_api>::type, generic::once<Job>>;
        state->job_done = false;
        return std::make_unique<machine>(state, trace);
    }
}
