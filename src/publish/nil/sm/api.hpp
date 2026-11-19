// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "concepts.hpp"
#include "detail.hpp"

#include <nil/xalt/coalesce.hpp>

namespace nil::sm::api
{
    struct Default final
    {
        template <typename T>
        struct state final
        {
            using instance_t = T;
            using regions_t = nil::xalt::coalesce_t<T, detail::regions_tag>;
            using events_t = nil::xalt::coalesce_t<T, detail::events_tag>;
            using captures_t = nil::xalt::coalesce_t<T, detail::captures_tag>;
            using args_t = nil::xalt::coalesce_t<T, detail::args_tag>;
            using props_t = nil::xalt::coalesce_t<T, detail::props_tag>;

            template <typename... Args>
            static instance_t make(Metadata /* metadata */, Args*... args)
            {
                return T{args...};
            }

            template <typename E>
            static auto on_event(T& state, const E& event)
            {
                // will not be called if not in events list
                static_assert(concepts::has_valid_on_event<T, E>);
                return state.on_event(event);
            }

            template <typename E>
            static auto on_capture(T& state, const E& event)
            {
                // will not be called if not in captures list
                static_assert(concepts::has_valid_on_capture<T, E>);
                return state.on_capture(event);
            }

            static auto on_enter(T& state)
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

            static auto on_exit(T& state)
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

            static auto on_regions_finalized(T& state)
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
        template <typename T>
        struct state final
        {
            using api_state_t = typename API::template state<T>;
            using default_state_t = typename Default::template state<T>;
            NIL_XALT_COALESCE_TAG(instance_t, T);
            NIL_XALT_COALESCE_TAG(events_t, default_state_t::events_t);
            NIL_XALT_COALESCE_TAG(regions_t, default_state_t::regions_t);
            NIL_XALT_COALESCE_TAG(captures_t, default_state_t::captures_t);
            NIL_XALT_COALESCE_TAG(args_t, default_state_t::args_t);
            NIL_XALT_COALESCE_TAG(props_t, default_state_t::props_t);

            // Aliases come from the partial API when it declares them, otherwise from the default.
            // A partial API that makes a wrapper must also declare it as its instance_t.
            using instance_t = nil::xalt::coalesce_t<api_state_t, instance_t_tag>;
            using regions_t = nil::xalt::coalesce_t<api_state_t, regions_t_tag>;
            using events_t = nil::xalt::coalesce_t<api_state_t, events_t_tag>;
            using captures_t = nil::xalt::coalesce_t<api_state_t, captures_t_tag>;
            using args_t = nil::xalt::coalesce_t<api_state_t, args_t_tag>;
            using props_t = nil::xalt::coalesce_t<api_state_t, props_t_tag>;

            static instance_t make(const Metadata& metadata, auto*... args)
            {
                static constexpr auto api_has_make
                    = requires() { api_state_t::make(metadata, args...); };

                if constexpr (api_has_make)
                {
                    return api_state_t::make(metadata, args...);
                }
                else
                {
                    return default_state_t::make(metadata, args...);
                }
            }

            template <typename E>
            static auto on_event(instance_t& state, const E& event)
            {
                if constexpr (requires() { api_state_t::on_event(state, event); })
                {
                    return api_state_t::on_event(state, event);
                }
                else
                {
                    return default_state_t::on_event(state, event);
                }
            }

            template <typename E>
            static auto on_capture(instance_t& state, const E& event)
            {
                if constexpr (requires() { api_state_t::on_capture(state, event); })
                {
                    return api_state_t::on_capture(state, event);
                }
                else
                {
                    return default_state_t::on_capture(state, event);
                }
            }

            static auto on_enter(instance_t& state)
            {
                if constexpr (requires() { api_state_t::on_enter(state); })
                {
                    return api_state_t::on_enter(state);
                }
                else
                {
                    return default_state_t::on_enter(state);
                }
            }

            static auto on_exit(instance_t& state)
            {
                if constexpr (requires() { api_state_t::on_exit(state); })
                {
                    return api_state_t::on_exit(state);
                }
                else
                {
                    return default_state_t::on_exit(state);
                }
            }

            static auto on_regions_finalized(instance_t& state)
            {
                if constexpr (requires() { api_state_t::on_regions_finalized(state); })
                {
                    return api_state_t::on_regions_finalized(state);
                }
                else
                {
                    return default_state_t::on_regions_finalized(state);
                }
            }
        };
    };
}
