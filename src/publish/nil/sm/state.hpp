#pragma once

#include "api.hpp"
#include "detail.hpp"

#include <nil/xalt/checks.hpp>
#include <nil/xalt/tlist.hpp>
#include <nil/xalt/typed.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace nil::sm
{
    template <template <typename...> typename API, typename T>
    class State: public detail::IState
    {
    public:
        using api_t = API<T>;
        using self_t = State<API, T>;
        using metadata_t = Metadata;

    private:
        using state_t = typename api_t::state_t;
        using regions_t = typename api_t::regions_t;
        using events_t = typename api_t::events_t;
        using captures_t = typename api_t::captures_t;
        using event_dispatch_t = detail::event_dispatcher<self_t, events_t>;
        using capture_dispatch_t = detail::capture_dispatcher<self_t, captures_t>;
        using state_context_t = typename api_t::state_context_t;
        using api_context_t = typename api_t::api_context_t;
        using on_event_results_t = std::array<detail::on_event_t, regions_t::size>;

        using region_dispatcher_t = detail::region_dispatcher<API, regions_t>;

        struct sub_state_scan_t
        {
            bool handle = false;
            bool forward = false;
            on_event_results_t results = {};
        };

        template <typename... R, std::size_t... I>
        static std::array<detail::Region, regions_t::size> init_regions(
            [[maybe_unused]] self_t* self,
            [[maybe_unused]] detail::Queues* qs,
            [[maybe_unused]] detail::Contexts* contexts,
            nil::xalt::tlist<R...> /* regions */,
            std::index_sequence<I...> /* region indices */
        )
        {
            auto on_enter_result = self->on_enter();
            if (std::holds_alternative<detail::Emit>(on_enter_result))
            {
                qs->push_emit(std::get<detail::Emit>(on_enter_result));
            }

            return std::array<detail::Region, regions_t::size>{detail::Region{
                std::make_unique<State<API, R>>(
                    std::addressof(self->current_state),
                    qs,
                    contexts,
                    I,
                    0,
                    std::addressof(self->metadata)
                ),
                {},
                false
            }...};
        }

    public:
        template <typename Parent>
        explicit State(
            Parent* init_parent,
            detail::Queues* init_qs,
            detail::Contexts* init_contexts,
            std::size_t init_region,
            std::size_t init_state,
            const metadata_t* init_parent_metadata
        )
            : detail::IState(Metadata{
                  .state = init_state,
                  .region = init_region,
                  .subregions = regions_t::size,
                  .name = detail::type_name<T>(),
                  .parent = init_parent_metadata
              })
            , qs(init_qs)
            , contexts(init_contexts)
            , current_state(api_t::make(
                  init_parent,
                  static_cast<state_context_t*>(contexts->state),
                  static_cast<api_context_t*>(contexts->api),
                  this->metadata
              ))
            , regions(init_regions(
                  this,
                  qs,
                  contexts,
                  regions_t(),
                  std::make_index_sequence<regions_t::size>()
              ))
        {
        }

        State(State&&) = delete;
        State(const State&) = delete;
        State& operator=(State&&) = delete;
        State& operator=(const State&) = delete;

        ~State() override
        {
            for (auto it = regions.rbegin(); it != regions.rend(); ++it)
            {
                it->active_state.reset();
            }

            for (auto i = 0U; i < regions_t::size; ++i)
            {
                const auto index = i;
                detail::flush_deferred_events_for_region(
                    regions,
                    index,
                    [this](const detail::Emit& event) { qs->push_defer(event); }
                );
            }

            auto on_exit_result = on_exit();
            if (std::holds_alternative<detail::Emit>(on_exit_result))
            {
                qs->push_emit(std::get<detail::Emit>(on_exit_result));
            }
        }

        detail::on_event_t on_event(const detail::Emit& e) override
        {
            const auto is_regions_finalized_event
                = e.id == nil::xalt::type_id<detail::EvRegionsFinalized>;
            if (is_regions_finalized_event)
            {
                const auto* ev = static_cast<const detail::EvRegionsFinalized*>(e.data);
                if (ev->target == std::addressof(current_state))
                {
                    finalized = true;
                    return std::visit(
                        []<typename V>(V v) -> detail::on_event_t
                        {
                            if constexpr (std::is_same_v<NOOP, V>)
                            {
                                return Discard();
                            }
                            else
                            {
                                return v;
                            }
                        },
                        on_regions_finalized()
                    );
                }
            }

            if (!is_regions_finalized_event)
            {
                auto capture_result = capture_dispatch_t::dispatch(
                    e,
                    current_state,
                    static_cast<api_context_t*>(contexts->api)
                );
                if (!std::holds_alternative<Unhandled>(capture_result)
                    && !std::holds_alternative<Forward>(capture_result))
                {
                    return capture_result;
                }
            }

            auto sub_state = dispatch_to_regions(e);

            if (!is_regions_finalized_event && sub_state.handle)
            {
                auto this_result = event_dispatch_t::dispatch(
                    e,
                    current_state,
                    static_cast<api_context_t*>(contexts->api)
                );
                if (std::holds_alternative<Unhandled>(this_result))
                {
                    commit_region_results(e, sub_state.results);
                    if (sub_state.forward)
                    {
                        return Forward();
                    }

                    return Unhandled();
                }

                if (!std::holds_alternative<detail::Transit>(this_result))
                {
                    commit_region_results(e, sub_state.results);
                }

                if (std::holds_alternative<detail::Emit>(this_result))
                {
                    qs->push_emit(std::get<detail::Emit>(this_result));
                    return Discard();
                }

                return this_result;
            }

            commit_region_results(e, sub_state.results);
            return Discard();
        }

    private:
        detail::Queues* qs;
        detail::Contexts* contexts;
        state_t current_state;
        std::array<detail::Region, regions_t::size> regions;
        bool finalized = false;

        detail::on_enter_t on_enter()
        {
            auto result
                = api_t::on_enter(current_state, static_cast<api_context_t*>(contexts->api));
            return to_runtime_action_as<detail::on_enter_t>(result);
        }

        detail::on_exit_t on_exit()
        {
            auto result = api_t::on_exit(current_state, static_cast<api_context_t*>(contexts->api));
            return to_runtime_action_as<detail::on_exit_t>(result);
        }

        detail::on_regions_finalized_t on_regions_finalized()
        {
            auto result = api_t::on_regions_finalized(
                current_state,
                static_cast<api_context_t*>(contexts->api)
            );
            return to_runtime_action_as<detail::on_regions_finalized_t>(result);
        }

        sub_state_scan_t dispatch_to_regions(const detail::Emit& e)
        {
            sub_state_scan_t scan = {};
            auto no_region_handled = true;
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                scan.results[i] = regions[i].active_state->on_event(e);
                std::visit(
                    [&]<typename R>(const R& /* r */)
                    {
                        if constexpr (std::is_same_v<R, Forward>)
                        {
                            scan.forward = true;
                            no_region_handled = false;
                        }
                        else if constexpr (!std::is_same_v<R, Unhandled>)
                        {
                            no_region_handled = false;
                        }
                    },
                    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                    scan.results[i]
                );
            }

            scan.handle = scan.forward || no_region_handled;

            return scan;
        }

    public:
        template <typename O, typename R>
        static O to_runtime_action_as(R& r)
        {
            if constexpr (nil::xalt::is_of_template_v<std::remove_cvref_t<R>, std::variant>)
            {
                return std::visit([]<typename V>(V& v) { return to_runtime_action_as<O>(v); }, r);
            }
            else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::Transit>)
            {
                return O{detail::Transit{.target = nil::xalt::type_id<typename R::type>}};
            }
            else if constexpr (nil::xalt::is_of_template_v<R, ::nil::sm::Emit>)
            {
                return O{detail::Emit{
                    .id = r.id,
                    .deleter = r.deleter,
                    .cloner = r.cloner,
                    .data = std::exchange(r.data, nullptr)
                }};
            }
            else
            {
                return O{r};
            }
        }

    private:
        void commit_region_results(const detail::Emit& e, on_event_results_t& sub_state_result)
        {
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                std::visit(
                    [&]<typename R>(R& r)
                    {
                        detail::apply_region_runtime_action(
                            i,
                            r,
                            e,
                            regions,
                            [this](std::size_t idx, const void* target)
                            {
                                return region_dispatcher_t::template make(
                                    idx,
                                    target,
                                    std::addressof(current_state),
                                    qs,
                                    contexts,
                                    std::addressof(this->metadata)
                                );
                            },
                            [this](std::size_t idx)
                            {
                                return std::make_unique<State<API, Fin>>(
                                    std::addressof(current_state),
                                    qs,
                                    contexts,
                                    idx,
                                    0,
                                    std::addressof(this->metadata)
                                );
                            },
                            [this](const detail::Emit& event) { qs->push_emit(event); },
                            [this](const detail::Emit& event) { qs->push_defer(event); }
                        );
                    },
                    sub_state_result[i]
                );
            }

            if (!finalized && !regions.empty()
                && std::all_of(
                    regions.begin(),
                    regions.end(),
                    [](const auto& r) { return r.terminated; }
                ))
            {
                auto r = Emit<detail::EvRegionsFinalized>(std::addressof(current_state));
                qs->push_emit( //
                    detail::Emit{
                        .id = r.id,
                        .deleter = r.deleter,
                        .cloner = r.cloner,
                        .data = std::exchange(r.data, nullptr)
                    }
                );
            }
        }
    };

    template <template <typename...> typename API, typename... Regions>
    class RootState: public detail::IState
    {
    public:
        static_assert(sizeof...(Regions) > 0);
        using metadata_t = Metadata;

    private:
        using state_t = Root;
        using regions_t = nil::xalt::tlist<Regions...>;
        using on_event_results_t = std::array<detail::on_event_t, regions_t::size>;
        using region_dispatcher_t = detail::region_dispatcher<API, regions_t>;

        template <std::size_t... I>
        static std::array<detail::Region, regions_t::size> init_regions(
            [[maybe_unused]] state_t* self,
            [[maybe_unused]] detail::Queues* qs,
            [[maybe_unused]] detail::Contexts* contexts,
            std::index_sequence<I...> /* region indices */
        )
        {
            return std::array<detail::Region, regions_t::size>{detail::Region{
                std::make_unique<State<API, Regions>>(self, qs, contexts, I, 0, nullptr),
                {},
                false
            }...};
        }

    public:
        explicit RootState(detail::Queues* init_qs, detail::Contexts* init_contexts)
            : detail::IState(Metadata{
                  .state = 0,
                  .region = 0,
                  .subregions = regions_t::size,
                  .name = "[--]",
                  .parent = nullptr
              })
            , qs(init_qs)
            , contexts(init_contexts)
            , regions(init_regions(
                  &root_state,
                  qs,
                  contexts,
                  std::index_sequence_for<Regions...>() //
              ))
        {
        }

        RootState(RootState&&) = delete;
        RootState(const RootState&) = delete;
        RootState& operator=(RootState&&) = delete;
        RootState& operator=(const RootState&) = delete;

        ~RootState() override
        {
            for (auto it = regions.rbegin(); it != regions.rend(); ++it)
            {
                it->active_state.reset();
            }

            for (auto i = 0U; i < regions_t::size; ++i)
            {
                const auto index = i;
                detail::flush_deferred_events_for_region(
                    regions,
                    index,
                    [this](const detail::Emit& event) { qs->push_defer(event); }
                );
            }
        }

        detail::on_event_t on_event(const detail::Emit& e) override
        {
            on_event_results_t results;
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                results[i] = regions[i].active_state->on_event(e);
            }
            commit_region_results(e, results);
            return Discard();
        }

    private:
        detail::Queues* qs;
        detail::Contexts* contexts;
        state_t root_state;
        std::array<detail::Region, regions_t::size> regions;

        void commit_region_results(const detail::Emit& e, on_event_results_t& sub_state_result)
        {
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                std::visit(
                    [&]<typename R>(R& r)
                    {
                        detail::apply_region_runtime_action(
                            i,
                            r,
                            e,
                            regions,
                            [this](std::size_t idx, const void* target)
                            {
                                return region_dispatcher_t::template make(
                                    idx,
                                    target,
                                    std::addressof(root_state),
                                    qs,
                                    contexts,
                                    nullptr
                                );
                            },
                            [this](std::size_t idx) {
                                return std::make_unique<State<API, Fin>>(
                                    &root_state,
                                    qs,
                                    contexts,
                                    idx,
                                    0,
                                    nullptr
                                );
                            },
                            [this](const detail::Emit& event) { qs->push_emit(event); },
                            [this](const detail::Emit& event) { qs->push_defer(event); }
                        );
                    },
                    sub_state_result[i]
                );
            }
        }
    };

}

