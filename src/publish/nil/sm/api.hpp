#pragma once

#include "detail.hpp"

#include <nil/xalt/coalesce.hpp>

namespace nil::sm::api
{
    template <typename A = void>
    struct Default final
    {
        template <typename T>
        struct type final
        {
            using api_context_t = A;
            using regions_t = nil::xalt::coalesce_t<T, detail::regions_tag>;
            using events_t = nil::xalt::coalesce_t<T, detail::events_tag>;
            using captures_t = nil::xalt::coalesce_t<T, detail::captures_tag>;
            using args_t = nil::xalt::coalesce_t<T, detail::args_tag>;
            using provides_t = nil::xalt::coalesce_t<T, detail::provides_tag>;

            template <typename... Args>
            static T make(api_context_t* /* api_contexts */, Metadata /* metadata */, Args*... args)
            {
                return T(args...);
            }

            template <typename E>
            static auto on_event(T& state, const E& event, api_context_t* /* api_contexts */)
            {
                static_assert(concepts::has_on_event<T, E>);
                return state.on_event(event);
            }

            template <typename E>
            static auto on_capture(T& state, const E& event, api_context_t* /* api_contexts */)
            {
                static_assert(concepts::has_on_capture<T, E>);
                return state.on_capture(event);
            }

            static auto on_enter(T& state, api_context_t* /* api_contexts */)
            {
                if constexpr (concepts::has_on_enter<T>)
                {
                    return state.on_enter();
                }
                else
                {
                    return Unhandled();
                }
            }

            static auto on_exit(T& state, api_context_t* /* api_contexts */)
            {
                if constexpr (concepts::has_on_exit<T>)
                {
                    return state.on_exit();
                }
                else
                {
                    return Unhandled();
                }
            }

            static auto on_regions_finalized(T& state, api_context_t* /* api_contexts */)
            {
                if constexpr (concepts::has_on_regions_finalized<T>)
                {
                    return state.on_regions_finalized();
                }
                else
                {
                    return Unhandled();
                }
            }
        };
    };

    template <template <typename> typename API>
    struct Coalesce final
    {
        template <typename T>
        struct type final
        {
            using inner_t = T;
            NIL_XALT_COALESCE_TAG(api_context_t, void);

            using api_context_t = nil::xalt::coalesce_t<API<T>, api_context_t_tag>;

            using defaulter_t = Default<api_context_t>::template type<inner_t>;
            NIL_XALT_COALESCE_TAG(events_t, defaulter_t::events_t);
            NIL_XALT_COALESCE_TAG(regions_t, defaulter_t::regions_t);
            NIL_XALT_COALESCE_TAG(captures_t, defaulter_t::captures_t);
            NIL_XALT_COALESCE_TAG(args_t, defaulter_t::args_t);
            NIL_XALT_COALESCE_TAG(provides_t, defaulter_t::provides_t);

            using regions_t = nil::xalt::coalesce_t<T, regions_t_tag>;
            using events_t = nil::xalt::coalesce_t<T, events_t_tag>;
            using captures_t = nil::xalt::coalesce_t<T, captures_t_tag>;
            using args_t = nil::xalt::coalesce_t<T, args_t_tag>;
            using provides_t = nil::xalt::coalesce_t<T, provides_t_tag>;

            static T make(api_context_t* api_contexts, const Metadata& metadata, auto*... args)
            {
                static constexpr auto api_has_make
                    = requires() { API<T>::make(api_contexts, metadata, args...); };

                if constexpr (api_has_make)
                {
                    return API<T>::make(api_contexts, metadata, args...);
                }
                else
                {
                    return defaulter_t::make(api_contexts, metadata, args...);
                }
            }

            template <typename E>
            static auto on_event(T& state, const E& event, api_context_t* api_contexts)
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
            static auto on_capture(T& state, const E& event, api_context_t* api_contexts)
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

            static auto on_enter(T& state, api_context_t* api_contexts)
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

            static auto on_exit(T& state, api_context_t* api_contexts)
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

            static auto on_regions_finalized(T& state, api_context_t* api_contexts)
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
