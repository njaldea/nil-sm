#pragma once

#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace nil::sm::ir::response
{
    enum class EEntry
    {
        noop,
        emit
    };

    enum class EExit
    {
        noop,
        emit
    };

    enum class ERegionsFinalized
    {
        noop,
        emit
    };

    enum class EEvent
    {
        discard,
        forward,
        defer,
        emit
    };
}

namespace nil::sm::ir::action
{
    struct Entry
    {
        response::EEntry response;
    };

    struct Exit
    {
        response::EExit response;
    };

    struct RegionsFinalized
    {
        response::ERegionsFinalized response;
    };

    struct Event
    {
        std::string event_name;
        response::EEvent response;
    };

    struct Capture
    {
        std::string event_name;
        response::EEvent response;
    };

    using Info = std::variant<Entry, Exit, RegionsFinalized, Event, Capture>;
}

namespace nil::sm::ir::transit
{
    struct Event
    {
        std::string source_id;
        std::string target_id; // [*] as sentinel for termination
        std::string event_name;
    };

    struct Capture
    {
        std::string source_id;
        std::string target_id; // [*] as sentinel for termination
        std::string event_name;
    };

    using Info = std::variant<Event, Capture>;
}

namespace nil::sm::ir
{
    struct Node
    {
        std::string id;
        std::string display_name;
        bool is_initial = false; // first state of its region/root list
        std::vector<action::Info> actions;
        std::vector<transit::Info> transitions;
        std::vector<std::vector<Node>> regions; // empty regions => leaf state
    };

    struct Model
    {
        std::vector<Node> roots;
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

    inline std::string_view action_name(ir::response::EEntry response)
    {
        switch (response)
        {
            case ir::response::EEntry::noop:
                return "NOOP";
            case ir::response::EEntry::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::response::EExit response)
    {
        switch (response)
        {
            case ir::response::EExit::noop:
                return "NOOP";
            case ir::response::EExit::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::response::ERegionsFinalized response)
    {
        switch (response)
        {
            case ir::response::ERegionsFinalized::noop:
                return "NOOP";
            case ir::response::ERegionsFinalized::emit:
                return "Emit";
        }
        return "";
    }

    inline std::string_view action_name(ir::response::EEvent response)
    {
        switch (response)
        {
            case ir::response::EEvent::discard:
                return "Discard";
            case ir::response::EEvent::forward:
                return "Forward";
            case ir::response::EEvent::defer:
                return "Defer";
            case ir::response::EEvent::emit:
                return "Emit";
        }
        return "";
    }

    inline const std::string& source_id(const ir::transit::Info& transition)
    {
        return std::visit(
            [](const auto& info) -> const std::string& { return info.source_id; },
            transition
        );
    }

    inline const std::string& target_id(const ir::transit::Info& transition)
    {
        return std::visit(
            [](const auto& info) -> const std::string& { return info.target_id; },
            transition
        );
    }

    inline const std::string& event_name(const ir::transit::Info& transition)
    {
        return std::visit(
            [](const auto& info) -> const std::string& { return info.event_name; },
            transition
        );
    }

    inline bool is_capture(const ir::transit::Info& transition)
    {
        return std::holds_alternative<ir::transit::Capture>(transition);
    }
}
