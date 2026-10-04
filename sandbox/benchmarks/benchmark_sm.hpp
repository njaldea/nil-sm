// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <nil/sm.hpp>
#include <nil/sm/diagnostics.hpp>

#include <iostream>
#include <variant>

namespace benchmark
{
    // --- Contexts ---
    struct telemetry_context
    {
        int telemetry_id = 100;
        int pings = 0;
    };

    struct root_context
    {
        int session_id = 42;
        int speed = 0;
        telemetry_context telemetry{};
    };

    // --- Events ---
    struct ev_start
    {
    };

    struct ev_tick
    {
        int delta = 1;
    };

    struct ev_pause
    {
    };

    struct ev_resume
    {
    };

    struct ev_data
    {
        int payload = 0;
    };

    struct ev_ack
    {
    };

    struct ev_reset
    {
    };

    struct ev_stop
    {
    };

    struct ev_emergency
    {
    };

    struct ev_internal_pulse
    {
    };

    // Forward declarations of states
    struct idle;
    struct active;
    struct running;
    struct paused;
    struct emergency_stopped;
    struct processing_sub1;
    struct processing_sub2;
    struct net_connecting;
    struct net_connected;
    struct net_streaming;

    // --- Region 1 (Worker/Process) States ---

    struct processing_sub2
    {
        using events = nil::xalt::tlist<ev_tick, ev_ack>;

        static auto on_enter()
        {
            return nil::sm::NOOP{};
        }

        static auto on_event(const ev_tick& /* event */)
        {
            return nil::sm::Discard{};
        }

        static auto on_event(const ev_ack& /* event */)
        {
            return nil::sm::Terminate{};
        }

        static auto on_exit()
        {
            return nil::sm::NOOP{};
        }
    };

    struct processing_sub1
    {
        using args = nil::xalt::tlist<nil::sm::direct_parent<running>>;
        using events = nil::xalt::tlist<ev_data, ev_tick>;

        explicit processing_sub1(running* /* parent */)
        {
        }

        static auto on_event(const ev_data& /* event */)
        {
            return nil::sm::TransitTo<processing_sub2>{};
        }

        static auto on_event(const ev_tick& /* event */)
        {
            return nil::sm::Forward{};
        }
    };

    // Composite state in Region 1
    struct running
    {
        using regions = nil::xalt::tlist<processing_sub1>;
        using events = nil::xalt::tlist<ev_pause, ev_tick>;

        static auto on_event(const ev_pause& /* event */)
        {
            return nil::sm::Defer{};
        }

        static auto on_event(const ev_tick& /* event */)
        {
            return nil::sm::Discard{};
        }

        static auto on_regions_finalized()
        {
            return nil::sm::NOOP{};
        }
    };

    struct paused
    {
        using events = nil::xalt::tlist<ev_resume, ev_data>;

        static auto on_event(const ev_resume& /* event */)
        {
            return nil::sm::TransitTo<running>{};
        }

        static auto on_event(const ev_data& /* event */)
        {
            return nil::sm::DeferTo<running>{};
        }
    };

    struct idle
    {
        using events = nil::xalt::tlist<ev_start>;

        static auto on_event(const ev_start& /* event */)
        {
            return nil::sm::TransitTo<running>{};
        }
    };

    // --- Region 2 (Network/Telemetry) States ---

    struct net_streaming
    {
        using args = nil::xalt::tlist<telemetry_context>;
        using events = nil::xalt::tlist<ev_tick, ev_stop>;

        explicit net_streaming(telemetry_context* ctx)
            : tel(ctx)
        {
        }

        telemetry_context* tel = nullptr;

        auto on_event(const ev_tick& /* event */) const
        {
            if (tel != nullptr)
            {
                tel->pings++;
            }
            return nil::sm::Discard{};
        }

        static auto on_event(const ev_stop& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct net_connected
    {
        using args = nil::xalt::tlist<telemetry_context>;
        using events = nil::xalt::tlist<ev_start>;

        explicit net_connected(telemetry_context* ctx)
            : tel(ctx)
        {
        }

        telemetry_context* tel = nullptr;

        static auto on_event(const ev_start& /* event */)
        {
            return nil::sm::TransitTo<net_streaming>{};
        }
    };

    struct net_connecting
    {
        using args = nil::xalt::tlist<telemetry_context>;
        using events = nil::xalt::tlist<ev_ack>;

        explicit net_connecting(telemetry_context* ctx)
            : tel(ctx)
        {
        }

        telemetry_context* tel = nullptr;

        static auto on_enter()
        {
            return nil::sm::Emit<ev_internal_pulse>{};
        }

        static auto on_event(const ev_ack& /* event */)
        {
            return nil::sm::TransitTo<net_connected>{};
        }
    };

    // --- Parent Active State (hosts 2 orthogonal regions) ---
    struct active
    {
        using regions = nil::xalt::tlist<idle, net_connecting>;
        using captures = nil::xalt::tlist<ev_emergency>;
        using events = nil::xalt::tlist<ev_stop, ev_internal_pulse>;

        int step_count = 0;

        static auto on_enter()
        {
            return nil::sm::NOOP{};
        }

        static auto on_capture(const ev_emergency& /* event */)
        {
            return nil::sm::TransitTo<emergency_stopped>{};
        }

        static auto on_event(const ev_internal_pulse& /* event */)
        {
            return nil::sm::Discard{};
        }

        auto on_event(const ev_stop& /* event */) const
            -> std::variant<nil::sm::TransitTo<idle>, nil::sm::Terminate>
        {
            if (step_count > 10)
            {
                return nil::sm::Terminate{};
            }
            return nil::sm::TransitTo<idle>{};
        }

        static auto on_regions_finalized()
        {
            return nil::sm::Terminate{};
        }

        static auto on_exit()
        {
            return nil::sm::NOOP{};
        }
    };

    struct emergency_stopped
    {
        using events = nil::xalt::tlist<ev_reset>;

        static auto on_event(const ev_reset& /* event */)
        {
            return nil::sm::TransitTo<active>{};
        }
    };

    // --- Root State ---
    struct root
    {
        telemetry_context telemetry{};
        using regions = nil::xalt::tlist<active>;
        using captures = nil::xalt::tlist<ev_reset>;
        using props = nil::xalt::tlist<nil::sm::prop<telemetry_context, &root::telemetry>>;

        static auto on_capture(const ev_reset& /* event */)
        {
            return nil::sm::Forward{};
        }

        static auto on_regions_finalized()
        {
            return nil::sm::NOOP{};
        }
    };

    inline void run_benchmark_cycle(nil::sm::DefaultSM<root>& sm)
    {
        sm.post(ev_start{});
        sm.post(ev_tick{5});
        sm.post(ev_ack{});
        sm.post(ev_data{123});
        sm.post(ev_tick{1});
        sm.post(ev_ack{});
        sm.post(ev_pause{});
        sm.post(ev_resume{});
        sm.post(ev_stop{});
        sm.post(ev_emergency{});
        sm.post(ev_reset{});
    }
}
