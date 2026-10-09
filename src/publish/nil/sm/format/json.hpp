// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "../ir.hpp"
#include "diagram.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <ostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

// Writes the IR as JSON text without any JSON library. Type ids and barrier ids are written as
// decimal strings (null when absent): they are identities, and 64-bit values do not survive a
// JavaScript number.
namespace nil::sm::format::json
{
    inline void string(std::ostream& os, std::string_view text)
    {
        os << '"';
        for (const char c : text)
        {
            switch (c)
            {
                case '"':
                    os << "\\\"";
                    break;
                case '\\':
                    os << "\\\\";
                    break;
                case '\n':
                    os << "\\n";
                    break;
                case '\r':
                    os << "\\r";
                    break;
                case '\t':
                    os << "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20U)
                    {
                        std::array<char, 7> escaped{};
                        std::snprintf(
                            escaped.data(),
                            escaped.size(),
                            "\\u%04x",
                            static_cast<unsigned>(static_cast<unsigned char>(c))
                        );
                        os << escaped.data();
                    }
                    else
                    {
                        os << c;
                    }
            }
        }
        os << '"';
    }

    inline void boolean(std::ostream& os, bool value)
    {
        os << (value ? "true" : "false");
    }

    inline void pointer(std::ostream& os, const void* value)
    {
        if (value == nullptr)
        {
            os << "null";
            return;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        os << '"' << reinterpret_cast<std::uintptr_t>(value) << '"';
    }

    // `{`, then one field() per member, `}` when it goes out of scope.
    class object final
    {
    public:
        explicit object(std::ostream& init_os)
            : os(init_os)
        {
            os << '{';
        }

        object(const object&) = delete;
        object& operator=(const object&) = delete;
        object(object&&) = delete;
        object& operator=(object&&) = delete;

        ~object()
        {
            os << '}';
        }

        // Writes the member name; the caller writes the value to the returned stream.
        std::ostream& field(std::string_view name)
        {
            if (!first)
            {
                os << ',';
            }
            first = false;
            string(os, name);
            os << ':';
            return os;
        }

    private:
        std::ostream& os;
        bool first = true;
    };

    template <typename Range, typename Write>
    void array(std::ostream& os, const Range& range, Write&& write)
    {
        os << '[';
        auto first = true;
        for (const auto& item : range)
        {
            if (!first)
            {
                os << ',';
            }
            first = false;
            write(os, item);
        }
        os << ']';
    }

    namespace detail
    {
        inline std::string_view name(ir::response::EEntry response)
        {
            return response == ir::response::EEntry::noop ? "noop" : "emit";
        }

        inline std::string_view name(ir::response::EExit response)
        {
            return response == ir::response::EExit::noop ? "noop" : "emit";
        }

        inline std::string_view name(ir::response::ERegionsFinalized response)
        {
            return response == ir::response::ERegionsFinalized::noop ? "noop" : "emit";
        }

        inline std::string_view name(ir::response::EEvent response)
        {
            switch (response)
            {
                case ir::response::EEvent::discard:
                    return "discard";
                case ir::response::EEvent::forward:
                    return "forward";
                case ir::response::EEvent::defer:
                    return "defer";
                case ir::response::EEvent::emit:
                    return "emit";
            }
            return "";
        }

        inline void write(std::ostream& os, const ir::Prop& prop)
        {
            auto o = object(os);
            pointer(o.field("type_id"), prop.type_id);
            string(o.field("type_name"), prop.type_name);
        }

        inline void write(std::ostream& os, const ir::Arg& arg)
        {
            auto o = object(os);
            pointer(o.field("type_id"), arg.type_id);
            string(o.field("type_name"), arg.type_name);
            boolean(o.field("is_direct_parent"), arg.is_direct_parent);
        }

        inline void write(std::ostream& os, const ir::action::Info& info)
        {
            std::visit(
                [&os]<typename T>(const T& action)
                {
                    auto o = object(os);
                    if constexpr (std::is_same_v<T, ir::action::Entry>)
                    {
                        string(o.field("type"), "entry");
                    }
                    else if constexpr (std::is_same_v<T, ir::action::Exit>)
                    {
                        string(o.field("type"), "exit");
                    }
                    else if constexpr (std::is_same_v<T, ir::action::RegionsFinalized>)
                    {
                        string(o.field("type"), "regions_finalized");
                    }
                    else
                    {
                        string(
                            o.field("type"),
                            std::is_same_v<T, ir::action::Event> ? "event" : "capture"
                        );
                        string(o.field("event"), action.event_name);
                    }
                    string(o.field("response"), name(action.response));
                },
                info
            );
        }

        inline void write(std::ostream& os, const ir::transit::Info& info)
        {
            auto o = object(os);
            string(o.field("type"), ir::is_capture(info) ? "capture" : "event");
            string(o.field("target"), ir::target_id(info));
            string(o.field("event"), ir::event_name(info));
        }

        inline void write(std::ostream& os, const ir::Node& node);

        inline void write(std::ostream& os, const std::vector<ir::Node>& nodes)
        {
            array(os, nodes, [](std::ostream& out, const ir::Node& node) { write(out, node); });
        }

        inline void write(std::ostream& os, const ir::Node& node)
        {
            auto o = object(os);
            string(o.field("id"), node.id);
            string(o.field("display_name"), node.display_name);
            boolean(o.field("is_initial"), node.is_initial);
            boolean(o.field("is_final"), node.is_final);
            boolean(o.field("is_barrier"), node.is_barrier);
            pointer(o.field("type_id"), node.type_id);
            array(
                o.field("required_args"),
                node.required_args,
                [](std::ostream& out, const ir::Arg& arg) { write(out, arg); }
            );
            array(
                o.field("provided_props"),
                node.provided_props,
                [](std::ostream& out, const ir::Prop& prop) { write(out, prop); }
            );
            array(
                o.field("actions"),
                node.actions,
                [](std::ostream& out, const ir::action::Info& info) { write(out, info); }
            );
            array(
                o.field("transitions"),
                node.transitions,
                [](std::ostream& out, const ir::transit::Info& info) { write(out, info); }
            );
            array(
                o.field("regions"),
                node.regions,
                [](std::ostream& out, const std::vector<ir::Node>& region) { write(out, region); }
            );
            pointer(o.field("barrier_id"), node.barrier_id);
        }
    }

    // A JSON array of the nodes, like the other formatters render a region.
    inline void render(std::ostream& os, std::span<const ir::Node> roots)
    {
        array(os, roots, [](std::ostream& out, const ir::Node& node) { detail::write(out, node); });
    }

    // The whole IR of one machine:
    // {"name", "model": {"roots", "barriers": [{"id", "name", "roots"}]}, "root_props"}.
    // `root_props` are the SM constructor args, which count as props above the roots.
    inline void write(
        std::ostream& os,
        std::string_view name,
        const ir::Model& model,
        std::span<const ir::Prop> root_props = {}
    )
    {
        auto machine = object(os);
        string(machine.field("name"), name);

        auto m = object(machine.field("model"));
        detail::write(m.field("roots"), model.roots);
        array(
            m.field("barriers"),
            model.barriers,
            [](std::ostream& out, const ir::BarrierDefinition& barrier)
            {
                auto b = object(out);
                pointer(b.field("id"), barrier.id);
                string(b.field("name"), barrier.name);
                detail::write(b.field("roots"), barrier.roots);
            }
        );

        array(
            machine.field("root_props"),
            root_props,
            [](std::ostream& out, const ir::Prop& prop) { detail::write(out, prop); }
        );
    }
}

namespace nil::sm
{
    template <typename SM>
    using json = format::diagram<SM, &format::json::render>;
}
