// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include <nil/sm/barrier.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    struct finish_first
    {
    };

    struct finish_second
    {
    };

    struct first_child
    {
        using events = nil::xalt::tlist<finish_first>;

        static bool first_received;

        static auto on_event(const finish_first& /* event */)
        {
            first_received = true;
            return nil::sm::Terminate{};
        }
    };

    struct second_child
    {
        using events = nil::xalt::tlist<finish_second>;
        static bool second_received;

        static auto on_event(const finish_second& /* event */)
        {
            second_received = true;
            return nil::sm::Terminate{};
        }
    };

    using default_api = nil::sm::api::Default<>;

    NIL_SM_BARRIER_DECLARE(first_barrier_state, default_api);
    NIL_SM_BARRIER_DECLARE(second_barrier_state, default_api);

    NIL_SM_BARRIER_DEFINE(first_barrier_state, first_child);
    NIL_SM_BARRIER_DEFINE(second_barrier_state, second_child);

    bool first_child::first_received = false;
    bool second_child::second_received = false;

    using second_barrier = nil::sm::barrier::State<nil::sm::Terminate, second_barrier_state>;
    using first_barrier
        = nil::sm::barrier::State<nil::sm::TransitTo<second_barrier>, first_barrier_state>;

    using barrier_graph = nil::sm::detail::region_reachability_graph<default_api, first_barrier>;
    static_assert(barrier_graph::states::template contains<second_barrier>);

    struct root
    {
        using regions = nil::xalt::tlist<first_barrier>;

        static auto on_regions_finalized()
        {
            return nil::sm::Terminate{};
        }
    };
}

TEST(BarrierRuntime, CompletionActionChainsToNextBarrier)
{
    first_child::first_received = false;
    second_child::second_received = false;
    nil::sm::DefaultSM<root> machine;

    machine.post(finish_first{});
    EXPECT_TRUE(first_child::first_received);
    EXPECT_FALSE(machine.is_finalized());

    machine.post(finish_second{});
    EXPECT_TRUE(second_child::second_received);
    EXPECT_TRUE(machine.is_finalized());
}

namespace
{
    struct finish_with_exit
    {
    };

    struct child_exit_notification
    {
    };

    struct exit_child
    {
        using events = nil::xalt::tlist<finish_with_exit>;

        static bool exited;

        static auto on_event(const finish_with_exit& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_exit()
        {
            exited = true;
            return nil::sm::Emit<child_exit_notification>{};
        }
    };

    bool exit_child::exited = false;

    using exit_api = nil::sm::api::Default<>;

    NIL_SM_BARRIER_DECLARE(exit_barrier_state, exit_api);
    NIL_SM_BARRIER_DEFINE(exit_barrier_state, exit_child);
    using exit_barrier = nil::sm::barrier::State<nil::sm::Terminate, exit_barrier_state>;

    struct exit_sink
    {
        using events = nil::xalt::tlist<child_exit_notification>;
        static bool received;

        static auto on_event(const child_exit_notification& /* event */)
        {
            received = true;
            return nil::sm::Terminate{};
        }
    };

    bool exit_sink::received = false;

    struct exit_root
    {
        using regions = nil::xalt::tlist<exit_barrier, exit_sink>;

        static auto on_regions_finalized()
        {
            return nil::sm::Terminate{};
        }
    };
}

TEST(BarrierRuntime, ChildExitEmissionReachesSharedQueue)
{
    exit_child::exited = false;
    exit_sink::received = false;
    nil::sm::DefaultSM<exit_root> machine;

    machine.post(finish_with_exit{});

    EXPECT_TRUE(exit_child::exited);
    EXPECT_TRUE(exit_sink::received);
    EXPECT_TRUE(machine.is_finalized());
}

namespace
{
    struct owner_target;

    struct owner_event
    {
    };

    struct owner_done
    {
    };

    struct child_finishing_state
    {
        using events = nil::xalt::tlist<owner_event>;

