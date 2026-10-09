// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "api.hpp"
#include "detail.hpp"
#include "id.hpp"
#include "state_validation.hpp"

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nil::sm::ir::response
{
    enum class EEntry
    {
        noop,
        emit
    };

    enum class EExit
    {
        noop,
        emit
    };

    enum class ERegionsFinalized
    {
        noop,
        emit
    };

    enum class EEvent
    {
        discard,
        forward,
        defer,
        emit
    };
}

namespace nil::sm::ir::action
{
    struct Entry
    {
        response::EEntry response;
    };

    struct Exit
    {
        response::EExit response;
    };

    struct RegionsFinalized
    {
        response::ERegionsFinalized response;
    };

    struct Event
    {
        std::string event_name;
        response::EEvent response;
    };

    struct Capture
    {
        std::string event_name;
        response::EEvent response;
    };

    using Info = std::variant<Entry, Exit, RegionsFinalized, Event, Capture>;
}

namespace nil::sm::ir::transit
{
    struct Event
    {
        // target_id uses reserved::termination_node as a synthetic termination sentinel (Terminate
        // action). Otherwise it stores a concrete node id.
        std::string target_id;
        // event_name is the transition label.
        // For on_regions_finalized hooks, the IR builder emits reserved::ev_regions_finalized.
        std::string event_name;
    };

    // Same target_id semantics as Event
    struct Capture
    {
        std::string target_id;
        std::string event_name;
    };

    using Info = std::variant<Event, Capture>;
}

namespace nil::sm::ir
{
    struct Dependency
    {
        const void* type_id = nullptr;
        std::string_view type_name;
        bool is_direct_parent = false;
    };

    struct Node
    {
        // Stable occurrence ID. Barrier definition nodes use barrier-local metadata;
        // barrier occurrences retain their host ancestry so repeated occurrences stay distinct.
        std::string id;

        // display_name is the rendered state label (normally type_name<T>).
        std::string_view display_name;

        // True when this is the first concrete state in its region (state index 0).
        // Formatters use this to render initial markers/attributes.
        bool is_initial = false;
        bool is_final = false;
        bool is_barrier = false;
        const void* type_id = nullptr;
        std::vector<Dependency> required_args;
        std::vector<Dependency> provided_props;
        std::vector<action::Info> actions; // entry, exit, regions-finalized, event, capture
        std::vector<transit::Info> transitions;
        // event/capture transitions (no entry/exit/regions-finalized transitions)
        std::vector<std::vector<Node>> regions; // empty regions => leaf state
        const void* barrier_id = nullptr; // non-null when this node references a barrier model
    };

    struct BarrierDefinition
    {
        const void* id = nullptr;
        std::string_view name;
        std::vector<Node> roots;
    };

    struct Model
    {
        std::vector<Node> roots;
        std::vector<BarrierDefinition> barriers;
    };

    // Types of the arguments passed to the SM constructor; they act as props above the roots.
    template <typename... RootProps>
    constexpr std::vector<Dependency> make_root_props()
    {
        return {Dependency{
            nil::xalt::type_id<RootProps>,
            nil::sm::detail::type_name<RootProps>(),
            false
        }...};
    }
}

namespace nil::sm::ir
{
    constexpr const std::string& target_id(const transit::Info& transition)
    {
        return std::visit(
            [](const auto& info) -> const std::string& { return info.target_id; },
            transition
        );
    }

    constexpr const std::string& event_name(const transit::Info& transition)
    {
        return std::visit(
            [](const auto& info) -> const std::string& { return info.event_name; },
            transition
        );
    }

    constexpr bool is_capture(const transit::Info& transition)
    {
        return std::holds_alternative<transit::Capture>(transition);
    }
}

namespace nil::sm::ir::detail
{
    // A direct_parent<T> requirement is keyed by T, the type the parent exposes (as_parent<T>).
    template <typename... Args>
    constexpr std::vector<ir::Dependency> make_required_args(nil::xalt::tlist<Args...> /* a */)
    {
        return {
            ir::Dependency{nil::xalt::type_id<nil::sm::detail::resolved_arg_type_t<Args>>, nil::sm::detail::type_name<nil::sm::detail::resolved_arg_type_t<Args>>(), nil::xalt::is_of_template_v<Args, direct_parent>}...
        };
    }

    template <typename... Props>
    constexpr std::vector<ir::Dependency> make_provided_props(nil::xalt::tlist<Props...> /* p */)
    {
        return {ir::Dependency{
            nil::xalt::type_id<typename Props::type>,
            nil::sm::detail::type_name<typename Props::type>(),
            false
        }...};
    }

