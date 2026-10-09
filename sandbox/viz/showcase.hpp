#pragma once

#include "nil/sm/structs.hpp"
#include <nil/sm.hpp>
#include <nil/sm/barrier.hpp>

#include <string_view>
#include <variant>

// Only the IR of this machine is rendered; it is never posted events. It exercises:
//  - every event response: TransitTo, Terminate, Discard, Forward, Defer, DeferTo, Emit, variants
//  - lifecycle hooks: on_enter / on_exit / on_regions_finalized (NOOP, Emit, TransitTo, Terminate)
//  - captures, custom display names, orthogonal and nested regions
//  - args via props and direct_parent
//  - barriers: nested, and one definition used by two hosts
//  - unsatisfied args: the barriers need a telemetry prop that only the failed state provides
namespace viz_demo::showcase
{
    struct ev_next
    {
    };

    struct ev_fail
    {
    };

    struct ev_retry
    {
    };

    struct ev_reset
    {
    };

    struct ev_halt
    {
    };

    struct ev_log
    {
    };

    struct ev_audit
    {
    };

    struct ev_data
    {
        int value = 0;
    };

    struct config
    {
        int retries = 3;
    };

    struct telemetry
    {
        int samples = 0;
    };

    struct plant;
    struct loading;
    struct running;
    struct failed;
    struct done;
    struct monitor_ok;
    struct sup_watch;

    // ---- barrier: calibration (nested inside diagnostics) ----

    struct cal_hold
    {
        using events = nil::xalt::tlist<ev_next>;

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct cal_probe
    {
        using args = nil::xalt::tlist<config, telemetry>;
        using events = nil::xalt::tlist<ev_next, ev_fail>;

        cal_probe(config* /* cfg */, telemetry* /* sensors */)
        {
        }

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::TransitTo<cal_hold>{};
        }

