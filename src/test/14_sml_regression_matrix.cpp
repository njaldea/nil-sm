#include <nil/sml.hpp>

#include <gtest/gtest.h>

namespace
{
    struct e1
    {
    };

    enum class region_rx
    {
        unhandled,
        forward,
        discard,
        transit,
    };

    struct probe
    {
        int parent_calls = 0;
        int r1_source_calls = 0;
        int r2_source_calls = 0;
        int r1_target_calls = 0;
        int r2_target_calls = 0;
    };

    probe* g_probe = nullptr;

    struct probe_scope
    {
        explicit probe_scope(probe& p)
        {
            g_probe = &p;
        }

        ~probe_scope()
        {
            g_probe = nullptr;
        }
    };

    template <int I>
    void note_source()
    {
        if constexpr (I == 1)
        {
            ++g_probe->r1_source_calls;
        }
        else
        {
            ++g_probe->r2_source_calls;
        }
    }

    template <int I>
    void note_target()
    {
        if constexpr (I == 1)
        {
            ++g_probe->r1_target_calls;
        }
        else
        {
            ++g_probe->r2_target_calls;
        }
    }

    template <region_rx K, int I>
    struct region_source;

    template <int I>
    struct region_source<region_rx::unhandled, I>
    {
        static constexpr region_rx kind = region_rx::unhandled;
    };

    template <int I>
    struct region_target
    {
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            note_target<I>();
            return nil::sml::Discard{};
        }
    };

    template <int I>
    struct region_source<region_rx::forward, I>
    {
        static constexpr region_rx kind = region_rx::forward;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            note_source<I>();
            return nil::sml::Forward{};
        }
    };

    template <int I>
    struct region_source<region_rx::discard, I>
    {
        static constexpr region_rx kind = region_rx::discard;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            note_source<I>();
            return nil::sml::Discard{};
        }
    };

    template <int I>
    struct region_source<region_rx::transit, I>
    {
        static constexpr region_rx kind = region_rx::transit;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            note_source<I>();
            return nil::sml::Transit<region_target<I>>();
        }
    };

    template <typename R1, typename R2>
    struct parent
    {
        using regions = nil::xalt::tlist<R1, R2>;
        using events = nil::xalt::tlist<e1>;

        static auto on_event(const e1& /* event */)
        {
            ++g_probe->parent_calls;
            return nil::sml::Discard{};
        }
    };

    constexpr bool parent_should_be_called(region_rx r1, region_rx r2)
    {
        return r1 == region_rx::forward || r2 == region_rx::forward
            || (r1 == region_rx::unhandled && r2 == region_rx::unhandled);
    }

    template <region_rx K1, region_rx K2>
    void run_matrix_case(const char* label)
    {
        using r1 = region_source<K1, 1>;
        using r2 = region_source<K2, 2>;
        using root = parent<r1, r2>;

        probe p;
        probe_scope scope(p);
        nil::sml::SM<nil::xalt::tlist<root>> sm{};

        sm.process_event(e1{});

        const bool expect_parent = parent_should_be_called(K1, K2);
        EXPECT_EQ(p.parent_calls, expect_parent ? 1 : 0) << label;

        const int expect_r1_source_first = (K1 == region_rx::unhandled) ? 0 : 1;
        const int expect_r2_source_first = (K2 == region_rx::unhandled) ? 0 : 1;
        EXPECT_EQ(p.r1_source_calls, expect_r1_source_first) << label;
        EXPECT_EQ(p.r2_source_calls, expect_r2_source_first) << label;

        sm.process_event(e1{});

        const int expect_r1_target = (K1 == region_rx::transit) ? 1 : 0;
        const int expect_r2_target = (K2 == region_rx::transit) ? 1 : 0;
        EXPECT_EQ(p.r1_target_calls, expect_r1_target) << label;
        EXPECT_EQ(p.r2_target_calls, expect_r2_target) << label;

        const int expect_r1_source_total
            = (K1 == region_rx::unhandled) ? 0 : ((K1 == region_rx::transit) ? 1 : 2);
        const int expect_r2_source_total
            = (K2 == region_rx::unhandled) ? 0 : ((K2 == region_rx::transit) ? 1 : 2);
        EXPECT_EQ(p.r1_source_calls, expect_r1_source_total) << label;
        EXPECT_EQ(p.r2_source_calls, expect_r2_source_total) << label;
    }
}

TEST(sml_feature_regression_matrix, orthogonal_two_region_reaction_matrix)
{
    run_matrix_case<region_rx::unhandled, region_rx::unhandled>("UU");
    run_matrix_case<region_rx::unhandled, region_rx::forward>("UF");
    run_matrix_case<region_rx::unhandled, region_rx::discard>("UD");
    run_matrix_case<region_rx::unhandled, region_rx::transit>("UT");

    run_matrix_case<region_rx::forward, region_rx::unhandled>("FU");
    run_matrix_case<region_rx::forward, region_rx::forward>("FF");
    run_matrix_case<region_rx::forward, region_rx::discard>("FD");
    run_matrix_case<region_rx::forward, region_rx::transit>("FT");

    run_matrix_case<region_rx::discard, region_rx::unhandled>("DU");
    run_matrix_case<region_rx::discard, region_rx::forward>("DF");
    run_matrix_case<region_rx::discard, region_rx::discard>("DD");
    run_matrix_case<region_rx::discard, region_rx::transit>("DT");

    run_matrix_case<region_rx::transit, region_rx::unhandled>("TU");
    run_matrix_case<region_rx::transit, region_rx::forward>("TF");
    run_matrix_case<region_rx::transit, region_rx::discard>("TD");
    run_matrix_case<region_rx::transit, region_rx::transit>("TT");
}