    struct BuildContext
    {
        std::vector<ir::BarrierDefinition> barriers;
        std::vector<std::pair<const void*, std::size_t>> barrier_indices;

        constexpr bool contains(const void* barrier_id) const
        {
            return std::any_of(
                barrier_indices.begin(),
                barrier_indices.end(),
                [barrier_id](const auto& pair) { return pair.first == barrier_id; }
            );
        }

        constexpr void add(const void* barrier_id, std::string_view barrier_name, ir::Model model)
        {
            if (!contains(barrier_id))
            {
                barrier_indices.emplace_back(barrier_id, barriers.size());
                barriers.push_back(
                    ir::BarrierDefinition{barrier_id, barrier_name, std::move(model.roots)}
                );
            }

            for (auto& barrier : model.barriers)
            {
                if (!contains(barrier.id))
                {
                    barrier_indices.emplace_back(barrier.id, barriers.size());
                    barriers.push_back(std::move(barrier));
                }
            }
        }
    };

    constexpr std::string format_stable_id(std::uint64_t value)
    {
        constexpr auto hex_digits = "0123456789abcdef";
        std::string result = "ST_0000000000000000";
        for (std::size_t i = 0; i < 16; ++i)
        {
            result[18 - i] = hex_digits[(value >> (i * 4U)) & 0x0FU];
        }
        return result;
    }

    template <typename R, typename ActionT, typename ResponseT>
    constexpr void emit_lifecycle_action(std::vector<ir::action::Info>& actions)
    {
        if constexpr (std::is_same_v<R, NOOP>)
        {
            actions.push_back(ActionT{ResponseT::noop});
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
        {
            actions.push_back(ActionT{ResponseT::emit});
        }
        else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            [&]<typename... V>(nil::xalt::tlist<V...>) {
                (emit_lifecycle_action<V, ActionT, ResponseT>(actions), ...);
            }(nil::xalt::to_tlist_t<R>{});
        }
    }

    template <typename API, typename RegionInitial, typename R>
    constexpr void emit_regions_complete_action(const nil::sm::Metadata& metadata, ir::Node& node)
    {
        if constexpr (std::is_same_v<R, NOOP>)
        {
            node.actions.emplace_back(
                ir::action::RegionsFinalized{ir::response::ERegionsFinalized::noop}
            );
        }
        else if constexpr (nil::xalt::is_of_template_v<R, TransitTo>)
        {
            using reachable_states_t
                = nil::sm::detail::region_reachability_graph<API, RegionInitial>;
            const auto target_state = reachable_states_t::template index_of<typename R::type>();
            const auto target_metadata = nil::sm::detail::make_metadata<typename R::type>(
                metadata.region,
                target_state,
                API::template state<typename R::type>::regions_t::size,
                metadata.parent
            );
            node.transitions.emplace_back(ir::transit::Event{
                format_stable_id(nil::sm::id::stable_id(target_metadata)),
                std::string(reserved::ev_regions_finalized)
            });
        }
        else if constexpr (std::is_same_v<R, Terminate>)
        {
            node.transitions.emplace_back(ir::transit::Event{
                std::string(reserved::termination_node),
                std::string(reserved::ev_regions_finalized)
            });
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
        {
            node.actions.emplace_back(
                ir::action::RegionsFinalized{ir::response::ERegionsFinalized::emit}
            );
        }
        else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            [&]<typename... V>(nil::xalt::tlist<V...>) {
                (emit_regions_complete_action<API, RegionInitial, V>(metadata, node), ...);
            }(nil::xalt::to_tlist_t<R>{});
        }
    }

    template <
        typename API,
        typename RegionInitial,
        typename E,
        typename R,
        typename ActionInfoT,
        typename TransitionInfoT>
    constexpr void emit_reaction_action(const nil::sm::Metadata& metadata, ir::Node& node)
    {
        const auto event_name = nil::sm::detail::type_name<E>();

        if constexpr (std::is_same_v<R, Discard>)
        {
            node.actions.emplace_back(
                ActionInfoT{std::string(event_name), ir::response::EEvent::discard}
            );
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
        {
            node.actions.emplace_back(
                ActionInfoT{std::string(event_name), ir::response::EEvent::emit}
            );
        }
        else if constexpr (std::is_same_v<R, Forward>)
        {
            node.actions.emplace_back(
                ActionInfoT{std::string(event_name), ir::response::EEvent::forward}
            );
        }
        else if constexpr (std::is_same_v<R, Defer>)
        {
            node.actions.emplace_back(
                ActionInfoT{std::string(event_name), ir::response::EEvent::defer}
            );
        }
        else if constexpr (std::is_same_v<R, Unhandled>)
        {
            return;
        }
        else if constexpr (std::is_same_v<R, Terminate>)
        {
            node.transitions.push_back(
                TransitionInfoT{std::string(reserved::termination_node), std::string(event_name)}
            );
        }
        else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            [&]<typename... V>(nil::xalt::tlist<V...>)
            {
                (emit_reaction_action<API, RegionInitial, E, V, ActionInfoT, TransitionInfoT>(
                     metadata,
                     node
                 ),
                 ...);
            }(nil::xalt::to_tlist_t<R>{});
        }
        else
        {
            if constexpr (nil::xalt::is_of_template_v<R, DeferTo>)
            {
                node.actions.emplace_back(
                    ActionInfoT{std::string(event_name), ir::response::EEvent::defer}
                );
            }

            const auto target_state
                = nil::sm::detail::region_reachability_graph<API, RegionInitial>::template index_of<
                    typename R::type>();
            const auto target_metadata = nil::sm::detail::make_metadata<typename R::type>(
                metadata.region,
                target_state,
                API::template state<typename R::type>::regions_t::size,
                metadata.parent
            );
            node.transitions.push_back(TransitionInfoT{
                format_stable_id(nil::sm::id::stable_id(target_metadata)),
                std::string(event_name)
            });
        }
    }

