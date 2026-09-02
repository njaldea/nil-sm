#pragma once

#include "../tollbooth/common.hpp"

#include <nil/sm/barrier.hpp>
#include <nil/sm/ir.hpp>

#include <memory>

// Each factory is compiled in its own translation unit, same as tollbooth_libs, but builds a
// nil::sm::barrier::SM sharing the host's Runtime instead of a fully-owning nil::sm::SM.
// ir_X() runs only the compile-time formatter traversal (no runtime State/Region), so the
// diagram tool pays that cost in the same library instead of every consumer of slot.hpp.
namespace toll::barrier_jobs
{
    std::unique_ptr<nil::sm::ISM> make_startup(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    );
    std::unique_ptr<nil::sm::ISM> make_collection(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    );
    std::unique_ptr<nil::sm::ISM> make_shift(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    );
    std::unique_ptr<nil::sm::ISM> make_maintenance(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    );

    nil::sm::ir::Model ir_startup(const nil::sm::Metadata* parent_metadata);
    nil::sm::ir::Model ir_collection(const nil::sm::Metadata* parent_metadata);
    nil::sm::ir::Model ir_shift(const nil::sm::Metadata* parent_metadata);
    nil::sm::ir::Model ir_maintenance(const nil::sm::Metadata* parent_metadata);
}
