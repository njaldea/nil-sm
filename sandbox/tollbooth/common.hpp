#pragma once

#include "events.hpp"

#include <nil/sm.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <iostream>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

namespace toll
{
    struct booth_context final
    {
        int base_fare = 5;
        int vehicle_class = 1;
        int pending_amount = 0;

        int vehicles = 0;
        int revenue = 0;
        int rejected = 0;

        bool finished = false;

        struct trace_context* trace = nullptr;

        int fare_for(int klass) const
        {
            return base_fare * std::max(1, klass);
        }

        // NOLINTNEXTLINE
        void log(std::string_view who, std::string_view what)
        {
            std::cout << "    " << who << " | " << what << '\n';
        }

        // NOLINTNEXTLINE
        void log(std::string_view who, std::string_view what, int value)
        {
            std::cout << "    " << who << " | " << what << ' ' << value << '\n';
        }

        void print(std::ostream& out) const
        {
            out << "  booth : vehicles=" << vehicles //
                << " revenue=" << revenue            //
                << " rejected=" << rejected          //
                << " base_fare=" << base_fare        //
                << " class=" << vehicle_class        //
                << " pending=" << pending_amount << '\n';
        }
    };

    // Counters live in the API context so the tracing layer stays out of the states themselves.
    struct trace_context final
    {
        std::size_t constructed = 0;
        std::size_t entered = 0;
        std::size_t exited = 0;
        std::size_t events = 0;
        std::size_t max_depth = 0;

        static void print_path(std::ostream& out, const nil::sm::Metadata* state_metadata)
        {
            if (state_metadata->parent != nullptr)
            {
                print_path(out, state_metadata->parent);
                out << " --> ";
            }

            out << state_metadata->name;
            if (state_metadata->parent != nullptr && state_metadata->parent->subregions > 1)
            {
                out << '[' << state_metadata->region << ']';
            }
        }

        void print(std::ostream& out) const
        {
            out << "  trace : constructed=" << constructed //
                << " entered=" << entered                  //
                << " exited=" << exited                    //
                << " events=" << events                    //
                << " max_depth=" << max_depth << '\n';
        }
    };
}

namespace toll::policy
{
    struct no_sink final
    {
        template <typename E>
        static void accept(const E& /* event */, booth_context* /* ctx */)
        {
        }
    };

    struct no_action final
    {
        static void apply(booth_context* /* ctx */)
        {
        }
    };

    struct record_vehicle final
    {
        static void accept(const ev::arrive& event, booth_context* ctx)
        {
            ctx->vehicle_class = event.vehicle_class;
            ctx->pending_amount = 0;
            ctx->log("lane", "vehicle of class", event.vehicle_class);
        }
    };

    struct record_payment final
    {
        static void accept(const ev::pay& event, booth_context* ctx)
        {
            ctx->pending_amount = event.amount;
        }
    };

    struct fare_check final
    {
        static bool check(const ev::pay& event, booth_context* ctx)
        {
            const auto due = ctx->fare_for(ctx->vehicle_class);
            if (event.amount >= due)
            {
                return true;
            }
            ctx->rejected++;
            ctx->log("lane", "underpaid, due", due);
            return false;
        }
    };

    struct commit_fare final
    {
        static void apply(booth_context* ctx)
        {
            ctx->revenue += ctx->pending_amount;
            ctx->vehicles++;
        }
    };

    struct report final
    {
        static void apply(booth_context* ctx)
        {
            ctx->print(std::cout);
        }
    };
}

namespace toll::generic
{
    // Every generic state is parameterized on the *result* type of its transitions
    // (TransitTo<X>, Terminate, Discard, ...) so `return Next();` composes bottom-up.

