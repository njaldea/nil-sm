#include "collection.hpp"

#include "../tollbooth/jobs.hpp"

namespace toll::bslot
{
    NIL_SM_BARRIER_DEFINE(
        collection_provider,
        nil::sm::api::Coalesce<tracing_api>::type,
        job::collection_job
    )
}
