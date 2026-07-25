#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

#include <variant>

namespace
{
    struct e_self
    {
        bool transit = false;
    };

    // State that transits to itself or discards based on event payload
    struct SelfTransitState
    {
        using events = nil::xalt::tlist<e_self>;

        static auto on_event(const e_self& ev) -> std::variant<Discard, Transit<SelfTransitState>>
        {
            if (ev.transit)
            {
                return Transit<SelfTransitState>{};
            }
            return Discard{};
        }
    };
}

// Test: self-transition reconstructs the state
TEST(sm_feature_transition_semantics, self_transition_reconstructs_state)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    // First instance
    EXPECT_CALL(mock, on_make_called(type_id<SelfTransitState>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<SelfTransitState>)).Times(1);
    TestSM<SelfTransitState> sm(nullptr, &mock);

    // Self-transit: exits and re-creates
    {
        EXPECT_CALL(mock, on_event_called(type_id<SelfTransitState>, type_id<e_self>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<SelfTransitState>)).Times(1);
        // Second instance (after self-transit)
        EXPECT_CALL(mock, on_make_called(type_id<SelfTransitState>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<SelfTransitState>)).Times(1);
        sm.post(e_self{.transit = true});
    }

    {
        EXPECT_CALL(mock, on_event_called(type_id<SelfTransitState>, type_id<e_self>)).Times(1);
        sm.post(e_self{.transit = false});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<SelfTransitState>)).Times(1);
}

// Test: transition to a sibling state
TEST(sm_feature_transition_semantics, transition_to_sibling_state)
{
    struct Sibling
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<Sibling>{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Source>;
        // no events
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Source>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // Source transits to Sibling
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<Sibling>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<Sibling>)).Times(1);
        sm.post(e1{});
    }

    {
        EXPECT_CALL(mock, on_event_called(type_id<Sibling>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Sibling>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: transition to a sibling of the parent's child
TEST(sm_feature_transition_semantics, transition_to_parents_child)
{
    struct SiblingChild
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct SourceChild
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<SiblingChild>{};
        }
    };

    struct Mid
    {
        using regions = nil::xalt::tlist<SourceChild>;
        // no events
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Mid>;
        // no events
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Mid>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Mid>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<SourceChild>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<SourceChild>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // SourceChild transits to SiblingChild
    {
        EXPECT_CALL(mock, on_event_called(type_id<SourceChild>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<SourceChild>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<SiblingChild>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<SiblingChild>)).Times(1);
        sm.post(e1{});
    }

    {
        EXPECT_CALL(mock, on_event_called(type_id<SiblingChild>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<SiblingChild>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Mid>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: transition destroys the previous state
TEST(sm_feature_transition_semantics, transition_destroys_previous_state)
{
    struct Target
    {
        // no events - just sits alive
    };

    struct Source
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<Target>{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Source>)).Times(1);
    TestSM<Source> sm(nullptr, &mock);

    // Source transits to Target; Target has no events so lives until destruction
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<Target>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Target>)).Times(1);
}

// Test: multiple consecutive transitions (a → b → c)
TEST(sm_feature_transition_semantics, multiple_consecutive_transitions)
{
    struct C
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct B
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<C>{};
        }
    };

    struct A
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Transit<B>{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<A>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<A>)).Times(1);
    TestSM<A> sm(nullptr, &mock);

    // A → B
    {
        EXPECT_CALL(mock, on_event_called(type_id<A>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<A>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<B>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<B>)).Times(1);
        sm.post(e1{});
    }

    // B → C
    {
        EXPECT_CALL(mock, on_event_called(type_id<B>, type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<B>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<C>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<C>)).Times(1);
        sm.post(e1{});
    }

    // C discards
    {
        EXPECT_CALL(mock, on_event_called(type_id<C>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<C>)).Times(1);
}

// Test: transition occurs after a forwarded event
TEST(sm_feature_transition_semantics, transition_after_forwarded_event)
{
    struct Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct e_fwd_or_transit
    {
        bool forward = false;
    };

    // Source: forwards when event.forward==true, transits otherwise
    struct Source
    {
        using events = nil::xalt::tlist<e_fwd_or_transit>;

        static auto on_event(const e_fwd_or_transit& ev) -> std::variant<Forward, Transit<Target>>
        {
            if (ev.forward)
            {
                return Forward{};
            }
            return Transit<Target>{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Source>;
        using events = nil::xalt::tlist<e_fwd_or_transit>;

        static auto on_event(const e_fwd_or_transit& /* ev */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Source>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // forward=true: source forwards → root handles
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e_fwd_or_transit>)).Times(1);
        EXPECT_CALL(mock, on_event_called(type_id<Root>, type_id<e_fwd_or_transit>)).Times(1);
        sm.post(e_fwd_or_transit{.forward = true});
    }

    // forward=false: source transits
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e_fwd_or_transit>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<Target>)).Times(1);
        sm.post(e_fwd_or_transit{.forward = false});
    }

    // target discards
    {
        EXPECT_CALL(mock, on_event_called(type_id<Target>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}

// Test: transition occurs after a discarded event
TEST(sm_feature_transition_semantics, transition_after_discarded_event)
{
    struct Target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    struct e_discard_or_transit
    {
        bool discard = false;
    };

    // Source: discards when event.discard==true, transits otherwise
    struct Source
    {
        using events = nil::xalt::tlist<e_discard_or_transit>;

        static auto on_event(const e_discard_or_transit& ev
        ) -> std::variant<Discard, Transit<Target>>
        {
            if (ev.discard)
            {
                return Discard{};
            }
            return Transit<Target>{};
        }
    };

    struct Root
    {
        using regions = nil::xalt::tlist<Source>;
        using events = nil::xalt::tlist<e_discard_or_transit>;

        static auto on_event(const e_discard_or_transit& /* ev */)
        {
            return Discard{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Root>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Source>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Source>)).Times(1);
    TestSM<Root> sm(nullptr, &mock);

    // discard=true: source discards → root NOT called
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e_discard_or_transit>)).Times(1);
        sm.post(e_discard_or_transit{.discard = true});
    }

    // discard=false: source transits
    {
        EXPECT_CALL(mock, on_event_called(type_id<Source>, type_id<e_discard_or_transit>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(type_id<Source>)).Times(1);
        EXPECT_CALL(mock, on_make_called(type_id<Target>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(type_id<Target>)).Times(1);
        sm.post(e_discard_or_transit{.discard = false});
    }

    // target discards
    {
        EXPECT_CALL(mock, on_event_called(type_id<Target>, type_id<e1>)).Times(1);
        sm.post(e1{});
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Target>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Root>)).Times(1);
}
