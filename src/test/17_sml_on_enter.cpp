#include <nil/sml.hpp>

#include <gtest/gtest.h>

namespace
{
    struct e1
    {
    };

    struct e4
    {
    };

    template <typename T>
    struct counts
    {
        static inline int reacts = 0;
    };

    template <typename T>
    void reset_reacts()
    {
        counts<T>::reacts = 0;
    }

    template <typename Tag>
    struct emit_on_enter
    {
        static auto on_enter()
        {
            return nil::sml::Emit<e4>();
        }
    };

    template <typename Tag>
    struct on_enter_emit_sink
    {
        using events = nil::xalt::tlist<e4>;

        static auto on_event(const e4& /* event */)
        {
            ++counts<on_enter_emit_sink<Tag>>::reacts;
            return nil::sml::Discard{};
        }
    };
}

TEST(sml_feature_on_enter, on_enter_can_publish_event)
{
    using publisher = emit_on_enter<struct tag_on_enter_publisher>;
    using sink = on_enter_emit_sink<struct tag_on_enter_sink>;

    reset_reacts<sink>();

    nil::sml::SM<nil::xalt::tlist<publisher, sink>> sm{};

    EXPECT_EQ(counts<sink>::reacts, 0);
    sm.process_event(e1{});
    EXPECT_EQ(counts<sink>::reacts, 1);
}
