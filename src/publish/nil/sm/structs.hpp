// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <nil/xalt/str_name.hpp>
#include <nil/xalt/typed.hpp>

#include <cstddef>
#include <limits>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace nil::sm
{
    namespace reserved
    {
        // [**] names the synthetic termination node.
        inline constexpr auto termination_node = "[**]";

        // [**] labels the regions-finalized event.
        inline constexpr auto ev_regions_finalized = "[**]";

        // [/] is the runtime barrier state's reserved name.
        inline constexpr auto barrier = "[/]";
    }

    namespace barrier
    {
        template <typename Action, typename T>
        struct State;
    }

    template <typename T>
    struct siblings
    {
        using type = void;
    };

    // Declares a value a state exposes via get(): `using props = tlist<prop<Member,
    // &State::member>>;`. The descriptor can store a value or pointer data member, a non-const
    // member accessor, or a free accessor accepting the state by reference, each returning
    // Member*.
    template <typename Member, auto MemberPtr>
    struct prop final
    {
        static_assert(!std::is_const_v<Member>, "Properties must expose a mutable type.");

        using type = Member;
        static constexpr auto ptr = MemberPtr;

        template <typename State>
        static type* get(State& state)
        {
            if constexpr (std::is_member_object_pointer_v<decltype(ptr)>)
            {
                return std::addressof(state.*ptr);
            }
            else if constexpr (std::is_member_function_pointer_v<decltype(ptr)>)
            {
                static_assert(
                    !std::is_invocable_v<decltype(ptr), const State&>,
                    "Property member functions must not be const."
                );
                return (state.*ptr)();
            }
            else
            {
                return ptr(state);
            }
        }
    };

    // Opt-in escape hatch: `using args = tlist<direct_parent<ParentType>>;` resolves to the
    // immediate parent's own state address, cast to `ParentType*` (you assert what type it
    // actually is - no prop<> needed on the parent's side).
    template <typename T>
    struct direct_parent final
    {
        using type = T;
    };

    struct Metadata final
    {
        std::size_t state = 0;
        std::size_t region = 0;
        std::size_t subregions = 0;
        std::size_t depth = 0;
        bool is_final = false;
        bool is_barrier = false;
        std::string_view name;
        const Metadata* parent = nullptr;
    };
}

namespace nil::sm::detail
{
    template <typename T>
    std::string_view type_name()
    {
        if constexpr (requires() { T::name; })
        {
            return T::name;
        }
        else
        {
            return nil::xalt::str_short_base_name_sv<T>;
        }
    }

    template <typename T>
    void deleter(void* v)
    {
        delete static_cast<T*>(v); // NOLINT
    }

    template <typename T>
    void* cloner(void* v)
    {
        return new T(*static_cast<T*>(v)); // NOLINT
    }

    class Queues;
    struct IState;

    struct EvRegionsFinalized final
    {
        // This is the state instance owned by State<T>
        const void* target = nullptr;
    };

    struct TransitTo final
    {
        bool defer = false;
        // type id of the target state to transit into
        const void* target = nullptr;
    };

    struct Event final
    {
        const void* id = nullptr; // type id of the event to emit
        void (*deleter)(void*) = nullptr;
        void* (*cloner)(void*) = nullptr;
        void* data = nullptr;

        Event clone() const
        {
            return Event{
                .id = id,
                .deleter = deleter,
                .cloner = cloner,
                .data = cloner(data),
            };
        }
    };
}

namespace nil::sm
{
    struct Fin final
    {
        // NOLINTNEXTLINE
        Fin(const auto&...)
        {
        }

        static constexpr auto name = reserved::termination_node;
        // Reserved Metadata::state value; never a real reachable-state index.
        static constexpr std::size_t state_index = std::numeric_limits<std::size_t>::max();
    };

    struct Unhandled final
    {
        // Mainly to be used by api policy when the hook is not provided by the user.
    };

    struct Terminate final
    {
    };

    struct Forward final
    {
    };

    struct Defer final
    {
    };

    struct Discard final
    {
    };

    using action_t = std::variant<Forward, Unhandled, Discard>;

    struct NOOP final
    {
    };

    template <typename T>
    struct TransitTo final
    {
        static_assert(
            !std::is_same_v<T, Fin>,
            "TransitTo<Fin> is not allowed; use Terminate instead."
        );
        using type = T;
    };

    template <typename T>
    struct DeferTo final
    {
        static_assert(
            !std::is_same_v<T, Fin>,
            "TransitTo<Fin> is not allowed; use Terminate instead."
        );
        using type = T;
    };

    template <typename T>
    struct Emit final
    {
        static_assert(
            std::copy_constructible<T>,
            "Events emitted through Emit must be copy constructible."
        );

        template <typename... Args>
        explicit Emit(Args&&... args)
            : id(nil::xalt::type_id<T>)
            , deleter(&detail::deleter<T>)
            , cloner(&detail::cloner<T>)
            , data(new T{std::forward<Args>(args)...})
        {
        }

        Emit(Emit&& o) noexcept
            : id(o.id)
            , deleter(o.deleter)
            , cloner(o.cloner)
            , data(std::exchange(o.data, nullptr))
        {
        }

        Emit& operator=(Emit&& o) noexcept
        {
            if (this != &o)
            {
                if (data != nullptr)
                {
                    deleter(data);
                }

                id = o.id;
                deleter = o.deleter;
                cloner = o.cloner;
                data = std::exchange(o.data, nullptr);
            }
            return *this;
        }

        Emit(const Emit& o) = delete;
        Emit& operator=(const Emit& o) = delete;

        ~Emit()
        {
            if (data != nullptr)
            {
                deleter(data);
            }
        }

        // NOLINTNEXTLINE
        operator detail::Event() &&
        {
            return detail::Event{
                .id = id,
                .deleter = deleter,
                .cloner = cloner,
                .data = std::exchange(data, nullptr),
            };
        }

    private:
        using type = T;
        const void* id = nullptr;
        void (*deleter)(void*) = nullptr;
        void* (*cloner)(void*) = nullptr;
        void* data = nullptr;
    };

    template <template <typename> typename API, typename T>
    class State;
}
