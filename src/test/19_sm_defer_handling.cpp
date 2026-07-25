#include <nil/sm.hpp>

#include "test_api.hpp"

#include <gtest/gtest.h>

namespace
{
    // Events shared across defer tests
    struct e_save
    {
    };

    struct e_go
    {
    };

    struct e_data
    {
        int v = 0;
    };

    // Observer to track which events were received
    class DeferObserver
    {
    public:
        MOCK_METHOD(void, event_received, (), ());
        MOCK_METHOD(void, value_received, (int), ());
    };

    // ---- States for basic defer + transit test ----

    // Receives e_save events; reports to observer
    struct SaveReceiver
    {
        using events = nil::xalt::tlist<e_save>;

        DeferObserver* obs;

        explicit SaveReceiver(auto* /* parent */, DeferObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e_save& /* event */) const
        {
            obs->event_received();
            return Discard{};
        }
    };

    // Defers e_save; transits to SaveReceiver on e_go
    struct SaveDefer
    {
        using events = nil::xalt::tlist<e_save, e_go>;

        static auto on_event(const e_save& /* event */)
        {
            return Defer{};
        }

        static auto on_event(const e_go& /* event */)
        {
            return Transit<SaveReceiver>{};
        }
    };

    struct DeferTransitRoot
    {
        using regions = nil::xalt::tlist<SaveDefer>;
    };

    template <typename T>
    using DeferTestAPI = nil::sm::default_api<T, DeferObserver, void>;

    template <typename... Regions>
    using DeferTestSM = nil::sm::SM<DeferTestAPI, Regions...>;

    // ---- States for payload preservation test ----

    // Receives e_data events; reports payload value to observer
    struct DataReceiver
    {
        using events = nil::xalt::tlist<e_data>;

        DeferObserver* obs;

        explicit DataReceiver(auto* /* parent */, DeferObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e_data& ev) const
        {
            obs->value_received(ev.v);
            return Discard{};
        }
    };

    // Defers e_data; transits to DataReceiver on e_go
    struct DataDefer
    {
        using events = nil::xalt::tlist<e_data, e_go>;

        static auto on_event(const e_data& /* event */)
        {
            return Defer{};
        }

        static auto on_event(const e_go& /* event */)
        {
            return Transit<DataReceiver>{};
        }
    };

    struct DataDeferTransitRoot
    {
        using regions = nil::xalt::tlist<DataDefer>;
    };

    // ---- States for orthogonal region defer test ----

    // ---- States for orthogonal region defer test ----
    // Region 2 uses e_data so its deferred event doesn't cross-dispatch to region 1

    struct DataReceiver2
    {
        using events = nil::xalt::tlist<e_data>;

        DeferObserver* obs;

        explicit DataReceiver2(auto* /* parent */, DeferObserver* o)
            : obs(o)
        {
        }

        auto on_event(const e_data& /* event */) const
        {
            obs->event_received();
            return Discard{};
        }
    };

    struct DataDefer2
    {
        using events = nil::xalt::tlist<e_data, e_go>;

        static auto on_event(const e_data& /* event */)
        {
            return Defer{};
        }

        static auto on_event(const e_go& /* event */)
        {
            return Transit<DataReceiver2>{};
        }
    };

    struct OrthogonalDeferRoot
    {
        using regions = nil::xalt::tlist<SaveDefer, DataDefer2>;
    };

    // Test: deferred event is dispatched to new state after transit
    TEST(sm_feature_defer_handling, defer_then_transit_flushes_event)
    {
        testing::StrictMock<DeferObserver> obs;

        DeferTestSM<DeferTransitRoot> sm(&obs, {});

        // DeferState defers e_save
        {
            sm.post(e_save{});
        }

        // DeferState -> SaveReceiver (flush), SaveReceiver handles deferred e_save
        {
            EXPECT_CALL(obs, event_received()).Times(1);
            sm.post(e_go{});
        }
    }

    // Test: deferred event payload is preserved through the defer/flush cycle
    TEST(sm_feature_defer_handling, deferred_payload_preserved)
    {
        testing::StrictMock<DeferObserver> obs;

        DeferTestSM<DataDeferTransitRoot> sm(&obs, {});

        {
            sm.post(e_data{42});
        }

        {
            EXPECT_CALL(obs, value_received(42)).Times(1);
            sm.post(e_go{});
        }
    }

    // Test: multiple deferred events are all flushed and dispatched on transit
    TEST(sm_feature_defer_handling, multiple_defers_all_flushed)
    {
        testing::StrictMock<DeferObserver> obs;

        DeferTestSM<DeferTransitRoot> sm(&obs, {});

        {
            sm.post(e_save{});
        }
        {
            sm.post(e_save{});
        }
        {
            sm.post(e_save{});
        }

        {
            EXPECT_CALL(obs, event_received()).Times(3);
            sm.post(e_go{});
        }
    }

    // Test: orthogonal regions defer independently; each flushes on its own transit
    // Region 1 defers e_save; Region 2 defers e_data (different types to avoid cross-dispatch)
    TEST(sm_feature_defer_handling, orthogonal_regions_defer_independently)
    {
        testing::StrictMock<DeferObserver> obs;

        DeferTestSM<OrthogonalDeferRoot> sm(&obs, {});

        // Region 1 defers e_save, Region 2 defers e_data
        {
            sm.post(e_save{});
        }
        {
            sm.post(e_data{});
        }

        // Both regions transit on e_go: each flushes its own deferred event
        // Region 1: SaveDefer→SaveReceiver, receives e_save → event_received()
        // Region 2: DataDefer2→DataReceiver2, receives e_data → event_received()
        {
            EXPECT_CALL(obs, event_received()).Times(2);
            sm.post(e_go{});
        }
    }
}
