#pragma once

#include "detail.hpp"
#include "id.hpp"

#include <format>
#include <string>
#include <type_traits>
#include <unordered_map>
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
    struct Node
    {
        // Stable occurrence ID. Provider definition nodes use provider-local metadata;
        // barrier occurrences retain their host ancestry so repeated occurrences stay distinct.
        std::string id;

        // display_name is the rendered state label (normally type_name<T>).
        std::string_view display_name;

        // True when this is the first concrete state in its region (state index 0).
        // Formatters use this to render initial markers/attributes.
        bool is_initial = false;
        bool is_final = false;
        bool is_barrier = false;
        std::vector<action::Info> actions; // entry, exit, regions-finalized, event, capture
        std::vector<transit::Info> transitions;
        // event/capture transitions (no entry/exit/regions-finalized transitions)
        std::vector<std::vector<Node>> regions; // empty regions => leaf state
        const void* provider_id = nullptr; // non-null when this node references a provider model
    };

    struct Provider
    {
        const void* id = nullptr;
        std::string_view name;
        std::vector<Node> roots;
    };

    struct Model
    {
        std::vector<Node> roots;
        std::vector<Provider> providers;
    };
}

namespace nil::sm::ir
{
    inline const std::string& target_id(const transit::Info& transition)
    {
        return std::visit(
            [](const auto& info) -> const std::string& { return info.target_id; },
            transition
        );
    }

    inline const std::string& event_name(const transit::Info& transition)
    {
        return std::visit(
            [](const auto& info) -> const std::string& { return info.event_name; },
            transition
        );
    }

    inline bool is_capture(const transit::Info& transition)
    {
        return std::holds_alternative<transit::Capture>(transition);
    }
}

namespace nil::sm::ir::detail
{
    struct BuildContext
    {
        std::vector<ir::Provider> providers;
        std::unordered_map<const void*, std::size_t> provider_indices;

        bool contains(const void* provider_id) const
        {
            return provider_indices.contains(provider_id);
        }

