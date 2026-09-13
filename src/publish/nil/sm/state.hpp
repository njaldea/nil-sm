// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "api.hpp"
#include "concepts.hpp"
#include "detail.hpp"
#include "state_validation.hpp"

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
    template <typename API, typename T>
    class State;

    template <typename API, typename T>
        requires(!detail::is_state_valid<API, T>)
    class State<API, T> final: public detail::IState
    {
        using metadata_t = Metadata;

    public:
        explicit State(
            detail::IState* init_parent = nullptr,
            detail::Queues* /* init_queues */ = nullptr,
            typename API::context_t* /* init_contexts */ = nullptr,
            metadata_t init_metadata = {}
        )
            : detail::IState(init_parent, init_metadata)
        {
        }

        detail::on_event_t on_event(const detail::Event& /* event */) override
        {
            return {};
        }

        // Looks up a value owned by this state or one of its ancestors, by type id.
        void* get(const void* /* requested_id */) override
        {
            return nullptr;
        }

        // clang-format off
        static_assert(detail::has_state_typedefs<API, T>, "State API typedefs are invalid or not tlist");
        static_assert(detail::is_make_valid<API, T>, "State API make() is invalid");
        static_assert(detail::are_props_valid<API, T>, "State API props are invalid");
        static_assert(detail::are_event_hooks_valid<API, T>, "State API event hooks are invalid");
        static_assert(detail::are_capture_hooks_valid<API, T>, "State API capture hooks are invalid");
        static_assert(detail::is_on_enter_hook_valid<API, T>, "State API on_enter hook is invalid");
        static_assert(detail::is_on_exit_hook_valid<API, T>, "State API on_exit hook is invalid");
        static_assert(detail::is_on_regions_finalized_hook_valid<API, T>, "State API on_regions_finalized hook is invalid");
        // clang-format on
    };

    template <typename API, typename T>
        requires(detail::is_state_valid<API, T>)
    class State<API, T> final: public detail::IState
    {
    public:
        using api_t = API;

    private:
        using metadata_t = Metadata;
        using self_t = State<API, T>;
        using context_t = typename API::context_t;

        using state_t = api_t::template state<T>;
        using args_t = typename state_t::args_t;
        using props_t = typename state_t::props_t;
        using regions_t = typename state_t::regions_t;
        using events_t = typename state_t::events_t;
        using captures_t = typename state_t::captures_t;
        using event_dispatch_t = detail::event_dispatcher<self_t, T, events_t>;
        using capture_dispatch_t = detail::capture_dispatcher<self_t, T, captures_t>;
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
            [[maybe_unused]] context_t* contexts,
            nil::xalt::tlist<R...> /* regions */,
            std::index_sequence<I...> /* region indices */
        )
        {
            on_enter(self->current_state, queues, contexts);

            [[maybe_unused]] const auto metadata = std::addressof(self->metadata);
            return std::array<detail::Region, regions_t::size>{detail::Region(
                typename detail::Region::template tag<API, R>{},
                I,
                self,
                queues,
                contexts,
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
            context_t* contexts,
            Metadata metadata,
            nil::xalt::tlist<Args...> /* args */
        )
        {
            return state_t::make(contexts, metadata, resolve_arg<Args>(parent)...);
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
            context_t* init_contexts,
            metadata_t init_metadata
        )
            : detail::IState(init_parent, init_metadata)
            , current_state(make_current_state(
                  init_parent,
                  static_cast<context_t*>(init_contexts),
                  this->metadata,
                  args_t()
              ))
            , queues(init_queues)
            , contexts(init_contexts)
            , regions(init_regions(
                  this,
                  queues,
                  contexts,
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
                    auto finalized_result = on_regions_finalized();
                    return finalized_result.visit(
                        []<typename V>(V& v) -> detail::on_event_t
                        {
                            if constexpr (std::is_same_v<NOOP, V>)
                            {
                                return detail::on_event_t{Discard()};
                            }
                            else
                            {
                                return detail::on_event_t{v};
                            }
                        }
                    );
                }
            }

            auto capture_result = capture_dispatch_t::dispatch(e, current_state, contexts);
            if (!capture_result.template holds<Unhandled>()
                && !capture_result.template holds<Forward>())
            {
                return capture_result;
            }

            auto sub_state = dispatch_to_regions(e);

            if (sub_state.handle)
            {
                auto this_result = event_dispatch_t::dispatch(e, current_state, contexts);
                if (this_result.template holds<Unhandled>())
                {
                    commit_region_results(e, sub_state.results);
                    check_finalize();
                    if (sub_state.forward)
                    {
                        return detail::on_event_t{Forward()};
                    }

                    return detail::on_event_t{Unhandled()};
                }

                // If the current state transitions, its child regions are destroyed,
                // so their pending actions are not committed.
                //
                // TODO: evaluate if dropping of event/deferral actions from child regions is
                // acceptable
                if (!this_result.template holds<detail::TransitTo>())
                {
                    commit_region_results(e, sub_state.results);
                    check_finalize();
                }

                return this_result;
            }

            commit_region_results(e, sub_state.results);
            check_finalize();
            return detail::on_event_t{Discard()};
        }

    private:
        T current_state;
        detail::Queues* queues;
        context_t* contexts;
        std::array<detail::Region, regions_t::size> regions;
        bool finalized = false;

        static void on_enter(T& state, detail::Queues* init_queues, context_t* init_contexts)
        {
            const auto on_enter_result = detail::to_runtime_action_as<detail::on_enter_t>(
                state_t::on_enter(state, init_contexts)
            );

            if (on_enter_result.template holds<detail::Event>())
            {
                init_queues->push_emit(on_enter_result.template get<detail::Event>());
            }
        }

        void on_exit()
        {
            const auto on_exit_result = detail::to_runtime_action_as<detail::on_exit_t>(
                state_t::on_exit(current_state, contexts)
            );

            if (on_exit_result.template holds<detail::Event>())
            {
                queues->push_emit(on_exit_result.template get<detail::Event>());
            }
        }

        detail::on_regions_finalized_t on_regions_finalized()
        {
            return detail::to_runtime_action_as<detail::on_regions_finalized_t>(
                state_t::on_regions_finalized(current_state, contexts)
            );
        }

        sub_state_scan_t dispatch_to_regions(const detail::Event& e)
        {
            sub_state_scan_t scan = {};
            auto no_region_handled = true;
            for (auto i = 0U; i < regions_t::size; ++i)
            {
                scan.results[i] = regions[i].active_state->on_event(e);
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
                scan.results[i].visit(
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
                    }
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

    template <typename API, typename T, typename... Props>
    class SM final: public ISM
    {
        using api_t = API::template state<T>;
        using context_t = typename API::context_t;
        using region_dispatcher_t = detail::region_dispatcher<API, nil::xalt::tlist<T>>;

        struct construct_tag final
        {
        };

        explicit SM(construct_tag /* tag */, context_t* init_contexts, Props*... init_root_args)
            : contexts(init_contexts)
            , root(init_root_args...)
            , region(detail::Region::tag<API, T>{}, 0, &root, &queues, contexts, nullptr)
        {
            flush();
        }

    public:
        explicit SM(context_t* init_contexts, Props*... init_root_args)
            requires(!std::is_void_v<context_t>)
            : SM(construct_tag{}, init_contexts, init_root_args...)
        {
        }

        explicit SM(Props*... init_root_args)
            requires std::is_void_v<context_t>
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
        context_t* contexts;
        detail::RootState<Props...> root;
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

    template <typename T, typename... Props>
    using DefaultSM = SM<api::Default<>, T, Props...>;

    template <typename API, typename T, typename... Props>
    using CoalescedSM = SM<api::Coalesce<API>, T, Props...>;
}
