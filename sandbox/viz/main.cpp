#include "../tollbooth/jobs.hpp"
#include "../tollbooth_barrier/slot.hpp"
#include "../traffic_lights/user.hpp"
#include "showcase.hpp"

#include <nil/service.hpp>
#include <nil/sm.hpp>
#include <nil/sm/barrier.hpp>
#include <nil/sm/diagnostics.hpp>
#include <nil/sm/format/json.hpp>
#include <nil/xit.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace viz_demo
{
    struct st_dont_walk;

    struct pedestrian_button
    {
    };

    struct st_walk
    {
        using events = nil::xalt::tlist<ev_tick>;

        static auto on_event(const ev_tick& /* event */)
        {
            return nil::sm::TransitTo<st_dont_walk>{};
        }
    };

    struct st_dont_walk
    {
        using args = nil::xalt::tlist<pedestrian_button>;
        using events = nil::xalt::tlist<ev_tick>;

        explicit st_dont_walk(pedestrian_button* /* button */)
        {
        }

        static auto on_event(const ev_tick& /* event */)
        {
            return nil::sm::TransitTo<st_walk>{};
        }
    };

    NIL_SM_BARRIER_DECLARE(crosswalk, nil::sm::api::Default<>);
    NIL_SM_BARRIER_DEFINE(crosswalk, st_dont_walk);
    using crosswalk_barrier = nil::sm::barrier::State<nil::sm::Terminate, crosswalk>;

    // Traffic light and pedestrian crosswalk (barrier) run as orthogonal regions.
    struct st_intersection
    {
        using regions = nil::xalt::tlist<st_red, crosswalk_barrier>;

        using events = nil::xalt::tlist<ev_tick>;

        static auto on_event(const ev_tick& /* event */)
        {
            return nil::sm::Discard{};
        }
    };
}

using DefaultApi = nil::sm::api::Default<>;
using tollbooth_api = nil::sm::api::Coalesce<toll::tracing_api>;

// Not static_asserts: barrier definitions are built at runtime, and the demos are unsatisfied on
// purpose.
void report_validity()
{
    const auto report = [](std::string_view name, bool valid)
    { std::cout << name << ": " << (valid ? "valid" : "unmet args") << '\n'; };

    report("showcase", nil::sm::validate<viz_demo::showcase::plant, DefaultApi>());
    report("intersection", nil::sm::validate<viz_demo::st_intersection, DefaultApi>());
    report("tollbooth", nil::sm::validate<toll::booth, tollbooth_api, toll::booth_context>());
    report(
        "tollbooth_barrier",
        nil::sm::validate<toll::bslot::booth, tollbooth_api, toll::booth_context>()
    );
}

int main()
{
    report_validity();

    auto server = nil::service::http::server::create({.host = "127.0.0.1", .port = 1101});
    auto core = nil::xit::make_core(*server, *server->use_ws("/ws"));
    nil::xit::setup_svelte_server(*server);
    nil::xit::set_groups(*core, {{"base", std::filesystem::path(__FILE__).parent_path() / "gui"}});

    auto& frame = nil::xit::add_unique_frame(*core, "index", {"base", "Frame.svelte"});
    nil::xit::unique::add_value(
        frame,
        "model",
        []()
        {
            namespace json = nil::sm::format::json;
            namespace ir = nil::sm::ir;

            std::ostringstream os;
            {
                auto root = json::object(os);
                auto& out = root.field("machines");
                out << '[';
                json::write(out, "showcase", ir::build<viz_demo::showcase::plant>());
                out << ',';
                json::write(out, "intersection", ir::build<viz_demo::st_intersection>());
                out << ',';
                json::write(
                    out,
                    "tollbooth",
                    ir::build<toll::booth, tollbooth_api>(),
                    ir::make_root_props<toll::booth_context>()
                );
                out << ',';
                json::write(
                    out,
                    "tollbooth_barrier",
                    ir::build<toll::bslot::booth, tollbooth_api>(),
                    ir::make_root_props<toll::booth_context>()
                );
                out << ']';
            }
            const auto text = os.str();
            return std::vector<std::uint8_t>(text.begin(), text.end());
        }
    );

    server->on_ready([](const nil::service::ID& id)
                     { std::cout << "Server is ready: http://" << to_string(id) << std::endl; });
    server->run();
    return 0;
}
