#pragma once

#include "../ir.hpp"

#include <ostream>
#include <string_view>

namespace nil::sm::format
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
}
