// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include <nil/xalt/fn_sign.hpp>
#include <nil/xalt/str_name.hpp>
#include <nil/xalt/typed.hpp>

#include <cstddef>
#include <functional>
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

    namespace detail
    {
        template <typename>
        struct member_object_traits;

        template <typename State, typename Data>
        struct member_object_traits<Data State::*>
        {
            using data_type = Data;
        };
    }

    template <auto Ptr>
    struct prop;

    template <auto Ptr>
        requires xalt::is_fn<decltype(Ptr)>
    struct prop<Ptr> final
    {
        using type = std::remove_pointer_t<typename xalt::fn_sign<decltype(Ptr)>::return_type>;

        static_assert(!std::is_const_v<type>, "Properties must expose a mutable type.");

        static type* get(auto& state)
        {
            return std::invoke(Ptr, state);
        }
    };

    template <auto Ptr>
        requires std::is_member_object_pointer_v<decltype(Ptr)>
    struct prop<Ptr> final
    {
        using type = typename detail::member_object_traits<decltype(Ptr)>::data_type;

        static_assert(!std::is_const_v<type>, "Properties must expose a mutable type.");

        static type* get(auto& state)
        {
            return std::addressof(state.*Ptr);
        }
    };

    template <typename Base>
    struct as_parent final
    {
        static_assert(!std::is_const_v<Base>, "Properties must expose a mutable type.");

        using type = Base;

        static type* get(Base& state)
        {
            return std::addressof(state);
        }
    };

    // Opt-in escape hatch: `using args = tlist<direct_parent<Type>>;` takes `Type*` from the
    // direct parent's own props only, so the parent must expose `Type`, for example with
    // `as_parent<Type>`. In a barrier's first states it uses the barrier's parent instead.
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
    constexpr std::string_view type_name()
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
}

namespace nil::sm
{
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
        using type = T;

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
        operator Event() &&
        {
            return Event{
                .id = id,
                .deleter = deleter,
                .cloner = cloner,
                .data = std::exchange(data, nullptr),
            };
        }

    private:
        const void* id = nullptr;
        void (*deleter)(void*) = nullptr;
        void* (*cloner)(void*) = nullptr;
        void* data = nullptr;
    };

    template <typename API, typename T>
    class State;
}
