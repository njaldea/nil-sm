#include "startup.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(
        startup_provider,
        nil::sm::api::Coalesce<tracing_api>::type,
        job::startup_job
    )
}
