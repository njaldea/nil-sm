// See sandbox/README.md. One machine driven by one CLI, but each job is an opaque child machine
// behind job_slot, so the job template instantiations are paid by their own libraries.

#include "slot.hpp"

int main()
{
    toll::booth_context state_context;
    toll::trace_context api_context;
    state_context.trace = &api_context;

    nil::sm::SM<nil::sm::api::Coalesce<toll::tracing_api>::type, toll::slot::booth> machine{
        &state_context,
        &api_context
    };

    return toll::repl::loop(
        state_context,
        api_context,
        "toll booth - job_slot adapter",
        [&](std::string_view command, std::string_view argument)
        {
            if (command == "shutdown")
            {
                machine.post<toll::ev::shutdown>();
                std::cout << std::flush;
                return true;
            }
            if (command == "job")
            {
                if (argument == "startup")
                {
                    machine.post<toll::ev::select_startup>();
                }
                else if (argument == "collection")
                {
                    machine.post<toll::ev::select_collection>();
                }
                else if (argument == "shift")
                {
                    machine.post<toll::ev::select_shift>();
                }
                else if (argument == "maintenance")
                {
                    machine.post<toll::ev::select_maintenance>();
                }
                else
                {
                    std::cout << "job: expected startup|collection|shift|maintenance\n";
                }
                std::cout << std::flush;
                return true;
            }
            return toll::repl::feed(machine, command, argument);
        }
    );
}
