// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "diagnostics.hpp"
#include "../ir.hpp"
#include "diagram.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <ostream>
#include <span>
#include <string_view>
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

    // The functions below are defined in the nil-sm-format library.

    // `[issue, ...]`
    void render_issues(std::ostream& os, std::span<const Issue> issues);

    // `[{"id", "unsatisfied_args", "subtree_unmet"}, ...]`
    void render_reports(std::ostream& os, std::span<const ChildReport> reports);

    // One level of states, each without its own regions: `{"regions": [[node, ...], ...]}`.
    void render_level(std::ostream& os, std::span<const std::vector<ir::Node>> regions);

    // What a viewer needs before it asks for any level: `{"barriers": [{"id", "name"}],
    // "root_props"}`.
    void write_index(
        std::ostream& os,
        const ir::Model& model,
        std::span<const ir::Prop> root_props = {}
    );

    // A JSON array of the nodes, like the other formatters render a region.
    void render(std::ostream& os, std::span<const ir::Node> roots);

    // The whole IR of one machine:
    // {"name", "model": {"roots", "barriers": [{"id", "name", "roots"}]}, "root_props"}.
    // `root_props` are the SM constructor args, which count as props above the roots.
    void write(
        std::ostream& os,
        const ir::Model& model,
        std::span<const ir::Prop> root_props = {}
    );
}

namespace nil::sm
{
    template <typename SM>
    using json = format::diagram<SM, &format::json::render>;
}
