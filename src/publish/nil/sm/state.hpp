// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "api.hpp"
#include "detail.hpp"

#include <nil/xalt/tlist.hpp>
#include <nil/xalt/typed.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>

namespace nil::sm::barrier
{
    template <typename Action, typename T>
    struct State final
    {
        // Barrier state name is only surfaced in IR (see ir.hpp's node_builder); the runtime
        // metadata name stays the reserved barrier token.
        static constexpr auto name = reserved::barrier;

        // Structural completion is handled by the runtime barrier wrapper.
        static auto on_regions_finalized() -> Action;
    };
}

namespace nil::sm
{
    template <template <typename> typename API, typename T>
    class State final: public detail::IState
    {
    public:
        using api_t = API<T>;
        using self_t = State<API, T>;
        using metadata_t = Metadata;
        using api_context_t = typename api_t::api_context_t;

    private:
        using regions_t = typename api_t::regions_t;
        using events_t = typename api_t::events_t;
        using captures_t = typename api_t::captures_t;
        using event_dispatch_t = detail::event_dispatcher<self_t, T, events_t>;
        using capture_dispatch_t = detail::capture_dispatcher<self_t, T, captures_t>;
        using args_t = typename api_t::args_t;
        using props_t = typename api_t::props_t;
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
            [[maybe_unused]] State<API, T>* self,
            [[maybe_unused]] detail::Queues* queues,
            [[maybe_unused]] api_context_t* api_contexts,
            nil::xalt::tlist<R...> /* regions */,
            std::index_sequence<I...> /* region indices */
        )
        {
            on_enter(self->current_state, queues, api_contexts);

            [[maybe_unused]] const auto metadata = std::addressof(self->metadata);
            return std::array<detail::Region, regions_t::size>{detail::Region(
                typename detail::Region::template tag<API, R>{},
                I,
                self,
                queues,
                api_contexts,
                metadata
            )...};
        }

        // Resolves a single arg via the parent chain's get(); direct_parent<T> always
        // matches on the fixed direct_parent_marker sentinel, then casts to T*.
        template <typename Arg>
        static auto resolve_arg(detail::IState* parent)
        {
            if constexpr (nil::xalt::is_of_template_v<Arg, direct_parent>)
            {
                return static_cast<typename Arg::type*>(
                    parent->get(nil::xalt::type_id<detail::direct_parent_marker>)
                );
            }
            else
            {
                return static_cast<Arg*>(parent->get(nil::xalt::type_id<Arg>));
            }
        }

        // Resolves each arg in args_t via the parent chain's get(), then forwards the
        // resolved pointers to api_t::make() after the metadata argument.
        template <typename... Args>
        static T make_current_state(
            [[maybe_unused]] detail::IState* parent,
            api_context_t* api_contexts,
            Metadata metadata,
            nil::xalt::tlist<Args...> /* args */
        )
        {
            return api_t::make(api_contexts, metadata, resolve_arg<Args>(parent)...);
        }

        // Matches requested_id against every prop<Member, Ptr> in props_t, resolving
        // its value through the descriptor when one matches.
        template <typename... Props>
        void* match_props(
            [[maybe_unused]] const void* requested_id,
            nil::xalt::tlist<Props...> /* props */
        )
        {
            void* result = nullptr;
            (void)((requested_id == nil::xalt::type_id<typename Props::type>
                        ? (result = Props::get(current_state), true)
                        : false)
                   || ...);
            return result;
        }

    public:
        explicit State(
            detail::IState* init_parent,
            detail::Queues* init_queues,
            api_context_t* init_api_contexts,
            metadata_t init_metadata
        )
            : detail::IState(init_parent, init_metadata)
            , current_state(make_current_state(
                  init_parent,
                  static_cast<api_context_t*>(init_api_contexts),
                  this->metadata,
                  args_t()
              ))
            , queues(init_queues)
            , api_contexts(init_api_contexts)
            , regions(init_regions(
                  this,
                  queues,
                  api_contexts,
                  regions_t(),
                  std::make_index_sequence<regions_t::size>()
              ))
        {
        }

