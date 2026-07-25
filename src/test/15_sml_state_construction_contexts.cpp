#include <nil/sml.hpp>

#include <gtest/gtest.h>

namespace
{
    struct e1
    {
    };

    struct custom_context
    {
        int marker = 0;
    };

    struct custom_context2
    {
        int marker = 0;
    };

    struct parent_and_context_state
    {
        using events = nil::xalt::tlist<e1>;

        static inline bool saw_non_null_parent = false;
        static inline int seen_context_marker = -1;
        static inline int ctor_count = 0;
        static inline int react_count = 0;

        template <typename Parent>
        explicit parent_and_context_state(Parent* parent, custom_context* context)
        {
            ++ctor_count;
            saw_non_null_parent = (parent != nullptr);
            seen_context_marker = context->marker;
        }

        static auto on_event(const e1& /* event */)
        {
            ++react_count;
            return nil::sml::Discard{};
        }
    };

    struct default_only_state
    {
        using events = nil::xalt::tlist<e1>;

        static inline int default_ctor_count = 0;
        static inline int react_count = 0;

        default_only_state()
        {
            ++default_ctor_count;
        }

        static auto on_event(const e1& /* event */)
        {
            ++react_count;
            return nil::sml::Discard{};
        }
    };

    struct parent_and_two_contexts_state
    {
        using events = nil::xalt::tlist<e1>;

        static inline bool saw_non_null_parent = false;
        static inline int seen_context_1_marker = -1;
        static inline int seen_context_2_marker = -1;
        static inline int ctor_count = 0;
        static inline int react_count = 0;

        template <typename Parent>
        explicit parent_and_two_contexts_state(
            Parent* parent,
            custom_context* context_1,
            custom_context2* context_2
        )
        {
            ++ctor_count;
            saw_non_null_parent = (parent != nullptr);
            seen_context_1_marker = context_1->marker;
            seen_context_2_marker = context_2->marker;
        }

        static auto on_event(const e1& /* event */)
        {
            ++react_count;
            return nil::sml::Discard{};
        }
    };

    struct parent_type_probe
    {
        static inline bool saw_expected_parent_type = false;

        static void reset()
        {
            saw_expected_parent_type = false;
        }
    };

    struct expected_parent_state
    {
        using regions = nil::xalt::tlist<struct child_parent_type_state>;
    };

    struct child_parent_type_state
    {
        template <typename Parent>
        explicit child_parent_type_state(Parent* /* parent */)
        {
            parent_type_probe::saw_expected_parent_type
                = std::is_same_v<Parent, expected_parent_state>;
        }
    };

    void reset_parent_and_context_state()
    {
        parent_and_context_state::saw_non_null_parent = false;
        parent_and_context_state::seen_context_marker = -1;
        parent_and_context_state::ctor_count = 0;
        parent_and_context_state::react_count = 0;
    }

    void reset_default_only_state()
    {
        default_only_state::default_ctor_count = 0;
        default_only_state::react_count = 0;
    }

    void reset_parent_and_two_contexts_state()
    {
        parent_and_two_contexts_state::saw_non_null_parent = false;
        parent_and_two_contexts_state::seen_context_1_marker = -1;
        parent_and_two_contexts_state::seen_context_2_marker = -1;
        parent_and_two_contexts_state::ctor_count = 0;
        parent_and_two_contexts_state::react_count = 0;
    }

    void reset_parent_type_probe()
    {
        parent_type_probe::reset();
    }
}

TEST(sml_feature_state_construction_contexts, state_constructs_with_parent_and_context_args)
{
    reset_parent_and_context_state();

    custom_context ctx{.marker = 42};

    using sm_t = nil::sml::
        SM<nil::xalt::tlist<parent_and_context_state>, nil::xalt::tlist<custom_context>>;

    sm_t sm{std::make_tuple(&ctx)};

    EXPECT_TRUE(parent_and_context_state::saw_non_null_parent);
    EXPECT_EQ(parent_and_context_state::seen_context_marker, 42);
    EXPECT_EQ(parent_and_context_state::ctor_count, 1);

    sm.process_event(e1{});
    EXPECT_EQ(parent_and_context_state::react_count, 1);
}

TEST(
    sml_feature_state_construction_contexts,
    state_can_still_default_construct_when_it_expects_nothing
)
{
    reset_default_only_state();

    nil::sml::SM<nil::xalt::tlist<default_only_state>> sm{};

    EXPECT_EQ(default_only_state::default_ctor_count, 1);

    sm.process_event(e1{});
    EXPECT_EQ(default_only_state::react_count, 1);
}

TEST(sml_feature_state_construction_contexts, state_constructs_with_parent_and_two_contexts)
{
    reset_parent_and_two_contexts_state();

    custom_context ctx_1{.marker = 7};
    custom_context2 ctx_2{.marker = 99};

    using sm_t = nil::sml::SM<
        nil::xalt::tlist<parent_and_two_contexts_state>,
        nil::xalt::tlist<custom_context, custom_context2>>;

    sm_t sm{std::make_tuple(&ctx_1, &ctx_2)};

    EXPECT_TRUE(parent_and_two_contexts_state::saw_non_null_parent);
    EXPECT_EQ(parent_and_two_contexts_state::seen_context_1_marker, 7);
    EXPECT_EQ(parent_and_two_contexts_state::seen_context_2_marker, 99);
    EXPECT_EQ(parent_and_two_contexts_state::ctor_count, 1);

    sm.process_event(e1{});
    EXPECT_EQ(parent_and_two_contexts_state::react_count, 1);
}

TEST(sml_feature_state_construction_contexts, child_constructor_receives_parent_user_state_type)
{
    reset_parent_type_probe();

    nil::sml::SM<nil::xalt::tlist<expected_parent_state>> sm{};

    (void)sm;
    EXPECT_TRUE(parent_type_probe::saw_expected_parent_type);
}
