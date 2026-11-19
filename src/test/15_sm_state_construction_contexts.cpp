// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

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

    struct parent_base
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

    struct identity_recorder
    {
        parent_base* entered_parent = nullptr;
        parent_base* received_parent = nullptr;
    };

    struct parent_identity_child
    {
        using args = nil::xalt::tlist<nil::sm::direct_parent<parent_base>, identity_recorder>;

        explicit parent_identity_child(parent_base* parent, identity_recorder* recorder)
        {
            recorder->received_parent = parent;
        }
    };

    struct parent_identity_parent: parent_base
    {
        using args = nil::xalt::tlist<identity_recorder>;
        using props = nil::xalt::tlist<nil::sm::as_parent<parent_base>>;
        using regions = nil::xalt::tlist<parent_identity_child>;

        identity_recorder* recorder;

        explicit parent_identity_parent(identity_recorder* init_recorder)
            : recorder(init_recorder)
        {
        }

        auto on_enter()
        {
            recorder->entered_parent = this;
            return NOOP{};
        }
    };

    struct data_member_prop
    {
    };

    struct member_function_prop
    {
    };

    struct free_function_prop
    {
    };

    struct prop_recorder
    {
        data_member_prop* data_member = nullptr;
        member_function_prop* member_function = nullptr;
        free_function_prop* free_function = nullptr;
    };

    struct prop_parent;
    free_function_prop* get_free_function_prop(prop_parent& state);

    struct prop_child
    {
        using args = nil::xalt::tlist<
            data_member_prop,
            member_function_prop,
            free_function_prop,
            prop_recorder>;

        prop_child(
            data_member_prop* init_data_member,
            member_function_prop* init_member_function,
            free_function_prop* init_free_function,
            prop_recorder* recorder
        )
        {
            recorder->data_member = init_data_member;
            recorder->member_function = init_member_function;
            recorder->free_function = init_free_function;
        }
    };

    struct prop_parent
    {
        using regions = nil::xalt::tlist<prop_child>;

        data_member_prop data_member_value;
        member_function_prop member_function_value;
        free_function_prop free_function_value;

        member_function_prop* get_member_function()
        {
            return &member_function_value;
        }

        using props = nil::xalt::tlist<
            nil::sm::prop<&prop_parent::data_member_value>,
            nil::sm::prop<&prop_parent::get_member_function>,
            nil::sm::prop<&get_free_function_prop>>;
    };

    free_function_prop* get_free_function_prop(prop_parent& state)
    {
        return &state.free_function_value;
    }
}

TEST(sm_feature_state_construction_contexts, state_constructs_with_parent_and_context_args)

{
    testing::StrictMock<ConstructionObserver> obs;

    custom_context ctx{.marker = 42};

    testing::InSequence seq;

    EXPECT_CALL(obs, on_construct(42)).Times(1);
    using sm_t = nil::sm::
        SM<nil::sm::api::Default, parent_and_context_state, custom_context, ConstructionObserver>;
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

    using sm_t = nil::sm::SM<nil::sm::api::Default, default_only_state, ConstructionObserver>;
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
        nil::sm::api::Default,
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

TEST(sm_feature_state_construction_contexts, child_receives_immediate_parent)
{
    identity_recorder recorder;

    nil::sm::DefaultSM<parent_identity_parent, identity_recorder> sm{&recorder};

    EXPECT_NE(recorder.entered_parent, nullptr);
    EXPECT_EQ(recorder.received_parent, recorder.entered_parent);
}

TEST(sm_feature_state_construction_contexts, child_receives_values_from_all_prop_accessor_forms)
{
    prop_recorder recorder;

    nil::sm::DefaultSM<prop_parent, prop_recorder> sm{&recorder};

    EXPECT_NE(recorder.data_member, nullptr);
    EXPECT_NE(recorder.member_function, nullptr);
    EXPECT_NE(recorder.free_function, nullptr);
}
