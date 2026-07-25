#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

namespace
{
    struct out_a
    {
        int value = 0;

        explicit out_a(int v)
            : value(v)
        {
        }
    };

    struct out_b
    {
        int value = 0;

        explicit out_b(int v)
            : value(v)
        {
        }
    };

    class EmitObserver
    {
    public:
        MOCK_METHOD(void, received_a, (int value), ());
        MOCK_METHOD(void, received_b, (int value), ());
        MOCK_METHOD(void, parent_handled, (), ());
    };

    // Sink state that calls the observer when it receives emitted events
    struct EmitSink
    {
        using events = nil::xalt::tlist<out_a, out_b>;

        EmitObserver* obs;

        explicit EmitSink(auto* /* parent */, EmitObserver* o)
            : obs(o)
        {
        }

        auto on_event(const out_a& ev) const
        {
            obs->received_a(ev.value);
            return Discard{};
        }

        auto on_event(const out_b& ev) const
        {
            obs->received_b(ev.value);
            return Discard{};
        }
    };

    // Leaf that emits out_a(11) when it receives e1
    struct EmitLeaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Emit<out_a>(11);
        }
    };

    struct e_emit_or_fwd
    {
        bool emit = false;
    };

    // Leaf that emits out_a(21) or forwards based on event payload
    struct EmitOrForwardLeaf
    {
        using events = nil::xalt::tlist<e_emit_or_fwd>;

        static auto on_event(const e_emit_or_fwd& ev) -> std::variant<Emit<out_a>, Forward>
        {
            if (ev.emit)
            {
                return Emit<out_a>(21);
            }
            return Forward{};
        }
    };

    // Parent that emits out_b
    struct EmitParent
    {
        using regions = nil::xalt::tlist<struct UnhandledChild>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Emit<out_b>(22);
        }
    };

    struct UnhandledChild
    {
        // no events
    };

    // Leaf that emits out_a on e1 and out_b on e2
    struct EmitSequenceLeaf
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            return Emit<out_a>(31);
        }

        static auto on_event(const e2& /* event */)
        {
            return Emit<out_b>(32);
        }
    };

    template <typename T>
    using EmitTestAPI = nil::sm::default_api<T, EmitObserver, void>;

    template <typename... Regions>
    using EmitTestSM = nil::sm::SM<EmitTestAPI, Regions...>;

    struct ForwardParent
    {
        using regions = nil::xalt::tlist<EmitOrForwardLeaf>;
        using events = nil::xalt::tlist<e_emit_or_fwd>;

        EmitObserver* obs;

        explicit ForwardParent(auto* /* parent */, EmitObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e_emit_or_fwd& /* ev */) const
        {
            obs->parent_handled();
            return Discard{};
        }
    };
}

// Test: emit from leaf reaction is delivered to sink
TEST(sm_feature_emit_handling, emit_from_leaf_reaction)
{
    testing::StrictMock<EmitObserver> obs;

    EmitTestSM<EmitLeaf, EmitSink> sm(&obs, {});
    {
        EXPECT_CALL(obs, received_a(11)).Times(1);
        sm.post(e1{});
    }
}

// Test: emit during forward path is delivered
TEST(sm_feature_emit_handling, emit_during_forward_path)
{
    testing::StrictMock<EmitObserver> obs;
    testing::InSequence sequence;

    EmitTestSM<ForwardParent, EmitSink> sm(&obs, {});
    {
        EXPECT_CALL(obs, received_a(21)).Times(1);
        EXPECT_CALL(obs, parent_handled()).Times(1);
        sm.post(e_emit_or_fwd{.emit = true});
    }
    {
        sm.post(e_emit_or_fwd{.emit = false});
    }
}

// Test: emit from parent reaction is delivered to sink
TEST(sm_feature_emit_handling, emit_from_parent_reaction)
{
    testing::StrictMock<EmitObserver> obs;

    EmitTestSM<EmitParent, EmitSink> sm(&obs, {});
    {
        EXPECT_CALL(obs, received_b(22)).Times(1);
        sm.post(e1{});
    }
}

// Test: both orthogonal regions emit, both arrive at sink
TEST(sm_feature_emit_handling, emit_from_orthogonal_regions)
{
    struct EmitLeaf1
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Emit<out_a>(11);
        }
    };

    struct EmitLeaf2
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return Emit<out_a>(11);
        }
    };

    testing::StrictMock<EmitObserver> obs;

    EmitTestSM<EmitLeaf1, EmitLeaf2, EmitSink> sm(&obs, {});
    {
        EXPECT_CALL(obs, received_a(11)).Times(2);
        sm.post(e1{});
    }
}

// Test: multiple emit events preserve order
TEST(sm_feature_emit_handling, multiple_emit_events_preserve_order)
{
    testing::StrictMock<EmitObserver> obs;
    testing::InSequence sequence;

    EmitTestSM<EmitSequenceLeaf, EmitSink> sm(&obs, {});
    {
        EXPECT_CALL(obs, received_a(31)).Times(1);
        sm.post(e1{});
    }
    {
        EXPECT_CALL(obs, received_b(32)).Times(1);
        sm.post(e2{});
    }
    {
        EXPECT_CALL(obs, received_a(31)).Times(1);
        sm.post(e1{});
    }
}