    template <typename Tag, typename Ok, typename Fail>
    struct step final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<ev::ok, ev::fail>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit step(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "enter");
            return {};
        }

        auto on_event(const ev::ok& /* event */) const
        {
            ctx->log(name, "ok");
            return Ok();
        }

        auto on_event(const ev::fail& /* event */) const
        {
            ctx->log(name, "fail");
            return Fail();
        }
    };

    template <typename Tag, typename E, typename Then, typename Sink = policy::no_sink>
    struct waiting final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<E>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit waiting(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "enter");
            return {};
        }

        auto on_event(const E& event) const
        {
            Sink::accept(event, ctx);
            return Then();
        }
    };

    template <
        typename Tag,
        typename E1,
        typename Then1,
        typename E2,
        typename Then2,
        typename Action = policy::no_action>
    struct choosing final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<E1, E2>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit choosing(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "enter");
            Action::apply(ctx);
            return {};
        }

        auto on_event(const E1& /* event */) const
        {
            ctx->log(name, "primary");
            return Then1();
        }

        auto on_event(const E2& /* event */) const
        {
            ctx->log(name, "secondary");
            return Then2();
        }
    };

    template <typename Tag, typename E, typename Then, int Needed>
    struct counting final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<E>;

        booth_context* ctx = nullptr;
        int seen = 0;

        template <typename Parent>
        explicit counting(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "needs", Needed);
            return {};
        }

        auto on_event(const E& /* event */) -> std::variant<Then, nil::sm::Discard>
        {
            if (++seen >= Needed)
            {
                return Then();
            }
            ctx->log(name, "remaining", Needed - seen);
            return nil::sm::Discard();
        }
    };

    template <
        typename Tag,
        typename Deferred,
        typename E,
        typename Then,
        typename Sink = policy::no_sink>
    struct parking final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<Deferred, E>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit parking(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "enter");
            return {};
        }

        auto on_event(const Deferred& /* event */) const
        {
            ctx->log(name, "deferred");
            return nil::sm::Defer();
        }

        auto on_event(const E& event) const
        {
            Sink::accept(event, ctx);
            return Then();
        }
    };

    template <
        typename Tag,
        typename Deferred,
        typename E,
        typename Check,
        typename Yes,
        typename No,
        typename Sink = policy::no_sink>
    struct screening final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<Deferred, E>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit screening(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "enter");
            return {};
        }

        auto on_event(const Deferred& /* event */) const
        {
            ctx->log(name, "deferred");
            return nil::sm::Defer();
        }

        auto on_event(const E& event) const -> std::variant<Yes, No>
        {
            Sink::accept(event, ctx);
            if (Check::check(event, ctx))
            {
                return Yes();
            }
            return No();
        }
    };

    template <typename Tag, typename Emitted, typename E, typename Then>
    struct announcing final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<E>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit announcing(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const
        {
            ctx->log(name, "enter, emitting");
            return nil::sm::Emit<Emitted>();
        }

        static auto on_event(const E& /* event */)
        {
            return Then();
        }
    };

    template <typename Tag, typename E, typename Then, typename Escape>
    struct escaping final
    {
        static constexpr std::string_view name = Tag::name;

        using events = nil::xalt::tlist<E, Escape>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit escaping(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            ctx->log(name, "enter");
            return {};
        }

        static auto on_event(const E& /* event */)
        {
            return Then();
        }

        auto on_event(const Escape& /* event */) const
        {
            ctx->log(name, "forwarding to parent");
            return nil::sm::Forward();
        }
    };

    // Composite used both as a top-level job and as a nested sub-machine.
    template <typename Tag, typename Next, typename... Regions>
    struct workflow final
    {
        static constexpr std::string_view name = Tag::name;

        using regions = nil::xalt::tlist<Regions...>;
        using events = nil::xalt::tlist<ev::abort, ev::cancel>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit workflow(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_enter() const -> nil::sm::NOOP
        {
            std::cout << ">> " << name << " started\n";
            return {};
        }

        auto on_exit() const -> nil::sm::NOOP
        {
            std::cout << "<< " << name << " left\n";
            return {};
        }

        auto on_event(const ev::abort& /* event */) const
        {
            ctx->log(name, "aborted");
            return Next();
        }

        auto on_event(const ev::cancel& /* event */) const
        {
            ctx->log(name, "absorbed cancel");
            return nil::sm::Discard();
        }

        auto on_regions_finalized() const
        {
            ctx->log(name, "all regions finalized");
            return Next();
        }
    };

    // Top-level shell: owns the shutdown capture and restarts its region after every job.
    template <typename Inner>
    struct session final
    {
        static constexpr std::string_view name = "session";

        using regions = nil::xalt::tlist<Inner>;
        using captures = nil::xalt::tlist<ev::shutdown>;

        booth_context* ctx = nullptr;

        template <typename Parent>
        explicit session(Parent* /* parent */, booth_context* context)
            : ctx(context)
        {
        }

        auto on_capture(const ev::shutdown& /* event */) const
        {
            ctx->log(name, "shutdown captured");
            ctx->finished = true;
            return nil::sm::Terminate();
        }

        auto on_regions_finalized() const
        {
            ctx->log(name, "job done, back to idle");
            return nil::sm::TransitTo<session<Inner>>();
        }
    };
}