    template <typename API, typename T, typename RegionInitial, typename... E>
    constexpr void emit_events(
        const nil::sm::Metadata& metadata,
        ir::Node& node,
        nil::xalt::tlist<E...> /* events */
    )
    {
        using state_t = API::template state<T>;
        using context_t = typename API::context_t;

        (emit_reaction_action<
             API,
             RegionInitial,
             E,
             decltype(state_t::template on_event<E>(
                 std::declval<T&>(),
                 std::declval<const E&>(),
                 static_cast<context_t*>(nullptr)
             )),
             ir::action::Event,
             ir::transit::Event>(metadata, node),
         ...);
    }

    template <typename API, typename T, typename RegionInitial, typename... E>
    constexpr void emit_captures(
        const nil::sm::Metadata& metadata,
        ir::Node& node,
        nil::xalt::tlist<E...> /* captures */
    )
    {
        using state_t = API::template state<T>;
        using context_t = typename API::context_t;

        (emit_reaction_action<
             API,
             RegionInitial,
             E,
             decltype(state_t::template on_capture<E>(
                 std::declval<T&>(),
                 std::declval<const E&>(),
                 static_cast<context_t*>(nullptr)
             )),
             ir::action::Capture,
             ir::transit::Capture>(metadata, node),
         ...);
    }

    template <typename API, typename T, typename RegionInitial>
    constexpr void emit_node_annotations(const nil::sm::Metadata& metadata, ir::Node& node)
    {
        using state_t = API::template state<T>;
        using context_t = typename API::context_t;
        using on_enter_result_t
            = decltype(state_t::on_enter(std::declval<T&>(), static_cast<context_t*>(nullptr)));
        using on_exit_result_t
            = decltype(state_t::on_exit(std::declval<T&>(), static_cast<context_t*>(nullptr)));
        using on_regions_finalized_result_t = decltype(state_t::on_regions_finalized(
            std::declval<T&>(),
            static_cast<context_t*>(nullptr)
        ));

        emit_lifecycle_action<on_enter_result_t, ir::action::Entry, ir::response::EEntry>(
            node.actions
        );
        emit_lifecycle_action<on_exit_result_t, ir::action::Exit, ir::response::EExit>(node.actions
        );
        emit_captures<API, T, RegionInitial>(metadata, node, typename state_t::captures_t{});
        emit_events<API, T, RegionInitial>(metadata, node, typename state_t::events_t{});
        emit_regions_complete_action<API, RegionInitial, on_regions_finalized_result_t>(
            metadata,
            node
        );
    }

    template <typename API, typename T, typename RegionInitial>
    constexpr ir::Node build_node(
        const nil::sm::Metadata* parent,
        std::size_t region,
        std::size_t state,
        BuildContext& context
    );

    template <typename API, typename T>
    constexpr std::vector<ir::Node> build_region(
        const nil::sm::Metadata* parent,
        std::size_t index,
        BuildContext& context
    )
    {
        using reachable_t = typename nil::sm::detail::region_reachability_graph<API, T>::states;
        auto nodes =
            [&]<typename... C, std::size_t... I>(nil::xalt::tlist<C...>, std::index_sequence<I...>)
        {
            return std::vector<ir::Node>{build_node<API, C, T>(parent, index, I, context)...};
        }(reachable_t{}, std::make_index_sequence<reachable_t::size>{});

        const auto has_termination = std::any_of(
            nodes.begin(),
            nodes.end(),
            [](const auto& node)
            {
                return std::any_of(
                    node.transitions.begin(),
                    node.transitions.end(),
                    [](const auto& transition)
                    { return target_id(transition) == reserved::termination_node; }
                );
            }
        );

        if (has_termination
            && !std::any_of(
                nodes.begin(),
                nodes.end(),
                [](const auto& node) { return node.is_final; }
            ))
        {
            nodes.push_back(build_node<API, Fin, T>(parent, index, Fin::state_index, context));
        }

        return nodes;
    }

