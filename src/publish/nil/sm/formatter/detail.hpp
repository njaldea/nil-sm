#pragma once

#include "../detail.hpp"
#include "../id.hpp"
#include "../ir.hpp"
#include "../structs.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace nil::sm::formatter::detail
{
    // Customization point for opaque leaf states such as barrier::State: specialized in
    // formatter/barrier.hpp so this header does not need to depend on barrier.hpp. When
    // available, build_node splices model().roots in as this node's one nested region.
    template <typename T>
    struct barrier_ir
    {
        static constexpr bool available = false;
    };

    inline std::string format_stable_id(std::uint64_t value)
    {
        return std::format("ST_{:016x}", value);
    }

    template <typename R, typename ActionT, typename ResponseT>
    void emit_lifecycle_action(std::vector<ir::action::Info>& actions)
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

    template <template <typename> typename API, typename RegionInitial, typename R>
    void emit_regions_complete_action(const nil::sm::Metadata& node_metadata, ir::Node& node)
    {
        if constexpr (std::is_same_v<R, NOOP>)
        {
            using type = ir::action::RegionsFinalized;
            node.actions.emplace_back(type{ir::response::ERegionsFinalized::noop});
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Transit>)
        {
            using reachable_states_t
                = nil::sm::detail::region_reachability_graph<API, RegionInitial>;
            const auto target_state = reachable_states_t::template index_of<typename R::type>();
            const auto target_metadata = nil::sm::detail::make_metadata<typename R::type>(
                node_metadata.region,
                target_state,
                API<typename R::type>::regions_t::size,
                node_metadata.parent
            );

            using type = ir::transit::Event;
            node.transitions.emplace_back(
                type{format_stable_id(nil::sm::id::stable_id(target_metadata)), "[**]"}
            );
        }
        else if constexpr (std::is_same_v<R, Terminate>)
        {
            using type = ir::transit::Event;
            node.transitions.emplace_back(type{"[*]", "[**]"});
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
        {
            using type = ir::action::RegionsFinalized;
            node.actions.emplace_back(type{ir::response::ERegionsFinalized::emit});
        }
        else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            [&]<typename... V>(nil::xalt::tlist<V...>) {
                (emit_regions_complete_action<API, RegionInitial, V>(node_metadata, node), ...);
            }(nil::xalt::to_tlist_t<R>{});
        }
    }

    template <
        template <typename>
        typename API,
        typename RegionInitial,
        typename E,
        typename R,
        typename ActionInfoT,
        typename TransitionInfoT>
    void emit_reaction_action(const nil::sm::Metadata& node_metadata, ir::Node& node)
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
        else if constexpr (nil::xalt::is_of_template_v<R, Transit>)
        {
            const auto target_state
                = nil::sm::detail::region_reachability_graph<API, RegionInitial>::template index_of<
                    typename R::type>();
            const auto target_metadata = nil::sm::detail::make_metadata<typename R::type>(
                node_metadata.region,
                target_state,
                API<typename R::type>::regions_t::size,
                node_metadata.parent
            );
            node.transitions.push_back(TransitionInfoT{
                format_stable_id(nil::sm::id::stable_id(target_metadata)),
                std::string(event_name)
            });
        }
        if constexpr (std::is_same_v<R, Terminate>)
        {
            node.transitions.push_back(TransitionInfoT{"[*]", std::string(event_name)});
        }
        else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            [&]<typename... V>(nil::xalt::tlist<V...>)
            {
                (emit_reaction_action<API, RegionInitial, E, V, ActionInfoT, TransitionInfoT>(
                     node_metadata,
                     node
                 ),
                 ...);
            }(nil::xalt::to_tlist_t<R>{});
        }
    }

    template <
        template <typename...>
        typename API,
        typename T,
        typename RegionInitial,
        typename... E>
    void emit_events(
        const nil::sm::Metadata& node_metadata,
        ir::Node& node,
        nil::xalt::tlist<E...> /* events */
    )
    {
        using api_t = API<T>;
        using state_t = typename api_t::state_t;
        using api_context_t = typename api_t::api_context_t;

        (emit_reaction_action<
             API,
             RegionInitial,
             E,
             decltype(api_t::template on_event<E>(
                 std::declval<state_t&>(),
                 std::declval<const E&>(),
                 static_cast<api_context_t*>(nullptr)
             )),
             ir::action::Event,
             ir::transit::Event>(node_metadata, node),
         ...);
    }

    template <
        template <typename...>
        typename API,
        typename T,
        typename RegionInitial,
        typename... E>
    void emit_captures(
        const nil::sm::Metadata& node_metadata,
        ir::Node& node,
        nil::xalt::tlist<E...> /* captures */
    )
    {
        using api_t = API<T>;
        using state_t = typename api_t::state_t;
        using api_context_t = typename api_t::api_context_t;

        (emit_reaction_action<
             API,
             RegionInitial,
             E,
             decltype(api_t::template on_capture<E>(
                 std::declval<state_t&>(),
                 std::declval<const E&>(),
                 static_cast<api_context_t*>(nullptr)
             )),
             ir::action::Capture,
             ir::transit::Capture>(node_metadata, node),
         ...);
    }

    template <template <typename> typename API, typename T, typename RegionInitial>
    void emit_node_annotations(const nil::sm::Metadata& node_metadata, ir::Node& node)
    {
        using api_t = API<T>;
        using state_t = typename api_t::state_t;
        using api_context_t = typename api_t::api_context_t;
        using on_enter_result_t = decltype(api_t::on_enter(
            std::declval<state_t&>(),
            static_cast<api_context_t*>(nullptr)
        ));
        using on_exit_result_t = decltype(api_t::on_exit(
            std::declval<state_t&>(),
            static_cast<api_context_t*>(nullptr)
        ));
        using on_regions_finalized_result_t = decltype(api_t::on_regions_finalized(
            std::declval<state_t&>(),
            static_cast<api_context_t*>(nullptr)
        ));

        emit_lifecycle_action<on_enter_result_t, ir::action::Entry, ir::response::EEntry>(
            node.actions
        );
        emit_lifecycle_action<on_exit_result_t, ir::action::Exit, ir::response::EExit>(node.actions
        );
        emit_captures<API, T, RegionInitial>(node_metadata, node, typename api_t::captures_t{});
        emit_events<API, T, RegionInitial>(node_metadata, node, typename api_t::events_t{});
        emit_regions_complete_action<API, RegionInitial, on_regions_finalized_result_t>(
            node_metadata,
            node
        );
    }

    template <template <typename> typename API, typename T, typename RegionInitial>
    ir::Node build_node(const nil::sm::Metadata* parent, std::size_t region, std::size_t state);

    template <template <typename> typename API, typename T>
    std::vector<ir::Node> build_region(const nil::sm::Metadata* parent, std::size_t index)
    {
        using reachable_t = typename nil::sm::detail::region_reachability_graph<API, T>::states;

        auto nodes =
            [&]<typename... C, std::size_t... I>(nil::xalt::tlist<C...>, std::index_sequence<I...>)
        {
            return std::vector<ir::Node>{build_node<API, C, T>(parent, index, I)...};
        }(reachable_t{}, std::make_index_sequence<reachable_t::size>{});

        const auto has_termination = std::any_of(
            nodes.begin(),
            nodes.end(),
            [](const auto& node)
            {
                return std::any_of(
                    node.transitions.begin(),
                    node.transitions.end(),
                    [](const auto& transition) { return target_id(transition) == "[*]"; }
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
            nodes.push_back(build_node<API, Fin, T>(parent, index, Fin::state_index));
        }

        return nodes;
    }

    template <template <typename> typename API, typename... R>
    std::vector<std::vector<ir::Node>> build_regions(const nil::sm::Metadata* parent)
    {
        return [&]<std::size_t... I>(std::index_sequence<I...> /* indices */) {
            return std::vector<std::vector<ir::Node>>{build_region<API, R>(parent, I)...};
        }(std::index_sequence_for<R...>());
    }

    template <template <typename> typename API, typename T, typename RegionInitial>
    ir::Node build_node(const nil::sm::Metadata* parent, std::size_t region, std::size_t state)
    {
        const auto metadata
            = nil::sm::detail::make_metadata<T>(region, state, API<T>::regions_t::size, parent);

        auto node = ir::Node{
            .id = format_stable_id(nil::sm::id::stable_id(metadata)),
            .display_name = metadata.name,
            .is_initial = state == 0,
            .is_final = metadata.is_final,
            .actions = {},
            .transitions = {},
            .regions = {},
        };

        using regions_t = typename API<T>::regions_t;

        if constexpr (barrier_ir<T>::available)
        {
            node.regions = {barrier_ir<T>::model(&metadata).roots};
        }
        else if constexpr (regions_t::size > 0)
        {
            node.regions = [&metadata]<typename... R>(nil::xalt::tlist<R...>)
            { return build_regions<API, R...>(&metadata); }(regions_t{});
        }

        emit_node_annotations<API, T, RegionInitial>(metadata, node);

        return node;
    }
}

namespace nil::sm::ir
{
    // Public entry point (unlike the rest of this header): builds the format-neutral IR for
    // API/T, for use by custom renderers or by code that needs a barrier's Provider::ir().
    template <template <typename> typename API, typename T>
    Model build(const nil::sm::Metadata* parent = nullptr)
    {
        return Model{.roots = formatter::detail::build_region<API, T>(parent, 0)};
    }
}
