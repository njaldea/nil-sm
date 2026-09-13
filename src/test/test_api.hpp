// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <nil/sm.hpp>

#include <gmock/gmock.h>

// Convenience using declarations for common template types
using nil::sm::Defer;
using nil::sm::Discard;
using nil::sm::Emit;
using nil::sm::Forward;
using nil::sm::NOOP;
using nil::sm::Terminate;
using nil::sm::TransitTo;
using nil::xalt::tlist;
using nil::xalt::type_id;

// Events
template <int N>
struct ev
{
};

using e1 = ev<1>;
using e2 = ev<2>;
using e3 = ev<3>;
using e4 = ev<4>;

// Mock interface for state lifecycle and reactions
class APIMock
{
public:
    MOCK_METHOD(void, on_enter_called, (const void* state_id), ());
    MOCK_METHOD(void, on_exit_called, (const void* state_id), ());
    MOCK_METHOD(void, on_make_called, (const void* state_id), ());
    MOCK_METHOD(void, on_event_called, (const void* state_id, const void* event_id), ());
    MOCK_METHOD(void, on_capture_called, (const void* state_id, const void* event_id), ());
    MOCK_METHOD(void, on_regions_finalized_called, (const void* state_id), ());
};

// Mock for tracking state event processing
class StateMock
{
public:
    MOCK_METHOD(void, on_state_event, (const void* state_id, const void* event_id), ());
};

// Custom API template that receives mock through APIContexts
// This allows lifecycle hooks (on_enter, on_exit, etc.) to call mock methods
struct TestAPI
{
    using context_t = testing::StrictMock<APIMock>;

    template <typename State>
    struct state
    {
        using state_t = State;
        using default_state_t = typename nil::sm::api::Default<context_t>::template state<State>;
        using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
        using events_t = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;
        using captures_t = nil::xalt::coalesce_t<State, nil::sm::detail::captures_tag>;
        using args_t = nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>;
        using props_t = nil::xalt::coalesce_t<State, nil::sm::detail::props_tag>;

        // Make the state - delegate to api::Default
        template <typename... Args>
        static state_t make(context_t* contexts, const nil::sm::Metadata& metadata, Args*... args)
        {
            if constexpr (!std::is_same_v<state_t, nil::sm::Fin>)
            {
                contexts->on_make_called(nil::xalt::type_id<state_t>);
            }
            return default_state_t::make(contexts, metadata, args...);
        }

        // Lifecycle hooks that receive the mock from APIContexts
        static auto on_enter(state_t& state, context_t* contexts)
        {
            if constexpr (!std::is_same_v<state_t, nil::sm::Fin>)
            {
                contexts->on_enter_called(nil::xalt::type_id<state_t>);
            }
            return default_state_t::on_enter(state, contexts);
        }

        static auto on_exit(state_t& state, context_t* contexts)
        {
            if constexpr (!std::is_same_v<state_t, nil::sm::Fin>)
            {
                contexts->on_exit_called(nil::xalt::type_id<state_t>);
            }
            return default_state_t::on_exit(state, contexts);
        }

        static auto on_regions_finalized(state_t& state, context_t* contexts)
        {
            if constexpr (!std::is_same_v<state_t, nil::sm::Fin>)
            {
                contexts->on_regions_finalized_called(nil::xalt::type_id<state_t>);
            }
            return default_state_t::on_regions_finalized(state, contexts);
        }

        template <typename E>
        static auto on_event(state_t& state, const E& event, context_t* contexts)
        {
            if constexpr (!std::is_same_v<state_t, nil::sm::Fin>)
            {
                contexts->on_event_called(nil::xalt::type_id<state_t>, nil::xalt::type_id<E>);
            }
            return default_state_t::on_event(state, event, contexts);
        }

        template <typename E>
        static auto on_capture(state_t& state, const E& event, context_t* contexts)
        {
            if constexpr (!std::is_same_v<state_t, nil::sm::Fin>)
            {
                contexts->on_capture_called(nil::xalt::type_id<state_t>, nil::xalt::type_id<E>);
            }
            return default_state_t::on_capture(state, event, contexts);
        }
    };
};

template <typename T, typename... RootArgs>
using TestSM = nil::sm::SM<TestAPI, T, RootArgs...>;
