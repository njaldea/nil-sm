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

    struct e3
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
    struct discard_on_e1
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            ++counts<discard_on_e1<Tag>>::reacts;
            return nil::sml::Discard{};
        }
    };

    template <typename Tag>
    struct no_events_region
    {
    };

    template <typename Tag>
    struct empty_regions_root
    {
        using regions = nil::xalt::tlist<>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            ++counts<empty_regions_root<Tag>>::reacts;
            return nil::sml::Discard{};
        }
    };

    template <typename Tag, typename... Regions>
    struct root_with_regions
    {
        using regions = nil::xalt::tlist<Regions...>;
    };

    template <typename Tag, typename Child>
    struct chain_node
    {
        using regions = nil::xalt::tlist<Child>;
    };

    template <typename Tag>
    struct reentrant_state
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Emit<e2>();
        }

        static auto on_event(const e2& /* event */)
        {
            ++counts<reentrant_state<Tag>>::reacts;
            return nil::sml::Discard{};
        }
    };

    template <typename Tag>
    struct reentrant_chain_state
    {
        using events = nil::xalt::tlist<e1, e2, e3>;

        static auto on_event(const e1& /* event */)
        {
            return nil::sml::Emit<e2>();
        }

        static auto on_event(const e2& /* event */)
        {
            ++counts<reentrant_chain_state<Tag>>::reacts;
            return nil::sml::Emit<e3>();
        }

        static auto on_event(const e3& /* event */)
        {
            ++counts<reentrant_chain_state<Tag>>::reacts;
            return nil::sml::Discard{};
        }
    };

    template <typename Tag>
    struct terminate_on_e1
    {
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            ++counts<terminate_on_e1<Tag>>::reacts;
            return nil::sml::Terminate{};
        }

        static auto on_event(const e2& /* event */)
        {
            ++counts<terminate_on_e1<Tag>>::reacts;
            return nil::sml::Discard{};
        }
    };

    template <typename Tag, typename Child>
    struct parent_with_child_termination
    {
        using regions = nil::xalt::tlist<Child>;
        using events = nil::xalt::tlist<e1, e2>;

        static auto on_event(const e1& /* event */)
        {
            ++counts<parent_with_child_termination<Tag, Child>>::reacts;
            return nil::sml::Discard{};
        }

        static auto on_event(const e2& /* event */)
        {
            ++counts<parent_with_child_termination<Tag, Child>>::reacts;
            return nil::sml::Discard{};
        }
    };

}

TEST(sml_feature_edge_cases, state_with_no_regions)
{
    struct tag_root
    {
    };

    using root = discard_on_e1<tag_root>;

    reset_reacts<root>();

    nil::sml::SM<nil::xalt::tlist<root>> sm{};
    sm.process_event(e1{});

    EXPECT_EQ(counts<root>::reacts, 1);
}

TEST(sml_feature_edge_cases, state_with_no_events)
{
    struct tag_root
    {
    };

    struct tag_child
    {
    };

    using child = discard_on_e1<tag_child>;
    using root = root_with_regions<tag_root, child>;

    reset_reacts<child>();

    nil::sml::SM<nil::xalt::tlist<root>> sm{};
    sm.process_event(e1{});

    EXPECT_EQ(counts<child>::reacts, 1);
}

TEST(sml_feature_edge_cases, composite_with_empty_regions)
{
    struct tag_root
    {
    };

    using root = empty_regions_root<tag_root>;

    reset_reacts<root>();

    nil::sml::SM<nil::xalt::tlist<root>> sm{};
    sm.process_event(e1{});

    EXPECT_EQ(counts<root>::reacts, 1);
}

