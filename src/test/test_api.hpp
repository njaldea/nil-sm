// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <nil/sm.hpp>

#include <gmock/gmock.h>

#include <tuple>
#include <type_traits>
#include <utility>

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

// Owns a state together with the mock it reports to.
template <typename Mock, typename State>
struct mocked
{
    template <typename... Args>
    explicit mocked(Mock* init_mock, Args*... args)
        : state{args...}
        , mock(init_mock)
    {
    }

    State state;
    Mock* mock;
};

// Custom API that reports every hook to a mock. The mock is passed to the machine as a root
// argument; every state receives it through an extra arg and keeps it in a wrapper that owns
// the state, so Fin and final states (such as barrier descriptors) are wrapped as well.
// Prop accessors are applied to the wrapped `state` member.
struct TestAPI
{
    using mock_t = testing::StrictMock<APIMock>;

    template <typename State>
    struct state
    {
        static constexpr bool reports = !std::is_same_v<State, nil::sm::Fin>;

        using instance_t = mocked<mock_t, State>;
        using default_state_t = typename nil::sm::api::Default::template state<State>;
        using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
        using events_t = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;
        using captures_t = nil::xalt::coalesce_t<State, nil::sm::detail::captures_tag>;
        using props_t = nil::xalt::coalesce_t<State, nil::sm::detail::props_tag>;
        using args_t = nil::xalt::tlist_join_t<
            nil::xalt::tlist<mock_t>,
            nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>>;

        // The mock is the first arg; the rest are the state's own args.
        template <typename... Args>
        static instance_t make(const nil::sm::Metadata& /* metadata */, mock_t* mock, Args*... args)
        {
            if constexpr (reports)
            {
                mock->on_make_called(nil::xalt::type_id<State>);
            }

            return instance_t(mock, args...);
        }

        static auto on_enter(instance_t& instance)
        {
            if constexpr (reports)
            {
                instance.mock->on_enter_called(nil::xalt::type_id<State>);
            }
            return default_state_t::on_enter(instance.state);
        }

        static auto on_exit(instance_t& instance)
        {
            if constexpr (reports)
            {
                instance.mock->on_exit_called(nil::xalt::type_id<State>);
            }
            return default_state_t::on_exit(instance.state);
        }

        static auto on_regions_finalized(instance_t& instance)
        {
            if constexpr (reports)
            {
                instance.mock->on_regions_finalized_called(nil::xalt::type_id<State>);
            }
            return default_state_t::on_regions_finalized(instance.state);
        }

        template <typename E>
        static auto on_event(instance_t& instance, const E& event)
        {
            if constexpr (reports)
            {
                instance.mock->on_event_called(nil::xalt::type_id<State>, nil::xalt::type_id<E>);
            }
            return default_state_t::on_event(instance.state, event);
        }

        template <typename E>
        static auto on_capture(instance_t& instance, const E& event)
        {
            if constexpr (reports)
            {
                instance.mock->on_capture_called(nil::xalt::type_id<State>, nil::xalt::type_id<E>);
            }
            return default_state_t::on_capture(instance.state, event);
        }
    };
};

// The mock is the first root argument, ahead of the test's own root arguments.
template <typename T, typename... RootArgs>
using TestSM = nil::sm::SM<TestAPI, T, TestAPI::mock_t, RootArgs...>;
