#include "maintenance.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(
        maintenance_provider,
        nil::sm::api::Coalesce<tracing_api>::type,
        job::maintenance_job
    )
}
