#include "shift.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(shift_provider, nil::sm::api::Coalesce<tracing_api>::type, job::shift_job)
}
