#include <nil/sm/uml.hpp>

#include <iostream>

// Each orthogonal region below isolates a single state machine feature so the
// generated diagrams read as a feature-by-feature reference rather than a
// realistic (and noisier) end-to-end workflow.
namespace sandbox::uml_example
{
    struct ev_next
    {
    };

    struct ev_bubble
    {
    };

    struct ev_save
    {
    };

    struct ev_go
    {
    };

    struct ev_intercept
    {
    };

    struct ev_toggle
    {
    };

    struct ev_stop
    {
    };

    // --- Region 1: Transit --- linear chain, second event terminates
    struct transit_state_b;

    struct transit_state_a
    {
        using events = nil::xalt::tlist<ev_next>;

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::Transit<transit_state_b>{};
        }
    };

    struct transit_state_b
    {
        using events = nil::xalt::tlist<ev_next>;

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    // --- Region 2: Forward --- child bubbles the event up, parent handles it
    struct bubble_child
    {
        using events = nil::xalt::tlist<ev_bubble>;

        static auto on_event(const ev_bubble& /* event */)
        {
            return nil::sm::Forward{};
        }
    };

    struct bubble_parent
    {
        using regions = nil::xalt::tlist<bubble_child>;
        using events = nil::xalt::tlist<ev_bubble>;

        static auto on_event(const ev_bubble& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    // --- Region 3: Defer --- event is deferred, then replayed after transit
    struct defer_target;

    struct defer_source
    {
        using events = nil::xalt::tlist<ev_save, ev_go>;

        static auto on_event(const ev_save& /* event */)
        {
            return nil::sm::Defer{};
        }

        static auto on_event(const ev_go& /* event */)
        {
            return nil::sm::Transit<defer_target>{};
        }
    };

    struct defer_target
    {
        using events = nil::xalt::tlist<ev_save>;

        static auto on_event(const ev_save& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    // --- Region 4: Capture --- intercepted before the child region ever reacts
    struct capture_child
    {
        using captures = nil::xalt::tlist<ev_intercept>;

        static auto on_capture(const ev_intercept& /* event */)
        {
            return nil::sm::Discard{};
        }
    };

    struct capture_parent
    {
        using regions = nil::xalt::tlist<capture_child>;
        using captures = nil::xalt::tlist<ev_intercept>;

        static auto on_capture(const ev_intercept& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    // --- Region 5: Loop --- states cycle between each other until stopped
    struct loop_state_b;

    struct loop_state_a
    {
        using events = nil::xalt::tlist<ev_toggle, ev_stop>;

        static auto on_event(const ev_toggle& /* event */)
        {
            return nil::sm::Transit<loop_state_b>{};
        }

        static auto on_event(const ev_stop& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    struct loop_state_b
    {
        using events = nil::xalt::tlist<ev_toggle, ev_stop>;

        static auto on_event(const ev_toggle& /* event */)
        {
            return nil::sm::Transit<loop_state_a>{};
        }

        static auto on_event(const ev_stop& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

    // --- Top level: on_regions_finalized fires once all five regions terminate
    struct showcase
    {
        using regions = nil::xalt::
            tlist<transit_state_a, bubble_parent, defer_source, capture_parent, loop_state_a>;

        static auto on_regions_finalized()
        {
            return nil::sm::Terminate{};
        }
    };
}

int main(int argc, const char** argv)
{
    using SM = nil::sm::DefaultSM<sandbox::uml_example::showcase>;
    if (argc > 1)
    {
        std::string_view t = argv[1];
        if (t == "puml")
        {
            std::cout << nil::sm::puml<SM>();
            return 0;
        }
        if (t == "mermaid")
        {
            std::cout << nil::sm::mermaid<SM>();
            return 0;
        }
        if (t == "dot")
        {
            std::cout << nil::sm::dot<SM>();
            return 0;
        }
        if (t == "scxml")
        {
            std::cout << nil::sm::scxml<SM>();
            return 0;
        }
        if (t == "xstate")
        {
            std::cout << nil::sm::xstate<SM>();
            return 0;
        }
    }

    std::cout << "- puml\n"
                 "- mermaid\n"
                 "- dot\n"
                 "- scxml\n"
                 "- xstate\n"
              << std::flush;
    return 1;
}
