// PlantUML for the barrier-based toll booth. Each job's Provider exposes a static ir(),
// so its graph is spliced in as a nested region even though its concrete API/root types live
// in a separate library.

#include "slot.hpp"

#include <nil/sm/uml.hpp>

#include <iostream>

int main()
{
    using machine
        = nil::sm::SM<nil::sm::api::Coalesce<toll::tracing_api>::type, toll::bslot::booth>;
    std::cout << nil::sm::puml<machine>() << std::flush;
    return 0;
}
