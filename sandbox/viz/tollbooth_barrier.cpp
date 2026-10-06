#include "../tollbooth_barrier/slot.hpp"

#include <nil/sm.hpp>

// The job barriers are defined in the tollbooth_barrier_job_* libraries.
nil::sm::ir::Model viz_build_tollbooth_barrier()
{
    return nil::sm::ir::build<nil::sm::api::Coalesce<toll::tracing_api>, toll::bslot::booth>();
}
