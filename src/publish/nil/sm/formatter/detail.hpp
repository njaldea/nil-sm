#pragma once

#include "../detail.hpp"
#include "../structs.hpp"
#include "ir.hpp"

#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace nil::sm::formatter::detail
{
    inline std::string stable_state_id(const nil::sm::Metadata& metadata);

    template <template <typename...> typename API, typename StateT>
    nil::sm::Metadata make_metadata(
        const nil::sm::Metadata* parent,
        std::size_t region,
        std::size_t state
    );

    template <template <typename...> typename API, typename... R>
    std::vector<std::vector<ir::Node>> build_regions(const nil::sm::Metadata* parent);

    template <template <typename...> typename API, typename T>
    std::vector<ir::Node> build_region(const nil::sm::Metadata* parent, std::size_t index);

    template <template <typename...> typename API, typename T, typename RegionInitial>
    ir::Node build_node(const nil::sm::Metadata* parent, std::size_t region, std::size_t state);

    template <template <typename...> typename API, typename Target, typename S, std::size_t I = 0>
    consteval std::size_t sibling_index_in_region()
    {
        using reachable_t = typename nil::sm::detail::region_reachability_graph<API, S>::states;

        static_assert(I < reachable_t::size, "Transit target not found in region reachable set");

        using TT = std::remove_cvref_t<typename reachable_t::template at<I>>;
        if constexpr (std::is_same_v<TT, Target>)
        {
            return I;
        }
        else
        {
            return sibling_index_in_region<API, Target, S, I + 1>();
        }
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
                (emit_lifecycle_action<std::remove_cvref_t<V>, ActionT, ResponseT>(actions), ...);
            }(nil::xalt::to_tlist_t<R>{});
        }
    }

    template <template <typename...> typename API, typename RegionInitial, typename R>
    void emit_regions_complete_action(
        const nil::sm::Metadata& node_metadata,
        ir::Node& node,
        std::string_view source_id
    )
    {
        if constexpr (std::is_same_v<R, NOOP>)
        {
            node.actions.emplace_back(
                ir::action::RegionsFinalized{ir::response::ERegionsFinalized::noop}
            );
        }
        else if constexpr (std::is_same_v<R, Terminate>)
        {
            node.transitions.emplace_back(ir::transit::Event{std::string(source_id), "[*]", "[**]"}
            );
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Transit>)
        {
            static constexpr auto target_state
                = sibling_index_in_region<API, typename R::type, RegionInitial>();
            const auto target_metadata = make_metadata<API, typename R::type>(
                node_metadata.parent,
                node_metadata.region,
                target_state
            );
            node.transitions.emplace_back(
                ir::transit::Event{std::string(source_id), stable_state_id(target_metadata), "[**]"}
            );
        }
        else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
        {
            node.actions.emplace_back(
                ir::action::RegionsFinalized{ir::response::ERegionsFinalized::emit}
            );
        }
        else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
        {
            [&]<typename... V>(nil::xalt::tlist<V...>)
            {
                (emit_regions_complete_action<API, RegionInitial, std::remove_cvref_t<V>>(
                     node_metadata,
                     node,
                     source_id
                 ),
                 ...);
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
    void emit_reaction_action(
        const nil::sm::Metadata& node_metadata,
        ir::Node& node,
        std::string_view source_id
    )
    {
        const auto event_name = nil::sm::detail::type_name<E>();

        if constexpr (std::is_same_v<R, Terminate>)
        {
            node.transitions.push_back(
                TransitionInfoT{std::string(source_id), "[*]", std::string(event_name)}
            );
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
            static constexpr auto target_state
                = sibling_index_in_region<API, typename R::type, RegionInitial>();
            const auto target_metadata = make_metadata<API, typename R::type>(
                node_metadata.parent,
                node_metadata.region,
                target_state
            );
            node.transitions.push_back(TransitionInfoT{
                std::string(source_id),
                stable_state_id(target_metadata),
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
                (emit_reaction_action<
                     API,
                     RegionInitial,
                     E,
                     std::remove_cvref_t<V>,
                     ActionInfoT,
                     TransitionInfoT>(node_metadata, node, source_id),
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
        [[maybe_unused]] std::string_view source_id,
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
             std::remove_cvref_t<decltype(api_t::template on_event<E>(
                 std::declval<state_t&>(),
                 std::declval<const E&>(),
                 static_cast<api_context_t*>(nullptr)
             ))>,
             ir::action::Event,
             ir::transit::Event>(node_metadata, node, source_id),
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
        [[maybe_unused]] std::string_view source_id,
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
             std::remove_cvref_t<decltype(api_t::template on_capture<E>(
                 std::declval<state_t&>(),
                 std::declval<const E&>(),
                 static_cast<api_context_t*>(nullptr)
             ))>,
             ir::action::Capture,
             ir::transit::Capture>(node_metadata, node, source_id),
         ...);
    }

    template <template <typename...> typename API, typename T, typename RegionInitial>
    void emit_node_annotations(const nil::sm::Metadata& node_metadata, ir::Node& node)
    {
        using api_t = API<T>;
        using state_t = typename api_t::state_t;
        using api_context_t = typename api_t::api_context_t;
        using on_enter_result_t = std::remove_cvref_t<
            decltype(api_t::on_enter(std::declval<state_t&>(), static_cast<api_context_t*>(nullptr))
            )>;
        using on_exit_result_t = std::remove_cvref_t<
            decltype(api_t::on_exit(std::declval<state_t&>(), static_cast<api_context_t*>(nullptr))
            )>;
        using on_regions_finalized_result_t
            = std::remove_cvref_t<decltype(api_t::on_regions_finalized(
                std::declval<state_t&>(),
                static_cast<api_context_t*>(nullptr)
            ))>;

        const auto& source_id = node.id;
        emit_lifecycle_action<on_enter_result_t, ir::action::Entry, ir::response::EEntry>(
            node.actions
        );
        emit_lifecycle_action<on_exit_result_t, ir::action::Exit, ir::response::EExit>(node.actions
        );
        emit_captures<API, T, RegionInitial>(
            node_metadata,
            node,
            source_id,
            typename api_t::captures_t{}
        );
        emit_events<API, T, RegionInitial>(
            node_metadata,
            node,
            source_id,
            typename api_t::events_t{}
        );
        emit_regions_complete_action<API, RegionInitial, on_regions_finalized_result_t>(
            node_metadata,
            node,
            source_id
        );
    }

    constexpr std::uint64_t fnv_offset = 14695981039346656037ULL;
    constexpr std::uint64_t fnv_prime = 1099511628211ULL;

    inline void hash_byte(std::uint64_t& h, std::uint8_t byte)
    {
        h ^= byte;
        h *= fnv_prime;
    }

    inline void hash_u64(std::uint64_t& h, std::uint64_t value)
    {
        for (std::size_t i = 0; i < 8; ++i)
        {
            hash_byte(h, static_cast<std::uint8_t>((value >> (i * 8U)) & 0xFFU));
        }
    }

    inline void hash_sv(std::uint64_t& h, std::string_view value)
    {
        for (const auto c : value)
        {
            hash_byte(h, static_cast<std::uint8_t>(c));
        }
    }

    inline std::string stable_state_id(const nil::sm::Metadata& metadata)
    {
        auto hash = fnv_offset;

        hash_sv(hash, "nil::sm::uml::stable-id-v1");

        auto ancestry = std::vector<const nil::sm::Metadata*>{};
        for (const auto* parent = metadata.parent; parent != nullptr; parent = parent->parent)
        {
            ancestry.push_back(parent);
        }

        for (const auto* segment : ancestry | std::views::reverse)
        {
            hash_u64(hash, segment->region);
            hash_u64(hash, segment->state);
        }

        hash_u64(hash, metadata.region);
        hash_u64(hash, metadata.state);
        hash_sv(hash, metadata.name);

        return std::format("ST_{:016x}", hash);
    }

    template <template <typename...> typename API, typename StateT>
    nil::sm::Metadata make_metadata(
        const nil::sm::Metadata* parent,
        std::size_t region,
        std::size_t state
    )
    {
        return nil::sm::Metadata{
            .state = state,
            .region = region,
            .subregions = API<StateT>::regions_t::size,
            .name = nil::sm::detail::type_name<StateT>(),
            .parent = parent,
        };
    }

    template <template <typename...> typename API, typename T>
    std::vector<ir::Node> build_region(const nil::sm::Metadata* parent, std::size_t index)
    {
        using reachable_t = typename nil::sm::detail::region_reachability_graph<API, T>::states;

        auto states = std::vector<ir::Node>{};
        states.reserve(reachable_t::size);

        [&]<typename... C>(nil::xalt::tlist<C...>) {
            (([&] { states.push_back(build_node<API, C, T>(parent, index, states.size())); }()),
             ...);
        }(reachable_t{});

        if (!states.empty())
        {
            states.front().is_initial = true;
        }

        return states;
    }

    template <template <typename...> typename API, typename... R>
    std::vector<std::vector<ir::Node>> build_regions(const nil::sm::Metadata* parent)
    {
        auto regions = std::vector<std::vector<ir::Node>>{};
        regions.reserve(sizeof...(R));

        ([&] { regions.push_back(build_region<API, R>(parent, regions.size())); }(), ...);

        return regions;
    }

    template <template <typename...> typename API, typename T, typename RegionInitial>
    ir::Node build_node(const nil::sm::Metadata* parent, std::size_t region, std::size_t state)
    {
        const auto metadata = make_metadata<API, T>(parent, region, state);

        auto node = ir::Node{
            .id = stable_state_id(metadata),
            .display_name = std::string(metadata.name),
            .is_initial = false,
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
