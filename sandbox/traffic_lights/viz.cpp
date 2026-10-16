#include "user.hpp"
#include <nil/sm/viz.hpp>

int main()
{
    return nil::sm::viz::run(nil::sm::ir::build<st_main>(), {}, 1102);
}