        void add(const void* provider_id, std::string_view provider_name, ir::Model model)
        {
            if (!contains(provider_id))
            {
                provider_indices.emplace(provider_id, providers.size());
                providers.push_back(ir::Provider{provider_id, provider_name, std::move(model.roots)}
                );
            }

            for (auto& provider : model.providers)
            {
                if (!contains(provider.id))
                {
                    provider_indices.emplace(provider.id, providers.size());
                    providers.push_back(std::move(provider));
                }
            }
        }
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
    void emit_regions_complete_action(const nil::sm::Metadata& metadata, ir::Node& node)
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
                API<typename R::type>::regions_t::size,
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
        template <typename>
        typename API,
        typename RegionInitial,
        typename E,
        typename R,
        typename ActionInfoT,
        typename TransitionInfoT>
    void emit_reaction_action(const nil::sm::Metadata& metadata, ir::Node& node)
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
                API<typename R::type>::regions_t::size,
                metadata.parent
            );
            node.transitions.push_back(TransitionInfoT{
                format_stable_id(nil::sm::id::stable_id(target_metadata)),
                std::string(event_name)
            });
        }
    }

    template <
        template <typename...>
        typename API,
        typename T,
        typename RegionInitial,
        typename... E>
    void emit_events(
        const nil::sm::Metadata& metadata,
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
             ir::transit::Event>(metadata, node),
         ...);
    }

    template <
        template <typename...>
        typename API,
        typename T,
        typename RegionInitial,
        typename... E>
    void emit_captures(
        const nil::sm::Metadata& metadata,
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
             ir::transit::Capture>(metadata, node),
         ...);
    }

    template <template <typename> typename API, typename T, typename RegionInitial>
    void emit_node_annotations(const nil::sm::Metadata& metadata, ir::Node& node)
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
        emit_captures<API, T, RegionInitial>(metadata, node, typename api_t::captures_t{});
        emit_events<API, T, RegionInitial>(metadata, node, typename api_t::events_t{});
        emit_regions_complete_action<API, RegionInitial, on_regions_finalized_result_t>(
            metadata,
            node
        );
    }

    template <template <typename> typename API, typename T, typename RegionInitial>
    ir::Node build_node(
        const nil::sm::Metadata* parent,
        std::size_t region,
        std::size_t state,
        BuildContext& context
    );

    template <template <typename> typename API, typename T>
    std::vector<ir::Node> build_region(
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

    template <template <typename> typename API, typename... R>
    std::vector<std::vector<ir::Node>> build_regions(
        const nil::sm::Metadata* parent,
        BuildContext& context
    )
    {
        return [&]<std::size_t... I>(std::index_sequence<I...> /* indices */) {
            return std::vector<std::vector<ir::Node>>{build_region<API, R>(parent, I, context)...};
        }(std::index_sequence_for<R...>());
    }

    // Primary template builds a node from API<T>::regions_t.
    template <template <typename> typename API, typename T>
    struct node_builder
    {
        template <typename RegionInitial>
        static ir::Node node(
            const nil::sm::Metadata* metadata,
            std::size_t state,
            BuildContext& context
        )
        {
            using regions_t = typename API<T>::regions_t;
            auto node = ir::Node{
                .id = format_stable_id(nil::sm::id::stable_id(*metadata)),
                .display_name = metadata->name,
                .is_initial = state == 0,
                .is_final = metadata->is_final,
                .is_barrier = metadata->is_barrier,
                .actions = {},
                .transitions = {},
                .regions = [metadata, &context]<typename... R>(nil::xalt::tlist<R...>)
                { return build_regions<API, R...>(metadata, context); }(regions_t{}),
                .provider_id = nullptr
            };

            emit_node_annotations<API, T, RegionInitial>(*metadata, node);
            return node;
        }
    };

    template <template <typename> typename API, typename FinalizeAction, typename Provider>
    struct node_builder<API, barrier::State<FinalizeAction, Provider>>
    {
        template <typename RegionInitial>
        static ir::Node node(const Metadata* metadata, std::size_t state, BuildContext& context)
        {
            constexpr auto provider_type_id = Provider::id;
            if (!context.contains(provider_type_id))
            {
                context.add(
                    provider_type_id,
                    nil::sm::detail::type_name<Provider>(),
                    Provider::ir(nullptr)
                );
            }

            auto node = ir::Node{
                .id = format_stable_id(nil::sm::id::stable_id(*metadata)),
                .display_name = nil::sm::detail::type_name<Provider>(),
                .is_initial = state == 0,
                .is_final = metadata->is_final,
                .is_barrier = metadata->is_barrier,
                .actions = {},
                .transitions = {},
                .regions = {},
                .provider_id = provider_type_id,
            };

            emit_node_annotations<API, barrier::State<FinalizeAction, Provider>, RegionInitial>(
                *metadata,
                node
            );
            return node;
        }
    };

    template <template <typename> typename API, typename T, typename RegionInitial>
    ir::Node build_node(
        const nil::sm::Metadata* parent,
        std::size_t region,
        std::size_t state,
        BuildContext& context
    )
    {
        const auto metadata
            = nil::sm::detail::make_metadata<T>(region, state, API<T>::regions_t::size, parent);
        return node_builder<API, T>::template node<RegionInitial>(&metadata, state, context);
    }
}

namespace nil::sm::ir
{
    template <template <typename> typename API, typename T>
    Model build(const nil::sm::Metadata* parent = nullptr)
    {
        auto context = detail::BuildContext{};
        return Model{
            .roots = detail::build_region<API, T>(parent, 0, context),
            .providers = std::move(context.providers),
        };
    }

    inline const Provider* find_provider(const Model& model, const void* provider_id)
    {
        for (const auto& provider : model.providers)
        {
            if (provider.id == provider_id)
            {
                return &provider;
            }
        }
        return nullptr;
    }

    template <typename ProviderT>
    const Provider* find_provider(const Model& model)
    {
        return find_provider(model, ProviderT::id);
    }

    template <typename Function>
    void for_each_provider(const Model& model, Function&& function)
    {
        for (const auto& provider : model.providers)
        {
            function(provider);
        }
    }
}
