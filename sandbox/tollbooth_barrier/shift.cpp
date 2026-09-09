#include "shift.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(shift_barrier_state, job::shift_job);
}
