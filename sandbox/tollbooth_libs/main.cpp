// See sandbox/README.md. No state machine is instantiated here: main.cpp only sees nil::sm::ISM
// and each job's template instantiation is paid by its own library.

#include "job_api.hpp"

#include <iostream>
#include <memory>
#include <string_view>

namespace
{
    std::unique_ptr<nil::sm::ISM> start_job(
        std::string_view name,
        toll::booth_context* state,
        toll::trace_context* trace
    )
    {
        if (name == "startup")
        {
            return toll::libs::make_startup(state, trace);
        }
        if (name == "collection")
        {
            return toll::libs::make_collection(state, trace);
        }
        if (name == "shift")
        {
            return toll::libs::make_shift(state, trace);
        }
        if (name == "maintenance")
        {
            return toll::libs::make_maintenance(state, trace);
        }
        return {};
    }
}

int main()
{
    toll::booth_context state_context;
    toll::trace_context api_context;
    std::unique_ptr<nil::sm::ISM> active;

    return toll::repl::loop(
        state_context,
        api_context,
        "toll booth - per-job libraries",
        [&](std::string_view command, std::string_view argument)
        {
            if (command == "shutdown")
            {
                active.reset();
                state_context.finished = true;
                std::cout << "    session | shutdown\n" << std::flush;
                return true;
            }

            if (command == "job")
            {
                if (active != nullptr)
                {
                    std::cout << "    session | a job is already running\n";
                    return true;
                }
                active = start_job(argument, &state_context, &api_context);
                if (active == nullptr)
                {
                    std::cout << "job: expected startup|collection|shift|maintenance\n";
                }
                std::cout << std::flush;
                return true;
            }

            if (active == nullptr)
            {
                std::cout << "    booth:waiting | idle - pick a job\n";
                return true;
            }

            const auto handled = toll::repl::feed(*active, command, argument);
            if (handled && state_context.job_done)
            {
                active.reset();
                std::cout << "    session | job done, back to idle\n" << std::flush;
            }
            return handled;
        }
    );
}
