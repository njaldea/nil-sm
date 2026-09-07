#pragma once

#include "ir.hpp" // IWYU pragma: keep
#include "state.hpp"

#include <memory>
#include <type_traits>

// Not `final`: to override `name` (or add other properties, e.g. payload()),
// inherit from the declared struct and declare it in the derived class.
#define NIL_SM_BARRIER_DECLARE(NAME, API)                                                          \
    struct NAME                                                                                    \
    {                                                                                              \
        using api_t = API<nil::sm::Root>;                                                          \
        using state_context_t = api_t::state_context_t;                                            \
        using api_context_t = api_t::api_context_t;                                                \
        [[maybe_unused]] static constexpr auto id = nil::xalt::type_id<NAME>;                      \
        static std::unique_ptr<nil::sm::ISM>                                                       \
            make(nil::sm::detail::Queues*, nil::sm::detail::Contexts*, const nil::sm::Metadata*);  \
        static nil::sm::ir::Model ir(const nil::sm::Metadata*);                                    \
    }

#define NIL_SM_BARRIER_DEFINE(NAME, API, STATE)                                                    \
    [[maybe_unused]] std::unique_ptr<nil::sm::ISM> NAME::make(                                     \
        nil::sm::detail::Queues* queues,                                                           \
        nil::sm::detail::Contexts* contexts,                                                       \
        const nil::sm::Metadata* parent_metadata                                                   \
    )                                                                                              \
    {                                                                                              \
        return std::make_unique<nil::sm::barrier::SM<API, STATE>>(                                 \
            queues,                                                                                \
            contexts,                                                                              \
            parent_metadata                                                                        \
        );                                                                                         \
    }                                                                                              \
    [[maybe_unused]] nil::sm::ir::Model NAME::ir(const nil::sm::Metadata* parent_metadata)         \
    {                                                                                              \
        return nil::sm::ir::build<API, STATE>(parent_metadata);                                    \
    }                                                                                              \
    static_assert(                                                                                 \
        std::is_same_v<typename NAME::api_t, API<nil::sm::Root>>,                                  \
        "NIL_SM_BARRIER_DEFINE: "                                                                  \
        "API must match the API passed to NIL_SM_BARRIER_DECLARE(" #NAME ")"                       \
    )

namespace nil::sm::barrier
{
    // Customization point: fully specialize for context pairs the defaults below cannot handle.
    template <typename ParentContext, typename ChildContext>
    struct context_adapter;

    template <typename ParentContext, typename ChildContext>
    concept borrowable_context = std::is_convertible_v<ParentContext*, ChildContext*>;

    template <typename ParentContext, typename ChildContext>
    concept constructible_context = std::is_constructible_v<ChildContext, ParentContext>;

    template <typename ParentContext, typename ChildContext>
        requires borrowable_context<ParentContext, ChildContext>
    void adapt_context(ParentContext* parent_context, ChildContext** out)
    {
        *out = parent_context;
    }

    // Resolved against the overload above and any found through argument-dependent lookup;
    // the child context slot is the output parameter.
    template <typename ParentContext, typename ChildContext>
    concept adapted_context = requires(ParentContext* parent_context, ChildContext** out) {
        adapt_context(parent_context, out);
    };

    template <typename ParentContext, typename ChildContext>
        requires adapted_context<ParentContext, ChildContext>
    struct context_adapter<ParentContext, ChildContext> final
    {
        explicit context_adapter(ParentContext* parent_context)
        {
            adapt_context(parent_context, &child_context);
        }

        ChildContext* context()
        {
            return child_context;
        }

    private:
        ChildContext* child_context = nullptr;
    };

    template <typename ParentContext, typename ChildContext>
        requires(!adapted_context<ParentContext, ChildContext> && constructible_context<ParentContext, ChildContext>)
    struct context_adapter<ParentContext, ChildContext> final
    {
        explicit context_adapter(ParentContext* parent_context)
            : child_context(*parent_context)
        {
        }

        ChildContext* context()
        {
            return &child_context;
        }

    private:
        ChildContext child_context;
    };

    // Auto-posted right after construction when Provider defines
    // static T payload(state_context_t*); T is deduced via aggregate CTAD.
    template <typename T>
    struct EvPayload final
    {
        T value;
    };

    // Reusable gate state: waits for EvPayload<T> then transits to Next.
    // A composite ancestor can capture EvPayload<T> and Forward it here unchanged
    // after processing it, so no separate "received" signal is needed.
    template <typename Next, typename T>
    struct WaitForPayload final
    {
        using events = nil::xalt::tlist<EvPayload<T>>;

        static auto on_event(const EvPayload<T>& /* event */)
        {
            return Next{};
        }
    };

    // Borrows the host queues and contexts; the host remains responsible for flushing queues.
    template <template <typename> typename API, typename T>
    class SM final: public ISM
    {
        using region_dispatcher_t = detail::region_dispatcher<API, Root, nil::xalt::tlist<T>>;

    public:
        explicit SM(
            detail::Queues* init_queues,
            detail::Contexts* init_contexts,
            const Metadata* init_parent_metadata = nullptr
        )
            : queues(init_queues)
            , contexts(init_contexts)
            , region(
                  detail::Region::tag<API, T>{},
                  0,
                  &root,
                  queues,
                  contexts,
                  init_parent_metadata
              )
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
        typename API<Root>::state_t root{};
        detail::Queues* queues;
        detail::Contexts* contexts;
        detail::Region region;

        action_t post_impl(detail::Event event) override
        {
            auto action = region.active_state->on_event(event);
            return region.template consume_action<region_dispatcher_t>(event, action);
        }
    };

}

namespace nil::sm
{
    template <template <typename> typename API, typename FinalizeAction, typename Provider>
    class State<API, barrier::State<FinalizeAction, Provider>> final: public detail::IState
    {
    public:
        template <typename Parent>
        explicit State(
            Parent* /* parent */,
            detail::Queues* init_queues,
            detail::Contexts* init_contexts,
            Metadata init_metadata
        )
            : detail::IState(init_metadata)
            , state_adapter(static_cast<parent_state_context_t*>(init_contexts->state))
            , api_adapter(static_cast<parent_api_context_t*>(init_contexts->api))
            , child_contexts{.state = state_adapter.context(), .api = api_adapter.context()}
            , child(Provider::make(init_queues, &child_contexts, std::addressof(this->metadata)))
        {
            if constexpr (requires() { Provider::payload(state_adapter.context()); })
            {
                child->post(barrier::EvPayload{Provider::payload(state_adapter.context())});
            }
        }

        detail::on_event_t on_event(const detail::Event& e) override
        {
            if (finalized)
            {
                return Unhandled();
            }

            auto result = child->post(e);

            if (child->is_finalized())
            {
                finalized = true;
                return detail::to_runtime_action_as<detail::on_event_t>(FinalizeAction{});
            }

            return detail::to_runtime_action_as<detail::on_event_t>(result);
        }

    private:
        using parent_state_context_t = typename API<Root>::state_context_t;
        using parent_api_context_t = typename API<Root>::api_context_t;
        using child_state_context_t = typename Provider::state_context_t;
        using child_api_context_t = typename Provider::api_context_t;

        using state_adapter_t
            = barrier::context_adapter<parent_state_context_t, child_state_context_t>;
        using api_adapter_t = barrier::context_adapter<parent_api_context_t, child_api_context_t>;

        state_adapter_t state_adapter;
        api_adapter_t api_adapter;
        detail::Contexts child_contexts;
        std::unique_ptr<ISM> child;
        bool finalized = false;
    };
}
