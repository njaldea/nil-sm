#pragma once

#include "job_api.hpp"

#include "../tollbooth/jobs.hpp"

#include <nil/sm/barrier.hpp>
#include <nil/sm/formatter/detail.hpp>

#include <memory>

namespace toll::barrier_jobs
{
    // Job (e.g. job::startup_job) already returns Terminate from its own on_regions_finalized,
    // same as it does directly inside session's region in the basic (non-barrier) assembly. Used
    // as the barrier::SM's root directly - no run-once wrapper - so is_finalized() reflects the
    // job's own termination and, combined with build_node's flattening, the rendered diagram is
    // identical to the basic assembly's; only the underlying barrier::State type differs.
    template <typename Job>
    std::unique_ptr<nil::sm::ISM> make_job(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    )
    {
        using machine = nil::sm::barrier::SM<nil::sm::api::Coalesce<tracing_api>::type, Job>;
        return std::make_unique<machine>(runtime, parent_metadata);
    }

    template <typename Job>
    nil::sm::ir::Model ir_job(const nil::sm::Metadata* parent_metadata)
    {
        return nil::sm::ir::build<nil::sm::api::Coalesce<tracing_api>::type, Job>(parent_metadata);
    }
}
