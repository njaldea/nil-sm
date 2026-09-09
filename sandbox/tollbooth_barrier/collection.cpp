#include "collection.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(collection_barrier_state, job::collection_job);
}
