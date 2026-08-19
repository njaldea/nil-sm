#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
    enum class region_rx
    {
        unhandled,
        forward,
        discard,
        transit,
    };

    class RegressionMatrixObserver
    {
    public:
        MOCK_METHOD(void, on_parent_event, (const e1&), ());
        MOCK_METHOD(void, on_source_event, (int region_id, const e1&), ());
        MOCK_METHOD(void, on_target_event, (int region_id, const e1&), ());
    };

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

        RegressionMatrixObserver* obs;

        explicit region_target(auto* /* parent */, RegressionMatrixObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e1& event) const
        {
            obs->on_target_event(I, event);
            return nil::sm::Discard{};
        }
    };

    template <int I>
    struct region_source<region_rx::forward, I>
    {
        static constexpr region_rx kind = region_rx::forward;
        using events = nil::xalt::tlist<e1>;

        RegressionMatrixObserver* obs;

        explicit region_source(auto* /* parent */, RegressionMatrixObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e1& event) const
        {
            obs->on_source_event(I, event);
            return nil::sm::Forward{};
        }
    };

    template <int I>
    struct region_source<region_rx::discard, I>
    {
        static constexpr region_rx kind = region_rx::discard;
        using events = nil::xalt::tlist<e1>;

        RegressionMatrixObserver* obs;

        explicit region_source(auto* /* parent */, RegressionMatrixObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e1& event) const
        {
            obs->on_source_event(I, event);
            return Discard{};
        }
    };

    template <int I>
    struct region_source<region_rx::transit, I>
    {
        static constexpr region_rx kind = region_rx::transit;
        using events = nil::xalt::tlist<e1>;

        RegressionMatrixObserver* obs;

        explicit region_source(auto* /* parent */, RegressionMatrixObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e1& event) const
        {
            obs->on_source_event(I, event);
            return Transit<region_target<I>>();
        }
    };

    template <typename R1, typename R2>
    struct parent
    {
        using regions = nil::xalt::tlist<R1, R2>;
        using events = nil::xalt::tlist<e1>;

        RegressionMatrixObserver* obs;

        explicit parent(auto* /* parent */, RegressionMatrixObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e1& event) const
        {
            obs->on_parent_event(event);
            return Discard{};
        }
    };

    constexpr bool parent_should_be_called(region_rx r1, region_rx r2)
    {
        return r1 == region_rx::forward || r2 == region_rx::forward
            || (r1 == region_rx::unhandled && r2 == region_rx::unhandled);
    }

    constexpr bool parent_should_be_called_after_transits(region_rx r1, region_rx r2)
    {
        return parent_should_be_called(
            r1 == region_rx::transit ? region_rx::discard : r1,
            r2 == region_rx::transit ? region_rx::discard : r2
        );
    }

    template <typename T>
    using MatrixTestAPI = nil::sm::api::Default<T, RegressionMatrixObserver, void>;

    template <typename... Regions>
    using MatrixTestSM = nil::sm::SM<MatrixTestAPI, Regions...>;

    template <region_rx K1, region_rx K2>
    void run_matrix_case(const char* /* label */)
    {
        using r1 = region_source<K1, 1>;
        using r2 = region_source<K2, 2>;
        using root = parent<r1, r2>;

        testing::StrictMock<RegressionMatrixObserver> obs;

        MatrixTestSM<root> sm(&obs, {});

        {
            testing::InSequence sequence;

            if constexpr (K1 != region_rx::unhandled)
            {
                EXPECT_CALL(obs, on_source_event(1, testing::_)).Times(1);
            }
            if constexpr (K2 != region_rx::unhandled)
            {
                EXPECT_CALL(obs, on_source_event(2, testing::_)).Times(1);
            }
            if constexpr (parent_should_be_called(K1, K2))
            {
                EXPECT_CALL(obs, on_parent_event(testing::_)).Times(1);
            }
            sm.post(e1{});
        }

        {
            testing::InSequence sequence;

            if constexpr (K1 == region_rx::transit)
            {
                EXPECT_CALL(obs, on_target_event(1, testing::_)).Times(1);
            }
            else if constexpr (K1 != region_rx::unhandled)
            {
                EXPECT_CALL(obs, on_source_event(1, testing::_)).Times(1);
            }
            if constexpr (K2 == region_rx::transit)
            {
                EXPECT_CALL(obs, on_target_event(2, testing::_)).Times(1);
            }
            else if constexpr (K2 != region_rx::unhandled)
            {
                EXPECT_CALL(obs, on_source_event(2, testing::_)).Times(1);
            }
            if constexpr (parent_should_be_called_after_transits(K1, K2))
            {
                EXPECT_CALL(obs, on_parent_event(testing::_)).Times(1);
            }
            sm.post(e1{});
        }
    }
}

TEST(sm_feature_regression_matrix, orthogonal_two_region_reaction_matrix)
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
