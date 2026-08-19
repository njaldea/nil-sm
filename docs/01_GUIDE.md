# Core Guide

This guide explains the parts you need for a small, useful state machine.
For custom APIs, continue with [Extensibility](02_EXTENSIBILITY.md).
For ready-made solutions, see [Patterns](03_PATTERNS.md).

## 1. States and Events

A state is an ordinary C++ type. An event is another ordinary type.
The `events` list tells the library which events a state can handle.

```cpp
struct start {};
struct stop {};

struct running;

struct stopped
{
    using events = nil::xalt::tlist<start>;

    static auto on_event(const start&)
    {
        return nil::sm::Transit<running>{};
    }
};

struct running
{
    using events = nil::xalt::tlist<stop>;

    static auto on_event(const stop&)
    {
        return nil::sm::Transit<stopped>{};
    }
};
```

A state may also contain data, constructors, and helper methods. It does not
need to inherit from a library class.

## 2. Starting the Machine

`DefaultSM` uses the built-in API. Pass `nullptr` when no context is needed.

```cpp
int main()
{
    nil::sm::DefaultSM<stopped> machine{nullptr, nullptr};
    machine.post(start{}); // stopped -> running
    machine.post(stop{});  // running -> stopped
}
```

`post()` is synchronous. It finishes processing one event before it returns.
The library does not create threads or lock the machine.

## 3. Actions

An event handler returns one action:

| Action | Meaning |
| --- | --- |
| `Discard{}` | Consume the event here. |
| `Forward{}` | Let a parent state handle it. |
| `Transit<T>{}` | Leave this state and enter `T`. |
| `Terminate{}` | End this region. |
| `Emit<E>(value)` | Queue a new event for later in this dispatch. |
| `Defer{}` | Save this event until the next transition in this region. |

Example:

```cpp
struct retrying
{
    using events = nil::xalt::tlist<success, failed>;

    static auto on_event(const success&)
    {
        return nil::sm::Transit<idle>{};
    }

    static auto on_event(const failed&)
    {
        return nil::sm::Defer{};
    }
};
```

`Unhandled` is produced by the library when no handler matches. User code
should not return it directly.

### Returning more than one action

One method must have one return type. If different branches return different
actions, declare a `std::variant` and use a trailing return type:

```cpp
struct waiting
{
    using events = nil::xalt::tlist<start>;

    auto on_event(const start& event)
        -> std::variant<nil::sm::Transit<running>, nil::sm::Discard>
    {
        if (can_start(event))
            return nil::sm::Transit<running>{};

        return nil::sm::Discard{};
    }
};
```

This rule also applies to `on_capture()`, `on_enter()`, `on_exit()`, and
`on_regions_finalized()`. A hook that always returns one action can return
that action directly. A hook with multiple possible actions must list all of
them in a `std::variant`.

### Choosing an action

- Use `Discard` when the state owns the event.
- Use `Forward` when a parent should decide what to do.
- Use `Transit` when the current mode changes.
- Use `Emit` for a follow-up event generated during handling.
- Use `Defer` only when a later state should receive the event.
- Use `Terminate` when the region is finished.

## 4. Hierarchy and Regions

A state can contain child regions:

```cpp
struct worker
{
    using events = nil::xalt::tlist<work>;

    static auto on_event(const work&)
    {
        return nil::sm::Discard{};
    }
};

struct application
{
    using regions = nil::xalt::tlist<worker>;
};
```

Child regions receive an event before their parent. If a child returns
`Forward` or has no matching handler, the parent gets a chance to handle it.

Multiple region types in the same list are active at the same time:

```cpp
struct network {};
struct user_interface {};

struct application
{
    using regions = nil::xalt::tlist<network, user_interface>;
};
```

These are called orthogonal regions. Each region processes the event in
declaration order.

## 5. Capturing Important Events

A parent can intercept an event before its children see it:

```cpp
struct emergency_stop {};

struct safety_controller
{
    using captures = nil::xalt::tlist<emergency_stop>;
    using regions = nil::xalt::tlist<worker>;

    static auto on_capture(const emergency_stop&)
    {
        return nil::sm::Terminate{};
    }
};
```

Use captures for events such as emergency stop or shutdown. If `on_capture`
returns `Forward`, normal child dispatch continues. Any other action handles
the event immediately.

## 6. Lifecycle Hooks

Lifecycle hooks are optional methods on a state:

```cpp
struct connected
{
    auto on_enter() -> nil::sm::NOOP
    {
        open_socket();
        return {};
    }

    auto on_exit() -> nil::sm::NOOP
    {
        close_socket();
        return {};
    }
};
```

- `on_enter()` runs when the state is created.
- `on_exit()` runs when the state is destroyed.
- `on_regions_finalized()` runs when all direct child regions terminate.

Hooks may return `NOOP` or `Emit<E>`. `on_regions_finalized()` may also return
`Transit<T>` or `Terminate`. If a hook can return more than one of these,
declare a `std::variant` trailing return type, for example:

```cpp
auto on_enter()
    -> std::variant<nil::sm::NOOP, nil::sm::Emit<ready>>
{
    if (ready_now())
        return nil::sm::Emit<ready>{};

    return nil::sm::NOOP{};
}
```
Prefer RAII for resources that must always be released.

## 7. Event Order

For one `post(event)` call, the important order is:

1. Captures are checked.
2. Child regions process the event.
3. The parent may process it if it was not consumed.
4. Transitions and terminations are applied.
5. Deferred events are replayed after a transition.
6. Emitted events are delivered in FIFO order.

An emitted event sees the state configuration after the transition. Deferred
events are stored per region and are discarded if that region terminates.

## 8. Compile-Time Checks

The library checks these rules while compiling:

- Every event in `events` has a valid handler.
- Handler return types are valid actions.
- A transition target belongs to the machine's reachable state graph.
- Lifecycle hook return types are valid.

For example, this is an error because `not_part_of_machine` is not reachable:

```cpp
struct root
{
    using regions = nil::xalt::tlist<idle>;

    static auto on_event(const reset&)
    {
        return nil::sm::Transit<not_part_of_machine>{};
    }
};
```

The compiler catches the mistake before the program runs.

## 9. Contexts

The built-in API can pass a context pointer to state constructors:

```cpp
struct AppContext
{
    int user_id;
};

template <typename State>
using AppAPI = nil::sm::default_api<State, AppContext>;

struct logged_in
{
    template <typename Parent>
    logged_in(Parent*, AppContext* context)
        : user_id(context->user_id) {}

    int user_id;
};

AppContext context{42};
nil::sm::SM<AppAPI, logged_in> machine{&context, nullptr};
```

The machine stores a pointer; it does not own or copy `context`. Keep the
context alive until after the machine is destroyed.

## 10. Where to Go Next

- Read [Extensibility](02_EXTENSIBILITY.md) to add logging, timers, or custom allocation.
- Read [Patterns](03_PATTERNS.md) for common state machine designs.
- Browse [Use Cases](04_USE_CASES.md) for domain examples.
- Read [Advanced](05_ADVANCED.md) only when you need to write a custom API.