        static auto on_event(const owner_event& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    using owner_child_api = nil::sm::api::Default<>;

    NIL_SM_BARRIER_DECLARE(owner_barrier_state, owner_child_api);
    NIL_SM_BARRIER_DEFINE(owner_barrier_state, child_finishing_state);
    using owner_barrier = nil::sm::barrier::State<nil::sm::Terminate, owner_barrier_state>;

    struct owner_state
    {
        using regions = nil::xalt::tlist<owner_barrier>;

        static auto on_regions_finalized()
        {
            return nil::sm::TransitTo<owner_target>{};
        }
    };

    struct owner_target
    {
        using events = nil::xalt::tlist<owner_done>;

        static auto on_event(const owner_done& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct shutdown
    {
    };

    struct capture_observer
    {
        bool child_received = false;
    };

    struct capture_child_state
    {
        using events = nil::xalt::tlist<shutdown>;
        using args = nil::xalt::tlist<capture_observer>;

        explicit capture_child_state(capture_observer* init_observer)
            : observer(init_observer)
        {
        }

        auto on_event(const shutdown& /* event */)
        {
            observer->child_received = true;
            return nil::sm::Discard{};
        }

        capture_observer* observer;
    };

    using capture_child_api = nil::sm::api::Default<>;

    NIL_SM_BARRIER_DECLARE(capture_barrier_state, capture_child_api);
    NIL_SM_BARRIER_DEFINE(capture_barrier_state, capture_child_state);
    using capture_barrier = nil::sm::barrier::State<nil::sm::Terminate, capture_barrier_state>;

    struct capture_owner
    {
        capture_observer* observer;
        using regions = nil::xalt::tlist<capture_barrier>;
        using captures = nil::xalt::tlist<shutdown>;
        using args = nil::xalt::tlist<capture_observer>;
        using props = nil::xalt::tlist<nil::sm::prop<capture_observer*, &capture_owner::observer>>;

        explicit capture_owner(capture_observer* init_observer)
            : observer(init_observer)
        {
        }

        static auto on_capture(const shutdown& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct bubble_event
    {
    };

    struct bubbling_child_state
    {
        using events = nil::xalt::tlist<bubble_event>;

        static auto on_event(const bubble_event& /* event */)
        {
            return nil::sm::Forward{};
        }
    };

    using bubbling_child_api = nil::sm::api::Default<>;

    NIL_SM_BARRIER_DECLARE(bubbling_barrier_state, bubbling_child_api);
    NIL_SM_BARRIER_DEFINE(bubbling_barrier_state, bubbling_child_state);
    using bubbling_barrier = nil::sm::barrier::State<nil::sm::Terminate, bubbling_barrier_state>;

    struct bubbling_owner
    {
        using regions = nil::xalt::tlist<bubbling_barrier>;
        using events = nil::xalt::tlist<bubble_event>;

        static auto on_event(const bubble_event& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    NIL_SM_BARRIER_DECLARE(lifecycle_barrier_state, TestAPI);

    struct lifecycle_child
    {
    };

    NIL_SM_BARRIER_DEFINE(lifecycle_barrier_state, lifecycle_child);
    using lifecycle_barrier = nil::sm::barrier::State<nil::sm::Terminate, lifecycle_barrier_state>;

    struct lifecycle_root
    {
        using regions = nil::xalt::tlist<lifecycle_barrier>;
    };

    using lifecycle_machine = nil::sm::SM<TestAPI, lifecycle_root>;
}

TEST(BarrierRuntime, OwnerHandlesChildFinalization)
{
    nil::sm::DefaultSM<owner_state> machine;

    machine.post(owner_event{});
    EXPECT_FALSE(machine.is_finalized());

    machine.post(owner_done{});
    EXPECT_TRUE(machine.is_finalized());
}

TEST(BarrierRuntime, OwnerCapturePreemptsBarrierChild)
{
    capture_observer observer;
    nil::sm::DefaultSM<capture_owner, capture_observer> machine{&observer};

    machine.post(shutdown{});

    EXPECT_FALSE(observer.child_received);
    EXPECT_TRUE(machine.is_finalized());
}

TEST(BarrierRuntime, ChildForwardBubblesToOwner)
{
    nil::sm::DefaultSM<bubbling_owner> machine;

    machine.post(bubble_event{});

    EXPECT_TRUE(machine.is_finalized());
}

TEST(BarrierRuntime, ChildLifecycleRunsWithoutDescriptorLifecycle)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence sequence;

    EXPECT_CALL(mock, on_make_called(nil::xalt::type_id<lifecycle_root>));
    EXPECT_CALL(mock, on_enter_called(nil::xalt::type_id<lifecycle_root>));
    EXPECT_CALL(mock, on_make_called(nil::xalt::type_id<lifecycle_child>));
    EXPECT_CALL(mock, on_enter_called(nil::xalt::type_id<lifecycle_child>));
    EXPECT_CALL(mock, on_exit_called(nil::xalt::type_id<lifecycle_child>));
    EXPECT_CALL(mock, on_exit_called(nil::xalt::type_id<lifecycle_root>));

    lifecycle_machine machine(&mock);
}
