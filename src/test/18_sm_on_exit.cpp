// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    class ExitObserver
    {
    public:
        MOCK_METHOD(void, on_exit_called, (), ());
        MOCK_METHOD(void, on_exit_from_state, (int state_id), ());
    };

    struct exit_only_state
    {
        ExitObserver* obs;

        using args = nil::xalt::tlist<ExitObserver>;

        explicit exit_only_state(ExitObserver* o)
            : obs(o)
        {
        }

        auto on_exit() const
        {
            obs->on_exit_called();
            return NOOP{};
        }
    };

    struct child_exit_state
    {
        ExitObserver* obs;

        using args = nil::xalt::tlist<ExitObserver>;

        explicit child_exit_state(ExitObserver* o)
            : obs(o)
        {
        }

        auto on_exit() const
        {
            obs->on_exit_from_state(1);
            return NOOP{};
        }
    };

    struct parent_exit_state
    {
        using regions = nil::xalt::tlist<child_exit_state>;

        ExitObserver* obs;

        using args = nil::xalt::tlist<ExitObserver>;

        explicit parent_exit_state(ExitObserver* o)
            : obs(o)
        {
        }

        auto on_exit() const
        {
            obs->on_exit_from_state(2);
            return NOOP{};
        }
    };

    struct r1_exit_state
    {
        ExitObserver* obs;

        using args = nil::xalt::tlist<ExitObserver>;

        explicit r1_exit_state(ExitObserver* o)
            : obs(o)
        {
        }

        auto on_exit() const
        {
            obs->on_exit_from_state(1);
            return NOOP{};
        }
    };

    struct r2_exit_state
    {
        ExitObserver* obs;

        using args = nil::xalt::tlist<ExitObserver>;

        explicit r2_exit_state(ExitObserver* o)
            : obs(o)
        {
        }

        auto on_exit() const
        {
            obs->on_exit_from_state(2);
            return NOOP{};
        }
    };

    struct exit_emit_payload
    {
        int id = 0;

        explicit exit_emit_payload(int i = 0)
            : id(i)
        {
        }
    };

    struct exit_emit_state
    {
        ExitObserver* obs;

        using args = nil::xalt::tlist<ExitObserver>;

        explicit exit_emit_state(ExitObserver* o)
            : obs(o)
        {
        }

        static auto on_exit()
        {
            return Emit<exit_emit_payload>();
        }
    };

    template <typename T, typename... RootArgs>
    using ExitTestSM = nil::sm::DefaultSM<T, RootArgs...>;
}

TEST(sm_feature_on_exit, invokes_on_exit_on_state_destruction)
{
    testing::StrictMock<ExitObserver> obs;
    testing::InSequence sequence;

    {
        ExitTestSM<exit_only_state, ExitObserver> sm(&obs);
        EXPECT_CALL(obs, on_exit_called()).Times(1);
    }
}

TEST(sm_feature_on_exit, destroys_child_before_parent_on_exit)
{
    testing::StrictMock<ExitObserver> obs;
    testing::InSequence sequence;

    {
        ExitTestSM<parent_exit_state, ExitObserver> sm(&obs);
        EXPECT_CALL(obs, on_exit_from_state(1)).Times(1);
        EXPECT_CALL(obs, on_exit_from_state(2)).Times(1);
    }
}

TEST(sm_feature_on_exit, destroys_regions_in_reverse_order)
{
    testing::StrictMock<ExitObserver> obs;
    testing::InSequence sequence;

    struct Root
    {
        using regions = nil::xalt::tlist<r1_exit_state, r2_exit_state>;
    };

    {
        ExitTestSM<Root, ExitObserver> sm(&obs);
        EXPECT_CALL(obs, on_exit_from_state(2)).Times(1);
        EXPECT_CALL(obs, on_exit_from_state(1)).Times(1);
    }
}

TEST(sm_feature_on_exit, supports_emit_action_on_exit)
{
    testing::StrictMock<ExitObserver> obs;

    {
        ExitTestSM<exit_emit_state, ExitObserver> sm(&obs);

        // Destruction is the trigger; strict mock verification covers unexpected observer calls.
    }
}
