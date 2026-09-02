#include "job_impl.hpp"

namespace toll::barrier_jobs
{
    std::unique_ptr<nil::sm::ISM> make_collection(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    )
    {
        return make_job<job::collection_job>(runtime, parent_metadata);
    }

    nil::sm::ir::Model ir_collection(const nil::sm::Metadata* parent_metadata)
    {
        return ir_job<job::collection_job>(parent_metadata);
    }
}