namespace nil::sm
{
    class ISM
    {
    public:
        ISM() = default;
        ISM(ISM&) = delete;
        ISM(const ISM&&) = delete;
        ISM& operator=(ISM&&) = delete;
        ISM& operator=(const ISM&) = delete;
        virtual ~ISM() noexcept = default;

        template <typename T>
        void post(T event = {})
        {
            post_impl(detail::Emit{
                .id = nil::xalt::type_id<T>,
                .deleter = &detail::deleter<T>,
                .cloner = &detail::cloner<T>,
                .data = &event
            });
        }

    private:
        virtual void post_impl(detail::Emit event) = 0;
    };

    template <template <typename...> typename API, typename... Regions>
    class SM final: public ISM
    {
        using api_t = API<struct ST>;
        using regions_t = typename api_t::regions_t;
        using state_context_t = typename api_t::state_context_t;
        using api_context_t = typename api_t::api_context_t;

    public:
        explicit SM(state_context_t* state_contexts, api_context_t* api_contexts)
            : contexts({.state = state_contexts, .api = api_contexts})
            , state(&queues, &contexts)
        {
            queues.flush(state);
        }

        ~SM() noexcept override = default;

        SM(const SM&) = delete;
        SM& operator=(const SM&) = delete;

        SM(SM&&) noexcept = delete;
        SM& operator=(SM&&) noexcept = delete;

    private:
        detail::Queues queues;
        detail::Contexts contexts;
        RootState<API, Regions...> state;

        void post_impl(detail::Emit event) override
        {
            state.on_event(event);
            queues.flush(state);
        }
    };

    template <typename... Regions>
    using DefaultSM = SM<api::Default, Regions...>;

    template <template <typename...> typename API, typename... Regions>
    using CoalescedSM = SM<api::Coalesce<API>::template type, Regions...>;
}
