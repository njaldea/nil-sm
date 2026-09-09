#pragma once

#include "../tollbooth/common.hpp"
#include <nil/sm/barrier.hpp>

namespace toll::bslot
{
    NIL_SM_BARRIER_DECLARE(collection_barrier_state, nil::sm::api::Coalesce<tracing_api>::type);
}
