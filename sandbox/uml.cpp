#include <nil/sm/uml.hpp>

#include <iostream>
#include <string_view>

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

    struct transit_state_b;

    struct transit_state_a
    {
        using events = nil::xalt::tlist<ev_next>;

        static auto on_event(const ev_next& /* event */)
        {
            return nil::sm::TransitTo<transit_state_b>();
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
            return nil::sm::TransitTo<defer_target>();
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

    struct loop_state_b;

    struct loop_state_a
    {
        using events = nil::xalt::tlist<ev_toggle, ev_stop>;

        static auto on_event(const ev_toggle& /* event */)
        {
            return nil::sm::TransitTo<loop_state_b>();
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
            return nil::sm::TransitTo<loop_state_a>();
        }

        static auto on_event(const ev_stop& /* event */)
        {
            return nil::sm::Terminate{};
        }
    };

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
    using machine = nil::sm::DefaultSM<sandbox::uml_example::showcase>;
    if (argc > 1)
    {
        const std::string_view format = argv[1];
        if (format == "puml")
        {
            std::cout << nil::sm::puml<machine>().root;
            return 0;
        }
        if (format == "mermaid")
        {
            std::cout << nil::sm::mermaid<machine>().root;
            return 0;
        }
        if (format == "dot")
        {
            std::cout << nil::sm::dot<machine>().root;
            return 0;
        }
        if (format == "scxml")
        {
            std::cout << nil::sm::scxml<machine>().root;
            return 0;
        }
        if (format == "xstate")
        {
            std::cout << nil::sm::xstate<machine>().root;
            return 0;
        }
    }

    std::cout << "- puml\n"
                 "- mermaid\n"
                 "- dot\n"
                 "- scxml\n"
                 "- xstate\n";
    return 1;
}
