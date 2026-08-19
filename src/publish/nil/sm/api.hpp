#pragma once

#include "detail.hpp"

#include <nil/xalt/coalesce.hpp>

#include <type_traits>

namespace nil::sm::api
{
    template <typename T, typename S = void, typename A = void>
    struct Default final
    {
        using state_t = T;
        using state_context_t = S;
        using api_context_t = A;
        using regions_t = nil::xalt::coalesce_t<T, detail::regions_tag>;
        using events_t = nil::xalt::coalesce_t<T, detail::events_tag>;
        using captures_t = nil::xalt::coalesce_t<T, detail::captures_tag>;

        template <typename Parent>
        static state_t make(
            Parent* parent,
            state_context_t* state_contexts,
            api_context_t* /* api_contexts */,
            Metadata /* metadata */
        )
        {
            if constexpr (requires() { T(parent, state_contexts); })
            {
                return T(parent, state_contexts);
            }
            else if constexpr (requires() { T(parent); })
            {
                return T(parent);
            }
            else
            {
                static_assert(
                    std::is_default_constructible_v<T>,
                    "State must be constructible from parent/contexts or "
                    "default-constructible"
                );
                return T();
            }
        }

        template <typename E>
        static auto on_event(
            state_t& state,
            const E& event,
            api_context_t* /* api_contexts */
        )
        {
            static_assert(concepts::has_on_event<state_t, E>);
            return state.on_event(event);
        }

        template <typename E>
        static auto on_capture(
            state_t& state,
            const E& event,
            api_context_t* /* api_contexts */
        )
        {
            static_assert(concepts::has_on_capture<state_t, E>);
            return state.on_capture(event);
        }

        static auto on_enter(state_t& state, api_context_t* /* api_contexts */)
        {
            if constexpr (concepts::has_on_enter<state_t>)
            {
                return state.on_enter();
            }
            else
            {
                return Unhandled();
            }
        }

        static auto on_exit(state_t& state, api_context_t* /* api_contexts */)
        {
            if constexpr (concepts::has_on_exit<state_t>)
            {
                return state.on_exit();
            }
            else
            {
                return Unhandled();
            }
        }

        static auto on_regions_finalized(state_t& state, api_context_t* /* api_contexts */)
        {
            if constexpr (concepts::has_on_regions_finalized<state_t>)
            {
                return state.on_regions_finalized();
            }
            else
            {
                return Unhandled();
            }
        }
    };

    template <template <typename...> typename API>
    struct Coalesce final
    {
        template <typename T>
        struct type final
        {
            using inner_t = T;
            NIL_XALT_COALESCE_TAG(state_context_t, void*);
            NIL_XALT_COALESCE_TAG(api_context_t, void*);

            using state_context_t = nil::xalt::coalesce_t<API<T>, state_context_t_tag>;
            using api_context_t = nil::xalt::coalesce_t<API<T>, api_context_t_tag>;

            using defaulter_t = Default<inner_t, state_context_t, api_context_t>;
            NIL_XALT_COALESCE_TAG(state_t, defaulter_t::state_t);
            NIL_XALT_COALESCE_TAG(events_t, defaulter_t::events_t);
            NIL_XALT_COALESCE_TAG(regions_t, defaulter_t::regions_t);
            NIL_XALT_COALESCE_TAG(captures_t, defaulter_t::captures_t);

            using state_t = nil::xalt::coalesce_t<T, state_t_tag>;
            using regions_t = nil::xalt::coalesce_t<T, regions_t_tag>;
            using events_t = nil::xalt::coalesce_t<T, events_t_tag>;
            using captures_t = nil::xalt::coalesce_t<T, captures_t_tag>;

            template <typename Parent>
            static state_t make(
                Parent* parent,
                state_context_t* state_contexts,
                api_context_t* api_contexts,
                const Metadata& metadata
            )
            {
                static constexpr auto api_has_make
                    = requires() { API<T>::make(parent, state_contexts, api_contexts, metadata); };

                if constexpr (api_has_make)
                {
                    return API<T>::make(parent, state_contexts, api_contexts, metadata);
                }
                else
                {
                    return defaulter_t::make(parent, state_contexts, api_contexts, metadata);
                }
            }

            template <typename E>
            static auto on_event(state_t& state, const E& event, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_event(state, event, api_contexts); })
                {
                    return API<T>::on_event(state, event, api_contexts);
                }
                else
                {
                    return defaulter_t::on_event(state, event, api_contexts);
                }
            }

            template <typename E>
            static auto on_capture(state_t& state, const E& event, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_capture(state, event, api_contexts); })
                {
                    return API<T>::on_capture(state, event, api_contexts);
                }
                else
                {
                    return defaulter_t::on_capture(state, event, api_contexts);
                }
            }

            static auto on_enter(state_t& state, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_enter(state, api_contexts); })
                {
                    return API<T>::on_enter(state, api_contexts);
                }
                else
                {
                    return defaulter_t::on_enter(state, api_contexts);
                }
            }

            static auto on_exit(state_t& state, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_exit(state, api_contexts); })
                {
                    return API<T>::on_exit(state, api_contexts);
                }
                else
                {
                    return defaulter_t::on_exit(state, api_contexts);
                }
            }

            static auto on_regions_finalized(state_t& state, api_context_t* api_contexts)
            {
                if constexpr (requires() { API<T>::on_regions_finalized(state, api_contexts); })
                {
                    return API<T>::on_regions_finalized(state, api_contexts);
                }
                else
                {
                    return defaulter_t::on_regions_finalized(state, api_contexts);
                }
            }
        };
    };
}
