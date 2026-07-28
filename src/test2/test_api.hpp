#pragma once

#include <nil/sml.hpp>
#include <gmock/gmock.h>
#include <variant>

namespace sml_test2
{
    // Events
    struct e1
    {
    };

    struct e2
    {
    };

    // Mock interface for state lifecycle and reactions
    class StateMock
    {
    public:
        virtual ~StateMock() = default;

        MOCK_METHOD(void, on_enter_called, (const void* state_id), ());
        MOCK_METHOD(void, on_exit_called, (const void* state_id), ());
        MOCK_METHOD(void, on_make_called, (const void* state_id), ());
        MOCK_METHOD(void, on_event_called, (const void* state_id, const void* event_id), ());
        MOCK_METHOD(void, on_regions_complete_called, (const void* state_id), ());
    };

    // Custom API template that receives mock through APIContexts
    // This allows lifecycle hooks (on_enter, on_exit, etc.) to call mock methods
    template <typename State>
    struct TestAPI
    {
        using state_t = State;
        using regions_t = nil::xalt::coalesce_t<State, nil::sml::detail::regions_tag>;
        using events_t = nil::xalt::coalesce_t<State, nil::sml::detail::events_tag>;

        // Make the state without mock - delegate to default_api
        template <typename Parent, typename StateCtxs, typename APICtxs>
        static state_t make(Parent* parent, StateCtxs* state_contexts, APICtxs* api_contexts)
        {
            if constexpr (!nil::xalt::is_of_template_v<state_t, nil::sml::detail::root>) {
                auto mock = std::get<0>(*api_contexts);
                mock->on_make_called(nil::xalt::type_id<state_t>);
            }
            return nil::sml::detail::default_api<State>::make(parent, state_contexts, api_contexts);
        }

        // Lifecycle hooks that receive the mock from APIContexts
        static auto on_enter(state_t& state, StateMock* mock)
        {
            if constexpr (!nil::xalt::is_of_template_v<state_t, nil::sml::detail::root>) {
                mock->on_enter_called(nil::xalt::type_id<state_t>);
            }
            return nil::sml::detail::default_api<State>::on_enter(state, mock);
        }

        static auto on_exit(state_t& state, StateMock* mock)
        {
            if constexpr (!nil::xalt::is_of_template_v<state_t, nil::sml::detail::root>) {
                mock->on_exit_called(nil::xalt::type_id<state_t>);
            }
            return nil::sml::detail::default_api<State>::on_exit(state, mock);
        }

        static auto on_regions_complete(state_t& state, StateMock* mock)
        {
            if constexpr (!nil::xalt::is_of_template_v<state_t, nil::sml::detail::root>) {
                mock->on_regions_complete_called(nil::xalt::type_id<state_t>);
            }
            return nil::sml::detail::default_api<State>::on_regions_complete(state, mock);
        }

        template <typename E>
        static auto on_event(state_t& state, const E& event, StateMock* mock)
        {
            if constexpr (!nil::xalt::is_of_template_v<state_t, nil::sml::detail::root>) {
                mock->on_event_called(nil::xalt::type_id<state_t>, nil::xalt::type_id<E>);
            }
            return nil::sml::detail::default_api<State>::on_event(state, event, mock);
        }
    };
}
