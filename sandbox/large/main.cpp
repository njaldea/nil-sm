// A synthetic machine, built straight as IR, to see how the viewer copes with a size that
// serialized whole (the old `/model` payload) reaches hundreds of MB.
//
//   viz_sandbox_large [--depth=8] [--regions=3] [--states=9] [--composites=2] [--measure]
//
// Every composite state has `regions` regions of `states` states each; the first `composites` of
// a region are composite again, down to `depth` levels. The defaults give about 1.5M states.
// `--measure` also streams the whole IR through the json writer, without keeping it, and prints
// how big that payload would have been.
//
// Some leaves need a `config` that only part of the tree provides, and some leaves host barriers
// (one of which hosts the other), so the unmet-args side has something to report too.

#include "../viz/viewer.hpp"

#include <nil/sm/format/json.hpp>
#include <nil/sm/ir.hpp>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <ostream>
#include <span>
#include <streambuf>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace
{
    namespace ir = nil::sm::ir;

    struct Shape
    {
        std::size_t depth = 8;
        std::size_t regions = 3;
        std::size_t states = 9;
        std::size_t composites = 2;
    };

    // The addresses stand in for type ids and barrier ids; elements of one array are distinct.
    char markers[3] = {};
    const void* const config_marker = &markers[0];
    const void* const inventory_marker = &markers[1];
    const void* const audit_marker = &markers[2];

    constexpr std::array<std::string_view, 6> events{
        "ev_start",
        "ev_stop",
        "ev_tick",
        "ev_fail",
        "ev_retry",
        "ev_flush",
    };

    constexpr std::array<ir::response::EEvent, 4> responses{
        ir::response::EEvent::discard,
        ir::response::EEvent::forward,
        ir::response::EEvent::defer,
        ir::response::EEvent::emit,
    };

    // Names repeat all over the tree, so they are kept once and viewed.
    class Names final
    {
    public:
        std::string_view intern(std::string text)
        {
            return *pool.insert(std::move(text)).first;
        }

    private:
        std::unordered_set<std::string> pool;
    };

    struct Hosted
    {
        const void* id;
        std::string_view name;
    };

    // What one scope, the root tree or a barrier body, is built from.
    struct Scope
    {
        std::size_t depth = 0;
        std::size_t regions = 0;
        // the level whose first composite provides `config`; 0 provides it nowhere
        std::size_t provides_at = 0;
        // the level from which the last leaf of a region needs `config`
        std::size_t requires_from = 0;
        // the barriers the second to last state of a region hosts, by level
        std::vector<Hosted> hosts;
        std::uint64_t next_id = 0;
    };

    class Generator final
    {
    public:
        Generator(const Shape& init_shape, Names& init_names)
            : shape(init_shape)
            , names(init_names)
        {
        }

        ir::Model build()
        {
            auto inventory = scope(3, 2, 0, 2, {});
            auto audit = scope(4, 2, 0, 3, {Hosted{inventory_marker, "inventory"}});
            auto root = scope(
                shape.depth,
                shape.regions,
                2,
                3,
                {Hosted{inventory_marker, "inventory"}, Hosted{audit_marker, "audit"}}
            );

            ir::Model model;
            model.roots.push_back(top(root, "machine"));
            model.barriers.push_back(ir::BarrierDefinition{
                inventory_marker,
                "inventory",
                {top(inventory, "inventory_root")}
            });
            model.barriers.push_back(
                ir::BarrierDefinition{audit_marker, "audit", {top(audit, "audit_root")}}
            );
            return model;
        }

    private:
        static Scope scope(
            std::size_t depth,
            std::size_t regions,
            std::size_t provides_at,
            std::size_t requires_from,
            std::vector<Hosted> hosts
        )
        {
            Scope result;
            result.depth = depth;
            result.regions = regions;
            result.provides_at = provides_at;
            result.requires_from = requires_from;
            result.hosts = std::move(hosts);
            return result;
        }

        static std::string next_id(Scope& in)
        {
            std::array<char, 24> text{};
            std::snprintf(
                text.data(),
                text.size(),
                "ST_%016llx",
                static_cast<unsigned long long>(in.next_id++)
            );
            return text.data();
        }

        ir::Node top(Scope& in, std::string_view name)
        {
            return state(in, 1, 0, {next_id(in)}, name);
        }

        std::vector<ir::Node> region(Scope& in, std::size_t level)
        {
            std::vector<std::string> ids;
            ids.reserve(shape.states);
            for (std::size_t i = 0; i < shape.states; ++i)
            {
                ids.push_back(next_id(in));
            }

            std::vector<ir::Node> nodes;
            nodes.reserve(shape.states);
            for (std::size_t i = 0; i < shape.states; ++i)
            {
                nodes.push_back(state(in, level, i, ids, {}));
            }
            return nodes;
        }

        // `siblings` are the ids of the region the state belongs to; its own is at `index`.
        ir::Node state(
            Scope& in,
            std::size_t level,
            std::size_t index,
            const std::vector<std::string>& siblings,
            std::string_view top_name
        )
        {
            const auto composite = level < in.depth && index < shape.composites;
            const auto hosting
                = !composite && !in.hosts.empty() && level >= 3 && index + 2 == shape.states;
            const auto needs_config
                = !composite && !hosting && level >= in.requires_from && index + 1 == shape.states;
            const auto provides = composite && level == in.provides_at && index == 0;

            std::string_view name = top_name;
            if (hosting)
            {
                name = in.hosts[level % in.hosts.size()].name;
            }
            else if (name.empty())
            {
                name = names.intern(
                    std::string(composite ? "comp_" : "leaf_") + std::to_string(level) + "_"
                    + std::to_string(index)
                );
            }

            ir::Node node;
            node.id = siblings[index];
            node.display_name = name;
            node.is_initial = index == 0;
            node.is_barrier = hosting;
            node.type_id = name.data();
            if (needs_config)
            {
                node.required_args.push_back(ir::Arg{config_marker, "config", false});
            }
            if (provides)
            {
                node.provided_props.push_back(ir::Prop{config_marker, "config"});
            }
            if (hosting)
            {
                node.barrier_id = in.hosts[level % in.hosts.size()].id;
            }

            if (composite)
            {
                node.actions.emplace_back(ir::action::Entry{ir::response::EEntry::noop});
                node.actions.emplace_back(ir::action::Exit{ir::response::EExit::emit});
                node.actions.emplace_back(
                    ir::action::RegionsFinalized{ir::response::ERegionsFinalized::noop}
                );
                node.actions.emplace_back(
                    ir::action::Capture{"ev_audit", ir::response::EEvent::discard}
                );
            }
            for (std::size_t k = 0; k < 3; ++k)
            {
                const auto pick = level + index + k;
                node.actions.emplace_back(ir::action::Event{
                    std::string(events[pick % events.size()]),
                    responses[pick % responses.size()],
                });
            }

            if (siblings.size() > 1)
            {
                node.transitions.emplace_back(
                    ir::transit::Event{siblings[(index + 1) % siblings.size()], "ev_next"}
                );
                if (index != 0)
                {
                    node.transitions.emplace_back(ir::transit::Event{siblings[0], "ev_reset"});
                }
            }
            if (index + 1 == siblings.size())
            {
                node.transitions.emplace_back(
                    ir::transit::Event{std::string(nil::sm::reserved::termination_node), "ev_halt"}
                );
            }

            if (composite)
            {
                for (std::size_t r = 0; r < in.regions; ++r)
                {
                    node.regions.push_back(region(in, level + 1));
                }
            }
            return node;
        }

        Shape shape;
        Names& names;
    };

    std::size_t count(const std::vector<ir::Node>& nodes)
    {
        std::size_t total = 0;
        for (const auto& node : nodes)
        {
            ++total;
            for (const auto& region : node.regions)
            {
                total += count(region);
            }
        }
        return total;
    }

    // Takes the bytes of whatever is streamed into it, and only counts them.
    class counting_buffer final: public std::streambuf
    {
    public:
        std::uint64_t size = 0;

    protected:
        int_type overflow(int_type c) override
        {
            ++size;
            return traits_type::not_eof(c);
        }

        std::streamsize xsputn(const char* /* data */, std::streamsize length) override
        {
            size += static_cast<std::uint64_t>(length);
            return length;
        }
    };

    std::size_t option(
        std::span<const char* const> args,
        std::string_view name,
        std::size_t fallback
    )
    {
        for (const std::string_view arg : args)
        {
            if (arg.size() > name.size() + 1 && arg.starts_with(name) && arg[name.size()] == '=')
            {
                std::size_t value = fallback;
                const auto text = arg.substr(name.size() + 1);
                std::from_chars(text.data(), text.data() + text.size(), value);
                return value;
            }
        }
        return fallback;
    }

    bool flag(std::span<const char* const> args, std::string_view name)
    {
        for (const std::string_view arg : args)
        {
            if (arg == name)
            {
                return true;
            }
        }
        return false;
    }
}

