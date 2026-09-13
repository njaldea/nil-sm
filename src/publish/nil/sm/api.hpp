// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "concepts.hpp"
#include "detail.hpp"

#include <nil/xalt/coalesce.hpp>

namespace nil::sm::api
{
    template <typename A = void>
    struct Default final
    {
        using context_t = A;

        template <typename T>
        struct state final
        {
            using regions_t = nil::xalt::coalesce_t<T, detail::regions_tag>;
            using events_t = nil::xalt::coalesce_t<T, detail::events_tag>;
            using captures_t = nil::xalt::coalesce_t<T, detail::captures_tag>;
            using args_t = nil::xalt::coalesce_t<T, detail::args_tag>;
            using props_t = nil::xalt::coalesce_t<T, detail::props_tag>;

            template <typename... Args>
            static T make(context_t* /* contexts */, Metadata /* metadata */, Args*... args)
            {
                return T{args...};
            }

            template <typename E>
            static auto on_event(T& state, const E& event, context_t* /* contexts */)
            {
                // will not be called if not in events list
                static_assert(concepts::has_valid_on_event<T, E>);
                return state.on_event(event);
            }

            template <typename E>
            static auto on_capture(T& state, const E& event, context_t* /* contexts */)
            {
                // will not be called if not in captures list
                static_assert(concepts::has_valid_on_capture<T, E>);
                return state.on_capture(event);
            }

            static auto on_enter(T& state, context_t* /* contexts */)
            {
                // need to check for existence first
                if constexpr (concepts::has_on_enter<T>)
                {
                    // then check for validity
                    static_assert(concepts::has_valid_on_enter<T>);
                    return state.on_enter();
                }
                else
                {
                    return Unhandled();
                }
            }

            static auto on_exit(T& state, context_t* /* contexts */)
            {
                // need to check for existence first
                if constexpr (concepts::has_on_exit<T>)
                {
                    // then check for validity
                    static_assert(concepts::has_valid_on_exit<T>);
                    return state.on_exit();
                }
                else
                {
                    return Unhandled();
                }
            }

            static auto on_regions_finalized(T& state, context_t* /* contexts */)
            {
                // need to check for existence first
                if constexpr (concepts::has_on_regions_finalized<T>)
                {
                    // then check for validity
                    static_assert(concepts::has_valid_on_regions_finalized<T>);
                    return state.on_regions_finalized();
                }
                else
                {
                    return Unhandled();
                }
            }
        };
    };

    template <typename API>
    struct Coalesce final
    {
        NIL_XALT_COALESCE_TAG(context_t, void);
        using context_t = nil::xalt::coalesce_t<API, context_t_tag>;

        template <typename T>
        struct state final
        {
            using state_t = typename API::template state<T>;
            using default_state_t = typename Default<context_t>::template state<T>;
            NIL_XALT_COALESCE_TAG(events_t, default_state_t::events_t);
            NIL_XALT_COALESCE_TAG(regions_t, default_state_t::regions_t);
            NIL_XALT_COALESCE_TAG(captures_t, default_state_t::captures_t);
            NIL_XALT_COALESCE_TAG(args_t, default_state_t::args_t);
            NIL_XALT_COALESCE_TAG(props_t, default_state_t::props_t);

            using regions_t = nil::xalt::coalesce_t<T, regions_t_tag>;
            using events_t = nil::xalt::coalesce_t<T, events_t_tag>;
            using captures_t = nil::xalt::coalesce_t<T, captures_t_tag>;
            using args_t = nil::xalt::coalesce_t<T, args_t_tag>;
            using props_t = nil::xalt::coalesce_t<T, props_t_tag>;

            static T make(context_t* contexts, const Metadata& metadata, auto*... args)
            {
                static constexpr auto api_has_make
                    = requires() { state_t::make(contexts, metadata, args...); };

                if constexpr (api_has_make)
                {
                    return state_t::make(contexts, metadata, args...);
                }
                else
                {
                    return default_state_t::make(contexts, metadata, args...);
                }
            }

            template <typename E>
            static auto on_event(T& state, const E& event, context_t* contexts)
            {
                if constexpr (requires() { state_t::on_event(state, event, contexts); })
                {
                    return state_t::on_event(state, event, contexts);
                }
                else
                {
                    return default_state_t::on_event(state, event, contexts);
                }
            }

            template <typename E>
            static auto on_capture(T& state, const E& event, context_t* contexts)
            {
                if constexpr (requires() { state_t::on_capture(state, event, contexts); })
                {
                    return state_t::on_capture(state, event, contexts);
                }
                else
                {
                    return default_state_t::on_capture(state, event, contexts);
                }
            }

            static auto on_enter(T& state, context_t* contexts)
            {
                if constexpr (requires() { state_t::on_enter(state, contexts); })
                {
                    return state_t::on_enter(state, contexts);
                }
                else
                {
                    return default_state_t::on_enter(state, contexts);
                }
            }

            static auto on_exit(T& state, context_t* contexts)
            {
                if constexpr (requires() { state_t::on_exit(state, contexts); })
                {
                    return state_t::on_exit(state, contexts);
                }
                else
                {
                    return default_state_t::on_exit(state, contexts);
                }
            }

            static auto on_regions_finalized(T& state, context_t* contexts)
            {
                if constexpr (requires() { state_t::on_regions_finalized(state, contexts); })
                {
                    return state_t::on_regions_finalized(state, contexts);
                }
                else
                {
                    return default_state_t::on_regions_finalized(state, contexts);
                }
            }
        };
    };
}
