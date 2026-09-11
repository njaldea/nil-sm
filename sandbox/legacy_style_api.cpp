// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

// Demonstrates a custom API policy that reproduces the old T(Parent*, Context*)
// construction convention (pre-args_t/get() redesign), minus direct typed-parent access:
// the API always resolves a fixed (direct_parent, Context*) pair for every state's
// constructor, regardless of what (if anything) the state itself declares via `args`.

#include <nil/sm.hpp>

#include <cassert>
#include <iostream>

namespace legacy
{
    template <typename Context>
    struct api
    {
        template <typename T>
        struct type
        {
            using state_t = T;
            using api_context_t = void;
            using regions_t = nil::xalt::coalesce_t<T, nil::sm::detail::regions_tag>;
            using events_t = nil::xalt::coalesce_t<T, nil::sm::detail::events_tag>;
            using captures_t = nil::xalt::coalesce_t<T, nil::sm::detail::captures_tag>;
            using props_t = nil::xalt::tlist<>;

            using api_t = nil::sm::api::Default<>::type<T>;

            static constexpr auto args_f()
            {
                if constexpr (requires() { typename T::parent; })
                {
                    return nil::xalt::tlist<nil::sm::direct_parent<typename T::parent>, Context>{};
                }
                else
                {
                    return nil::xalt::tlist<Context>{};
                }
            }

            using args_t = decltype(args_f());

            static state_t make(
                api_context_t* /* api_contexts */,
                nil::sm::Metadata /* metadata */,
                auto*... args
            )
            {
                assert((args != nullptr && ...));
                return T(args...);
            }

            template <typename E>
            static auto on_event(state_t& state, const E& event, api_context_t* api_contexts)
            {
                return api_t::on_event(state, event, api_contexts);
            }

            template <typename E>
            static auto on_capture(state_t& state, const E& event, api_context_t* api_contexts)
            {
                return api_t::on_capture(state, event, api_contexts);
            }

            static auto on_enter(state_t& state, api_context_t* api_contexts)
            {
                return api_t::on_enter(state, api_contexts);
            }

            static auto on_exit(state_t& state, api_context_t* api_contexts)
            {
                return api_t::on_exit(state, api_contexts);
            }

            static auto on_regions_finalized(state_t& state, api_context_t* api_contexts)
            {
                return api_t::on_regions_finalized(state, api_contexts);
            }
        };
    };
}

namespace demo
{
    struct e1
    {
    };

    struct context
    {
        int value = 0;
    };

    struct base
    {
    };

    // Old-style two-arg constructor: (parent, context). `parent` is untyped (void*) since
    // the framework no longer knows the concrete parent state type - cast it yourself if
    // you know what it is.
    struct child
    {
        using events = nil::xalt::tlist<e1>;
        using parent = base;

        explicit child(base* /* parent */, context* context_value)
            : ctx(context_value)
        {
        }

        auto on_event(const e1& /* event */) const -> nil::sm::Discard
        {
            ctx->value++;
            return {};
        }

        context* ctx;
    };

    struct root: base
    {
        using regions = nil::xalt::tlist<child>;

        explicit root(context* /* ctx */)
        {
        }
    };

    template <typename T>
    using legacy_api_t = legacy::api<context>::template type<T>;
}

int main()
{
    demo::context ctx;

    nil::sm::SM<demo::legacy_api_t, demo::root, demo::context> machine{&ctx};

    machine.post(demo::e1{});

    assert(ctx.value == 1);
    std::cout << "legacy style api: ok\n";
}