namespace toll
{
    template <typename T>
    struct tracing_api
    {
        using state_context_t = booth_context;
        using api_context_t = trace_context;

        template <typename Parent>
        static T make(
            Parent* parent,
            booth_context* state_contexts,
            trace_context* api_contexts,
            const nil::sm::Metadata& metadata
        )
        {
            if (api_contexts != nullptr)
            {
                api_contexts->constructed++;
                api_contexts->max_depth = std::max(api_contexts->max_depth, metadata.depth);
                if (metadata.subregions == 0)
                {
                    trace_context::print_path(std::cout, &metadata);
                    std::cout << '\n';
                }
            }
            return nil::sm::api::Default<booth_context, trace_context>::type<T>::make(
                parent,
                state_contexts,
                api_contexts,
                metadata
            );
        }

        template <typename E>
        static auto on_event(T& state, const E& event, trace_context* api_contexts)
        {
            if (api_contexts != nullptr)
            {
                api_contexts->events++;
            }
            return nil::sm::api::Default<booth_context, trace_context>::type<T>::on_event(
                state,
                event,
                api_contexts
            );
        }

        static auto on_enter(T& state, trace_context* api_contexts)
        {
            if (api_contexts != nullptr)
            {
                api_contexts->entered++;
            }
            return nil::sm::api::Default<booth_context, trace_context>::type<T>::on_enter(
                state,
                api_contexts
            );
        }

        static auto on_exit(T& state, trace_context* api_contexts)
        {
            if (api_contexts != nullptr)
            {
                api_contexts->exited++;
            }
            return nil::sm::api::Default<booth_context, trace_context>::type<T>::on_exit(
                state,
                api_contexts
            );
        }
    };
}

namespace toll::repl
{
    inline void print_help()
    {
        std::cout                                                                   //
            << "commands:\n"                                                        //
            << "  job <startup|collection|shift|maintenance>\n"                     //
            << "                     start a job (only while idle)\n"               //
            << "  ok | fail          generic step outcome (broadcast to regions)\n" //
            << "  tick               generic clock/progress pulse\n"                //
            << "  arrive [class]     vehicle enters the lane (default 1)\n"         //
            << "  pay <amount>       pay the toll\n"                                //
            << "  receipt            request a receipt (gets deferred)\n"           //
            << "  cancel             forwarded up to the owning job\n"              //
            << "  depart             vehicle leaves\n"                              //
            << "  open | close       gate arm requests\n"                           //
            << "  alarm | reset      health region triggers\n"                      //
            << "  abort              abandon the current job, back to idle\n"       //
            << "  shutdown           captured at the top, ends the machine\n"       //
            << "  status | help | quit\n";
    }

    inline bool parse_int(std::string_view text, int& out)
    {
        if (text.empty())
        {
            return false;
        }
        const auto* first = text.data();
        const auto* last = text.data() + text.size();
        const auto result = std::from_chars(first, last, out);
        return result.ec == std::errc() && result.ptr == last;
    }

