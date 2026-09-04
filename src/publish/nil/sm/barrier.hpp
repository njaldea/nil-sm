#pragma once

#include "ir.hpp" // IWYU pragma: keep
#include "state.hpp"

#include <memory>
#include <stdexcept>

#define NIL_SM_BARRIER_DECLARE(NAME)                                                               \
    struct NAME final                                                                              \
    {                                                                                              \
        static std::unique_ptr<nil::sm::ISM>                                                       \
            make(nil::sm::barrier::Runtime*, const nil::sm::Metadata*);                            \
        static nil::sm::ir::Model ir(const nil::sm::Metadata*);                                    \
    }

#define NIL_SM_BARRIER_DEFINE(NAME, API, STATE)                                                    \
    std::unique_ptr<nil::sm::ISM> NAME::make(                                                      \
        nil::sm::barrier::Runtime* runtime,                                                        \
        const nil::sm::Metadata* parent_metadata                                                   \
    )                                                                                              \
    {                                                                                              \
        return std::make_unique<nil::sm::barrier::SM<API, STATE>>(runtime, parent_metadata);       \
    }                                                                                              \
    nil::sm::ir::Model NAME::ir(const nil::sm::Metadata* parent_metadata)                          \
    {                                                                                              \
        return nil::sm::ir::build<API, STATE>(parent_metadata);                                    \
    }

namespace nil::sm::barrier
{
    using Runtime = detail::Runtime;

    // Borrows the host runtime; the host remains responsible for flushing it.
    template <template <typename> typename API, typename T>
    class SM final: public ISM
    {
        using region_dispatcher_t = detail::region_dispatcher<API, Root, nil::xalt::tlist<T>>;

    public:
        explicit SM(Runtime* init_runtime, const Metadata* init_parent_metadata = nullptr)
            : runtime(validate_runtime(init_runtime))
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

        static Runtime* validate_runtime(Runtime* init_runtime)
        {
            if (init_runtime == nullptr || init_runtime->api_id != nil::xalt::type_id<API<Root>>)
            {
                throw std::invalid_argument("barrier::SM received incompatible runtime");
            }

            return init_runtime;
        }

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
        explicit State(Parent* /* parent */, barrier::Runtime* init_runtime, Metadata init_metadata)
            : detail::IState(init_metadata)
            , child(Provider::make(init_runtime, std::addressof(this->metadata)))
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
        std::unique_ptr<ISM> child;
        bool finalized = false;
    };
}
