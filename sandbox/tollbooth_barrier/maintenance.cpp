#include "maintenance.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(maintenance_barrier_state, job::maintenance_job);
}