        void* get(const void* requested_id) override
        {
            if (requested_id == nil::xalt::type_id<detail::direct_parent_marker>)
            {
                return static_cast<void*>(std::addressof(current_state));
            }

            if (auto* found = match_props(requested_id, props_t()))
            {
                return found;
            }

            return parent != nullptr ? parent->get(requested_id) : nullptr;
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

            auto capture_result = capture_dispatch_t::dispatch(e, current_state, api_contexts);
            if (!std::holds_alternative<Unhandled>(capture_result)
                && !std::holds_alternative<Forward>(capture_result))
            {
                return capture_result;
            }

            auto sub_state = dispatch_to_regions(e);

            if (sub_state.handle)
            {
                auto this_result = event_dispatch_t::dispatch(e, current_state, api_contexts);
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

                // If the current state transitions, its child regions are destroyed,
                // so their pending actions are not committed.
                //
                // TODO: evaluate if dropping of event/deferral actions from child regions is
                // acceptable
                if (!std::holds_alternative<detail::TransitTo>(this_result))
                {
                    commit_region_results(e, sub_state.results);
                    check_finalize();
                }

                return this_result;
            }

            commit_region_results(e, sub_state.results);
            check_finalize();
            return Discard();
        }

    private:
        T current_state;
        detail::Queues* queues;
        api_context_t* api_contexts;
        std::array<detail::Region, regions_t::size> regions;
        bool finalized = false;

        static void on_enter(
            T& state,
            detail::Queues* init_queues,
            api_context_t* init_api_contexts
        )
        {
            const auto on_enter_result = detail::to_runtime_action_as<detail::on_enter_t>(
                api_t::on_enter(state, init_api_contexts)
            );

            if (std::holds_alternative<detail::Event>(on_enter_result))
            {
                init_queues->push_emit(std::get<detail::Event>(on_enter_result));
            }
        }

        void on_exit()
        {
            const auto on_exit_result = detail::to_runtime_action_as<detail::on_exit_t>(
                api_t::on_exit(current_state, api_contexts)
            );

            if (std::holds_alternative<detail::Event>(on_exit_result))
            {
                queues->push_emit(std::get<detail::Event>(on_exit_result));
            }
        }

        detail::on_regions_finalized_t on_regions_finalized()
        {
            return detail::to_runtime_action_as<detail::on_regions_finalized_t>(
                api_t::on_regions_finalized(current_state, api_contexts)
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
                queues->push_emit(detail::Event(std::move(r)));
            }
        }

        void commit_region_results(const detail::Event& e, on_event_results_t& sub_state_result)
        {
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                regions[i].template consume_action<region_dispatcher_t>(e, sub_state_result[i]);
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

        // Posts an already type-erased event and returns its normalized action.
        action_t post(detail::Event event)
        {
            return post_impl(event);
        }

        virtual bool is_finalized() const = 0;

    private:
        virtual action_t post_impl(detail::Event event) = 0;
    };

    template <template <typename> typename API, typename T, typename... RootArgs>
    class SM final: public ISM
    {
        using api_t = API<T>;
        using api_context_t = typename api_t::api_context_t;
        using region_dispatcher_t = detail::region_dispatcher<API, nil::xalt::tlist<T>>;

        struct construct_tag final
        {
        };

        explicit SM(
            construct_tag /* tag */,
            api_context_t* init_api_contexts,
            RootArgs*... init_root_args
        )
            : api_contexts(init_api_contexts)
            , root(init_root_args...)
            , region(detail::Region::tag<API, T>{}, 0, &root, &queues, api_contexts, nullptr)
        {
            flush();
        }

    public:
        explicit SM(api_context_t* init_api_contexts, RootArgs*... init_root_args)
            requires(!std::is_void_v<api_context_t>)
            : SM(construct_tag{}, init_api_contexts, init_root_args...)
        {
        }

        explicit SM(RootArgs*... init_root_args)
            requires std::is_void_v<api_context_t>
            : SM(construct_tag{}, nullptr, init_root_args...)
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
        detail::Queues queues;
        api_context_t* api_contexts;
        detail::RootState<RootArgs...> root;
        detail::Region region;

        action_t dispatch(const detail::Event& event)
        {
            auto action = region.active_state->on_event(event);
            return region.template consume_action<region_dispatcher_t>(event, action);
        }

        void flush()
        {
            queues.flush([this](const detail::Event& event) { dispatch(event); });
        }

        action_t post_impl(detail::Event event) override
        {
            auto action = dispatch(event);
            flush();
            return action;
        }
    };

    template <typename T, typename... RootArgs>
    using DefaultSM = SM<api::Default<>::template type, T, RootArgs...>;

    template <template <typename> typename API, typename T, typename... RootArgs>
    using CoalescedSM = SM<api::Coalesce<API>::template type, T, RootArgs...>;
}
