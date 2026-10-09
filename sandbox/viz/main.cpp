#include "../tollbooth/jobs.hpp"
#include "../tollbooth_barrier/slot.hpp"
#include "../traffic_lights/user.hpp"
#include "showcase.hpp"

#include <nil/service.hpp>
#include <nil/sm.hpp>
#include <nil/sm/barrier.hpp>
#include <nil/sm/diagnostics.hpp>
#include <nil/xit.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nil::sm::ir
{
    namespace response
    {
        NLOHMANN_JSON_SERIALIZE_ENUM(EEntry, {{EEntry::noop, "noop"}, {EEntry::emit, "emit"}})
        NLOHMANN_JSON_SERIALIZE_ENUM(EExit, {{EExit::noop, "noop"}, {EExit::emit, "emit"}})
        NLOHMANN_JSON_SERIALIZE_ENUM(
            ERegionsFinalized,
            {{ERegionsFinalized::noop, "noop"}, {ERegionsFinalized::emit, "emit"}}
        )
        NLOHMANN_JSON_SERIALIZE_ENUM(
            EEvent,
            {{EEvent::discard, "discard"},
             {EEvent::forward, "forward"},
             {EEvent::defer, "defer"},
             {EEvent::emit, "emit"}}
        )
    }

    namespace action
    {
        void to_json(nlohmann::json& j, const Info& info)
        {
            std::visit(
                [&j]<typename T>(const T& a)
                {
                    if constexpr (std::is_same_v<T, Entry>)
                    {
                        j = {{"type", "entry"}, {"response", a.response}};
                    }
                    else if constexpr (std::is_same_v<T, Exit>)
                    {
                        j = {{"type", "exit"}, {"response", a.response}};
                    }
                    else if constexpr (std::is_same_v<T, RegionsFinalized>)
                    {
                        j = {{"type", "regions_finalized"}, {"response", a.response}};
                    }
                    else if constexpr (std::is_same_v<T, Event>)
                    {
                        j = {{"type", "event"}, {"event", a.event_name}, {"response", a.response}};
                    }
                    else
                    {
                        j
                            = {{"type", "capture"},
                               {"event", a.event_name},
                               {"response", a.response}};
                    }
                },
                info
            );
        }
    }

    namespace transit
    {
        void to_json(nlohmann::json& j, const Info& info)
        {
            j
                = {{"type", is_capture(info) ? "capture" : "event"},
                   {"target", target_id(info)},
                   {"event", event_name(info)}};
        }
    }

    namespace
    {
        std::uintptr_t to_uint(const void* ptr)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return reinterpret_cast<std::uintptr_t>(ptr);
        }
    }

    void to_json(nlohmann::json& j, const Dependency& d)
    {
        j
            = {{"type_id", to_uint(d.type_id)},
               {"type_name", d.type_name},
               {"is_direct_parent", d.is_direct_parent}};
    }

    void to_json(nlohmann::json& j, const Node& n)
    {
        j
            = {{"id", n.id},
               {"display_name", n.display_name},
               {"is_initial", n.is_initial},
               {"is_final", n.is_final},
               {"is_barrier", n.is_barrier},
               {"type_id", to_uint(n.type_id)},
               {"required_args", n.required_args},
               {"provided_props", n.provided_props},
               {"actions", n.actions},
               {"transitions", n.transitions},
               {"regions", n.regions},
               {"barrier_id", to_uint(n.barrier_id)}};
    }

    void to_json(nlohmann::json& j, const BarrierDefinition& b)
    {
        j = {{"id", to_uint(b.id)}, {"name", b.name}, {"roots", b.roots}};
    }

    void to_json(nlohmann::json& j, const Model& m)
    {
        j = {{"roots", m.roots}, {"barriers", m.barriers}};
    }
}

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

namespace viz
{
    struct machine
    {
        std::string name;
        nil::sm::ir::Model model;
        // Arguments given to the SM constructor; the diagram needs them to judge the root states.
        std::vector<nil::sm::ir::Dependency> root_props;
    };

    void to_json(nlohmann::json& j, const viz::machine& m)
    {
        j = {{"name", m.name}, {"model", m.model}, {"root_props", m.root_props}};
    }
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
            // clang-format off
            static const auto machines =  nlohmann::json::to_msgpack(nlohmann::json{{
                "machines",
                std::vector<viz::machine>{
                    {
                        "showcase",
                        nil::sm::ir::build<viz_demo::showcase::plant>(),
                        {}
                    },
                    {
                        "intersection",
                        nil::sm::ir::build<viz_demo::st_intersection>(),
                        {}
                    },
                    {
                        "tollbooth",
                        nil::sm::ir::build<toll::booth, tollbooth_api>(),
                        nil::sm::ir::make_root_props<toll::booth_context>()
                    },
                    {
                        "tollbooth_barrier",
                        nil::sm::ir::build<toll::bslot::booth, tollbooth_api>(),
                        nil::sm::ir::make_root_props<toll::booth_context>()
                    }
                }
            }});
            // clang-format on
            return machines;
        }
    );

    server->on_ready([](const nil::service::ID& id)
                     { std::cout << "Server is ready: http://" << to_string(id) << std::endl; });
    server->run();
    return 0;
}
