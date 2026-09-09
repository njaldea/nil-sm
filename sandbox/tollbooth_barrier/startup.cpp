#include "startup.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(startup_barrier_state, job::startup_job);
}
