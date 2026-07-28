#include "test_api.hpp"
#include <gtest/gtest.h>

using namespace sml_test2;

// State classes
class LeafStateForward
{
public:
    explicit LeafStateForward()
    {
    }

    using events = nil::xalt::tlist<e1>;

    auto on_event(const e1& /* event */) const
    {
        return nil::sml::Forward{};
    }
};

class LeafStateDiscard
{
public:
    explicit LeafStateDiscard(void* /* parent */)
    {
    }

    using events = nil::xalt::tlist<e1>;

    auto on_event(const e1& /* event */) const
    {
        return nil::sml::Discard{};
    }
};

class LeafStateUnhandled
{
public:
    explicit LeafStateUnhandled()
    {
    }
};

class LeafStateRegular
{
public:
    explicit LeafStateRegular()
    {
    }

    using events = nil::xalt::tlist<e1>;

    auto on_event(const e1& /* event */) const
    {
        return nil::sml::Discard{};
    }
};

template <typename Regions>
using TestSM = nil::sml::SM<Regions, nil::xalt::tlist<>, nil::xalt::tlist<testing::StrictMock<StateMock>>, TestAPI>;

// Test: leaf event is forwarded
TEST(sml_feature_basic_dispatch, leaf_event_forwarded)
{
    testing::StrictMock<StateMock> mock;

    {
        testing::InSequence sequence;
        EXPECT_CALL(mock, on_make_called(nil::xalt::type_id<LeafStateForward>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(nil::xalt::type_id<LeafStateForward>)).Times(1);
        EXPECT_CALL(mock, on_event_called(nil::xalt::type_id<LeafStateForward>, nil::xalt::type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(nil::xalt::type_id<LeafStateForward>)).Times(1);

        TestSM<nil::xalt::tlist<LeafStateForward>> sm(
            std::make_tuple(), 
            std::make_tuple(&mock)
        );

        sm.process_event(e1{});
    }
}

// Test: leaf event is discarded
TEST(sml_feature_basic_dispatch, leaf_event_discarded)
{
    testing::StrictMock<StateMock> mock;

    {
        testing::InSequence sequence;
        EXPECT_CALL(mock, on_make_called(nil::xalt::type_id<LeafStateDiscard>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(nil::xalt::type_id<LeafStateDiscard>)).Times(1);
        EXPECT_CALL(mock, on_event_called(nil::xalt::type_id<LeafStateDiscard>, nil::xalt::type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(nil::xalt::type_id<LeafStateDiscard>)).Times(1);

        TestSM<nil::xalt::tlist<LeafStateDiscard>> sm(
            std::make_tuple(), 
            std::make_tuple(&mock)
        );

        sm.process_event(e1{});
    }
}

// Test: leaf event is unhandled
TEST(sml_feature_basic_dispatch, leaf_event_unhandled)
{
    testing::StrictMock<StateMock> mock;

    {
        testing::InSequence sequence;
        EXPECT_CALL(mock, on_make_called(nil::xalt::type_id<LeafStateUnhandled>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(nil::xalt::type_id<LeafStateUnhandled>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(nil::xalt::type_id<LeafStateUnhandled>)).Times(1);

        TestSM<nil::xalt::tlist<LeafStateUnhandled>> sm(
            std::make_tuple(), 
            std::make_tuple(&mock)
        );

        sm.process_event(e1{});
    }
}

// Test: unrelated event not matched
TEST(sml_feature_basic_dispatch, unrelated_event_not_matched)
{
    testing::StrictMock<StateMock> mock;

    {
        testing::InSequence sequence;
        EXPECT_CALL(mock, on_make_called(nil::xalt::type_id<LeafStateRegular>)).Times(1);
        EXPECT_CALL(mock, on_enter_called(nil::xalt::type_id<LeafStateRegular>)).Times(1);
        EXPECT_CALL(mock, on_event_called(nil::xalt::type_id<LeafStateRegular>, nil::xalt::type_id<e1>)).Times(1);
        EXPECT_CALL(mock, on_exit_called(nil::xalt::type_id<LeafStateRegular>)).Times(1);

        TestSM<nil::xalt::tlist<LeafStateRegular>> sm(
            std::make_tuple(), 
            std::make_tuple(&mock)
        );

        sm.process_event(e1{});
        sm.process_event(e2{});  // e2 is not handled
    }
}
