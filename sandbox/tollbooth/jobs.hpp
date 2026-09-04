#pragma once

#include "common.hpp"

#include <nil/sm.hpp>

#include <string_view>

namespace toll::tag
{
    struct startup final
    {
        static constexpr std::string_view name = "job:startup";
    };

    struct collection final
    {
        static constexpr std::string_view name = "job:collection";
    };

    struct shift final
    {
        static constexpr std::string_view name = "job:shift";
    };

    struct maintenance final
    {
        static constexpr std::string_view name = "job:maintenance";
    };

    // The gate arm chain is instantiated once per owning job; Job keeps the types distinct.
    template <typename Job>
    struct gate_closed final
    {
        static constexpr std::string_view name = "gate:closed";
    };

    template <typename Job>
    struct gate_opening final
    {
        static constexpr std::string_view name = "gate:opening";
    };

    template <typename Job>
    struct gate_open final
    {
        static constexpr std::string_view name = "gate:open";
    };

    template <typename Job>
    struct gate_closing final
    {
        static constexpr std::string_view name = "gate:closing";
    };

    template <typename Job>
    struct selftest_boot final
    {
        static constexpr std::string_view name = "selftest:boot";
    };

    template <typename Job>
    struct selftest_gate final
    {
        static constexpr std::string_view name = "selftest:gate";
    };

    template <typename Job>
    struct selftest_done final
    {
        static constexpr std::string_view name = "selftest:done";
    };

    struct lane_empty final
    {
        static constexpr std::string_view name = "lane:empty";
    };

    struct lane_occupied final
    {
        static constexpr std::string_view name = "lane:occupied";
    };

    struct lane_classify final
    {
        static constexpr std::string_view name = "lane:classify";
    };

    struct lane_await_payment final
    {
        static constexpr std::string_view name = "lane:await_payment";
    };

    struct lane_paid final
    {
        static constexpr std::string_view name = "lane:paid";
    };

    struct lane_rejected final
    {
        static constexpr std::string_view name = "lane:rejected";
    };

    struct lane_dispensing final
    {
        static constexpr std::string_view name = "lane:dispensing";
    };

    struct health_nominal final
    {
        static constexpr std::string_view name = "health:nominal";
    };

    struct health_degraded final
    {
        static constexpr std::string_view name = "health:degraded";
    };

    struct cash_seal final
    {
        static constexpr std::string_view name = "cash:seal";
    };

    struct cash_drawer final
    {
        static constexpr std::string_view name = "cash:drawer";
    };

    struct cash_count final
    {
        static constexpr std::string_view name = "cash:count";
    };

    struct audit_start final
    {
        static constexpr std::string_view name = "audit:start";
    };

    struct audit_verify final
    {
        static constexpr std::string_view name = "audit:verify";
    };

    struct diag_start final
    {
        static constexpr std::string_view name = "diag:start";
    };

    struct diag_scan final
    {
        static constexpr std::string_view name = "diag:scan";
    };

    struct diag_report final
    {
        static constexpr std::string_view name = "diag:report";
    };
}

// Chains are declared bottom-up so every target of a Transit<> is already complete.
namespace toll::region
{
    template <typename Job>
    using gate_closing
        = generic::step<tag::gate_closing<Job>, nil::sm::Terminate, nil::sm::Terminate>;

    template <typename Job>
    using gate_open = generic::
        waiting<tag::gate_open<Job>, ev::gate_close, nil::sm::Transit<gate_closing<Job>>>;

    template <typename Job>
    using gate_opening = generic::
        step<tag::gate_opening<Job>, nil::sm::Transit<gate_open<Job>>, nil::sm::Terminate>;

    template <typename Job>
    using gate_arm = generic::
        waiting<tag::gate_closed<Job>, ev::gate_open, nil::sm::Transit<gate_opening<Job>>>;

    template <typename Job>
    using selftest_done
        = generic::step<tag::selftest_done<Job>, nil::sm::Terminate, nil::sm::Terminate>;

    template <typename Job>
    using selftest_gate = generic::step<
        tag::selftest_gate<Job>,
        nil::sm::Transit<selftest_done<Job>>,
        nil::sm::Transit<selftest_done<Job>>>;

    template <typename Job>
    using selftest = generic::step<
        tag::selftest_boot<Job>,
        nil::sm::Transit<selftest_gate<Job>>,
        nil::sm::Transit<selftest_done<Job>>>;

    using lane_dispensing
        = generic::escaping<tag::lane_dispensing, ev::depart, nil::sm::Terminate, ev::cancel>;

