#pragma once

#include "api.hpp"
#include "detail.hpp"

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
    template <template <typename> typename API, typename T>
    class State final: public detail::IState
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

        using region_dispatcher_t = detail::region_dispatcher<API, state_t, regions_t>;

        struct sub_state_scan_t
        {
            bool handle = false;
            bool forward = false;
            on_event_results_t results = {};
        };

        template <typename... R, std::size_t... I>
        static std::array<detail::Region, regions_t::size> init_regions(
            [[maybe_unused]] self_t* self,
            [[maybe_unused]] detail::Runtime* runtime,
            nil::xalt::tlist<R...> /* regions */,
            std::index_sequence<I...> /* region indices */
        )
        {
            [[maybe_unused]] const auto parent = std::addressof(self->current_state);
            [[maybe_unused]] const auto metadata = std::addressof(self->metadata);
            return std::array<detail::Region, regions_t::size>{detail::Region(
                typename detail::Region::template tag<API, R>{},
                I,
                parent,
                runtime,
                metadata
            )...};
        }

    public:
        template <typename Parent>
        explicit State(Parent* init_parent, detail::Runtime* init_runtime, metadata_t init_metadata)
            : detail::IState(init_metadata)
            , current_state(api_t::make(
                  init_parent,
                  static_cast<state_context_t*>(init_runtime->contexts.state),
                  static_cast<api_context_t*>(init_runtime->contexts.api),
                  this->metadata
              ))
            , runtime(on_enter(current_state, init_runtime))
            , regions(init_regions(
                  this,
                  runtime,
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
                it->transit_out();
            }
            on_exit();
        }

        detail::on_event_t on_event(const detail::Event& e) override
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

            auto capture_result = capture_dispatch_t::dispatch(
                e,
                current_state,
                static_cast<api_context_t*>(runtime->contexts.api)
            );
            if (!std::holds_alternative<Unhandled>(capture_result)
                && !std::holds_alternative<Forward>(capture_result))
            {
                return capture_result;
            }

            auto sub_state = dispatch_to_regions(e);

            if (sub_state.handle)
            {
                auto this_result = event_dispatch_t::dispatch(
                    e,
                    current_state,
                    static_cast<api_context_t*>(runtime->contexts.api)
                );
                if (std::holds_alternative<Unhandled>(this_result))
                {
                    commit_region_results(e, sub_state.results);
                    check_finalize();
                    if (sub_state.forward)
                    {
                        return Forward();
                    }

                    return Unhandled();
                }

                if (!std::holds_alternative<detail::Transit>(this_result))
                {
                    commit_region_results(e, sub_state.results);
                    check_finalize();
                }

                if (std::holds_alternative<detail::Event>(this_result))
                {
                    runtime->queues.push_emit(std::get<detail::Event>(this_result));
                    return Discard();
                }

                return this_result;
            }

            commit_region_results(e, sub_state.results);
            check_finalize();
            return Discard();
        }

    private:
        state_t current_state;
        detail::Runtime* runtime;
        std::array<detail::Region, regions_t::size> regions;
        bool finalized = false;

        static detail::Runtime* on_enter(state_t& state, detail::Runtime* init_runtime)
        {
            const auto on_enter_result = detail::to_runtime_action_as<detail::on_enter_t>(
                api_t::on_enter(state, static_cast<api_context_t*>(init_runtime->contexts.api))
            );

            if (std::holds_alternative<detail::Event>(on_enter_result))
            {
                init_runtime->queues.push_emit(std::get<detail::Event>(on_enter_result));
            }

            return init_runtime;
        }

        void on_exit()
        {
            const auto on_exit_result = detail::to_runtime_action_as<detail::on_exit_t>(
                api_t::on_exit(current_state, static_cast<api_context_t*>(runtime->contexts.api))
            );

            if (std::holds_alternative<detail::Event>(on_exit_result))
            {
                runtime->queues.push_emit(std::get<detail::Event>(on_exit_result));
            }
        }

        detail::on_regions_finalized_t on_regions_finalized()
        {
            return detail::to_runtime_action_as<detail::on_regions_finalized_t>(
                api_t::on_regions_finalized(
                    current_state,
                    static_cast<api_context_t*>(runtime->contexts.api)
                )
            );
        }

        sub_state_scan_t dispatch_to_regions(const detail::Event& e)
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

        void check_finalize()
        {
            if (!finalized && !regions.empty()
                && std::all_of(
                    regions.begin(),
                    regions.end(),
                    [](const auto& r) { return r.terminated; }
                ))
            {
                auto r = Emit<detail::EvRegionsFinalized>(std::addressof(current_state));
                runtime->queues.push_emit(detail::Event(std::move(r)));
            }
        }

        void commit_region_results(const detail::Event& e, on_event_results_t& sub_state_result)
        {
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                regions[i].template consume_action<region_dispatcher_t>(
                    e,
                    sub_state_result[i],
                    std::addressof(this->metadata)
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
            requires(!std::is_same_v<T, detail::Event>)
        void post(T event = {})
        {
            (void)post_impl(detail::Event{
                .id = nil::xalt::type_id<T>,
                .deleter = &detail::deleter<T>,
                .cloner = &detail::cloner<T>,
                .data = &event
            });
        }

        // Posts an already type-erased event and returns the root region's
        // resulting action, for internal use by adapters such as BarrierState.
        detail::on_event_t post(detail::Event event)
        {
            return post_impl(event);
        }

        virtual bool is_finalized() const = 0;

    private:
        virtual detail::on_event_t post_impl(detail::Event event) = 0;
    };

    template <template <typename> typename API, typename T>
    class SM final: public ISM
    {
        using api_t = API<T>;
        using state_context_t = typename api_t::state_context_t;
        using api_context_t = typename api_t::api_context_t;
        using region_dispatcher_t = detail::region_dispatcher<API, Root, nil::xalt::tlist<T>>;

    public:
        explicit SM(
            state_context_t* state_contexts,
            api_context_t* api_contexts,
            const Metadata* init_parent_metadata = nullptr
        )
            : runtime{.queues = {}, .contexts = {.state = state_contexts, .api = api_contexts}}
            , region(detail::Region::tag<API, T>{}, 0, &root, &runtime, init_parent_metadata)
        {
            flush();
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
        detail::Runtime runtime;
        detail::Region region;

        detail::on_event_t dispatch(const detail::Event& event)
        {
            auto action = region.active_state->on_event(event);
            region.template consume_action<region_dispatcher_t>(event, action, nullptr);
            return action;
        }

        void flush()
        {
            runtime.queues.flush([this](const detail::Event& event) { dispatch(event); });
        }

        detail::on_event_t post_impl(detail::Event event) override
        {
            auto action = dispatch(event);
            flush();
            return action;
        }
    };

    template <typename T>
    using DefaultSM = SM<api::Default<>::template type, T>;

    template <template <typename> typename API, typename T>
    using CoalescedSM = SM<api::Coalesce<API>::template type, T>;
}
