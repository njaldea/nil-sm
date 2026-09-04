# Common Patterns

Short designs to adapt after reading the [Guide](01_GUIDE.md).

## Guard a transition

Return a variant when a condition decides between actions:

```cpp
auto on_event(const start& event)
    -> std::variant<nil::sm::TransitTo<running>, nil::sm::Discard>
{
    if (ready && can_start(event))
        return nil::sm::TransitTo<running>{};
    return nil::sm::Discard{};
}
```

## Defer until ready

```cpp
struct starting
{
    using events = nil::xalt::tlist<data, ready>;

    auto on_event(const data&) { return nil::sm::Defer{}; }
    auto on_event(const ready&) { return nil::sm::TransitTo<running>{}; }
};
```

The deferred event is replayed after the transition. It is discarded if the
region terminates instead.

Use `DeferTo<T>{}` when the event should be saved while transitioning:

```cpp
auto on_event(const data&) { return nil::sm::DeferTo<ready>{}; }
```

## Parent and child

A child can handle a local event or return `Forward`:

```cpp
struct child
{
    using events = nil::xalt::tlist<cancel>;
    auto on_event(const cancel&) { return nil::sm::Forward{}; }
};

struct screen
{
    using regions = nil::xalt::tlist<child>;
    using events = nil::xalt::tlist<cancel>;
    auto on_event(const cancel&) { return nil::sm::TransitTo<home>{}; }
};
```

## Communicate between regions

Emit a typed event when one active region should notify another:

```cpp
struct refresh {};

struct loader
{
    using events = nil::xalt::tlist<load>;
    auto on_event(const load&) { return nil::sm::Emit<refresh>{}; }
};

struct view
{
    using events = nil::xalt::tlist<refresh>;
    auto on_event(const refresh&) { return nil::sm::Discard{}; }
};

struct app
{
    using regions = nil::xalt::tlist<loader, view>;
};
```

## Acquire and release resources

Use `on_enter()` and `on_exit()` for state-scoped setup. Prefer RAII for
resources that must always be released.

```cpp
struct connected
{
    auto on_enter() -> nil::sm::NOOP { open_connection(); return {}; }
    auto on_exit() -> nil::sm::NOOP { close_connection(); return {}; }
};
```

## Capture an important event

Captures run before child dispatch:

```cpp
struct controller
{
    using captures = nil::xalt::tlist<shutdown>;
    using regions = nil::xalt::tlist<worker>;

    auto on_capture(const shutdown&) { return nil::sm::Terminate{}; }
};
```

## Request and response

Represent the waiting period explicitly:

```cpp
struct idle
{
    using events = nil::xalt::tlist<request>;
    auto on_event(const request& value)
    {
        send(value);
        return nil::sm::TransitTo<waiting>{};
    }
};

struct waiting
{
    using events = nil::xalt::tlist<response, timeout>;
    auto on_event(const response&) { return nil::sm::TransitTo<idle>{}; }
    auto on_event(const timeout&) { return nil::sm::TransitTo<idle>{}; }
};
```

## Practical rules

- Keep each state focused.
- Use `Forward` for parent-owned decisions.
- Use `Emit` for typed follow-up events.
- Keep emitted-event chains finite.
- Add external synchronization before cross-thread posting.
