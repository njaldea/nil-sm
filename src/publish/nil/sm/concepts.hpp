// Copyright (c) 2026, Neil Aldea <njaldea@gmail.com>
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "structs.hpp"

#include <nil/xalt/checks.hpp>
#include <nil/xalt/tlist.hpp>

#include <type_traits>
#include <variant>

namespace nil::sm::concepts::match
{
    template <typename T>
    struct action_leaf final
    {
        static constexpr bool value                      //
            = std::is_same_v<T, Terminate>               //
            || std::is_same_v<T, Forward>                //
            || std::is_same_v<T, Defer>                  //
            || std::is_same_v<T, Discard>                //
            || nil::xalt::is_of_template_v<T, TransitTo> //
            || nil::xalt::is_of_template_v<T, DeferTo>   //
            || nil::xalt::is_of_template_v<T, Emit>;
    };

    template <typename T>
    struct hook_leaf final
    {
        static constexpr bool value   //
            = std::is_same_v<T, NOOP> //
            || nil::xalt::is_of_template_v<T, Emit>;
    };

    template <typename T>
    struct finalized_leaf final
    {
        static constexpr bool value                      //
            = std::is_same_v<T, NOOP>                    //
            || std::is_same_v<T, Terminate>              //
            || nil::xalt::is_of_template_v<T, TransitTo> //
            || nil::xalt::is_of_template_v<T, Emit>;
    };

    template <template <typename> typename Leaf, typename T>
    struct match final: std::bool_constant<Leaf<T>::value>
    {
    };

    template <template <typename> typename Leaf, typename... T>
    struct match<Leaf, std::variant<T...>>: std::bool_constant<(match<Leaf, T>::value && ...)>
    {
    };

    template <typename T>
    concept action = match<action_leaf, T>::value;

    template <typename T>
    concept hook = match<hook_leaf, T>::value;

    template <typename T>
    concept finalized = match<finalized_leaf, T>::value;
}

namespace nil::sm::concepts
{
    template <typename T>
    concept has_on_enter = requires(T t) {
        { t.on_enter() };
    };

    template <typename T>
    concept has_on_exit = requires(T t) {
        { t.on_exit() };
    };

    template <typename T>
    concept has_on_regions_finalized = requires(T t) {
        { t.on_regions_finalized() };
    };

    template <typename T, typename E>
    concept has_valid_on_event = requires(T t, E event) {
        { t.on_event(event) } -> match::action;
    };

    template <typename T, typename E>
    concept has_valid_on_capture = requires(T t, E event) {
        { t.on_capture(event) } -> match::action;
    };

    template <typename T>
    concept has_valid_on_enter = requires(T t) {
        { t.on_enter() } -> match::hook;
    };

    template <typename T>
    concept has_valid_on_exit = requires(T t) {
        { t.on_exit() } -> match::hook;
    };

    template <typename T>
    concept has_valid_on_regions_finalized = requires(T t) {
        { t.on_regions_finalized() } -> match::finalized;
    };
}
