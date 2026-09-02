#include "job_impl.hpp"

namespace toll::libs
{
    std::unique_ptr<nil::sm::ISM> make_startup(booth_context* state, trace_context* trace)
    {
        return make_job<job::startup_job>(state, trace);
    }
}