int main(int argc, const char** argv)
{
    const auto args = std::span<const char* const>(argv, static_cast<std::size_t>(argc));

    Shape shape;
    shape.depth = option(args, "--depth", shape.depth);
    shape.regions = option(args, "--regions", shape.regions);
    shape.states = option(args, "--states", shape.states);
    shape.composites = option(args, "--composites", shape.composites);
    if (shape.depth < 2 || shape.regions < 1 || shape.composites < 1
        || shape.states < shape.composites + 2)
    {
        std::cerr << "needs depth >= 2, regions >= 1, composites >= 1 and at least two more "
                     "states than composites\n";
        return 1;
    }

    Names names;
    const auto model = Generator(shape, names).build();

    auto total = count(model.roots);
    for (const auto& barrier : model.barriers)
    {
        total += count(barrier.roots);
    }
    std::cout << "states: " << total << " (depth " << shape.depth << ", " << shape.regions
              << " regions, " << shape.states << " states per region, " << shape.composites
              << " composite)\n";

    if (flag(args, "--measure"))
    {
        counting_buffer buffer;
        std::ostream out(&buffer);
        nil::sm::format::json::write(out, model);
        std::cout << "whole model as json: " << buffer.size / (1024U * 1024U) << " MB\n";
    }

    return sandbox::viz::run(model, {}, 1105);
}
