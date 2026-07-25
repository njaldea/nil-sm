#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

// State classes
class LeafStateForward
{
public:
    explicit LeafStateForward() = default;

    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* event */)
    {
        return Forward{};
    }
};

class LeafStateDiscard
{
public:
    explicit LeafStateDiscard() = default;

    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* event */)
    {
        return Discard{};
    }
};

class LeafStateUnhandled
{
};

class LeafStateRegular
{
public:
    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* event */)
    {
        return Discard{};
    }
};

class LeafStateTerminate
{
public:
    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* event */)
    {
        return Terminate{};
    }
};

class LeafStateEmit
{
public:
    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* event */)
    {
        return Emit<e2>{};
    }
};

class LeafStateTransit
{
public:
    using events = nil::xalt::tlist<e1>;

    static auto on_event(const e1& /* event */)
    {
        return Transit<LeafStateDiscard>{};
    }
};

// Test: leaf event is forwarded
TEST(sm_feature_basic_dispatch, leaf_event_forwarded)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<LeafStateForward>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateForward>)).Times(1);
    TestSM<LeafStateForward> sm(nullptr, &mock);

    EXPECT_CALL(mock, on_event_called(type_id<LeafStateForward>, type_id<e1>)).Times(1);
    sm.post(e1{});

    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateForward>)).Times(1);
}

// Test: leaf event is discarded
TEST(sm_feature_basic_dispatch, leaf_event_discarded)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<LeafStateDiscard>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateDiscard>)).Times(1);
    TestSM<LeafStateDiscard> sm(nullptr, &mock);

    EXPECT_CALL(mock, on_event_called(type_id<LeafStateDiscard>, type_id<e1>)).Times(1);
    sm.post(e1{});

    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateDiscard>)).Times(1);
}

// Test: leaf event is unhandled
TEST(sm_feature_basic_dispatch, leaf_event_unhandled)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<LeafStateUnhandled>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateUnhandled>)).Times(1);
    TestSM<LeafStateUnhandled> sm(nullptr, &mock);

    sm.post(e1{});

    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateUnhandled>)).Times(1);
}

// Test: leaf event terminates the region; no SM destructor exit since region is already null
TEST(sm_feature_basic_dispatch, leaf_event_terminate)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<LeafStateTerminate>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateTerminate>)).Times(1);
    TestSM<LeafStateTerminate> sm(nullptr, &mock);

    // Returns Terminate → state exits during apply_sub_transits; region nulled (no dtor exit)
    EXPECT_CALL(mock, on_event_called(type_id<LeafStateTerminate>, type_id<e1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateTerminate>)).Times(1);
    sm.post(e1{});
}

// Test: leaf emits an event; emitted event is enqueued and dispatched; state stays alive
TEST(sm_feature_basic_dispatch, leaf_event_emit)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<LeafStateEmit>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateEmit>)).Times(1);
    TestSM<LeafStateEmit> sm(nullptr, &mock);

    // Returns Emit<e2> → e2 enqueued; no e2 handler so dropped; state stays alive
    EXPECT_CALL(mock, on_event_called(type_id<LeafStateEmit>, type_id<e1>)).Times(1);
    sm.post(e1{});

    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateEmit>)).Times(1);
}

// Test: leaf transits to another state; source exits, target is created and entered
TEST(sm_feature_basic_dispatch, leaf_event_transit)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<LeafStateTransit>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateTransit>)).Times(1);
    TestSM<LeafStateTransit> sm(nullptr, &mock);

    // Returns Transit<LeafStateDiscard> → source exits, target created
    EXPECT_CALL(mock, on_event_called(type_id<LeafStateTransit>, type_id<e1>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateTransit>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<LeafStateDiscard>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateDiscard>)).Times(1);
    sm.post(e1{});

    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateDiscard>)).Times(1);
}

// Test: state silently ignores events not in its events list
TEST(sm_feature_basic_dispatch, state_ignores_unregistered_event)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    EXPECT_CALL(mock, on_make_called(type_id<LeafStateRegular>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<LeafStateRegular>)).Times(1);
    TestSM<LeafStateRegular> sm(nullptr, &mock);

    EXPECT_CALL(mock, on_event_called(type_id<LeafStateRegular>, type_id<e1>)).Times(1);
    sm.post(e1{});
    sm.post(e2{}); // e2 is not handled

    EXPECT_CALL(mock, on_exit_called(type_id<LeafStateRegular>)).Times(1);
}