    template <typename API, typename... R>
    constexpr std::vector<std::vector<ir::Node>> build_regions(
        const nil::sm::Metadata* parent,
        BuildContext& context
    )
    {
        return [&]<std::size_t... I>(std::index_sequence<I...> /* indices */) {
            return std::vector<std::vector<ir::Node>>{build_region<API, R>(parent, I, context)...};
        }(std::index_sequence_for<R...>());
    }

    // Primary template builds a node from API::template state<T>::regions_t.
    template <typename API, typename T>
    struct node_builder
    {
        template <typename RegionInitial>
        constexpr static ir::Node node(
            const nil::sm::Metadata* metadata,
            std::size_t state,
            BuildContext& context
        )
        {
            using regions_t = typename API::template state<T>::regions_t;
            auto node = ir::Node{
                .id = format_stable_id(nil::sm::id::stable_id(*metadata)),
                .display_name = metadata->name,
                .is_initial = state == 0,
                .is_final = metadata->is_final,
                .is_barrier = metadata->is_barrier,
                .type_id = nil::xalt::type_id<T>,
                .required_args = make_required_args(typename API::template state<T>::args_t{}),
                .provided_props = make_provided_props(typename API::template state<T>::props_t{}),
                .actions = {},
                .transitions = {},
                .regions = [metadata, &context]<typename... R>(nil::xalt::tlist<R...>)
                { return build_regions<API, R...>(metadata, context); }(regions_t{}),
                .barrier_id = nullptr
            };

            emit_node_annotations<API, T, RegionInitial>(*metadata, node);
            return node;
        }
    };

    template <typename API, typename Action, typename T>
    struct node_builder<API, barrier::State<Action, T>>
    {
        template <typename RegionInitial>
        constexpr static ir::Node node(
            const Metadata* metadata,
            std::size_t state,
            BuildContext& context
        )
        {
            if (!context.contains(T::id))
            {
                context.add(T::id, nil::sm::detail::type_name<T>(), T::ir(nullptr));
            }

            auto node = ir::Node{
                .id = format_stable_id(nil::sm::id::stable_id(*metadata)),
                .display_name = nil::sm::detail::type_name<T>(),
                .is_initial = state == 0,
                .is_final = metadata->is_final,
                .is_barrier = metadata->is_barrier,
                .type_id = nil::xalt::type_id<barrier::State<Action, T>>,
                .required_args = {},
                .provided_props = {},
                .actions = {},
                .transitions = {},
                .regions = {},
                .barrier_id = T::id,
            };

            emit_node_annotations<API, barrier::State<Action, T>, RegionInitial>(*metadata, node);
            return node;
        }
    };

    template <typename API, typename T, typename RegionInitial>
    constexpr ir::Node build_node(
        const nil::sm::Metadata* parent,
        std::size_t region,
        std::size_t state,
        BuildContext& context
    )
    {
        const auto metadata = nil::sm::detail::make_metadata<T>(
            region,
            state,
            API::template state<T>::regions_t::size,
            parent
        );
        return node_builder<API, T>::template node<RegionInitial>(&metadata, state, context);
    }
}

namespace nil::sm::ir
{
    namespace detail
    {
        template <typename T, typename API = nil::sm::api::Default<>>
        constexpr Model build_unchecked(const nil::sm::Metadata* parent)
        {
            auto context = BuildContext{};
            return Model{
                .roots = build_region<API, T>(parent, 0, context),
                .barriers = std::move(context.barriers),
            };
        }
    }

    template <typename T, typename API = nil::sm::api::Default<>>
    constexpr Model build(const nil::sm::Metadata* parent = nullptr)
    {
        return detail::build_unchecked<T, API>(parent);
    }

    constexpr const BarrierDefinition* find_barrier(const Model& model, const void* barrier_id)
    {
        for (const auto& barrier : model.barriers)
        {
            if (barrier.id == barrier_id)
            {
                return &barrier;
            }
        }
        return nullptr;
    }

    template <typename T>
    const BarrierDefinition* find_barrier(const Model& model)
    {
        return find_barrier(model, T::id);
    }

    template <typename Function>
    void for_each_barrier(const Model& model, Function&& function)
    {
        for (const auto& barrier : model.barriers)
        {
            function(barrier);
        }
    }
}
