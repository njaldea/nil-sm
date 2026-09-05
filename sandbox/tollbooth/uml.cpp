#include "jobs.hpp"

#include <nil/sm/uml.hpp>

#include <iostream>

int main()
{
    using machine = nil::sm::SM<nil::sm::api::Coalesce<toll::tracing_api>::type, toll::booth>;
    nil::sm::puml<machine> diagram;
    std::cout << diagram.root << std::flush;
    return 0;
}