    // Poster is anything with a `post<E>()` / `post(E)` pair: a concrete SM or nil::sm::ISM.
    template <typename Poster>
    // NOLINTNEXTLINE
    bool feed(Poster& machine, std::string_view command, std::string_view argument)
    {
        if (command == "ok")
        {
            machine.template post<ev::ok>();
        }
        else if (command == "fail")
        {
            machine.template post<ev::fail>();
        }
        else if (command == "tick")
        {
            machine.template post<ev::tick>();
        }
        else if (command == "arrive")
        {
            auto vehicle_class = 1;
            if (!argument.empty() && !parse_int(argument, vehicle_class))
            {
                std::cout << "arrive: expected an integer class\n";
                return true;
            }
            machine.post(ev::arrive{.vehicle_class = vehicle_class});
        }
        else if (command == "pay")
        {
            auto amount = 0;
            if (!parse_int(argument, amount))
            {
                std::cout << "pay: expected an integer amount\n";
                return true;
            }
            machine.post(ev::pay{.amount = amount});
        }
        else if (command == "receipt")
        {
            machine.template post<ev::receipt>();
        }
        else if (command == "cancel")
        {
            machine.template post<ev::cancel>();
        }
        else if (command == "depart")
        {
            machine.template post<ev::depart>();
        }
        else if (command == "open")
        {
            machine.template post<ev::gate_open>();
        }
        else if (command == "close")
        {
            machine.template post<ev::gate_close>();
        }
        else if (command == "alarm")
        {
            machine.template post<ev::alarm>();
        }
        else if (command == "reset")
        {
            machine.template post<ev::reset>();
        }
        else if (command == "abort")
        {
            machine.template post<ev::abort>();
        }
        else
        {
            return false;
        }

        std::cout << std::flush;
        return true;
    }

    // Handles tokenizing and the version-independent commands; `handle` gets everything else.
    template <typename Handle>
    int loop(
        booth_context& state_context,
        trace_context& api_context,
        std::string_view title,
        Handle handle
    )
    {
        std::cout << "=== " << title << " ===\n";
        print_help();

        std::string line;
        while (!state_context.finished && std::getline(std::cin, line))
        {
            const auto begin = line.find_first_not_of(" \t");
            if (begin == std::string::npos)
            {
                continue;
            }
            const auto split = line.find_first_of(" \t", begin);
            const auto command = std::string_view(line).substr(
                begin,
                split == std::string::npos ? std::string::npos : split - begin
            );
            auto argument = std::string_view();
            if (split != std::string::npos)
            {
                const auto arg_begin = line.find_first_not_of(" \t", split);
                if (arg_begin != std::string::npos)
                {
                    argument = std::string_view(line).substr(arg_begin);
                }
            }

            if (command == "quit" || command == "q")
            {
                break;
            }
            if (command == "help")
            {
                print_help();
                continue;
            }
            if (command == "status")
            {
                state_context.print(std::cout);
                api_context.print(std::cout);
                continue;
            }

            if (!handle(command, argument))
            {
                std::cout << "unknown command: " << command << '\n';
            }
        }

        std::cout << "--- final ---\n";
        state_context.print(std::cout);
        api_context.print(std::cout);
        return 0;
    }

    template <typename Top>
    int run(std::string_view title)
    {
        booth_context state_context;
        trace_context api_context;

        nil::sm::SM<nil::sm::api::Coalesce<tracing_api>::type, Top> machine{
            &state_context,
            &api_context
        };

        return loop(
            state_context,
            api_context,
            title,
            [&](std::string_view command, std::string_view argument)
            {
                if (command == "shutdown")
                {
                    machine.template post<ev::shutdown>();
                    std::cout << std::flush;
                    return true;
                }
                if (command == "job")
                {
                    if (argument == "startup")
                    {
                        machine.template post<ev::select_startup>();
                    }
                    else if (argument == "collection")
                    {
                        machine.template post<ev::select_collection>();
                    }
                    else if (argument == "shift")
                    {
                        machine.template post<ev::select_shift>();
                    }
                    else if (argument == "maintenance")
                    {
                        machine.template post<ev::select_maintenance>();
                    }
                    else
                    {
                        std::cout << "job: expected startup|collection|shift|maintenance\n";
                    }
                    std::cout << std::flush;
                    return true;
                }
                return feed(machine, command, argument);
            }
        );
    }
}
