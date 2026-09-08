#pragma once

#include "../tollbooth/common.hpp"
#include "../tollbooth/events.hpp"

#include "collection.hpp"
#include "maintenance.hpp"
#include "shift.hpp"
#include "startup.hpp"

#include <nil/sm/barrier.hpp>

#include <string_view>

namespace toll::bslot
{
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
        using args = nil::xalt::tlist<booth_context>;

        explicit waiting(booth_context* context)
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
            return nil::sm::TransitTo<startup>();
        }

        static auto on_event(const ev::select_collection& /* event */)
        {
            return nil::sm::TransitTo<collection>();
        }

        static auto on_event(const ev::select_shift& /* event */)
        {
            return nil::sm::TransitTo<shift>();
        }

        static auto on_event(const ev::select_maintenance& /* event */)
        {
            return nil::sm::TransitTo<maintenance>();
        }
    };

    using booth = generic::session<waiting>;
}
