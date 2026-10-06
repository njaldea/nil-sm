#include "../tollbooth/jobs.hpp"

#include <nil/sm.hpp>

// Own translation unit: the full toll booth instantiation is by far the heaviest one.
nil::sm::ir::Model viz_build_tollbooth()
{
    return nil::sm::ir::build<nil::sm::api::Coalesce<toll::tracing_api>, toll::booth>();
}
