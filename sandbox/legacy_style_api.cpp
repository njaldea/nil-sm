// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

// Demonstrates a custom API policy that reproduces the old T(Parent*, Context*)
// construction convention (pre-args_t/get() redesign), minus direct typed-parent access:
// the API always resolves a fixed (direct_parent, Context*) pair for every state's
// constructor, regardless of what (if anything) the state itself declares via `args`.

#include <nil/sm.hpp>
#include <nil/sm/diagnostics.hpp>
#include <nil/sm/format/puml.hpp>

#include <nil/xalt/fn_make.hpp>

#include <cassert>
#include <iostream>

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

    template <typename T>
    struct SEvent
    {
        explicit SEvent(auto&&... args)
            : event(std::make_shared<T>(std::forward<decltype(args)>(args)...))
        {
        }

        const T& get() const
        {
            return *event;
        }

        std::shared_ptr<const T> event;
    };

    template <typename T>
        requires(std::is_copy_constructible_v<T>)
    struct SEvent<T>
    {
        explicit SEvent(auto&&... args)
            : event(std::forward<decltype(args)>(args)...)
        {
        }

        const T& get() const
        {
            return event;
        }

        T event;
    };

    template <typename T>
    using SEmit = nil::sm::Emit<SEvent<T>>;
}

namespace legacy
{
    struct fallback_parent
    {
    };

    struct legacy_api
    {
        using api_context_t = void;

        template <typename T>
        struct api
        {
            using regions_t = nil::xalt::coalesce_t<T, nil::sm::detail::regions_tag>;
            using events_t = nil::xalt::coalesce_t<T, nil::sm::detail::events_tag>::template apply<
                demo::SEvent>;
            using captures_t = nil::xalt::coalesce_t<T, nil::sm::detail::captures_tag>;
            using props_t = nil::xalt::tlist<>;

            using api_t = typename nil::sm::api::Default<api_context_t>::template api<T>;

            static constexpr auto args_f()
            {
                if constexpr (requires() { typename T::parent; })
                {
                    return nil::xalt::tlist<
                        nil::sm::direct_parent<typename T::parent>,
                        std::shared_ptr<demo::context>>{};
                }
                else
                {
                    return nil::xalt::tlist<fallback_parent, std::shared_ptr<demo::context>>{};
                }
            }

            using args_t = decltype(args_f());

            static T make(
                api_context_t* /* api_contexts */,
                const nil::sm::Metadata& /* metadata */,
                auto* parent,
                auto* context
            )
            {
                return nil::xalt::fn_make<T>(*parent, *context);
            }

            template <typename E>
            static auto on_event(T& state, const E& event, api_context_t* api_contexts)
            {
                return api_t::on_event(state, event.get(), api_contexts);
            }

            template <typename E>
            static auto on_capture(T& state, const E& event, api_context_t* api_contexts)
            {
                return api_t::on_capture(state, event.get(), api_contexts);
            }

            static auto on_enter(T& state, api_context_t* api_contexts)
            {
                return api_t::on_enter(state, api_contexts);
            }

            static auto on_exit(T& state, api_context_t* api_contexts)
            {
                return api_t::on_exit(state, api_contexts);
            }

            static auto on_regions_finalized(T& state, api_context_t* api_contexts)
            {
                return api_t::on_regions_finalized(state, api_contexts);
            }
        };
    };
}

namespace demo
{
    // Old-style two-arg constructor: (parent, context). `parent` is untyped (void*) since
    // the framework no longer knows the concrete parent state type - cast it yourself if
    // you know what it is.
    struct child
    {
        using events = nil::xalt::tlist<e1>;
        using parent = base;

        explicit child(auto& /* parent */, std::shared_ptr<context> context_value)
            : ctx(std::move(context_value))
        {
        }

        template <typename E>
        void foo(const E&) const;

        auto on_event(const e1& /* event */) const
        {
            ctx->value++;
            return nil::sm::Discard();
        }

        std::shared_ptr<context> ctx;
    };

    struct root: base
    {
        using regions = nil::xalt::tlist<child>;

        explicit root(const std::shared_ptr<context>& /* ctx */)
        {
        }
    };
}

int main()
{
    std::shared_ptr<demo::context> ctx = std::make_shared<demo::context>();
    legacy::fallback_parent fp{};

    using LegacySM = nil::sm::
        SM<legacy::legacy_api, demo::root, legacy::fallback_parent, std::shared_ptr<demo::context>>;

    LegacySM machine{&fp, &ctx};

    nil::sm::puml<LegacySM> diagram;

    std::cout << diagram.root;
    machine.post(demo::e1{});

    assert(ctx->value == 1);
    std::cout << "legacy style api: ok\n";
}
