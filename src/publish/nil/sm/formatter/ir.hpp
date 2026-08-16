#pragma once

#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace nil::sm::ir
{
    enum class entry_response
    {
        noop,
        emit
    };

    enum class exit_response
    {
        noop,
        emit
    };

    enum class regions_finalized_response
    {
        noop,
        emit
    };

    enum class event_response
    {
        discard,
        forward,
        defer,
        emit
    };

    struct entry_action_info
    {
        entry_response response;
    };

    struct exit_action_info
    {
        exit_response response;
    };

    struct regions_finalized_action_info
    {
        regions_finalized_response response;
    };

    struct event_action_info
    {
        std::string event_name;
        event_response response;
    };

    using action_info = std::variant<
        entry_action_info,
        exit_action_info,
        regions_finalized_action_info,
        event_action_info>;

    struct transition_info
    {
        std::string source_id;
        std::string target_id; // [*] as sentinel for termination
        std::string event_name;
    };

    struct state_node
    {
        std::string id;
        std::string display_name;
        std::vector<action_info> actions;
        std::vector<transition_info> transitions;
        std::vector<std::vector<state_node>> regions; // empty regions => leaf state
    };

    struct model
    {
        std::vector<state_node> roots;
    };
}

namespace nil::sm::formatter
{
    inline std::ostream& indent(std::ostream& os, std::size_t depth)
    {
        for (std::size_t i = 0; i < depth; ++i)
        {
            os << "    ";
        }

        return os;
    }

    inline std::string_view action_name(ir::entry_response response)
    {
        switch (response)
        {
            case ir::entry_response::noop:
                return "NOOP";
            case ir::entry_response::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::exit_response response)
    {
        switch (response)
        {
            case ir::exit_response::noop:
                return "NOOP";
            case ir::exit_response::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::regions_finalized_response response)
    {
        switch (response)
        {
            case ir::regions_finalized_response::noop:
                return "NOOP";
            case ir::regions_finalized_response::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::event_response response)
    {
        switch (response)
        {
            case ir::event_response::discard:
                return "Discard";
            case ir::event_response::forward:
                return "Forward";
            case ir::event_response::defer:
                return "Defer";
            case ir::event_response::emit:
                return "Emit";
        }
        return "";
    }
}
