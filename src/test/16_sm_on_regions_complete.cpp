#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    struct out_complete
    {
        int value = 0;

        explicit out_complete(int v)
            : value(v)
        {
        }
    };

    class RegionsCompleteObserver
    {
    public:
        MOCK_METHOD(void, on_complete_called, (int state_id), ());
        MOCK_METHOD(void, on_child_constructed, (void* parent_ptr), ());
        MOCK_METHOD(void, on_emit_received, (int value), ());
        MOCK_METHOD(void, on_transit_before, (void* state_ptr), ());
        MOCK_METHOD(void, on_transit_after, (), ());
    };

    struct terminate_leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Terminate{};
        }
    };

    struct keep_leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Discard{};
        }
    };

    template <typename R1, typename R2>
    struct completion_parent
    {
        using regions = nil::xalt::tlist<R1, R2>;

        RegionsCompleteObserver* obs;

        explicit completion_parent(auto* /* parent */, RegionsCompleteObserver* o)
            : obs(o)
        {
        }

        auto on_regions_finalized() const
        {
            obs->on_complete_called(1);
            return NOOP{};
        }
    };

    struct target_capture_child
    {
        RegionsCompleteObserver* obs;

        explicit target_capture_child(auto* parent, RegionsCompleteObserver* o)
            : obs(o)
        {
            o->on_child_constructed(parent);
        }
    };

    struct targeted_parent
    {
        using regions = nil::xalt::tlist<target_capture_child>;

        RegionsCompleteObserver* obs;

        explicit targeted_parent(auto* /* parent */, RegionsCompleteObserver* o)
            : obs(o)
        {
        }

        auto on_regions_finalized() const
        {
            obs->on_complete_called(2);
            return NOOP{};
        }
    };

    struct targeted_root
    {
        using regions = nil::xalt::tlist<targeted_parent>;

        RegionsCompleteObserver* obs;

        explicit targeted_root(auto* /* parent */, RegionsCompleteObserver* o)
            : obs(o)
        {
        }

        auto on_regions_finalized() const
        {
            obs->on_complete_called(3);
            return NOOP{};
        }
    };

    struct emitting_parent
    {
        using regions = nil::xalt::tlist<terminate_leaf>;

        explicit emitting_parent(
            auto* /* parent */,
            RegionsCompleteObserver* /* o */
        )
        {
        }

        static auto on_regions_finalized()
        {
            return Emit<out_complete>(77);
        }
    };

    struct emit_sink
    {
        using events = nil::xalt::tlist<out_complete>;

        RegionsCompleteObserver* obs;

        explicit emit_sink(auto* /* parent */, RegionsCompleteObserver* o)
            : obs(o)
        {
        }

        auto on_event(const out_complete& event) const
        {
            obs->on_emit_received(event.value);
            return Discard{};
        }
    };

    struct transit_target
    {
        using events = nil::xalt::tlist<e2>;

        RegionsCompleteObserver* obs;

        explicit transit_target(auto* /* parent */, RegionsCompleteObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e2& /* event */) const
        {
            obs->on_transit_after();
            return Discard{};
        }
    };

    struct transit_capture_child
    {
        RegionsCompleteObserver* obs;

        explicit transit_capture_child(auto* parent, RegionsCompleteObserver* o)
            : obs(o)
        {
            o->on_transit_before(parent);
        }
    };

    struct transit_source
    {
        using regions = nil::xalt::tlist<transit_capture_child>;
        using events = nil::xalt::tlist<e2>;

        explicit transit_source(auto* /* parent */, RegionsCompleteObserver* /* o */)
        {
        }

        static auto on_regions_finalized()
        {
            return Transit<transit_target>{};
        }

        static auto on_event(const e2& /* event */)
        {
            return Discard{};
        }
    };

    template <typename T>
    using RegionsTestAPI = nil::sm::api::Default<T, RegionsCompleteObserver, void>;

    template <typename... Regions>
    using RegionsTestSM = nil::sm::SM<RegionsTestAPI, Regions...>;
}

TEST(sm_feature_on_regions_finalized, triggers_only_when_all_regions_terminated)
{
    testing::StrictMock<RegionsCompleteObserver> obs;
    testing::InSequence sequence;

    {
        using root_tt = completion_parent<terminate_leaf, terminate_leaf>;
        RegionsTestSM<root_tt> sm_tt(&obs, {});
        {
            EXPECT_CALL(obs, on_complete_called(1)).Times(1);
            sm_tt.post(e1{});
        }
    }

    {
        using root_tk = completion_parent<terminate_leaf, keep_leaf>;
        RegionsTestSM<root_tk> sm_tk(&obs, {});
        {
            EXPECT_CALL(obs, on_complete_called).Times(0);
            sm_tk.post(e1{});
        }
    }

    {
        using root_kt = completion_parent<keep_leaf, terminate_leaf>;
        RegionsTestSM<root_kt> sm_kt(&obs, {});
        {
            EXPECT_CALL(obs, on_complete_called).Times(0);
            sm_kt.post(e1{});
        }
    }

    {
        using root_kk = completion_parent<keep_leaf, keep_leaf>;
        RegionsTestSM<root_kk> sm_kk(&obs, {});
        {
            EXPECT_CALL(obs, on_complete_called).Times(0);
            sm_kk.post(e1{});
        }
    }
}

TEST(sm_feature_on_regions_finalized, explicit_target_reaches_nested_state_only)
{
    testing::StrictMock<RegionsCompleteObserver> obs;
    testing::InSequence sequence;
    void* captured_parent = nullptr;

    EXPECT_CALL(obs, on_child_constructed)
        .Times(1)
        .WillOnce([&](void* parent_ptr) { captured_parent = parent_ptr; });

    RegionsTestSM<targeted_root> sm(&obs, {});
    ASSERT_NE(captured_parent, nullptr);

    {
        EXPECT_CALL(obs, on_complete_called(2)).Times(1);
        sm.post(nil::sm::detail::EvRegionsFinalized{captured_parent});
    }
}

TEST(sm_feature_on_regions_finalized, on_regions_finalized_can_emit_follow_up_event)
{
    testing::StrictMock<RegionsCompleteObserver> obs;
    testing::InSequence sequence;

    RegionsTestSM<emitting_parent, emit_sink> sm(&obs, {});

    {
        EXPECT_CALL(obs, on_emit_received(77)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_on_regions_finalized, on_regions_finalized_can_transit_targeted_state)
{
    testing::StrictMock<RegionsCompleteObserver> obs;
    testing::InSequence sequence;
    void* captured_parent = nullptr;

    EXPECT_CALL(obs, on_transit_before)
        .Times(1)
        .WillOnce([&](void* state_ptr) { captured_parent = state_ptr; });

    RegionsTestSM<transit_source> sm(&obs, {});
    ASSERT_NE(captured_parent, nullptr);

    {
        EXPECT_CALL(obs, on_transit_after()).Times(1);
        sm.post(nil::sm::detail::EvRegionsFinalized{captured_parent});
    }
    {
        sm.post(e2{});
    }
}
