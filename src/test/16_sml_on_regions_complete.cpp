#include <nil/sml.hpp>

#include <gtest/gtest.h>

namespace
{
    struct e1
    {
    };

    struct e2
    {
    };

    struct out_complete
    {
        int value = 0;

        explicit out_complete(int v)
            : value(v)
        {
        }
    };

    struct completion_probe
    {
        static inline int calls = 0;

        static void reset()
        {
            calls = 0;
        }
    };

    struct target_probe
    {
        static inline int parent_calls = 0;
        static inline int root_calls = 0;
        static inline void* parent_state_ptr = nullptr;

        static void reset()
        {
            parent_calls = 0;
            root_calls = 0;
            parent_state_ptr = nullptr;
        }
    };

    struct emit_probe
    {
        static inline int count = 0;
        static inline int last_value = 0;

        static void reset()
        {
            count = 0;
            last_value = 0;
        }
    };

    struct transit_probe
    {
        static inline int before_calls = 0;
        static inline int after_calls = 0;
        static inline void* before_state_ptr = nullptr;

        static void reset()
        {
            before_calls = 0;
            after_calls = 0;
            before_state_ptr = nullptr;
        }
    };

    struct terminate_leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Terminate{};
        }
    };

    struct keep_leaf
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Discard{};
        }
    };

    template <typename R1, typename R2>
    struct completion_parent
    {
        using regions = nil::xalt::tlist<R1, R2>;

        static auto on_regions_complete()
        {
            ++completion_probe::calls;
            return nil::sml::NOOP{};
        }
    };

    struct target_capture_child
    {
        template <typename Parent>
        explicit target_capture_child(Parent* parent)
        {
            target_probe::parent_state_ptr = parent;
        }
    };

    struct targeted_parent
    {
        using regions = nil::xalt::tlist<target_capture_child>;

        static auto on_regions_complete()
        {
            ++target_probe::parent_calls;
            return nil::sml::NOOP{};
        }
    };

    struct targeted_root
    {
        using regions = nil::xalt::tlist<targeted_parent>;

        static auto on_regions_complete()
        {
            ++target_probe::root_calls;
            return nil::sml::NOOP{};
        }
    };

    struct emitting_parent
    {
        using regions = nil::xalt::tlist<terminate_leaf>;

        static auto on_regions_complete()
        {
            return nil::sml::Emit<out_complete>(77);
        }
    };

    struct emit_sink
    {
        using events = nil::xalt::tlist<out_complete>;

        static auto on_event(const out_complete& event)
        {
            ++emit_probe::count;
            emit_probe::last_value = event.value;
            return nil::sml::Discard{};
        }
    };

    struct transit_target
    {
        using events = nil::xalt::tlist<e2>;

        static auto on_event(const e2& /* event */)
        {
            ++transit_probe::after_calls;
            return nil::sml::Discard{};
        }
    };

    struct transit_capture_child
    {
        template <typename Parent>
        explicit transit_capture_child(Parent* parent)
        {
            transit_probe::before_state_ptr = parent;
        }
    };

    struct transit_source
    {
        using regions = nil::xalt::tlist<transit_capture_child>;
        using events = nil::xalt::tlist<e2>;

        static auto on_regions_complete()
        {
            return nil::sml::Transit<transit_target>{};
        }

        static auto on_event(const e2& /* event */)
        {
            ++transit_probe::before_calls;
            return nil::sml::Discard{};
        }
    };
}

TEST(sml_feature_on_regions_complete, triggers_only_when_all_regions_terminated)
{
    completion_probe::reset();

    {
        using root = completion_parent<terminate_leaf, terminate_leaf>;
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        sm.process_event(e1{});
        EXPECT_EQ(completion_probe::calls, 1);
    }

    completion_probe::reset();

    {
        using root = completion_parent<terminate_leaf, keep_leaf>;
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        sm.process_event(e1{});
        EXPECT_EQ(completion_probe::calls, 0);
    }

    completion_probe::reset();

    {
        using root = completion_parent<keep_leaf, terminate_leaf>;
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        sm.process_event(e1{});
        EXPECT_EQ(completion_probe::calls, 0);
    }

    completion_probe::reset();

    {
        using root = completion_parent<keep_leaf, keep_leaf>;
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        sm.process_event(e1{});
        EXPECT_EQ(completion_probe::calls, 0);
    }
}

TEST(sml_feature_on_regions_complete, explicit_target_reaches_nested_state_only)
{
    target_probe::reset();
    nil::sml::SM<nil::xalt::tlist<targeted_root>> sm{};
    ASSERT_NE(target_probe::parent_state_ptr, nullptr);

    sm.process_event(nil::sml::detail::EvRegionsComplete{target_probe::parent_state_ptr});

    EXPECT_EQ(target_probe::parent_calls, 1);
    EXPECT_EQ(target_probe::root_calls, 0);
}

TEST(sml_feature_on_regions_complete, on_regions_complete_can_emit_follow_up_event)
{
    emit_probe::reset();
    nil::sml::SM<nil::xalt::tlist<emitting_parent, emit_sink>> sm{};

    sm.process_event(e1{});

    EXPECT_EQ(emit_probe::count, 1);
    EXPECT_EQ(emit_probe::last_value, 77);
}

TEST(sml_feature_on_regions_complete, on_regions_complete_can_transit_targeted_state)
{
    transit_probe::reset();
    nil::sml::SM<nil::xalt::tlist<transit_source>> sm{};
    ASSERT_NE(transit_probe::before_state_ptr, nullptr);

    sm.process_event(nil::sml::detail::EvRegionsComplete{transit_probe::before_state_ptr});
    sm.process_event(e2{});

    EXPECT_EQ(transit_probe::before_calls, 0);
    EXPECT_EQ(transit_probe::after_calls, 1);
}
