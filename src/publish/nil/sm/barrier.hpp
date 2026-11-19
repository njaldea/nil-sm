// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "ir.hpp" // IWYU pragma: keep
#include "state.hpp"

#include <nil/xalt/MACROS.h>

#include <memory> // IWYU pragma: keep

// Not `final`: to override `name` or add other state properties, inherit from the
// generated struct and declare them in the derived class.
#define NIL_SM_BARRIER_DECLARE_IMPL(NAME, API, DISPLAY_NAME)                                       \
    struct NAME                                                                                    \
    {                                                                                              \
        using api = API;                                                                           \
        static constexpr auto name = DISPLAY_NAME;                                                 \
        [[maybe_unused]] static constexpr auto id                                                  \
            = nil::xalt::type_id<typename API::template state<NAME>>;                              \
        static std::unique_ptr<nil::sm::ISM>                                                       \
            make(nil::sm::detail::IState*, nil::sm::detail::Queues*, const nil::sm::Metadata*);    \
        static nil::sm::ir::Model ir(const nil::sm::Metadata*);                                    \
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
        const nil::sm::Metadata* parent_metadata                                                   \
    )                                                                                              \
    {                                                                                              \
        return std::make_unique<nil::sm::barrier::SM<NAME::api, STATE>>(                           \
            parent,                                                                                \
            queues,                                                                                \
            parent_metadata                                                                        \
        );                                                                                         \
    }                                                                                              \
    [[maybe_unused]] nil::sm::ir::Model NAME::ir(const nil::sm::Metadata* parent_metadata)         \
    {                                                                                              \
        return nil::sm::ir::detail::build_unchecked<STATE, typename NAME::api>(parent_metadata);   \
    }

namespace nil::sm::barrier
{
    // Borrows the host queues; the host remains responsible for flushing queues.
    template <typename API, typename T>
    class SM final: public ISM
    {
        using region_dispatcher_t = detail::region_dispatcher<API, nil::xalt::tlist<T>>;

    public:
        explicit SM(
            detail::IState* init_parent,
            detail::Queues* init_queues,
            const Metadata* init_parent_metadata = nullptr
        )
            : queues(init_queues)
            , region(detail::Region::tag<API, T>{}, 0, init_parent, queues, init_parent_metadata)
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
        detail::Region region;

        action_t post_impl(Event event) override
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
    public:
        explicit State(
            detail::IState* init_parent,
            detail::Queues* init_queues,
            Metadata init_metadata
        )
            : detail::IState(init_parent, init_metadata)
            , queues(init_queues)
            , child(T::make(this, init_queues, std::addressof(this->metadata)))
        {
        }

        State(State&&) = delete;
        State(const State&) = delete;
        State& operator=(State&&) = delete;
        State& operator=(const State&) = delete;

        ~State() override = default;

        void* get(const void* requested_id) override
        {
            return parent != nullptr ? parent->get(requested_id) : nullptr;
        }

        // The barrier has no props, so direct_parent from its first states asks its parent.
        void* get_own(const void* requested_id) override
        {
            return parent != nullptr ? parent->get_own(requested_id) : nullptr;
        }

        detail::on_event_t on_event(const Event& e) override
        {
            if (finalized)
            {
                return detail::on_event_t{Unhandled()};
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
        detail::Queues* queues;
        std::unique_ptr<ISM> child;
        bool finalized = false;
    };
}
