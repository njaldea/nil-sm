#pragma once

#include "job_api.hpp"

#include <nil/sm/barrier.hpp>

#include <string_view>

namespace toll::bslot
{
    // Providers for nil::sm::barrier::State: each builds its job as a barrier::SM sharing the
    // host's Runtime, so the job's own emits/defers broadcast through the whole booth tree
    // instead of staying confined to a separate one. Unlike tollbooth_slot's hand-rolled
    // job_slot, no event list needs to be declared - every event forwards unconditionally.
    struct startup_provider final
    {
        static auto make(nil::sm::barrier::Runtime* runtime, const nil::sm::Metadata* parent)
        {
            return barrier_jobs::make_startup(runtime, parent);
        }

        static auto ir(const nil::sm::Metadata* parent)
        {
            return barrier_jobs::ir_startup(parent);
        }
    };

    struct collection_provider final
    {
        static auto make(nil::sm::barrier::Runtime* runtime, const nil::sm::Metadata* parent)
        {
            return barrier_jobs::make_collection(runtime, parent);
        }

        static auto ir(const nil::sm::Metadata* parent)
        {
            return barrier_jobs::ir_collection(parent);
        }
    };

    struct shift_provider final
    {
        static auto make(nil::sm::barrier::Runtime* runtime, const nil::sm::Metadata* parent)
        {
            return barrier_jobs::make_shift(runtime, parent);
        }

        static auto ir(const nil::sm::Metadata* parent)
        {
            return barrier_jobs::ir_shift(parent);
        }
    };

    struct maintenance_provider final
    {
        static auto make(nil::sm::barrier::Runtime* runtime, const nil::sm::Metadata* parent)
        {
            return barrier_jobs::make_maintenance(runtime, parent);
        }

        static auto ir(const nil::sm::Metadata* parent)
        {
            return barrier_jobs::ir_maintenance(parent);
        }
    };

    using startup = nil::sm::barrier::State<nil::sm::Terminate, startup_provider>;
    using collection = nil::sm::barrier::State<nil::sm::Terminate, collection_provider>;
    using shift = nil::sm::barrier::State<nil::sm::Terminate, shift_provider>;
    using maintenance = nil::sm::barrier::State<nil::sm::Terminate, maintenance_provider>;

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

        auto on_enter() -> nil::sm::NOOP
        {
            ctx->log(name, "idle - pick a job");
            return {};
        }

        auto on_event(const ev::select_startup& /* event */)
        {
            return nil::sm::Transit<startup>();
        }

        auto on_event(const ev::select_collection& /* event */)
        {
            return nil::sm::Transit<collection>();
        }

        auto on_event(const ev::select_shift& /* event */)
        {
            return nil::sm::Transit<shift>();
        }

        auto on_event(const ev::select_maintenance& /* event */)
        {
            return nil::sm::Transit<maintenance>();
        }
    };

    using booth = generic::session<waiting>;
}
