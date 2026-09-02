#pragma once

// Specializes formatter::detail::barrier_ir for barrier::State<FinalizeAction, Provider>, so
// puml/dot/mermaid/scxml/xstate render the child's graph nested inside this node when
// Provider exposes a static ir() -> formatter::ir::Model. Include this alongside
// nil/sm/uml.hpp wherever a barrier-using state graph is formatted; it is not pulled in by
// nil/sm/barrier.hpp itself, matching how barrier.hpp is not pulled in by nil/sm.hpp.

#include "../barrier.hpp"
#include "detail.hpp"

namespace nil::sm::formatter::detail
{
    template <typename FinalizeAction, typename Provider>
    struct barrier_ir<barrier::State<FinalizeAction, Provider>>
    {
        static constexpr bool available
            = requires(const Metadata* parent) { Provider::ir(parent); };

        static auto model(const Metadata* parent)
        {
            return Provider::ir(parent);
        }
    };
}
