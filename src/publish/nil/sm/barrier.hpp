#pragma once

// barrier::State<FinalizeAction, Provider>: a leaf state that owns a fully
// independent child SM (behind ISM) and stays active until that child
// finalizes. Provider::make(Runtime*, const Metadata*) builds the child -
// typically as a barrier::SM sharing this Runtime, so the child's own
// on_enter/emit/defer cascades broadcast through the whole host tree instead
// of staying confined to a separate one - and returns it. The child's
// Metadata chain links back into the outer graph via the given parent
// Metadata. The child's concrete API/root types never need to be visible
// wherever barrier::State is used - only wherever Provider is defined.
// Provider may also expose a static ir() hook (checked via requires) so
// formatters can render the child's graph nested inside this state; that
// hook is not invoked here and is reserved for future formatter integration.

#include "state.hpp"

#include <memory>
#include <variant>

namespace nil::sm::barrier
{
    using Runtime = detail::Runtime;

    // Like SM, but borrows an existing Runtime instead of owning one, and
    // never flushes it - draining is the Runtime owner's responsibility.
    // Intended for use inside a Provider::make, so a child's cascades
    // broadcast through the host's whole tree instead of a separate one.
    template <template <typename> typename API, typename T>
    class SM final: public ISM
    {
        using region_dispatcher_t = detail::region_dispatcher<API, Root, nil::xalt::tlist<T>>;

    public:
        explicit SM(Runtime* init_runtime, const Metadata* init_parent_metadata = nullptr)
            : runtime(init_runtime)
            , region(detail::Region::tag<API, T>{}, 0, &root, runtime, init_parent_metadata)
        {
        }

        ~SM() noexcept override = default;

        SM(const SM&) = delete;
        SM& operator=(const SM&) = delete;
        SM(SM&&) noexcept = delete;
        SM& operator=(SM&&) noexcept = delete;

        bool is_finalized() const override
        {
            return region.terminated;
        }

    private:
        Root root;
        Runtime* runtime;
        detail::Region region;

        detail::on_event_t post_impl(detail::Event event) override
        {
            auto action = region.active_state->on_event(event);
            region.template consume_action<region_dispatcher_t>(event, action, nullptr);
            return action;
        }
    };

    template <typename FinalizeAction, typename Provider>
    struct State final
    {
        static constexpr auto name = "[barrier]";

        // Declaration only, never called: nil::sm::State<API, barrier::State<...>>
        // below applies FinalizeAction directly. This exists purely so the
        // compile-time reachability scanner discovers FinalizeAction's target.
        static auto on_regions_finalized() -> FinalizeAction;
    };
}

namespace nil::sm
{
    template <template <typename> typename API, typename FinalizeAction, typename Provider>
    class State<API, barrier::State<FinalizeAction, Provider>> final: public detail::IState
    {
    public:
        template <typename Parent>
        explicit State(Parent* /* parent */, detail::Runtime* init_runtime, Metadata init_metadata)
            : detail::IState(init_metadata)
            , child(Provider::make(init_runtime, std::addressof(this->metadata)))
        {
            // No construction-time poll needed: on_enter can never directly
            // return Terminate, so any construction-time finalization always
            // needs a flush-redispatch cycle first, which will naturally
            // reach on_event below via the host's own broadcast.
        }

        detail::on_event_t on_event(const detail::Event& e) override
        {
            if (finalized)
            {
                return Unhandled();
            }

            auto result = sanitize(child->post(e));

            if (child->is_finalized())
            {
                finalized = true;
                return detail::to_runtime_action_as<detail::on_event_t>(FinalizeAction{});
            }

            return result;
        }

    private:
        // Transit/Terminate/Event are already fully applied inside the child's
        // own SM; Defer is already owned by the child's own region. Only
        // Forward/Unhandled carry meaning for this barrier's own parent.
        static detail::on_event_t sanitize(detail::on_event_t action)
        {
            if (std::holds_alternative<Forward>(action)
                || std::holds_alternative<Unhandled>(action))
            {
                return action;
            }

            return Discard();
        }

        std::unique_ptr<ISM> child;
        bool finalized = false;
    };
}
