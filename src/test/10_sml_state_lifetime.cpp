#include <nil/sml.hpp>

#include <gtest/gtest.h>

namespace
{
    struct e1
    {
    };

    template <typename T>
    struct counters
    {
        static inline int ctor = 0;
        static inline int dtor = 0;
        static inline int react = 0;
    };

    template <typename T>
    void reset_counters()
    {
        counters<T>::ctor = 0;
        counters<T>::dtor = 0;
        counters<T>::react = 0;
    }

    template <typename T>
    struct counted
    {
        counted()
        {
            ++counters<T>::ctor;
        }

        ~counted()
        {
            ++counters<T>::dtor;
        }
    };

    template <typename Tag>
    struct leaf_discard: counted<leaf_discard<Tag>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            ++counters<leaf_discard<Tag>>::react;
            return nil::sml::Discard{};
        }
    };

    template <typename Tag>
    struct leaf_no_events: counted<leaf_no_events<Tag>>
    {
    };

    template <typename Tag, typename Target>
    struct leaf_transit: counted<leaf_transit<Tag, Target>>
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            ++counters<leaf_transit<Tag, Target>>::react;
            return nil::sml::Transit<Target>();
        }
    };

    template <typename Tag, typename... Regions>
    struct node_no_events: counted<node_no_events<Tag, Regions...>>
    {
        using regions = nil::xalt::tlist<Regions...>;
    };
}

TEST(sml_feature_state_lifetime, state_constructor_called_once)
{
    struct tag_state
    {
    };

    using state = leaf_discard<tag_state>;

    reset_counters<state>();

    {
        nil::sml::SM<nil::xalt::tlist<state>> sm{};
        EXPECT_EQ(counters<state>::ctor, 1);
        EXPECT_EQ(counters<state>::dtor, 0);

        sm.process_event(e1{});
        EXPECT_EQ(counters<state>::react, 1);
    }

    EXPECT_EQ(counters<state>::dtor, 1);
}

TEST(sml_feature_state_lifetime, transition_destroys_previous_and_creates_new_instance)
{
    struct tag_source
    {
    };

    struct tag_target
    {
    };

    using target = leaf_discard<tag_target>;
    using source = leaf_transit<tag_source, target>;

    reset_counters<source>();
    reset_counters<target>();

    {
        nil::sml::SM<nil::xalt::tlist<source>> sm{};

        EXPECT_EQ(counters<source>::ctor, 1);
        EXPECT_EQ(counters<target>::ctor, 0);

        sm.process_event(e1{});

        EXPECT_EQ(counters<source>::react, 1);
        EXPECT_EQ(counters<source>::dtor, 1);
        EXPECT_EQ(counters<target>::ctor, 1);

        sm.process_event(e1{});
        EXPECT_EQ(counters<target>::react, 1);
    }

    EXPECT_EQ(counters<target>::dtor, 1);
}

TEST(sml_feature_state_lifetime, orthogonal_region_destruction)
{
    struct tag_r1
    {
    };

    struct tag_r2
    {
    };

    using r1 = leaf_no_events<tag_r1>;
    using r2 = leaf_no_events<tag_r2>;

    reset_counters<r1>();
    reset_counters<r2>();

    {
        nil::sml::SM<nil::xalt::tlist<r1, r2>> sm{};
        EXPECT_EQ(counters<r1>::ctor, 1);
        EXPECT_EQ(counters<r2>::ctor, 1);
    }

    EXPECT_EQ(counters<r1>::dtor, 1);
    EXPECT_EQ(counters<r2>::dtor, 1);
}

TEST(sml_feature_state_lifetime, parent_destruction_destroys_all_children)
{
    struct tag_root
    {
    };

    struct tag_child
    {
    };

    struct tag_grandchild
    {
    };

    using grandchild = leaf_no_events<tag_grandchild>;
    using child = node_no_events<tag_child, grandchild>;
    using root = node_no_events<tag_root, child>;

    reset_counters<root>();
    reset_counters<child>();
    reset_counters<grandchild>();

    {
        nil::sml::SM<nil::xalt::tlist<root>> sm{};
        EXPECT_EQ(counters<root>::ctor, 1);
        EXPECT_EQ(counters<child>::ctor, 1);
        EXPECT_EQ(counters<grandchild>::ctor, 1);
    }

    EXPECT_EQ(counters<grandchild>::dtor, 1);
    EXPECT_EQ(counters<child>::dtor, 1);
    EXPECT_EQ(counters<root>::dtor, 1);
}
