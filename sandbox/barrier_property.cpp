#include <nil/sm.hpp>
#include <nil/sm/barrier.hpp>

#include <cassert>
#include <iostream>

namespace demo
{
    struct finish
    {
    };

    struct child_context
    {
        int value = 23;
        int* result = nullptr;
    };

    template <typename T>
    using child_api = nil::sm::api::Default<>::type<T>;

    struct child
    {
        using events = nil::xalt::tlist<finish>;
        using args = nil::xalt::tlist<child_context>;

        explicit child(child_context* context_value)
            : context(context_value)
        {
            assert(context->value == 23);
        }

        auto on_event(const finish& event) const -> nil::sm::Terminate
        {
            (void)event;
            context->value++;
            *context->result = context->value;
            return {};
        }

        child_context* context;
    };

    NIL_SM_BARRIER_DECLARE(provider, child_api);

    NIL_SM_BARRIER_DEFINE(provider, child_api, child);

    using barrier = nil::sm::barrier::State<nil::sm::Terminate, provider>;

    // Exposes child_context to descendants (including the barrier-wrapped child) via get().
    struct root
    {
        using regions = nil::xalt::tlist<barrier>;
        using args = nil::xalt::tlist<int>;

        child_context child;

        explicit root(int* result)
            : child{.result = result}
        {
        }

        static constexpr auto child_ptr = &root::child;
        using provides = nil::xalt::tlist<nil::sm::provide<child_context, child_ptr>>;
    };
}

int main()
{
    int result = 23;
    nil::sm::DefaultSM<demo::root, int> machine{&result};

    machine.post(demo::finish{});

    assert(result == 24);
    std::cout << "barrier property adapter: ok\n";
}
