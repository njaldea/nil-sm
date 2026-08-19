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
        MOCK_METHOD(void, on_construct, (bool parent_non_null, int ctx_marker), ());
        MOCK_METHOD(
            void,
            on_construct_two,
            (bool parent_non_null, int ctx1_marker, int ctx2_marker),
            ()
        );
        MOCK_METHOD(void, on_react, (), ());
        MOCK_METHOD(void, on_correct_parent_type, (), ());
    };

    struct parent_and_context_state
    {
        using events = nil::xalt::tlist<e1>;

        ConstructionObserver* obs;

        explicit parent_and_context_state(
            auto* parent,
            std::tuple<custom_context*, ConstructionObserver*>* o
        )
            : obs(get<1>(*o))
        {
            obs->on_construct(parent != nullptr, get<0>(*o)->marker);
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

        ConstructionObserver* obs;

        explicit default_only_state(auto* /* parent */, ConstructionObserver* o)
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

        ConstructionObserver* obs;

        explicit parent_and_two_contexts_state(
            auto* parent,
            std::tuple<custom_context*, custom_context2*, ConstructionObserver*>* o
        )
            : obs(get<2>(*o))
        {
            obs->on_construct_two(parent != nullptr, get<0>(*o)->marker, get<1>(*o)->marker);
        }

        auto on_event(const e1& /* event */) const
        {
            obs->on_react();
            return Discard{};
        }
    };

    struct expected_parent_state
    {
        using regions = nil::xalt::tlist<struct child_parent_type_state>;
    };

    struct child_parent_type_state
    {
        template <typename Parent>
        explicit child_parent_type_state(Parent* /* parent */, ConstructionObserver* o)
        {
            if constexpr (std::is_same_v<Parent, expected_parent_state>)
            {
                o->on_correct_parent_type();
            }
        }
    };
}

template <typename S, typename A>
struct api_t
{
    template <typename T>
    using type = nil::sm::api::Default<T, S, A>;
};

TEST(sm_feature_state_construction_contexts, state_constructs_with_parent_and_context_args)

{
    testing::StrictMock<ConstructionObserver> obs;

    custom_context ctx{.marker = 42};

    testing::InSequence seq;

    EXPECT_CALL(obs, on_construct(true, 42)).Times(1);
    using sm_t = nil::sm::SM<
        api_t<std::tuple<custom_context*, ConstructionObserver*>, void>::type,
        parent_and_context_state>;
    auto state_contexts = std::tuple<custom_context*, ConstructionObserver*>(&ctx, &obs);
    sm_t sm(&state_contexts, nullptr);
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

    using sm_t = nil::sm::SM<api_t<ConstructionObserver, void>::type, default_only_state>;
    sm_t sm{&obs, nullptr};
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

    EXPECT_CALL(obs, on_construct_two(true, 7, 99)).Times(1);
    using sm_t = nil::sm::SM<
        api_t<std::tuple<custom_context*, custom_context2*, ConstructionObserver*>, void>::type,
        parent_and_two_contexts_state>;
    auto state_contexts = std::tuple<custom_context*, custom_context2*, ConstructionObserver*>(
        &ctx_1,
        &ctx_2,
        &obs
    );
    sm_t sm{&state_contexts, nullptr};

    {
        EXPECT_CALL(obs, on_react()).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_state_construction_contexts, child_constructor_receives_parent_user_state_type)
{
    testing::StrictMock<ConstructionObserver> obs;
    testing::InSequence sequence;

    EXPECT_CALL(obs, on_correct_parent_type()).Times(1);

    using sm_t = nil::sm::SM<api_t<ConstructionObserver, void>::type, expected_parent_state>;

    sm_t sm{&obs, nullptr};
    (void)sm;
}
