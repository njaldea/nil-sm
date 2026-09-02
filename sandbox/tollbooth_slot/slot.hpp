#pragma once

#include "../tollbooth_libs/job_api.hpp"

#include <nil/sm.hpp>

#include <memory>
#include <string_view>
#include <utility>
#include <variant>

namespace toll::slot
{
    // A leaf state that owns a job as an opaque child machine. Events are forwarded verbatim;
    // when the child reports completion the slot terminates, so the parent region moves on
    // exactly as it would for a normally nested composite.
    template <typename Tag, typename Factory>
    struct job_slot final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<
            ev::ok,
            ev::fail,
            ev::tick,
            ev::arrive,
            ev::pay,
            ev::receipt,
            ev::cancel,
            ev::depart,
            ev::gate_open,
            ev::gate_close,
            ev::alarm,
            ev::reset,
            ev::abort>;

        booth_context* ctx = nullptr;
        std::unique_ptr<nil::sm::ISM> child;

        template <typename Parent>
        explicit job_slot(Parent* /* parent */, booth_context* context)
            : ctx(context)
            , child(Factory::make(context, context->trace))
        {
        }

        template <typename E>
        auto on_event(const E& event) -> std::variant<nil::sm::Discard, nil::sm::Terminate>
        {
            child->post(event);
            if (ctx->job_done)
            {
                return nil::sm::Terminate();
            }
            return nil::sm::Discard();
        }
    };

    struct startup_factory final
    {
        static auto make(booth_context* state, trace_context* trace)
        {
            return libs::make_startup(state, trace);
        }
    };

    struct collection_factory final
    {
        static auto make(booth_context* state, trace_context* trace)
        {
            return libs::make_collection(state, trace);
        }
    };

    struct shift_factory final
    {
        static auto make(booth_context* state, trace_context* trace)
        {
            return libs::make_shift(state, trace);
        }
    };

    struct maintenance_factory final
    {
        static auto make(booth_context* state, trace_context* trace)
        {
            return libs::make_maintenance(state, trace);
        }
    };

    struct startup_tag final
    {
        static constexpr std::string_view name = "slot:startup";
    };

    struct collection_tag final
    {
        static constexpr std::string_view name = "slot:collection";
    };

    struct shift_tag final
    {
        static constexpr std::string_view name = "slot:shift";
    };

    struct maintenance_tag final
    {
        static constexpr std::string_view name = "slot:maintenance";
    };

    using startup = job_slot<startup_tag, startup_factory>;
    using collection = job_slot<collection_tag, collection_factory>;
    using shift = job_slot<shift_tag, shift_factory>;
    using maintenance = job_slot<maintenance_tag, maintenance_factory>;

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
