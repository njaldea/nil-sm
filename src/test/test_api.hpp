#pragma once

#include <nil/sm.hpp>

#include <gmock/gmock.h>

#include <tuple>

// Convenience using declarations for common template types
using nil::sm::Defer;
using nil::sm::Discard;
using nil::sm::Emit;
using nil::sm::Forward;
using nil::sm::NOOP;
using nil::sm::Terminate;
using nil::sm::Transit;
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
template <typename State, typename StateContexts = nil::xalt::tlist<>>
struct TestAPI;

template <typename State, typename... StateContexts>
struct TestAPI<State, nil::xalt::tlist<StateContexts...>>
{
    using state_t = State;
    using state_context_t = std::tuple<StateContexts*...>;
    using api_context_t = testing::StrictMock<APIMock>;
    using api_t = nil::sm::default_api<State, state_context_t, api_context_t>;
    using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
    using events_t = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;

    // Make the state - delegate to default_api
    template <typename Parent>
    static state_t make(
        Parent* parent,
        state_context_t* state_contexts,
        api_context_t* api_contexts,
        const nil::sm::state_metadata& metadata
    )
    {
        if constexpr (!std::is_same_v<state_t, nil::sm::fin>)
        {
            api_contexts->on_make_called(nil::xalt::type_id<state_t>);
        }
        return api_t::make(parent, state_contexts, api_contexts, metadata);
    }

    // Lifecycle hooks that receive the mock from APIContexts
    static auto on_enter(state_t& state, api_context_t* api_contexts)
    {
        if constexpr (!std::is_same_v<state_t, nil::sm::fin>)
        {
            api_contexts->on_enter_called(nil::xalt::type_id<state_t>);
        }
        return api_t::on_enter(state, api_contexts);
    }

    static auto on_exit(state_t& state, api_context_t* api_contexts)
    {
        if constexpr (!std::is_same_v<state_t, nil::sm::fin>)
        {
            api_contexts->on_exit_called(nil::xalt::type_id<state_t>);
        }
        return api_t::on_exit(state, api_contexts);
    }

    static auto on_regions_finalized(state_t& state, api_context_t* api_contexts)
    {
        if constexpr (!std::is_same_v<state_t, nil::sm::fin>)
        {
            api_contexts->on_regions_finalized_called(nil::xalt::type_id<state_t>);
        }
        return api_t::on_regions_finalized(state, api_contexts);
    }

    template <typename E>
    static auto on_event(state_t& state, const E& event, api_context_t* api_contexts)
    {
        if constexpr (!std::is_same_v<state_t, nil::sm::fin>)
        {
            api_contexts->on_event_called(nil::xalt::type_id<state_t>, nil::xalt::type_id<E>);
        }
        return api_t::on_event(state, event, api_contexts);
    }
};

template <typename... Regions>
using TestSM = nil::sm::SM<TestAPI, Regions...>;
