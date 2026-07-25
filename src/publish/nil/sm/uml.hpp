#include "../sm.hpp"

#include <cstdint>
#include <format>
#include <memory>
#include <ostream>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace nil::sm::uml::detail
{
    struct INode
    {
        INode() = default;
        INode(INode&&) = delete;
        INode(const INode&) = delete;
        INode& operator=(INode&&) = delete;
        INode& operator=(const INode&) = delete;
        virtual ~INode() = default;

        virtual std::string id() const = 0;
        virtual void render(std::ostream& os, std::size_t depth) const = 0;
    };

    template <template <typename...> typename API, typename T, typename RegionInitial>
    struct Node;

    template <template <typename...> typename API, typename T>
    struct RegionTag final
    {
    };

    struct Region
    {
        std::vector<std::unique_ptr<INode>> states;

        template <template <typename...> typename API, typename T>
        Region(
            RegionTag<API, T> /* tag */,
            const nil::sm::state_metadata* parent,
            std::size_t index
        )
        {
            using reachable_t = typename nil::sm::detail::region_reachability_graph<API, T>::states;

            states.reserve(reachable_t::size);

            [&]<std::size_t... I>(std::index_sequence<I...>)
            {
                ((
                     [&]
                     {
                         using candidate_t
                             = std::remove_cvref_t<typename reachable_t::template at<I>>;
                         states.push_back(
                             std::make_unique<Node<API, candidate_t, T>>(parent, index, I)
                         );
                     }()
                 ),
                 ...);
            }(std::make_index_sequence<reachable_t::size>{});
        }

        Region(Region&&) = default;
        Region(const Region&) = delete;
        Region& operator=(Region&&) = default;
        Region& operator=(const Region&) = delete;
        ~Region() noexcept = default;

        void render(std::ostream& os, std::size_t depth) const;

        template <template <typename...> typename API, typename... R>
        static std::vector<Region> make_all(const nil::sm::state_metadata* parent);
    };

    inline std::string stable_state_id(const nil::sm::state_metadata& metadata);

    template <template <typename...> typename API, typename StateT>
    nil::sm::state_metadata make_metadata(
        const nil::sm::state_metadata* parent,
        std::size_t region,
        std::size_t state
    );

    inline std::ostream& indent(std::ostream& os, std::size_t depth);

    template <template <typename...> typename API, typename T, typename RegionInitial>
    struct Node final: INode
    {
        nil::sm::state_metadata metadata;
        std::vector<Region> regions;

        Node(const nil::sm::state_metadata* parent, std::size_t region, std::size_t state)
            : metadata(make_metadata<API, T>(parent, region, state))
        {
            using regions_t = typename API<T>::regions_t;

            if constexpr (regions_t::size > 0)
            {
                regions = []<typename... R>(nil::xalt::tlist<R...>, const auto* parent_metadata) {
                    return Region::make_all<API, R...>(parent_metadata);
                }(regions_t{}, std::addressof(metadata));
            }
        }

        std::string id() const override
        {
            return stable_state_id(metadata);
        }

        void render(std::ostream& os, std::size_t depth) const override
        {
            const auto node_id = id();

            if (!regions.empty())
            {
                indent(os, depth) << "state " << node_id << " as \"" << metadata.name << "\" {\n";
                for (auto region_idx = std::size_t{0}; region_idx < regions.size(); ++region_idx)
                {
                    regions[region_idx].render(os, depth + 1);
                    if (region_idx + 1 < regions.size())
                    {
                        indent(os, depth + 1) << "||\n";
                    }
                }
                indent(os, depth) << "}\n";
            }
            else
            {
                indent(os, depth) << "state " << node_id << " as \"" << metadata.name << "\"\n";
            }

            emit_annotations(*this, os, depth);
        }

        template <typename Target, typename S, std::size_t I = 0>
        static consteval std::size_t sibling_index_in_region()
        {
            using reachable_t = typename nil::sm::detail::region_reachability_graph<API, S>::states;

            static_assert(
                I < reachable_t::size,
                "Transit target not found in region reachable set"
            );

            using TT = std::remove_cvref_t<typename reachable_t::template at<I>>;
            if constexpr (std::is_same_v<TT, Target>)
            {
                return I;
            }
            else
            {
                return sibling_index_in_region<Target, S, I + 1>();
            }
        }

        static void emit_annotations(const Node& node, std::ostream& os, std::size_t depth)
        {
            using api_t = API<T>;
            using state_t = typename api_t::state_t;
            using api_context_t = typename api_t::api_context_t;
            using on_enter_result_t = std::remove_cvref_t<decltype(api_t::on_enter(
                std::declval<state_t&>(),
                static_cast<api_context_t*>(nullptr)
            ))>;
            using on_exit_result_t = std::remove_cvref_t<decltype(api_t::on_exit(
                std::declval<state_t&>(),
                static_cast<api_context_t*>(nullptr)
            ))>;
            using on_regions_finalized_result_t
                = std::remove_cvref_t<decltype(api_t::on_regions_finalized(
                    std::declval<state_t&>(),
                    static_cast<api_context_t*>(nullptr)
                ))>;

            const auto source_id = node.id();
            emit_lifecycle_action<on_enter_result_t>(node, os, source_id, "Enter", depth);
            emit_lifecycle_action<on_exit_result_t>(node, os, source_id, "Exit", depth);
            emit_events(node, os, source_id, typename api_t::events_t{}, depth);
            emit_regions_complete_action<on_regions_finalized_result_t>(node, os, source_id, depth);
        }

        template <typename E, typename R>
        static void emit_event_action(
            const Node& node,
            std::ostream& os,
            const std::string& source_id,
            std::size_t depth
        )
        {
            if constexpr (std::is_same_v<R, Terminate>)
            {
                indent(os, depth) << source_id << " --> [*] : " << nil::sm::detail::type_name<E>()
                                  << "\n";
            }
            else if constexpr (std::is_same_v<R, Discard>)
            {
                indent(os, depth) << source_id << " : on " << nil::sm::detail::type_name<E>()
                                  << " / Discard\n";
            }
            else if constexpr (std::is_same_v<R, Forward>)
            {
                indent(os, depth) << source_id << " : on " << nil::sm::detail::type_name<E>()
                                  << " / Forward\n";
            }
            else if constexpr (std::is_same_v<R, Defer>)
            {
                indent(os, depth) << source_id << " : on " << nil::sm::detail::type_name<E>()
                                  << " / Defer\n";
            }
            else if constexpr (std::is_same_v<R, Unhandled>)
            {
                return;
            }
            else if constexpr (nil::xalt::is_of_template_v<R, Transit>)
            {
                static constexpr auto target_state
                    = sibling_index_in_region<typename R::type, RegionInitial>();
                const auto target_metadata = make_metadata<API, typename R::type>(
                    node.metadata.parent,
                    node.metadata.region,
                    target_state
                );
                const auto target_id = stable_state_id(target_metadata);

                indent(os, depth) << source_id << " --> " << target_id << " : "
                                  << nil::sm::detail::type_name<E>() << "\n";
            }
            else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
            {
                indent(os, depth) << source_id << " : on " << nil::sm::detail::type_name<E>()
                                  << " / Emit\n";
            }
            else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
            {
                [&]<typename... V>(nil::xalt::tlist<V...>) {
                    (emit_event_action<E, std::remove_cvref_t<V>>(node, os, source_id, depth), ...);
                }(nil::xalt::to_tlist_t<R>{});
            }
        }

        template <typename R>
        static void emit_lifecycle_action(
            const Node& node,
            std::ostream& os,
            const std::string& source_id,
            std::string_view hook_name,
            std::size_t depth
        )
        {
            (void)node;
            if constexpr (std::is_same_v<R, NOOP>)
            {
                indent(os, depth) << source_id << " : on " << hook_name << " / NOOP\n";
            }
            else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
            {
                indent(os, depth) << source_id << " : on " << hook_name << " / Emit\n";
            }
            else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
            {
                [&]<typename... V>(nil::xalt::tlist<V...>) {
                    (emit_lifecycle_action<
                         std::remove_cvref_t<V>>(node, os, source_id, hook_name, depth),
                     ...);
                }(nil::xalt::to_tlist_t<R>{});
            }
        }

        template <typename R>
        static void emit_regions_complete_action(
            const Node& node,
            std::ostream& os,
            const std::string& source_id,
            std::size_t depth
        )
        {
            if constexpr (std::is_same_v<R, NOOP>)
            {
                indent(os, depth) << source_id << " : on [**] / NOOP\n";
            }
            else if constexpr (std::is_same_v<R, Terminate>)
            {
                indent(os, depth) << source_id << " --> [*] : [**]\n";
            }
            else if constexpr (nil::xalt::is_of_template_v<R, Transit>)
            {
                static constexpr auto target_state
                    = sibling_index_in_region<typename R::type, RegionInitial>();
                const auto target_metadata = make_metadata<API, typename R::type>(
                    node.metadata.parent,
                    node.metadata.region,
                    target_state
                );
                const auto target_id = stable_state_id(target_metadata);

                indent(os, depth) << source_id << " --> " << target_id << " : [**]\n";
            }
            else if constexpr (nil::xalt::is_of_template_v<R, Emit>)
            {
                indent(os, depth) << source_id << " : on [**] / Emit\n";
            }
            else if constexpr (nil::xalt::is_of_template_v<R, std::variant>)
            {
                [&]<typename... V>(nil::xalt::tlist<V...>) {
                    (emit_regions_complete_action<std::remove_cvref_t<V>>(
                         node,
                         os,
                         source_id,
                         depth
                     ),
                     ...);
                }(nil::xalt::to_tlist_t<R>{});
            }
        }

        template <typename... E>
        static void emit_events(
            const Node& node,
            std::ostream& os,
            const std::string& source_id,
            nil::xalt::tlist<E...> events,
            std::size_t depth
        )
        {
            (void)events;
            using api_t = API<T>;
            using state_t = typename api_t::state_t;
            using api_context_t = typename api_t::api_context_t;

            (emit_event_action<
                 E,
                 std::remove_cvref_t<decltype(api_t::template on_event<E>(
                     std::declval<state_t&>(),
                     std::declval<const E&>(),
                     static_cast<api_context_t*>(nullptr)
                 ))>>(node, os, source_id, depth),
             ...);
        }
    };

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

    inline std::string stable_state_id(const nil::sm::state_metadata& metadata)
    {
        auto hash = fnv_offset;

        hash_sv(hash, "nil::sm::uml::stable-id-v1");

        auto ancestry = std::vector<const nil::sm::state_metadata*>{};
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
    nil::sm::state_metadata make_metadata(
        const nil::sm::state_metadata* parent,
        std::size_t region,
        std::size_t state
    )
    {
        return nil::sm::state_metadata{
            .state = state,
            .region = region,
            .region_count = API<StateT>::regions_t::size,
            .name = nil::sm::detail::type_name<StateT>(),
            .parent = parent,
        };
    }

    inline std::ostream& indent(std::ostream& os, std::size_t depth)
    {
        for (std::size_t i = 0; i < depth; ++i)
        {
            os << "  ";
        }

        return os;
    }

    inline void Region::render(std::ostream& os, std::size_t depth) const
    {
        for (const auto& state : states)
        {
            state->render(os, depth);
        }

        if (!states.empty())
        {
            indent(os, depth) << "[*] --> " << states.front()->id() << "\n";
        }
    }

    template <template <typename...> typename API, typename... R>
    std::vector<Region> Region::make_all(const nil::sm::state_metadata* parent)
    {
        auto regions = std::vector<Region>{};
        regions.reserve(sizeof...(R));
        auto idx = std::size_t{0};

        (
            [&]
            {
                regions.emplace_back(RegionTag<API, R>{}, parent, idx);
                ++idx;
            }(),
            ...
        );

        return regions;
    }
}

namespace nil::sm::uml
{
    template <template <typename...> typename API, typename... Regions>
    std::ostream& operator<<(std::ostream& os, const SM<API, Regions...>& /* sm */)
    {
        const auto regions = detail::Region::make_all<API, Regions...>(nullptr);

        os << "@startuml\n"
              "skin rose\n"
              "skinparam linetype ortho\n"
              "state SM {\n";

        for (auto region_idx = std::size_t{0}; region_idx < regions.size(); ++region_idx)
        {
            regions[region_idx].render(os, 1);
            if (region_idx + 1 < regions.size())
            {
                detail::indent(os, 1) << "||\n";
            }
        }

        os << "}\n"
              "@enduml\n";
        return os;
    }
}
