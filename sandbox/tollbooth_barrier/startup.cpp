#include "job_impl.hpp"

namespace toll::barrier_jobs
{
    std::unique_ptr<nil::sm::ISM> make_startup(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    )
    {
        return make_job<job::startup_job>(runtime, parent_metadata);
    }

    nil::sm::ir::Model ir_startup(const nil::sm::Metadata* parent_metadata)
    {
        return ir_job<job::startup_job>(parent_metadata);
    }
}
