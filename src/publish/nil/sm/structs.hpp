#pragma once

#include <nil/xalt/str_name.hpp>
#include <nil/xalt/typed.hpp>

#include <cstddef>
#include <string_view>
#include <utility>

namespace nil::sm
{
    struct Metadata final
    {
        std::size_t state = 0;
        std::size_t region = 0;
        std::size_t subregions = 1;
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
    struct Contexts;
    struct IState;

    struct EvRegionsFinalized final
    {
        // This is the state instance owned by State<T>
        const void* target = nullptr;
    };

    struct Transit final
    {
        const void* target = nullptr;
    };

    struct Emit final
    {
        const void* id = nullptr;
        void (*deleter)(void*) = nullptr;
        void* (*cloner)(void*) = nullptr;
        void* data = nullptr;
    };
}

namespace nil::sm
{
    struct Fin final
    {
        static constexpr auto name = "[**]";
    };

    struct Root final
    {
    };

    struct Unhandled final
    {
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

    struct NOOP final
    {
    };

    template <typename T>
    struct Transit final
    {
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

    private:
        using type = T;
        const void* id = nullptr;
        void (*deleter)(void*) = nullptr;
        void* (*cloner)(void*) = nullptr;
        void* data = nullptr;

        template <template <typename...> typename API, typename U>
        friend class State;
    };

    template <template <typename...> typename API, typename T>
    class State;
}
