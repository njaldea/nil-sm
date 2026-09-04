#include <nil/sm.hpp>
#include <nil/sm/barrier.hpp>

#include <cassert>
#include <iostream>
#include <memory>

namespace
{
    struct finish
    {
    };

    struct base_context
    {
        base_context() = default;
        base_context(const base_context&) = default;
        base_context& operator=(const base_context&) = default;
        base_context(base_context&&) noexcept = default;
        base_context& operator=(base_context&&) noexcept = default;
        virtual ~base_context() = default;
        int value = 0;
    };

    struct parent_context: base_context
    {
        parent_context()
        {
            value = 31;
        }
    };

    struct child_context
    {
        int value = 0;
    };

    template <typename T>
    using parent_api = nil::sm::api::Default<std::shared_ptr<parent_context>, void>::type<T>;

    template <typename T>
    using child_api = nil::sm::api::Default<std::shared_ptr<base_context>, void>::type<T>;

    struct child
    {
        using events = nil::xalt::tlist<finish>;

        explicit child(auto* /* parent */, std::shared_ptr<base_context>* context_value)
            : context(*context_value)
        {
            assert(context->value == 31);
        }

        auto on_event(const finish& event) const -> nil::sm::Terminate
        {
            (void)event;
            context->value++;
            return {};
        }

        std::shared_ptr<base_context> context;
    };

    NIL_SM_BARRIER_DECLARE(provider, child_api);
    NIL_SM_BARRIER_DEFINE(provider, child_api, child);

    using barrier = nil::sm::barrier::State<nil::sm::Terminate, provider>;

    struct root
    {
        using regions = nil::xalt::tlist<barrier>;
    };
}

int main()
{
    auto context = std::make_shared<parent_context>();
    nil::sm::SM<parent_api, root> machine{&context, nullptr};

    machine.post(finish{});

    assert(context->value == 32);
    std::cout << "barrier shared_ptr adapter: ok\n";
}
