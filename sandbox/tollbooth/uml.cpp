#include "jobs.hpp"

#include <nil/sm/uml.hpp>

#include <iostream>

int main()
{
    using machine = nil::sm::SM<nil::sm::api::Coalesce<toll::tracing_api>::type, toll::booth>;
    std::cout << nil::sm::puml<machine>() << std::flush;
    return 0;
}
