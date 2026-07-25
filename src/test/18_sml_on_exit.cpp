#include <nil/sml.hpp>

#include <gtest/gtest.h>

#include <vector>

namespace
{
    struct exit_probe
    {
        static inline int count = 0;
        static inline std::vector<int> order = {};

        static void reset()
        {
            count = 0;
            order.clear();
        }
    };

    struct exit_emit_payload
    {
        static inline int dtor_count = 0;

        static void reset()
        {
            dtor_count = 0;
        }

        ~exit_emit_payload()
        {
            ++dtor_count;
        }
    };

    struct exit_only_state
    {
        static auto on_exit()
        {
            ++exit_probe::count;
            return nil::sml::NOOP{};
        }
    };

    struct child_exit_state
    {
        static auto on_exit()
        {
            exit_probe::order.push_back(1);
            return nil::sml::NOOP{};
        }
    };

    struct parent_exit_state
    {
        using regions = nil::xalt::tlist<child_exit_state>;

        static auto on_exit()
        {
            exit_probe::order.push_back(2);
            return nil::sml::NOOP{};
        }
    };

    struct r1_exit_state
    {
        static auto on_exit()
        {
            exit_probe::order.push_back(1);
            return nil::sml::NOOP{};
        }
    };

    struct r2_exit_state
    {
        static auto on_exit()
        {
            exit_probe::order.push_back(2);
            return nil::sml::NOOP{};
        }
    };

    struct exit_emit_state
    {
        static auto on_exit()
        {
            return nil::sml::Emit<exit_emit_payload>();
        }
    };
}

TEST(sml_feature_on_exit, invokes_on_exit_on_state_destruction)
{
    exit_probe::reset();

    {
        nil::sml::SM<nil::xalt::tlist<exit_only_state>> sm{};
        (void)sm;
    }

    EXPECT_EQ(exit_probe::count, 1);
}

TEST(sml_feature_on_exit, destroys_child_before_parent_on_exit)
{
    exit_probe::reset();

    {
        nil::sml::SM<nil::xalt::tlist<parent_exit_state>> sm{};
        (void)sm;
    }

    ASSERT_EQ(exit_probe::order.size(), 2U);
    EXPECT_EQ(exit_probe::order[0], 1);
    EXPECT_EQ(exit_probe::order[1], 2);
}

TEST(sml_feature_on_exit, destroys_regions_in_reverse_order)
{
    exit_probe::reset();

    {
        nil::sml::SM<nil::xalt::tlist<r1_exit_state, r2_exit_state>> sm{};
        (void)sm;
    }

    ASSERT_EQ(exit_probe::order.size(), 2U);
    EXPECT_EQ(exit_probe::order[0], 2);
    EXPECT_EQ(exit_probe::order[1], 1);
}

TEST(sml_feature_on_exit, supports_emit_action_on_exit)
{
    exit_emit_payload::reset();

    {
        nil::sml::SM<nil::xalt::tlist<exit_emit_state>> sm{};
        (void)sm;
    }

    // on_exit emit is queued during State teardown and freed when the queue is destroyed.
    EXPECT_EQ(exit_emit_payload::dtor_count, 1);
}
