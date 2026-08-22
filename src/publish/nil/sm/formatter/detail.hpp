#pragma once

#include "../detail.hpp"
#include "../id.hpp"
#include "../structs.hpp"
#include "ir.hpp"

#include <format>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nil::sm::formatter::detail
{
    inline std::string format_stable_id(std::uint64_t value)
    {
        return std::format("ST_{:016x}", value);
    }

    template <template <typename...> typename API, typename T>
    nil::sm::Metadata make_metadata(
        const nil::sm::Metadata* parent,
        std::size_t region,
        std::size_t state
    )
    {
        return nil::sm::Metadata{
            .state = state,
            .region = region,
            .subregions = API<T>::regions_t::size,
            .name = nil::sm::detail::type_name<T>(),
            .parent = parent,
        };
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

    template <template <typename...> typename API, typename RegionInitial, typename R>
    void emit_regions_complete_action(const nil::sm::Metadata& node_metadata, ir::Node& node)
    {
        if constexpr (std::is_same_v<R, NOOP>)
        {
            using type = ir::action::RegionsFinalized;
            node.actions.emplace_back(type{ir::response::ERegionsFinalized::noop});
        }
        else if constexpr (std::is_same_v<R, Terminate>)
        {
            using type = ir::transit::Event;
            node.transitions.emplace_back(type{"[*]", "[**]"});
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Transit>)
        {
            using reachable_states_t
                = nil::sm::detail::region_reachability_graph<API, RegionInitial>;
            const auto target_state = reachable_states_t::template index_of<typename R::type>();
            const auto target_metadata = make_metadata<API, typename R::type>(
                node_metadata.parent,
                node_metadata.region,
                target_state
            );

            using type = ir::transit::Event;
            node.transitions.emplace_back(
                type{format_stable_id(nil::sm::id::stable_id(target_metadata)), "[**]"}
            );
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
        template <typename...>
        typename API,
        typename RegionInitial,
        typename E,
        typename R,
        typename ActionInfoT,
        typename TransitionInfoT>
    void emit_reaction_action(const nil::sm::Metadata& node_metadata, ir::Node& node)
    {
        const auto event_name = nil::sm::detail::type_name<E>();

        if constexpr (std::is_same_v<R, Terminate>)
        {
            node.transitions.push_back(TransitionInfoT{"[*]", std::string(event_name)});
        }
        else if constexpr (std::is_same_v<R, Discard>)
        {
            node.actions.emplace_back(
                ActionInfoT{std::string(event_name), ir::response::EEvent::discard}
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
            const auto target_metadata = make_metadata<API, typename R::type>(
                node_metadata.parent,
                node_metadata.region,
                target_state
            );
            node.transitions.push_back(TransitionInfoT{
                format_stable_id(nil::sm::id::stable_id(target_metadata)),
                std::string(event_name)
            });
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
        {
            node.actions.emplace_back(
                ActionInfoT{std::string(event_name), ir::response::EEvent::emit}
            );
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

    template <template <typename...> typename API, typename T, typename RegionInitial>
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

    template <template <typename...> typename API, typename T, typename RegionInitial>
    ir::Node build_node(const nil::sm::Metadata* parent, std::size_t region, std::size_t state);

    template <template <typename...> typename API, typename T>
    std::vector<ir::Node> build_region(const nil::sm::Metadata* parent, std::size_t index)
    {
        using reachable_t = typename nil::sm::detail::region_reachability_graph<API, T>::states;

        return
            [&]<typename... C, std::size_t... I>(nil::xalt::tlist<C...>, std::index_sequence<I...>)
        {
            return std::vector<ir::Node>{build_node<API, C, T>(parent, index, I)...};
        }(reachable_t{}, std::make_index_sequence<reachable_t::size>{});
    }

    template <template <typename...> typename API, typename... R>
    std::vector<std::vector<ir::Node>> build_regions(const nil::sm::Metadata* parent)
    {
        return [&]<std::size_t... I>(std::index_sequence<I...> /* indices */) {
            return std::vector<std::vector<ir::Node>>{build_region<API, R>(parent, I)...};
        }(std::index_sequence_for<R...>());
    }

    template <template <typename...> typename API, typename T, typename RegionInitial>
    ir::Node build_node(const nil::sm::Metadata* parent, std::size_t region, std::size_t state)
    {
        const auto metadata = make_metadata<API, T>(parent, region, state);

        auto node = ir::Node{
            .id = format_stable_id(nil::sm::id::stable_id(metadata)),
            .display_name = std::string(metadata.name),
            .is_initial = state == 0,
            .actions = {},
            .transitions = {},
            .regions = {},
        };

        using regions_t = typename API<T>::regions_t;

        if constexpr (regions_t::size > 0)
        {
            node.regions = [&metadata]<typename... R>(nil::xalt::tlist<R...>)
            { return build_regions<API, R...>(&metadata); }(regions_t{});
        }

        emit_node_annotations<API, T, RegionInitial>(metadata, node);

        return node;
    }

    template <template <typename...> typename API, typename T>
    ir::Model build_ir()
    {
        return ir::Model{.roots = build_region<API, T>(nullptr, 0)};
    }
}
