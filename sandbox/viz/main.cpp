#include "../traffic_lights/user.hpp"
#include "showcase.hpp"

#include <nil/service.hpp>
#include <nil/sm.hpp>
#include <nil/sm/barrier.hpp>
#include <nil/xit.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
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

    void to_json(nlohmann::json& j, const UnsatisfiedArgument& u)
    {
        j = {{"dependency", u.dependency}, {"barrier_path", u.barrier_path}};
    }

    void to_json(nlohmann::json& j, const BarrierError& e)
    {
        j
            = {{"dependency", e.dependency},
               {"barrier_path", e.barrier_path},
               {"host_path", e.host_path}};
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
               {"has_unsatisfied_args", n.has_unsatisfied_args},
               {"actions", n.actions},
               {"transitions", n.transitions},
               {"regions", n.regions},
               {"barrier_id", to_uint(n.barrier_id)}};
    }

    void to_json(nlohmann::json& j, const BarrierDefinition& b)
    {
        j
            = {{"id", to_uint(b.id)},
               {"name", b.name},
               {"roots", b.roots},
               {"unsatisfied_args", b.unsatisfied_args}};
    }

    void to_json(nlohmann::json& j, const Model& m)
    {
        j
            = {{"roots", m.roots},
               {"barriers", m.barriers},
               {"unsatisfied_args", m.unsatisfied_args},
               {"barrier_errors", m.barrier_errors},
               {"has_unsatisfied_args", m.has_unsatisfied_args}};
    }
}

namespace viz_demo
{
    struct st_dont_walk;

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
        using events = nil::xalt::tlist<ev_tick>;

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

nil::sm::ir::Model viz_build_tollbooth();
nil::sm::ir::Model viz_build_tollbooth_barrier();

int main()
{
    using DefaultApi = nil::sm::api::Default<>;

    const std::vector<std::pair<std::string, nil::sm::ir::Model>> machines = [&]
    {
        std::vector<std::pair<std::string, nil::sm::ir::Model>> out;
        out.emplace_back("showcase", nil::sm::ir::build<DefaultApi, viz_demo::showcase::plant>());
        out.emplace_back(
            "intersection",
            nil::sm::ir::build<DefaultApi, viz_demo::st_intersection>()
        );
        out.emplace_back("tollbooth", viz_build_tollbooth());
        out.emplace_back("tollbooth_barrier", viz_build_tollbooth_barrier());
        return out;
    }();

    nlohmann::json machines_json = nlohmann::json::array();
    for (const auto& [name, model] : machines)
    {
        machines_json.push_back({{"name", name}, {"model", model}});
    }
    const auto payload = nlohmann::json::to_msgpack(nlohmann::json{{"machines", machines_json}});

    auto server = nil::service::http::server::create({.host = "127.0.0.1", .port = 1101});
    auto core = nil::xit::make_core(*server, *server->use_ws("/ws"));
    nil::xit::setup_svelte_server(*server);
    nil::xit::set_groups(*core, {{"base", std::filesystem::path(__FILE__).parent_path() / "gui"}});

    auto& frame = nil::xit::add_unique_frame(*core, "model", {"base", "Frame.svelte"});
    nil::xit::unique::add_value(frame, "model", [&payload]() { return payload; });

    server->on_ready([](const nil::service::ID& id)
                     { std::cout << "Server is ready: http://" << to_string(id) << std::endl; });
    server->run();
    return 0;
}
