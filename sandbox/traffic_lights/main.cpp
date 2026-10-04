#include "user.hpp"

#include <iostream>

int main()
{
    using SM = nil::sm::DefaultSM<st_main>;

    std::cout << nil::sm::format::diagram<SM, &nil::sm::format::puml::render>().root << "\n";

    SM traffic_light;

    std::cout << "Starting at red\n";
    for (auto i = 0; i < 10; i++)
    {
        traffic_light.post<ev_tick>();
    }
}

// https://godbolt.org/z/cP6GKf4nY
