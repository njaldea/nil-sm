#include <nil/sm/barrier.hpp>
#include <nil/sm/uml.hpp>

#include <iostream>

namespace demo
{
    struct A
    {
    };

    struct child_idle
    {
        using events = nil::xalt::tlist<A>;

        static auto on_event(const A& /* event */)
        {
            return nil::sm::Terminate();
        }
    };

    template <typename T>
    using child_api = nil::sm::api::Default<>::type<T>;

    NIL_SM_BARRIER_DECLARE(shared_provider, child_api);
    NIL_SM_BARRIER_DEFINE(shared_provider, child_api, child_idle);

    using shared_barrier = nil::sm::barrier::State<nil::sm::Terminate, shared_provider>;

    struct first_parent
    {
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct second_parent
    {
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct root
    {
        using regions = nil::xalt::tlist<first_parent, second_parent>;
    };
}

int main()
{
    using machine = nil::sm::DefaultSM<demo::root>;
    nil::sm::puml<machine> diagram;

    std::cout << diagram.root;
    for (const auto& provider : diagram.providers)
    {
        std::cout << "\n' ===== " << provider.name() << " =====\n";
        std::cout << provider;
    }
    std::cout << std::flush;
    return 0;
}
