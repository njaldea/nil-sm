#include "assets.hpp"

#include <nil/service.hpp>
#include <nil/sm/diagnostics.hpp>
#include <nil/sm/format/json.hpp>
#include <nil/sm/viz.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace nil::sm::viz
{
    // The viewer asks for one level of the machine at a time:
    //   /model                     barriers and root props
    //   /model/<scope>             the states at the top of a scope, 0 being the root tree
    //   /model/<scope>/<state_id>  the regions of one state
    //   /model_val/0               what the root tree leaves unmet, and its top states' issues
    //   /model_val/<barrier_id>    what a barrier body leaves to its host
    //   /model_val/<occurrence>    the issues of the states in the regions of the occurrence
    // Every route is asked with a `.json` suffix (`/model/0.json`), as if it were a file.
    namespace
    {
        std::vector<std::string_view> segments(std::string_view route)
        {
            if (const auto query = route.find('?'); query != std::string_view::npos)
            {
                route = route.substr(0, query);
            }

            std::vector<std::string_view> parts;
            while (!route.empty())
            {
                if (route.front() == '/')
                {
                    route.remove_prefix(1);
                    continue;
                }
                const auto end = route.find('/');
                parts.push_back(route.substr(0, end));
                route = end == std::string_view::npos ? std::string_view() : route.substr(end);
            }
            if (!parts.empty() && parts.back().ends_with(".json"))
            {
                parts.back().remove_suffix(5);
            }
            return parts;
        }

        // Ids are `ST_<hex>`, and an occurrence joins them with `::`.
        bool is_identifier(std::string_view text)
        {
            return !text.empty()
                && std::all_of(
                    text.begin(),
                    text.end(),
                    [](char c) {
                        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'
                            || c == ':';
                    }
                );
        }

        bool is_number(std::string_view text)
        {
            return !text.empty()
                && std::all_of(
                    text.begin(),
                    text.end(),
                    [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }
                );
        }

        // 0 is the root tree; any other number is a barrier id as written by the json formatter.
        std::optional<const void*> parse_scope(std::string_view text)
        {
            if (!is_number(text))
            {
                return std::nullopt;
            }
            std::uintptr_t value = 0;
            const auto* last = text.data() + text.size();
            const auto result = std::from_chars(text.data(), last, value);
            if (result.ec != std::errc() || result.ptr != last)
            {
                return std::nullopt;
            }
            // NOLINTNEXTLINE
            return reinterpret_cast<const void*>(value);
        }

        bool write_model(
            std::ostream& os,
            const std::vector<std::string_view>& parts,
            const nil::sm::ir::Model& model,
            std::span<const nil::sm::ir::Prop> root_props,
            const nil::sm::Index& index
        )
        {
            if (parts.size() == 1)
            {
                nil::sm::format::json::write_index(os, model, root_props);
                return true;
            }
            if (parts.size() > 3)
            {
                return false;
            }

            const auto scope = parse_scope(parts[1]);
            const auto* roots = scope ? index.roots(*scope) : nullptr;
            if (roots == nullptr)
            {
                return false;
            }
            if (parts.size() == 2)
            {
                nil::sm::format::json::render_level(
                    os,
                    std::span<const std::vector<nil::sm::ir::Node>>(roots, 1)
                );
                return true;
            }

            const auto* entry = is_identifier(parts[2]) ? index.find(*scope, parts[2]) : nullptr;
            if (entry == nullptr)
            {
                return false;
            }
            nil::sm::format::json::render_level(os, entry->node->regions);
            return true;
        }

        bool write_validation(
            std::ostream& os,
            const std::vector<std::string_view>& parts,
            const nil::sm::Diagnostics& diagnostics
        )
        {
            if (parts.size() != 2)
            {
                return false;
            }
            const auto occurrence = parts[1];

            if (is_number(occurrence) && occurrence != "0")
            {
                const auto scope = parse_scope(occurrence);
                const auto requirements
                    = scope ? diagnostics.barrier_requirements(*scope) : std::nullopt;
                if (!requirements)
                {
                    return false;
                }
                auto o = nil::sm::format::json::object(os);
                nil::sm::format::json::render_issues(o.field("requires"), *requirements);
                return true;
            }

            const auto children
                = is_identifier(occurrence) ? diagnostics.children(occurrence) : std::nullopt;
            if (!children)
            {
                return false;
            }
            auto o = nil::sm::format::json::object(os);
            if (occurrence == "0")
            {
                nil::sm::format::json::render_issues(o.field("root"), diagnostics.root_issues());
            }
            nil::sm::format::json::render_reports(o.field("children"), *children);
            return true;
        }

        // False when the route is not for the model, so that other handlers can take it.
        bool serve(
            nil::service::WebTransaction& transaction,
            const nil::sm::ir::Model& model,
            std::span<const nil::sm::ir::Prop> root_props,
            const nil::sm::Diagnostics& diagnostics
        )
        {
            const auto parts = segments(nil::service::get_route(transaction));
            if (parts.empty() || (parts[0] != "model" && parts[0] != "model_val"))
            {
                return false;
            }

            std::ostringstream os;
            const auto found = parts[0] == "model"
                ? write_model(os, parts, model, root_props, diagnostics.lookup())
                : write_validation(os, parts, diagnostics);

            // the transport has no status codes, so a miss is told in the body
            const auto body = found ? os.str() : std::string(R"({"error":"not found"})");
            nil::service::set_content_type(transaction, "application/json");
            nil::service::send(transaction, body);
            return true;
        }

        // Only the bundle's own files are served, by fixed name.
        bool serve_file(nil::service::WebTransaction& transaction)
        {
            struct Asset
            {
                std::string_view route;
                std::string_view (*body)();
                std::string_view type;
            };

            static const std::array<Asset, 3> files{{
                {"/", assets::index_html, "text/html"},
                {"/index.html", assets::index_html, "text/html"},
                {"/index.js", assets::index_js, "text/javascript"},
            }};

            auto route = nil::service::get_route(transaction);
            if (const auto query = route.find('?'); query != std::string_view::npos)
            {
                route = route.substr(0, query);
            }
            const auto* asset = std::find_if(
                files.begin(),
                files.end(),
                [&](const Asset& a) { return a.route == route; }
            );
            if (asset == files.end())
            {
                return false;
            }

            nil::service::set_content_type(transaction, asset->type);
            nil::service::send(transaction, asset->body());
            return true;
        }
    }

    int run(
        const nil::sm::ir::Model& model,
        std::span<const nil::sm::ir::Prop> root_props,
        std::uint16_t port
    )
    {
        auto server = nil::service::http::server::create(
            {.host = "127.0.0.1", .port = port, .buffer = 100UL * 1024UL * 1024UL}
        );
        const auto diagnostics
            = nil::sm::Diagnostics(model, {root_props.begin(), root_props.end()});
        server->on_get(serve_file);
        server->on_get([&](nil::service::WebTransaction& transaction)
                       { return serve(transaction, model, root_props, diagnostics); });

        server->on_ready([](const nil::service::ID& id)
                         { std::cout << "Server is ready: http://" << to_string(id) << std::endl; }
        );
        server->run();
        return 0;
    }
}
