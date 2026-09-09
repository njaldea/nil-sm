# Core Guide

This page explains the runtime model. For contexts and custom hooks, see
[Extensibility](02_EXTENSIBILITY.md).

## 1. States and events

A state is an ordinary C++ type. It lists the events it handles:

```cpp
struct start {};
struct running;

struct stopped
{
    using events = nil::xalt::tlist<start>;

    static auto on_event(const start&)
    {
        return nil::sm::TransitTo<running>{};
    }
};
```

Start the machine with `DefaultSM`:

```cpp
nil::sm::DefaultSM<stopped> machine;
machine.post(start{});
```

## 2. Actions

| Action | Effect |
| --- | --- |
| `Discard{}` | Consume the event. |
| `Forward{}` | Let the parent handle the event. |
| `TransitTo<T>{}` | Replace the current state with `T`. |
| `Terminate{}` | Finish the current region. |
| `Defer{}` | Save the event until this region transitions. |
| `DeferTo<T>{}` | Save the event and transition to `T`. |
| `Emit<E>{}` | Queue a typed follow-up event. |

Use `std::variant` when a handler has multiple possible actions:

```cpp
auto on_event(const start& event)
    -> std::variant<nil::sm::TransitTo<running>, nil::sm::Discard>
{
    if (can_start(event))
        return nil::sm::TransitTo<running>{};
    return nil::sm::Discard{};
}
```

`Unhandled` is an internal result. User handlers do not return it.

## 3. Regions

A state can contain one or more child regions:

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

One region is hierarchical. Multiple regions are orthogonal and process an
event in declaration order.

## 4. Dispatch

For one `post(event)` call:

1. The current state checks captures.
2. Child regions receive the event.
3. The state handles it if it was not consumed.
4. Region actions are applied.
5. Deferred events are replayed after a transition.
6. Emitted events are drained from the queue.

A capture runs before child regions:

```cpp
struct shutdown {};

struct controller
{
    using captures = nil::xalt::tlist<shutdown>;
    using regions = nil::xalt::tlist<worker>;

    static auto on_capture(const shutdown&)
    {
        return nil::sm::Terminate{};
    }
};
```

A capture returning `Forward` continues into child regions. An event handler
returning `Forward` bubbles to the parent.

## 5. Lifecycle

Optional hooks are called around state lifetime:

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

`on_regions_finalized()` runs after all direct child regions terminate. It may
return `NOOP`, `Emit`, `TransitTo`, or `Terminate`.

## Compile-time checks

The compiler checks handler return types and reachable transition targets. A
`TransitTo<T>` target must belong to the machine's reachable graph.

Continue with [Patterns](03_PATTERNS.md), or go to
[Extensibility](02_EXTENSIBILITY.md).
