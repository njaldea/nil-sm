// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "concepts.hpp"

#include <nil/xalt/tlist.hpp>

namespace nil::sm::detail
{
    template <typename Arg>
    struct resolved_arg_type
    {
        using type = Arg;
    };

    template <typename Arg>
    struct resolved_arg_type<direct_parent<Arg>>
    {
        using type = Arg;
    };

    template <typename Arg>
    using resolved_arg_type_t = typename resolved_arg_type<Arg>::type;

    template <typename T>
    concept is_action_result_valid = concepts::match::result_of<concepts::match::action_leaf, T>;

    template <typename T>
    concept is_hook_result_valid = concepts::match::result_of<concepts::match::hook_leaf, T>;

    template <typename T>
    concept is_finalizer_result_valid
        = concepts::match::result_of<concepts::match::finalized_leaf, T>;

    template <typename API, typename T>
    concept has_state_typedefs
        = requires {
              typename API::template state<T>::regions_t;
              typename API::template state<T>::events_t;
              typename API::template state<T>::captures_t;
              typename API::template state<T>::args_t;
              typename API::template state<T>::props_t;
          } && nil::xalt::is_of_template_v<typename API::template state<T>::regions_t, nil::xalt::tlist> && nil::xalt::is_of_template_v<typename API::template state<T>::events_t, nil::xalt::tlist> && nil::xalt::is_of_template_v<typename API::template state<T>::captures_t, nil::xalt::tlist> && nil::xalt::is_of_template_v<typename API::template state<T>::args_t, nil::xalt::tlist> && nil::xalt::is_of_template_v<typename API::template state<T>::props_t, nil::xalt::tlist>;

    template <typename API, typename T, typename ArgsList>
    struct is_make_invocable_helper;

    template <typename API, typename T, typename... Args>
    struct is_make_invocable_helper<API, T, nil::xalt::tlist<Args...>>
    {
        static constexpr auto value = requires(
            typename API::context_t* ctx,
            Metadata meta,
            resolved_arg_type_t<Args>*... args
        ) {
            { API::template state<T>::make(ctx, meta, args...) } -> std::convertible_to<T>;
        };
    };

    template <typename API, typename T>
    concept is_make_valid = has_state_typedefs<API, T>
        && is_make_invocable_helper<API, T, typename API::template state<T>::args_t>::value;

    template <typename API, typename T>
    struct state_context
    {
        using api_t = API;
        using state_t = T;
    };

    template <typename Context, typename Prop>
    struct prop_valid_pred
    {
        using state_t = typename Context::state_t;

        static constexpr bool value = requires(state_t& state) { Prop::get(state); };
    };

    template <typename Context, typename List, template <typename, typename> typename Pred>
    struct all_valid_in_tlist;

    template <typename Context, template <typename, typename> typename Pred, typename... Items>
    struct all_valid_in_tlist<Context, nil::xalt::tlist<Items...>, Pred>
    {
        static constexpr bool value = (Pred<Context, Items>::value && ...);
    };

    template <typename API, typename T>
    concept are_props_valid = has_state_typedefs<API, T>
        && all_valid_in_tlist<state_context<API, T>,
                              typename API::template state<T>::props_t,
                              prop_valid_pred>::value;

    template <typename API, typename T, typename Event>
    concept is_event_hook_valid
        = requires(T& state, const Event& event, typename API::context_t* ctx) {
              { API::template state<T>::on_event(state, event, ctx) } -> is_action_result_valid;
          };

    template <typename Context, typename Event>
    struct event_hook_valid_pred
    {
        using api_t = typename Context::api_t;
        using state_t = typename Context::state_t;

        static constexpr bool value = is_event_hook_valid<api_t, state_t, Event>;
    };

    template <typename API, typename T>
    concept are_event_hooks_valid = has_state_typedefs<API, T>
        && all_valid_in_tlist<state_context<API, T>,
                              typename API::template state<T>::events_t,
                              event_hook_valid_pred>::value;

    template <typename API, typename T, typename Event>
    concept is_capture_hook_valid
        = requires(T& state, const Event& event, typename API::context_t* ctx) {
              { API::template state<T>::on_capture(state, event, ctx) } -> is_action_result_valid;
          };

    template <typename Context, typename Event>
    struct capture_hook_valid_pred
    {
        using api_t = typename Context::api_t;
        using state_t = typename Context::state_t;

        static constexpr bool value = is_capture_hook_valid<api_t, state_t, Event>;
    };

    template <typename API, typename T>
    concept are_capture_hooks_valid = has_state_typedefs<API, T>
        && all_valid_in_tlist<state_context<API, T>,
                              typename API::template state<T>::captures_t,
                              capture_hook_valid_pred>::value;

    template <typename API, typename T>
    concept is_on_enter_hook_valid = requires(T& state, typename API::context_t* ctx) {
        { API::template state<T>::on_enter(state, ctx) } -> is_hook_result_valid;
    };

    template <typename API, typename T>
    concept is_on_exit_hook_valid = requires(T& state, typename API::context_t* ctx) {
        { API::template state<T>::on_exit(state, ctx) } -> is_hook_result_valid;
    };

    template <typename API, typename T>
    concept is_on_regions_finalized_hook_valid = requires(T& state, typename API::context_t* ctx) {
        { API::template state<T>::on_regions_finalized(state, ctx) } -> is_finalizer_result_valid;
    };

    template <typename API, typename T>
    concept is_state_valid = has_state_typedefs<API, T> && is_on_enter_hook_valid<API, T>
        && is_on_exit_hook_valid<API, T> && is_on_regions_finalized_hook_valid<API, T>
        && is_make_valid<API, T> && are_props_valid<API, T> && are_event_hooks_valid<API, T>
        && are_capture_hooks_valid<API, T>;
}
