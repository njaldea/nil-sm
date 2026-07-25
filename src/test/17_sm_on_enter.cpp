#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    class OnEnterObserver
    {
    public:
        MOCK_METHOD(void, on_event, (const e4&), ());
    };

    template <typename Tag>
    struct emit_on_enter
    {
        static auto on_enter()
        {
            return Emit<e4>();
        }
    };

    template <typename Tag>
    struct on_enter_emit_sink
    {
        using events = nil::xalt::tlist<e4>;

        OnEnterObserver* obs;

        explicit on_enter_emit_sink(auto* /* parent */, OnEnterObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e4& event) const
        {
            obs->on_event(event);
            return Discard{};
        }
    };

    template <typename T>
    using OnEnterTestAPI = nil::sm::default_api<T, OnEnterObserver, void*>;

    template <typename... Regions>
    using OnEnterTestSM = nil::sm::SM<OnEnterTestAPI, Regions...>;
}

TEST(sm_feature_on_enter, on_enter_can_publish_event)
{
    using publisher = emit_on_enter<struct tag_on_enter_publisher>;
    using sink = on_enter_emit_sink<struct tag_on_enter_sink>;

    testing::StrictMock<OnEnterObserver> obs;

    EXPECT_CALL(obs, on_event).Times(1);
    OnEnterTestSM<publisher, sink> sm(&obs, {});
}

// Test: state with on_enter returning NOOP does not enqueue any event
TEST(sm_feature_on_enter, on_enter_noop_does_not_emit)
{
    struct NoopState
    {
        static auto on_enter()
        {
            return NOOP{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<NoopState>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<NoopState>)).Times(1);
    TestSM<NoopState> sm({}, &mock);

    {
        sm.post(e1{}); // ignored by state
    }

    EXPECT_CALL(mock, on_exit_called(type_id<NoopState>)).Times(1);
}

// Test: parent on_enter fires before child on_enter (construction order guarantee)
TEST(sm_feature_on_enter, parent_enters_before_child)
{
    struct Child
    {
        static auto on_enter()
        {
            return NOOP{};
        }
    };

    struct Parent
    {
        using regions = nil::xalt::tlist<Child>;

        static auto on_enter()
        {
            return NOOP{};
        }
    };

    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Parent>)).Times(1);
    EXPECT_CALL(mock, on_make_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_enter_called(type_id<Child>)).Times(1);
    // Parent entered before child; both exit on SM destroy
    TestSM<Parent> sm({}, &mock);

    {
        sm.post(e1{}); // ignored by both
    }

    EXPECT_CALL(mock, on_exit_called(type_id<Child>)).Times(1);
    EXPECT_CALL(mock, on_exit_called(type_id<Parent>)).Times(1);
}
