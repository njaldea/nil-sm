#pragma once

#include "ir.hpp" // IWYU pragma: keep
#include "state.hpp"

#include <memory>
#include <type_traits>

#define NIL_SM_BARRIER_DECLARE(NAME, API)                                                          \
    struct NAME final                                                                              \
    {                                                                                              \
        using state_context_t = API<nil::sm::Root>::state_context_t;                               \
        using api_context_t = API<nil::sm::Root>::api_context_t;                                   \
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
    }

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
        Root root;
        detail::Queues* queues;
        detail::Contexts* contexts;
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
        static constexpr auto name = "[/]";

        // Used by compile-time reachability analysis; the nil::sm::State
        // specialization further below applies the action when the child finalizes.
        static auto on_regions_finalized() -> FinalizeAction;
    };
}

namespace nil::sm::ir::detail
{
    template <template <typename> typename API, typename FinalizeAction, typename Provider>
    struct regions_builder<API, barrier::State<FinalizeAction, Provider>>
    {
        static std::vector<std::vector<ir::Node>> regions(const Metadata* metadata)
        {
            return {Provider::ir(metadata).roots};
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
