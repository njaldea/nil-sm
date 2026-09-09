# Core Guide

The shortest path to a working state machine. See [Extensibility](02_EXTENSIBILITY.md)
for contexts and custom hooks.

## States and events

States and events are ordinary C++ types. A state lists the events it handles.

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

## Start a machine

```cpp
nil::sm::DefaultSM<stopped> machine;
machine.post(start{});
```

The default machine has no context; see [Extensibility](02_EXTENSIBILITY.md) for
context-enabled construction.

## Actions

| Action | Meaning |
| --- | --- |
| `Discard{}` | Consume the event. |
| `Forward{}` | Let the parent handle the event. |
| `TransitTo<T>{}` | Replace the current state with `T`. |
| `DeferTo<T>{}` | Defer the event, then replace the current state with `T`. |
| `Terminate{}` | Finish the current region. |
| `Defer{}` | Save the event until this region transitions. |
| `Emit<E>{}` | Queue a typed follow-up event. |

One handler has one return type. Use `std::variant` when branches return
different actions:

```cpp
auto on_event(const start& event)
    -> std::variant<nil::sm::TransitTo<running>, nil::sm::Discard>
{
    if (can_start(event))
        return nil::sm::TransitTo<running>{};
    return nil::sm::Discard{};
}
```

`Unhandled` is produced by the library; user handlers should not return it.

`Defer{}` keeps the current state and saves the event until this region
transitions. `DeferTo<T>{}` combines deferral with a transition: the event is
saved for the new state. Use `TransitTo<T>{}` when the event should not be
replayed.

```cpp
struct starting
{
    using events = nil::xalt::tlist<data, ready>;

    static auto on_event(const data&) { return nil::sm::Defer{}; }
    static auto on_event(const ready&)
    {
        return nil::sm::DeferTo<running>{};
    }
};
```

Deferred events are replayed after the transition and are kept per region. If
the region terminates, its deferred events are discarded.

## Dispatch order

For one `post(event)` call:

1. Captures are checked.
2. Child regions process the event.
3. The parent may process it if it was not consumed.
4. Transitions and terminations are applied.
5. Deferred events are replayed after a transition.
6. Emitted events are delivered from the queue.

The sections below build on this order: captures run first, child regions
before their parent, and lifecycle hooks around transitions.

## Child regions

A state can contain child regions:

```cpp
struct worker
{
    using events = nil::xalt::tlist<work>;
    static auto on_event(const work&) { return nil::sm::Discard{}; }
};

struct application
{
    using regions = nil::xalt::tlist<worker>;
};
```

`Forward` asks the parent
to handle it. Multiple entries in `regions` create orthogonal regions; each is
active and processes events in declaration order.

## Captures

Use `captures` for events that must run before the child regions see them:

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

Returning `Forward` from a capture continues normal child dispatch. Other
actions handle the event at the capturing state.

## Lifecycle

Optional hooks run with state lifetime:

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

`on_regions_finalized()` runs after all direct child regions terminate. It can
return `NOOP`, `Emit`, `TransitTo`, or `Terminate` as appropriate.

## Compile-time checks

The compiler checks event handlers, action types, and reachable transition
targets. A target used by `TransitTo<T>` must belong to the machine's reachable
state graph.

For large regions with a known fixed state set, define
`template <> struct nil::sm::siblings<Initial> { using type = nil::xalt::tlist<...>; };`
to bypass recursive transition-target discovery.

## Next steps

See [Patterns](03_PATTERNS.md) for compact designs, or
[Extensibility](02_EXTENSIBILITY.md) for contexts and custom hooks.
