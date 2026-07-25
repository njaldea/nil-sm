#pragma once

#include <nil/sml.hpp>

#include <gmock/gmock.h>

#include <variant>

namespace sml_test
{
    template <typename Case, int Index>
    struct state_tag
    {
    };

    struct e1
    {
    };

    struct e2
    {
    };

    enum class reaction_choice
    {
        forward,
        discard,
        transit,
    };

    struct activity_mock
    {
        MOCK_METHOD(void, on_enter, (const void* state_id), ());
        MOCK_METHOD(reaction_choice, on_react, (const void* state_id), ());
        MOCK_METHOD(void, on_exit, (const void* state_id), ());
    };

    inline activity_mock* g_activity = nullptr;

    struct activity_scope
    {
        explicit activity_scope(activity_mock& mock)
        {
            g_activity = &mock;
        }

        ~activity_scope()
        {
            g_activity = nullptr;
        }
    };

    template <typename T>
    void note_enter()
    {
        g_activity->on_enter(nil::xalt::type_id<T>);
    }

    template <typename T>
    void note_exit()
    {
        g_activity->on_exit(nil::xalt::type_id<T>);
    }

    template <typename T>
    reaction_choice note_react()
    {
        return g_activity->on_react(nil::xalt::type_id<T>);
    }

    template <typename... Regions, typename E>
    void send(nil::sml::SM<nil::xalt::tlist<Regions...>>& sm, E event)
    {
        sm.process_event(std::move(event));
    }

    template <typename T>
    void expect_enter(activity_mock& activity)
    {
        EXPECT_CALL(activity, on_enter(nil::xalt::type_id<T>)).Times(1);
    }

    template <typename T>
    void expect_exit(activity_mock& activity)
    {
        EXPECT_CALL(activity, on_exit(nil::xalt::type_id<T>)).Times(1);
    }

    template <typename T>
    void expect_react(activity_mock& activity, reaction_choice choice)
    {
        EXPECT_CALL(activity, on_react(nil::xalt::type_id<T>)).WillOnce(testing::Return(choice));
    }

    template <typename T>
    void expect_no_react(activity_mock& activity)
    {
        EXPECT_CALL(activity, on_react(nil::xalt::type_id<T>)).Times(0);
    }

    template <typename T>
    struct traced
    {
        traced()
        {
            note_enter<T>();
        }

        ~traced()
        {
            note_exit<T>();
        }
    };

    using regular_reaction = std::variant<nil::sml::Forward, nil::sml::Discard>;

    template <typename T>
    regular_reaction react_regular_from_mock()
    {
        switch (note_react<T>())
        {
            case reaction_choice::forward:
                return nil::sml::Forward{};
            case reaction_choice::discard:
                return nil::sml::Discard{};
            case reaction_choice::transit:
                return nil::sml::Discard{};
        }

        return nil::sml::Discard{};
    }

    template <typename Tag>
    struct leaf_regular: traced<leaf_regular<Tag>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */) -> regular_reaction
        {
            return react_regular_from_mock<leaf_regular<Tag>>();
        }
    };

    template <typename Tag>
    struct leaf_unhandled: traced<leaf_unhandled<Tag>>
    {
    };

    template <typename Tag, typename Target>
    struct leaf_transit: traced<leaf_transit<Tag, Target>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
            -> std::
                variant<nil::sml::Forward, nil::sml::Discard, nil::sml::Transit<Target>>
        {
            switch (note_react<leaf_transit<Tag, Target>>())
            {
                case reaction_choice::forward:
                    return nil::sml::Forward{};
                case reaction_choice::discard:
                    return nil::sml::Discard{};
                case reaction_choice::transit:
                    return nil::sml::Transit<Target>();
            }

            return nil::sml::Discard{};
        }
    };

    template <typename Tag, typename... Regions>
    struct node_regular: traced<node_regular<Tag, Regions...>>
    {
        using regions = nil::xalt::tlist<Regions...>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */) -> regular_reaction
        {
            return react_regular_from_mock<node_regular<Tag, Regions...>>();
        }
    };

    template <typename Tag, typename... Regions>
    struct node_no_events: traced<node_no_events<Tag, Regions...>>
    {
        using regions = nil::xalt::tlist<Regions...>;
    };

    template <typename Case, int Index>
    using regular_leaf = leaf_regular<state_tag<Case, Index>>;

    template <typename Case, int Index>
    using unhandled_leaf = leaf_unhandled<state_tag<Case, Index>>;

    template <typename Case, int Index, typename Target>
    using transit_leaf_state = leaf_transit<state_tag<Case, Index>, Target>;

    template <typename Case, int Index, typename... Regions>
    using regular_node_state = node_regular<state_tag<Case, Index>, Regions...>;

    template <typename Case, int Index, typename... Regions>
    using no_event_node_state = node_no_events<state_tag<Case, Index>, Regions...>;
}
