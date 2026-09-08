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
        MOCK_METHOD(void, on_emit_received, (int value), ());
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
        using args = nil::xalt::tlist<RegionsCompleteObserver>;

        RegionsCompleteObserver* obs;

        explicit completion_parent(RegionsCompleteObserver* o)
            : obs(o)
        {
        }

        auto on_regions_finalized() const
        {
            obs->on_complete_called(1);
            return NOOP{};
        }
    };

    // Terminates on e1, letting its parent's on_regions_finalized fire naturally.
    struct target_capture_child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Terminate{};
        }
    };

    struct targeted_parent
    {
        using regions = nil::xalt::tlist<target_capture_child>;
        using args = nil::xalt::tlist<RegionsCompleteObserver>;

        RegionsCompleteObserver* obs;

        explicit targeted_parent(RegionsCompleteObserver* o)
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
        using args = nil::xalt::tlist<RegionsCompleteObserver>;

        RegionsCompleteObserver* obs;

        explicit targeted_root(RegionsCompleteObserver* o)
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

        static auto on_regions_finalized()
        {
            return Emit<out_complete>(77);
        }
    };

    struct emit_sink
    {
        using events = nil::xalt::tlist<out_complete>;
        using args = nil::xalt::tlist<RegionsCompleteObserver>;

        RegionsCompleteObserver* obs;

        explicit emit_sink(RegionsCompleteObserver* o)
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
        using args = nil::xalt::tlist<RegionsCompleteObserver>;

        RegionsCompleteObserver* obs;

        explicit transit_target(RegionsCompleteObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e2& /* event */) const
        {
            obs->on_transit_after();
            return Discard{};
        }
    };

    // Terminates on e1, letting transit_source's own on_regions_finalized fire naturally.
    struct transit_capture_child
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Terminate{};
        }
    };

    struct transit_source
    {
        using regions = nil::xalt::tlist<transit_capture_child>;

        static auto on_regions_finalized()
        {
            return TransitTo<transit_target>{};
        }
    };

    template <typename T>
    using RegionsTestAPI = nil::sm::api::Default<void>::template type<T>;

    template <typename T, typename... RootArgs>
    using RegionsTestSM = nil::sm::SM<RegionsTestAPI, T, RootArgs...>;
}

TEST(sm_feature_on_regions_finalized, triggers_only_when_all_regions_terminated)
{
    testing::StrictMock<RegionsCompleteObserver> obs;
    testing::InSequence sequence;

    {
        using root_tt = completion_parent<terminate_leaf, terminate_leaf>;
        RegionsTestSM<root_tt, RegionsCompleteObserver> sm_tt(&obs);
        {
            EXPECT_CALL(obs, on_complete_called(1)).Times(1);
            sm_tt.post(e1{});
        }
    }

    {
        using root_tk = completion_parent<terminate_leaf, keep_leaf>;
        RegionsTestSM<root_tk, RegionsCompleteObserver> sm_tk(&obs);
        {
            EXPECT_CALL(obs, on_complete_called).Times(0);
            sm_tk.post(e1{});
        }
    }

    {
        using root_kt = completion_parent<keep_leaf, terminate_leaf>;
        RegionsTestSM<root_kt, RegionsCompleteObserver> sm_kt(&obs);
        {
            EXPECT_CALL(obs, on_complete_called).Times(0);
            sm_kt.post(e1{});
        }
    }

    {
        using root_kk = completion_parent<keep_leaf, keep_leaf>;
        RegionsTestSM<root_kk, RegionsCompleteObserver> sm_kk(&obs);
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

    RegionsTestSM<targeted_root, RegionsCompleteObserver> sm(&obs);

    // target_capture_child terminating completes only targeted_parent's region, so only
    // targeted_parent's on_regions_finalized fires, not targeted_root's.
    {
        EXPECT_CALL(obs, on_complete_called(2)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_on_regions_finalized, on_regions_finalized_can_emit_follow_up_event)
{
    testing::StrictMock<RegionsCompleteObserver> obs;
    testing::InSequence sequence;

    struct Root
    {
        using regions = nil::xalt::tlist<emitting_parent, emit_sink>;
    };

    RegionsTestSM<Root, RegionsCompleteObserver> sm(&obs);

    {
        EXPECT_CALL(obs, on_emit_received(77)).Times(1);
        sm.post(e1{});
    }
}

TEST(sm_feature_on_regions_finalized, on_regions_finalized_can_transit_targeted_state)
{
    testing::StrictMock<RegionsCompleteObserver> obs;
    testing::InSequence sequence;

    RegionsTestSM<transit_source, RegionsCompleteObserver> sm(&obs);

    // transit_capture_child terminating completes transit_source's region naturally,
    // triggering its TransitTo<transit_target> from on_regions_finalized.
    sm.post(e1{});
    {
        EXPECT_CALL(obs, on_transit_after()).Times(1);
        sm.post(e2{});
    }
}