TEST(sml_feature_edge_cases, deep_hierarchy_ten_plus_levels)
{
    struct tag_leaf
    {
    };

    using leaf = discard_on_e1<tag_leaf>;
    using n10 = chain_node<struct tag10, leaf>;
    using n9 = chain_node<struct tag9, n10>;
    using n8 = chain_node<struct tag8, n9>;
    using n7 = chain_node<struct tag7, n8>;
    using n6 = chain_node<struct tag6, n7>;
    using n5 = chain_node<struct tag5, n6>;
    using n4 = chain_node<struct tag4, n5>;
    using n3 = chain_node<struct tag3, n4>;
    using n2 = chain_node<struct tag2, n3>;
    using n1 = chain_node<struct tag1, n2>;

    reset_reacts<leaf>();

    nil::sml::SM<nil::xalt::tlist<n1>> sm{};
    sm.process_event(e1{});

    EXPECT_EQ(counts<leaf>::reacts, 1);
}

TEST(sml_feature_edge_cases, many_orthogonal_regions)
{
    using r0 = discard_on_e1<struct tag0>;
    using r1 = discard_on_e1<struct tag1>;
    using r2 = discard_on_e1<struct tag2>;
    using r3 = discard_on_e1<struct tag3>;
    using r4 = discard_on_e1<struct tag4>;
    using r5 = discard_on_e1<struct tag5>;
    using r6 = discard_on_e1<struct tag6>;
    using r7 = discard_on_e1<struct tag7>;

    reset_reacts<r0>();
    reset_reacts<r1>();
    reset_reacts<r2>();
    reset_reacts<r3>();
    reset_reacts<r4>();
    reset_reacts<r5>();
    reset_reacts<r6>();
    reset_reacts<r7>();

    nil::sml::SM<nil::xalt::tlist<r0, r1, r2, r3, r4, r5, r6, r7>> sm{};
    sm.process_event(e1{});

    EXPECT_EQ(counts<r0>::reacts, 1);
    EXPECT_EQ(counts<r1>::reacts, 1);
    EXPECT_EQ(counts<r2>::reacts, 1);
    EXPECT_EQ(counts<r3>::reacts, 1);
    EXPECT_EQ(counts<r4>::reacts, 1);
    EXPECT_EQ(counts<r5>::reacts, 1);
    EXPECT_EQ(counts<r6>::reacts, 1);
    EXPECT_EQ(counts<r7>::reacts, 1);
}

TEST(sml_feature_edge_cases, event_not_present_anywhere)
{
    using root = root_with_regions<struct root_tag, no_events_region<struct child_tag>>;

    nil::sml::SM<nil::xalt::tlist<root>> sm{};
    sm.process_event(e1{});
    sm.process_event(e2{});

    SUCCEED();
}

TEST(sml_feature_edge_cases, reentrant_event_emission)
{
    using state = reentrant_state<struct tag_state>;
    reset_reacts<state>();

    nil::sml::SM<nil::xalt::tlist<state>> sm{};

    sm.process_event(e1{});

    EXPECT_EQ(counts<state>::reacts, 1);
}

TEST(sml_feature_edge_cases, reentrant_event_emission_chain)
{
    using state = reentrant_chain_state<struct tag_state_chain>;
    reset_reacts<state>();

    nil::sml::SM<nil::xalt::tlist<state>> sm{};

    sm.process_event(e1{});

    EXPECT_EQ(counts<state>::reacts, 2);
}

TEST(sml_feature_edge_cases, terminate_region_becomes_null_and_parent_handles_later_events)
{
    using child = terminate_on_e1<struct tag_terminated_child>;
    using root = parent_with_child_termination<struct tag_parent_after_terminate, child>;

    reset_reacts<child>();
    reset_reacts<root>();

    nil::sml::SM<nil::xalt::tlist<root>> sm{};

    sm.process_event(e1{});
    EXPECT_EQ(counts<child>::reacts, 1);
    EXPECT_EQ(counts<root>::reacts, 0);

    sm.process_event(e2{});
    EXPECT_EQ(counts<child>::reacts, 1);
    EXPECT_EQ(counts<root>::reacts, 1);
}
