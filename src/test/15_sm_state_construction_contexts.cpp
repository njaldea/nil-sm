#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    struct custom_context
    {
        int marker = 0;
    };

    struct custom_context2
    {
        int marker = 0;
    };

    class ConstructionObserver
    {
    public:
        MOCK_METHOD(void, on_construct, (int ctx_marker), ());
        MOCK_METHOD(void, on_construct_two, (int ctx1_marker, int ctx2_marker), ());
        MOCK_METHOD(void, on_react, (), ());
    };

    struct parent_and_context_state
    {
        using events = nil::xalt::tlist<e1>;
        using args = nil::xalt::tlist<custom_context, ConstructionObserver>;

        ConstructionObserver* obs;

        explicit parent_and_context_state(custom_context* ctx, ConstructionObserver* o)
            : obs(o)
        {
            obs->on_construct(ctx->marker);
        }

        auto on_event(const e1& /* event */) const
        {
            obs->on_react();
            return Discard{};
        }
    };

    struct default_only_state
    {
        using events = nil::xalt::tlist<e1>;
        using args = nil::xalt::tlist<ConstructionObserver>;

        ConstructionObserver* obs;

        explicit default_only_state(ConstructionObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e1& /* event */) const
        {
            obs->on_react();
            return Discard{};
        }
    };

    struct parent_and_two_contexts_state
    {
        using events = nil::xalt::tlist<e1>;
        using args = nil::xalt::tlist<custom_context, custom_context2, ConstructionObserver>;

        ConstructionObserver* obs;

        explicit parent_and_two_contexts_state(
            custom_context* ctx_1,
            custom_context2* ctx_2,
            ConstructionObserver* o
        )
            : obs(o)
        {
            obs->on_construct_two(ctx_1->marker, ctx_2->marker);
        }

        auto on_event(const e1& /* event */) const
        {
            obs->on_react();
            return Discard{};
        }
    };
}

TEST(sm_feature_state_construction_contexts, state_constructs_with_parent_and_context_args)

{
    testing::StrictMock<ConstructionObserver> obs;

    custom_context ctx{.marker = 42};

    testing::InSequence seq;

    EXPECT_CALL(obs, on_construct(42)).Times(1);
    using sm_t = nil::sm::SM<
        nil::sm::api::Default<void>::type,
        parent_and_context_state,
        custom_context,
        ConstructionObserver>;
    sm_t sm(&ctx, &obs);
    {
        EXPECT_CALL(obs, on_react()).Times(1);
        sm.post(e1{});
    }
}

TEST(
    sm_feature_state_construction_contexts,
    state_can_still_default_construct_when_it_expects_nothing
)
{
    testing::StrictMock<ConstructionObserver> obs;

    using sm_t
        = nil::sm::SM<nil::sm::api::Default<void>::type, default_only_state, ConstructionObserver>;
    sm_t sm{&obs};
    {
        EXPECT_CALL(obs, on_react()).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_state_construction_contexts, state_constructs_with_parent_and_two_contexts)
{
    testing::StrictMock<ConstructionObserver> obs;

    custom_context ctx_1{.marker = 7};
    custom_context2 ctx_2{.marker = 99};

    testing::InSequence seq;

    EXPECT_CALL(obs, on_construct_two(7, 99)).Times(1);
    using sm_t = nil::sm::SM<
        nil::sm::api::Default<void>::type,
        parent_and_two_contexts_state,
        custom_context,
        custom_context2,
        ConstructionObserver>;
    sm_t sm{&ctx_1, &ctx_2, &obs};

    {
        EXPECT_CALL(obs, on_react()).Times(1);
        sm.post(e1{});
    }
}
