#include "../viz/viewer.hpp"
#include "jobs.hpp"

int main()
{
    using Api = nil::sm::api::Coalesce<toll::tracing_api>;
    return sandbox::viz::run(
        nil::sm::ir::build<toll::booth, Api>(),
        nil::sm::ir::make_root_props<toll::booth_context>(),
        1103
    );
}
