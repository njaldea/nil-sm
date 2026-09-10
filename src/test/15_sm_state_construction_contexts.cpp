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

    struct parent_identity_context
    {
        parent_base* entered_parent = nullptr;
        parent_base* received_parent = nullptr;
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

    struct parent_identity_child
    {
        using args = nil::xalt::tlist<nil::sm::direct_parent<parent_base>>;

        explicit parent_identity_child(parent_base* /* parent */)
        {
        }
    };

    struct parent_identity_parent: parent_base
    {
        using regions = nil::xalt::tlist<parent_identity_child>;
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

    struct prop_parent;
    free_function_prop* get_free_function_prop(prop_parent& state);

    struct prop_child
    {
        using args = nil::xalt::tlist<data_member_prop, member_function_prop, free_function_prop>;

        prop_child(
            data_member_prop* init_data_member,
            member_function_prop* init_member_function,
            free_function_prop* init_free_function
        )
        {
            data_member = init_data_member;
            member_function = init_member_function;
            free_function = init_free_function;
        }

        static inline data_member_prop* data_member = nullptr;
        static inline member_function_prop* member_function = nullptr;
        static inline free_function_prop* free_function = nullptr;
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
            nil::sm::prop<data_member_prop, &prop_parent::data_member_value>,
            nil::sm::prop<member_function_prop, &prop_parent::get_member_function>,
            nil::sm::prop<free_function_prop, &get_free_function_prop>>;
    };

    free_function_prop* get_free_function_prop(prop_parent& state)
    {
        return &state.free_function_value;
    }

    template <typename T>
    struct parent_identity_api
    {
        using api_context_t = parent_identity_context;
        using api_t = nil::sm::api::Default<api_context_t>::template type<T>;
        using regions_t = typename api_t::regions_t;
        using events_t = typename api_t::events_t;
        using captures_t = typename api_t::captures_t;
        using args_t = typename api_t::args_t;
        using props_t = typename api_t::props_t;

        template <typename... Args>
        static T make(api_context_t* context, nil::sm::Metadata metadata, Args*... args)
        {
            if constexpr (std::is_same_v<T, parent_identity_child>)
            {
                auto capture_first = []<typename First, typename... Rest>(
                                         First* first,
                                         Rest*... /* rest */
                                     ) { return first; };
                context->received_parent = capture_first(args...);
            }
            return api_t::make(context, metadata, args...);
        }

        static auto on_enter(T& state, api_context_t* context)
        {
            if constexpr (std::is_same_v<T, parent_identity_parent>)
            {
                context->entered_parent = static_cast<parent_base*>(&state);
            }
            return api_t::on_enter(state, context);
        }

        static auto on_exit(T& state, api_context_t* context)
        {
            return api_t::on_exit(state, context);
        }

        static auto on_regions_finalized(T& state, api_context_t* context)
        {
            return api_t::on_regions_finalized(state, context);
        }

        template <typename E>
        static auto on_event(T& state, const E& event, api_context_t* context)
        {
            return api_t::on_event(state, event, context);
        }

        template <typename E>
        static auto on_capture(T& state, const E& event, api_context_t* context)
        {
            return api_t::on_capture(state, event, context);
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

TEST(sm_feature_state_construction_contexts, child_receives_immediate_parent)
{
    parent_identity_context context;

    using sm_t = nil::sm::SM<parent_identity_api, parent_identity_parent>;
    sm_t sm(&context);

    EXPECT_EQ(context.received_parent, context.entered_parent);
}

TEST(sm_feature_state_construction_contexts, child_receives_values_from_all_prop_accessor_forms)
{
    using sm_t = nil::sm::SM<nil::sm::api::Default<void>::type, prop_parent>;
    sm_t sm;

    EXPECT_NE(prop_child::data_member, nullptr);
    EXPECT_NE(prop_child::member_function, nullptr);
    EXPECT_NE(prop_child::free_function, nullptr);
}
