#pragma once

#include "structs.hpp"

#include <nil/xalt/checks.hpp>
#include <nil/xalt/tlist.hpp>

#include <type_traits>
#include <variant>

namespace nil::sm::concepts
{
    template <typename T>
    concept is_allowed_to_use_for_on_event         //
        = std::is_same_v<T, Terminate>             //
        || std::is_same_v<T, Forward>              //
        || std::is_same_v<T, Defer>                //
        || std::is_same_v<T, Discard>              //
        || nil::xalt::is_of_template_v<T, Transit> //
        || nil::xalt::is_of_template_v<T, Emit>;

    template <typename T>
    struct is_allowed_to_use_for_react_as_predicate final
    {
        static constexpr bool value = is_allowed_to_use_for_on_event<T>;
    };

    template <typename T>
    concept is_allowed_to_use_for_on_event_result
        = is_allowed_to_use_for_on_event<std::remove_cvref_t<T>>
        || (nil::xalt::is_of_template_v<std::remove_cvref_t<T>, std::variant>
            && nil::xalt::to_tlist_t<std::remove_cvref_t<T>>::template all_of<
                is_allowed_to_use_for_react_as_predicate>);

    template <typename T, typename E>
    concept has_on_event = requires(T t, E event) {
        { t.on_event(event) } -> is_allowed_to_use_for_on_event_result;
    };

    template <typename T, typename E>
    concept has_on_capture = requires(T t, E event) {
        { t.on_capture(event) } -> is_allowed_to_use_for_on_event_result;
    };

    template <typename T>
    concept is_allowed_to_use_for_lifecycle_hook
        = std::is_same_v<T, NOOP> || nil::xalt::is_of_template_v<T, Emit>;

    template <typename T>
    struct is_allowed_to_use_for_lifecycle_hook_as_predicate final
    {
        static constexpr bool value = is_allowed_to_use_for_lifecycle_hook<T>;
    };

    template <typename T>
    concept has_on_enter = requires(T t) {
        { t.on_enter() } -> is_allowed_to_use_for_lifecycle_hook;
    } || requires(T t) {
        requires nil::xalt::is_of_template_v<decltype(t.on_enter()), std::variant>;
        requires nil::xalt::to_tlist_t<decltype(t.on_enter()
        )>::template all_of<is_allowed_to_use_for_lifecycle_hook_as_predicate>;
    };

    template <typename T>
    concept has_on_exit = requires(T t) {
        { t.on_exit() } -> is_allowed_to_use_for_lifecycle_hook;
    } || requires(T t) {
        requires nil::xalt::is_of_template_v<decltype(t.on_exit()), std::variant>;
        requires nil::xalt::to_tlist_t<decltype(t.on_exit()
        )>::template all_of<is_allowed_to_use_for_lifecycle_hook_as_predicate>;
    };

    template <typename T>
    concept is_allowed_to_use_for_on_regions_finalized //
        = std::is_same_v<T, NOOP>                      //
        || std::is_same_v<T, Terminate>                //
        || nil::xalt::is_of_template_v<T, Transit>     //
        || nil::xalt::is_of_template_v<T, Emit>;

    template <typename T>
    struct is_allowed_to_use_for_on_regions_finalized_as_predicate final
    {
        static constexpr bool value = is_allowed_to_use_for_on_regions_finalized<T>;
    };

    template <typename T>
    concept has_on_regions_finalized = requires(T t) {
        { t.on_regions_finalized() } -> is_allowed_to_use_for_on_regions_finalized;
    } || requires(T t) {
        requires nil::xalt::is_of_template_v<decltype(t.on_regions_finalized()), std::variant>;
        requires nil::xalt::to_tlist_t<decltype(t.on_regions_finalized()
        )>::template all_of<is_allowed_to_use_for_on_regions_finalized_as_predicate>;
    };
}
