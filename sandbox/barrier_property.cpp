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

    struct parent_context
    {
        child_context child;
    };

    template <typename T>
    using parent_api = nil::sm::api::Default<parent_context, void>::type<T>;

    template <typename T>
    using child_api = nil::sm::api::Default<child_context, void>::type<T>;

    struct child
    {
        using events = nil::xalt::tlist<finish>;

        explicit child(auto* /* parent */, child_context* context_value)
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

    void adapt_context(parent_context* parent, child_context** out)
    {
        *out = &parent->child;
    }

    using barrier = nil::sm::barrier::State<nil::sm::Terminate, provider>;

    struct root
    {
        using regions = nil::xalt::tlist<barrier>;
    };
}

int main()
{
    int result = 23;
    demo::parent_context context{.child = {.result = &result}};
    nil::sm::SM<demo::parent_api, demo::root> machine{&context, nullptr};

    machine.post(demo::finish{});

    assert(result == 24);
    std::cout << "barrier property adapter: ok\n";
}