        static auto on_event(const ev_fail& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    NIL_SM_BARRIER_DECLARE(calibration, nil::sm::api::Default<>);
    NIL_SM_BARRIER_DEFINE(calibration, cal_probe);
    using calibration_barrier = nil::sm::barrier::State<nil::sm::Terminate, calibration>;

    // ---- barrier: diagnostics ----

    struct diag_report
    {
        using events = nil::xalt::tlist<ev_reset>;

        static auto on_enter()
        {
            return nil::sm::Emit<ev_log>{};
        }

        static auto on_event(const ev_reset& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct diag_scan
    {
        using regions = nil::xalt::tlist<calibration_barrier>;
        using events = nil::xalt::tlist<ev_halt>;

        static auto on_event(const ev_halt& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_regions_finalized()
        {
            return nil::sm::TransitTo<diag_report>{};
        }
    };

    struct diag_start
    {
        using events = nil::xalt::tlist<ev_next>;

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::TransitTo<diag_scan>{};
        }
    };

    NIL_SM_BARRIER_DECLARE(diagnostics, nil::sm::api::Default<>, "diagnostics");
    NIL_SM_BARRIER_DEFINE(diagnostics, diag_start);
    using diagnostics_barrier = nil::sm::barrier::State<nil::sm::Terminate, diagnostics>;

    // ---- region: pipeline ----

    struct done
    {
        static constexpr std::string_view name = "pipeline:done";
        using events = nil::xalt::tlist<ev_reset>;

        static auto on_event(const ev_reset& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct worker_busy
    {
        using events = nil::xalt::tlist<ev_data, ev_halt, ev_next>;

        static auto on_event(const ev_data& /* event */)
            -> std::variant<nil::sm::Emit<ev_audit>, nil::sm::Discard>
        {
            return nil::sm::Discard{};
        }

        static auto on_event(const ev_halt& /* event */)
        {
            return nil::sm::Forward{};
        }

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct monitor_alarm
    {
        using events = nil::xalt::tlist<ev_reset, ev_next>;

        static auto on_event(const ev_reset& /* event */)
        {
            return nil::sm::TransitTo<monitor_ok>{};
        }

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct monitor_ok
    {
        using events = nil::xalt::tlist<ev_fail, ev_next>;

        static auto on_event(const ev_fail& /* event */)
        {
            return nil::sm::TransitTo<monitor_alarm>{};
        }

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct running
    {
        static constexpr std::string_view name = "pipeline:running";

        using regions = nil::xalt::tlist<worker_busy, monitor_ok>;
        using events = nil::xalt::tlist<ev_halt>;
        using captures = nil::xalt::tlist<ev_data>;

        static auto on_enter()
        {
            return nil::sm::NOOP{};
        }

        static auto on_exit()
        {
            return nil::sm::Emit<ev_log>{};
        }

        static auto on_capture(const ev_data& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_event(const ev_halt& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_regions_finalized()
        {
            return nil::sm::TransitTo<done>{};
        }
    };

    struct failed
    {
        static constexpr std::string_view name = "pipeline:failed";

        telemetry sensors;
        using props = nil::xalt::tlist<nil::sm::prop<telemetry, &failed::sensors>>;

        using regions = nil::xalt::tlist<diagnostics_barrier>;
        using events = nil::xalt::tlist<ev_retry, ev_halt>;

        static auto on_event(const ev_retry& /* event */)
        {
            return nil::sm::TransitTo<loading>{};
        }

        static auto on_event(const ev_halt& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct loading
    {
        static constexpr std::string_view name = "pipeline:loading";

        using events = nil::xalt::tlist<ev_next, ev_fail, ev_log>;

        static auto on_enter()
        {
            return nil::sm::Emit<ev_log>{};
        }

        static auto on_exit()
        {
            return nil::sm::NOOP{};
        }

        static auto on_event(const ev_next& /* event */)
            -> std::variant<nil::sm::TransitTo<running>, nil::sm::Discard>
        {
            return nil::sm::Discard{};
        }

        static auto on_event(const ev_fail& /* event */)
        {
            return nil::sm::TransitTo<failed>{};
        }

        static auto on_event(const ev_log& /* event */)
        {
            return nil::sm::Discard{};
        }
    };

    struct plant;

    struct idle
    {
        static constexpr std::string_view name = "pipeline:idle";

        using args = nil::xalt::tlist<nil::sm::direct_parent<plant>>;

        using events = nil::xalt::tlist<ev_next, ev_log, ev_data>;

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::TransitTo<loading>{};
        }

        static auto on_event(const ev_log& /* event */)
        {
            return nil::sm::Defer{};
        }

        static auto on_event(const ev_data& /* event */)
        {
            return nil::sm::DeferTo<loading>{};
        }
    };

    // ---- region: supervisor (args via props and direct_parent) ----

    struct sup_report
    {
        using args = nil::xalt::tlist<nil::sm::direct_parent<plant>>;
        using events = nil::xalt::tlist<ev_reset>;

        explicit sup_report(plant* /* parent */)
        {
        }

        static auto on_event(const ev_reset& /* event */)
        {
            return nil::sm::TransitTo<sup_watch>{};
        }
    };

    struct sup_watch
    {
        using args = nil::xalt::tlist<config>;
        using events = nil::xalt::tlist<ev_audit, ev_log, ev_halt>;

        config* cfg;

        explicit sup_watch(config* c)
            : cfg(c)
        {
        }

        static auto on_event(const ev_audit& /* event */)
        {
            return nil::sm::TransitTo<sup_report>{};
        }

        static auto on_event(const ev_log& /* event */)
        {
            return nil::sm::Forward{};
        }

        static auto on_event(const ev_halt& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    // ---- root ----

    struct plant
    {
        static constexpr std::string_view name = "plant";

        // pipeline, supervisor and the diagnostics barrier run orthogonally
        using regions = nil::xalt::tlist<idle, sup_watch, diagnostics_barrier>;
        using events = nil::xalt::tlist<ev_log>;
        using captures = nil::xalt::tlist<ev_audit>;

        config cfg;
        static constexpr auto cfg_ptr = &plant::cfg;
        using props = nil::xalt::tlist<nil::sm::prop<config, cfg_ptr>>;

        static auto on_capture(const ev_audit& /* event */)
        {
            return nil::sm::Discard{};
        }

        static auto on_event(const ev_log& /* event */)
        {
            return nil::sm::Discard{};
        }

        static auto on_regions_finalized()
        {
            return nil::sm::Terminate{};
        }
    };
}
