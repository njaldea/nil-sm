#include "../viz/viewer.hpp"
#include "user.hpp"

int main()
{
    return sandbox::viz::run(nil::sm::ir::build<st_main>(), {}, 1102);
}
