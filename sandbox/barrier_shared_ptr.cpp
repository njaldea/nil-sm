// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

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

        using child_api = nil::sm::api::Default<>;

    struct child
    {
        using events = nil::xalt::tlist<finish>;
        using args = nil::xalt::tlist<std::shared_ptr<base_context>>;

        explicit child(std::shared_ptr<base_context>* context_value)
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

    NIL_SM_BARRIER_DECLARE(barrier_state, child_api);

    NIL_SM_BARRIER_DEFINE(barrier_state, child);

    using barrier = nil::sm::barrier::State<nil::sm::Terminate, barrier_state>;

    // Upcasts the shared_ptr<parent_context> root arg once, exposing it to descendants
    // (including the barrier-wrapped child) as shared_ptr<base_context> via get().
    struct root
    {
        using regions = nil::xalt::tlist<barrier>;
        using args = nil::xalt::tlist<std::shared_ptr<parent_context>>;

        std::shared_ptr<base_context> base_ctx;

        explicit root(std::shared_ptr<parent_context>* ctx)
            : base_ctx(*ctx)
        {
        }

        static constexpr auto base_ctx_ptr = &root::base_ctx;
        using props = nil::xalt::tlist<nil::sm::prop<std::shared_ptr<base_context>, base_ctx_ptr>>;
    };
}

int main()
{
    auto context = std::make_shared<parent_context>();
    nil::sm::DefaultSM<root, std::shared_ptr<parent_context>> machine{&context};

    machine.post(finish{});

    assert(context->value == 32);
    std::cout << "barrier shared_ptr adapter: ok\n";
}
