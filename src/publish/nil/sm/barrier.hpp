// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "ir.hpp" // IWYU pragma: keep
#include "state.hpp"

#include <nil/xalt/MACROS.h>

#include <memory> // IWYU pragma: keep
#include <type_traits>

// Not `final`: to override `name` or add other state properties, inherit from the
// generated struct and declare them in the derived class.
#define NIL_SM_BARRIER_DECLARE_IMPL(NAME, API, DISPLAY_NAME)                                                                  \
    struct NAME                                                                                                               \
    {                                                                                                                         \
        using api = API;                                                                                                      \
        static constexpr auto name = DISPLAY_NAME;                                                                            \
        [[maybe_unused]] static constexpr auto id                                                                             \
            = nil::xalt::type_id<typename API::template api<NAME>>;                                                           \
        static std::unique_ptr<nil::sm::ISM>                                                                                  \
            make(nil::sm::detail::IState*, nil::sm::detail::Queues*, typename API::api_context_t*, const nil::sm::Metadata*); \
        static nil::sm::ir::Model ir(const nil::sm::Metadata*);                                                               \
    }

#define NIL_SM_BARRIER_DECLARE_1(NAME, API) NIL_SM_BARRIER_DECLARE_IMPL(NAME, API, #NAME)
#define NIL_SM_BARRIER_DECLARE_2(NAME, API, DISPLAY_NAME)                                          \
    NIL_SM_BARRIER_DECLARE_IMPL(NAME, API, DISPLAY_NAME)
#define NIL_SM_BARRIER_DECLARE(...)                                                                \
    NIL_XALT_CONCAT(NIL_SM_BARRIER_DECLARE_, NIL_XALT_NARG(__VA_ARGS__))(__VA_ARGS__)

#define NIL_SM_BARRIER_DEFINE(NAME, STATE)                                                         \
    [[maybe_unused]] std::unique_ptr<nil::sm::ISM> NAME::make(                                     \
        nil::sm::detail::IState* parent,                                                           \
        nil::sm::detail::Queues* queues,                                                           \
        typename NAME::api::api_context_t* api_contexts,                                           \
        const nil::sm::Metadata* parent_metadata                                                   \
    )                                                                                              \
    {                                                                                              \
        return std::make_unique<nil::sm::barrier::SM<NAME::api, STATE>>(                           \
            parent,                                                                                \
            queues,                                                                                \
            api_contexts,                                                                          \
            parent_metadata                                                                        \
        );                                                                                         \
    }                                                                                              \
    [[maybe_unused]] nil::sm::ir::Model NAME::ir(const nil::sm::Metadata* parent_metadata)         \
    {                                                                                              \
        return nil::sm::ir::detail::build_unchecked<typename NAME::api, STATE>(parent_metadata);   \
    }

namespace nil::sm::barrier
{
    // Customization point: fully specialize for context pairs the defaults below cannot handle.
    template <typename ParentContext, typename ChildContext>
    struct context_adapter;

    template <typename ParentContext, typename ChildContext>
    concept borrowable_context = std::is_convertible_v<ParentContext*, ChildContext*>;

    template <typename ParentContext, typename ChildContext>
    concept constructible_context = std::is_constructible_v<ChildContext, ParentContext>;

    template <typename ParentContext, typename ChildContext>
        requires borrowable_context<ParentContext, ChildContext>
    void adapt_context(ParentContext* parent_context, ChildContext** out)
    {
        *out = parent_context;
    }

    // Resolved against the overload above and any found through argument-dependent lookup;
    // the child context slot is the output parameter.
    template <typename ParentContext, typename ChildContext>
    concept adapted_context = requires(ParentContext* parent_context, ChildContext** out) {
        adapt_context(parent_context, out);
    };

    template <typename ParentContext, typename ChildContext>
        requires adapted_context<ParentContext, ChildContext>
    struct context_adapter<ParentContext, ChildContext> final
    {
        explicit context_adapter(ParentContext* parent_context)
        {
            adapt_context(parent_context, &child_context);
        }

        ChildContext* context()
        {
            return child_context;
        }

    private:
        ChildContext* child_context = nullptr;
    };

    template <typename ParentContext, typename ChildContext>
        requires(!adapted_context<ParentContext, ChildContext> && constructible_context<ParentContext, ChildContext>)
    struct context_adapter<ParentContext, ChildContext> final
    {
        explicit context_adapter(ParentContext* parent_context)
            : child_context(*parent_context)
        {
        }

        ChildContext* context()
        {
            return &child_context;
        }

    private:
        ChildContext child_context;
    };

    // Borrows the host queues and contexts; the host remains responsible for flushing queues.
    template <typename API, typename T>
    class SM final: public ISM
    {
        using api_context_t = typename API::api_context_t;
        using region_dispatcher_t = detail::region_dispatcher<API, nil::xalt::tlist<T>>;

    public:
        explicit SM(
            detail::IState* init_parent,
            detail::Queues* init_queues,
            api_context_t* init_api_contexts,
            const Metadata* init_parent_metadata = nullptr
        )
            : queues(init_queues)
            , api_contexts(init_api_contexts)
            , region(
                  detail::Region::tag<API, T>{},
                  0,
                  init_parent,
                  queues,
                  api_contexts,
                  init_parent_metadata
              )
        {
        }

        ~SM() noexcept override = default;

        SM(const SM&) = delete;
        SM& operator=(const SM&) = delete;
        SM(SM&&) noexcept = delete;
        SM& operator=(SM&&) noexcept = delete;

        bool is_finalized() const override
        {
            return region.terminated;
        }

    private:
        detail::Queues* queues;
        api_context_t* api_contexts;
        detail::Region region;

        action_t post_impl(detail::Event event) override
        {
            return region.template consume_action<region_dispatcher_t>(
                event,
                region.active_state->on_event(event)
            );
        }
    };

}

namespace nil::sm
{
    template <typename API, typename Action, typename T>
    class State<API, barrier::State<Action, T>> final: public detail::IState
    {
        using child_api_context_t = typename T::api::api_context_t;
        using parent_api_context_t = typename API::api_context_t;
        using api_adapter_t = barrier::context_adapter<parent_api_context_t, child_api_context_t>;

    public:
        explicit State(
            detail::IState* init_parent,
            detail::Queues* init_queues,
            parent_api_context_t* init_api_contexts,
            Metadata init_metadata
        )
            : detail::IState(init_parent, init_metadata)
            , api_adapter(init_api_contexts)
            , queues(init_queues)
            , child(T::make(
                  this,
                  init_queues,
                  this->api_adapter.context(),
                  std::addressof(this->metadata)
              ))
        {
        }

        State(State&&) = delete;
        State(const State&) = delete;
        State& operator=(State&&) = delete;
        State& operator=(const State&) = delete;

        ~State() override = default;

        void* get(const void* requested_id) override
        {
            if (requested_id == nil::xalt::type_id<detail::direct_parent_marker>)
            {
                return nullptr;
            }

            return parent != nullptr ? parent->get(requested_id) : nullptr;
        }

        detail::on_event_t on_event(const detail::Event& e) override
        {
            if (finalized)
            {
                return Unhandled();
            }

            auto result = child->post(e);

            if (child->is_finalized())
            {
                finalized = true;
                return detail::to_runtime_action_as<detail::on_event_t>(Action{});
            }

            return detail::to_runtime_action_as<detail::on_event_t>(result);
        }

    private:
        api_adapter_t api_adapter;
        detail::Queues* queues;
        std::unique_ptr<ISM> child;
        bool finalized = false;
    };
}