    // The receipt deferred in await_payment is replayed here, after the payment transition.
    using lane_paid = generic::choosing<
        tag::lane_paid,
        ev::receipt,
        nil::sm::Discard,
        ev::ok,
        nil::sm::Terminate,
        policy::commit_fare>;

    using lane_rejected
        = generic::announcing<tag::lane_rejected, ev::alarm, ev::ok, nil::sm::Terminate>;

    using lane_await_payment = generic::screening<
        tag::lane_await_payment,
        ev::receipt,
        ev::pay,
        policy::fare_check,
        nil::sm::Transit<lane_paid>,
        nil::sm::Transit<lane_rejected>,
        policy::record_payment>;

    using lane_classify = generic::announcing<
        tag::lane_classify,
        ev::gate_open,
        ev::tick,
        nil::sm::Transit<lane_await_payment>>;

    using lane_occupied
        = generic::workflow<tag::lane_occupied, nil::sm::Transit<lane_dispensing>, lane_classify>;

    using lane = generic::waiting<
        tag::lane_empty,
        ev::arrive,
        nil::sm::Transit<lane_occupied>,
        policy::record_vehicle>;

    using health_degraded
        = generic::step<tag::health_degraded, nil::sm::Terminate, nil::sm::Terminate>;

    using health = generic::choosing<
        tag::health_nominal,
        ev::alarm,
        nil::sm::Transit<health_degraded>,
        ev::reset,
        nil::sm::Terminate>;

    using cash_count = generic::counting<tag::cash_count, ev::tick, nil::sm::Terminate, 3>;

    using cash_drawer = generic::choosing<
        tag::cash_drawer,
        ev::receipt,
        nil::sm::Discard,
        ev::ok,
        nil::sm::Transit<cash_count>>;

    using cash
        = generic::parking<tag::cash_seal, ev::receipt, ev::ok, nil::sm::Transit<cash_drawer>>;

    using audit_verify = generic::counting<tag::audit_verify, ev::tick, nil::sm::Terminate, 2>;

    using audit = generic::waiting<tag::audit_start, ev::tick, nil::sm::Transit<audit_verify>>;

    using diag_report = generic::choosing<
        tag::diag_report,
        ev::ok,
        nil::sm::Terminate,
        ev::fail,
        nil::sm::Terminate,
        policy::report>;

    using diag_scan = generic::
        announcing<tag::diag_scan, ev::gate_open, ev::tick, nil::sm::Transit<diag_report>>;

    using diag = generic::
        step<tag::diag_start, nil::sm::Transit<diag_scan>, nil::sm::Transit<diag_report>>;
}

// Each job is parameterized on what happens once it completes. The four top-level jobs simply
// terminate; the session then self-transitions, which brings the idle dispatcher back.
namespace toll::job
{
    template <typename Next>
    using startup = generic::workflow<
        tag::startup,
        Next,
        region::selftest<tag::startup>,
        region::gate_arm<tag::startup>>;

    template <typename Next>
    using collection = generic::workflow<
        tag::collection,
        Next,
        region::lane,
        region::gate_arm<tag::collection>,
        region::health>;

    template <typename Next>
    using shift = generic::workflow<tag::shift, Next, region::cash, region::audit>;

    template <typename Next>
    using maintenance = generic::
        workflow<tag::maintenance, Next, region::diag, region::gate_arm<tag::maintenance>>;

    using startup_job = startup<nil::sm::Terminate>;
    using collection_job = collection<nil::sm::Terminate>;
    using shift_job = shift<nil::sm::Terminate>;
    using maintenance_job = maintenance<nil::sm::Terminate>;

    // Idle dispatcher: the only place where a job can be picked.
    struct waiting final
    {
        static constexpr std::string_view name = "booth:waiting";

        using events = nil::xalt::tlist<
            ev::select_startup,
            ev::select_collection,
            ev::select_shift,
            ev::select_maintenance>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit waiting(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "idle - pick a job");
            return {};
        }

        static auto on_event(const ev::select_startup& /* event */)
        {
            return nil::sm::Transit<startup_job>();
        }

        static auto on_event(const ev::select_collection& /* event */)
        {
            return nil::sm::Transit<collection_job>();
        }

        static auto on_event(const ev::select_shift& /* event */)
        {
            return nil::sm::Transit<shift_job>();
        }

        static auto on_event(const ev::select_maintenance& /* event */)
        {
            return nil::sm::Transit<maintenance_job>();
        }
    };
}

namespace toll
{
    using booth = generic::session<job::waiting>;
}
