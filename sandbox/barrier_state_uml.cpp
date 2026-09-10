// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#include "nil/sm/structs.hpp"
#include <nil/sm/barrier.hpp>
#include <nil/sm/uml.hpp>

#include <iostream>

namespace demo
{
    struct A
    {
    };

    struct missing_one
    {
    };

    struct direct_missing
    {
        using args = nil::xalt::tlist<missing_one>;

        explicit direct_missing(missing_one* /* dependency */)
        {
        }
    };

    struct second_level_missing
    {
        using args = nil::xalt::tlist<missing_one>;

        explicit second_level_missing(missing_one* /* dependency */)
        {
        }
    };

    template <typename T>
    using child_api = nil::sm::api::Default<>::type<T>;

    NIL_SM_BARRIER_DECLARE(second_level_barrier_state, child_api);
    NIL_SM_BARRIER_DEFINE(second_level_barrier_state, second_level_missing);
    using second_level_barrier
        = nil::sm::barrier::State<nil::sm::Terminate, second_level_barrier_state>;

    struct first_level_satisfied
    {
        using regions = nil::xalt::tlist<second_level_barrier>;
    };

    struct third_level_missing
    {
        using args = nil::xalt::tlist<missing_one>;

        explicit third_level_missing(missing_one* /* dependency */)
        {
        }
    };

    NIL_SM_BARRIER_DECLARE(third_level_barrier_state, child_api);
    NIL_SM_BARRIER_DEFINE(third_level_barrier_state, third_level_missing);
    using third_level_barrier
        = nil::sm::barrier::State<nil::sm::Terminate, third_level_barrier_state>;

    struct first_level_missing
    {
        using args = nil::xalt::tlist<missing_one>;
        using regions = nil::xalt::tlist<third_level_barrier>;

        explicit first_level_missing(missing_one* /* dependency */)
        {
        }
    };

    struct nested_barrier_root
    {
        using regions
            = nil::xalt::tlist<direct_missing, first_level_satisfied, first_level_missing>;
    };

    NIL_SM_BARRIER_DECLARE(nested_barrier_state, child_api);
    NIL_SM_BARRIER_DEFINE(nested_barrier_state, nested_barrier_root);
    using nested_barrier = nil::sm::barrier::State<nil::sm::Terminate, nested_barrier_state>;

    struct child_idle
    {
        using regions = nil::xalt::tlist<nested_barrier>;
    };

    NIL_SM_BARRIER_DECLARE(shared_barrier_state, child_api);

    NIL_SM_BARRIER_DEFINE(shared_barrier_state, child_idle);

    using shared_barrier = nil::sm::barrier::State<nil::sm::Terminate, shared_barrier_state>;

    struct first_parent
    {
        missing_one one;
        using props = nil::xalt::tlist<nil::sm::prop<missing_one, &first_parent::one>>;
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct second_parent
    {
        using regions = nil::xalt::tlist<shared_barrier>;
    };

    struct root
    {
        using regions = nil::xalt::tlist<first_parent, second_parent>;
    };
}

int main()
{
    using machine = nil::sm::DefaultSM<demo::root>;
    nil::sm::puml<machine> diagram;

    nil::sm::ir::print_barrier_errors(std::cout, diagram.model);

    std::cout << diagram.root;
    for (const auto& barrier : diagram.barriers)
    {
        std::cout << "\n' ===== " << barrier.name() << " =====\n";
        std::cout << barrier;
    }
    std::cout << std::flush;
    return 0;
}
