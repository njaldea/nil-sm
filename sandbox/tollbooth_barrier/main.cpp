// See sandbox/README.md. One CLI-driven machine, but each job is a nil::sm::barrier::State
// wrapping a nil::sm::barrier::SM built in its own library, so the job's own template
// instantiation cost is paid once by that library instead of by every consumer - while the
// job's events/defers/emits still broadcast through the whole booth tree like a normal region.

#include "slot.hpp"

int main()
{
    toll::booth_context state_context;
    toll::trace_context api_context;

    nil::sm::
        SM<nil::sm::api::Coalesce<toll::tracing_api>::type, toll::bslot::booth, toll::booth_context>
            machine{&api_context, &state_context};

    return toll::repl::loop(
        state_context,
        api_context,
        "toll booth - barrier adapter",
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
