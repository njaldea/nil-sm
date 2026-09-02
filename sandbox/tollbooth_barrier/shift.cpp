#include "job_impl.hpp"

namespace toll::barrier_jobs
{
    std::unique_ptr<nil::sm::ISM> make_shift(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    )
    {
        return make_job<job::shift_job>(runtime, parent_metadata);
    }

    nil::sm::ir::Model ir_shift(const nil::sm::Metadata* parent_metadata)
    {
        return ir_job<job::shift_job>(parent_metadata);
    }
}
