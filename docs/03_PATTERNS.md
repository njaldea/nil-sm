# Common Patterns

This cookbook shows small patterns you can adapt. Read
[Guide](01_GUIDE.md) first for the basic actions.

## Guard a Transition

Check a condition before changing state. Keep the event in the current state
or forward it when the condition is not satisfied.

```cpp
struct waiting
{
    bool ready = false;
    using events = nil::xalt::tlist<start>;

    auto on_event(const start& event)
        -> std::variant<nil::sm::Transit<running>, nil::sm::Discard>
    {
        if (ready && can_start(event))
            return nil::sm::Transit<running>{};

        return nil::sm::Discard{};
    }
};
```

## Defer Until Ready

Use `Defer` when another state should receive the event later.

```cpp
struct starting
{
    using events = nil::xalt::tlist<data, ready>;

    auto on_event(const data&) { return nil::sm::Defer{}; }
    auto on_event(const ready&) { return nil::sm::Transit<running>{}; }
};

struct running
{
    using events = nil::xalt::tlist<data>;
    auto on_event(const data&) { return nil::sm::Discard{}; }
};
```

Deferred events are replayed after the transition, in FIFO order.

## Acquire and Release a Resource

Use lifecycle hooks for state-specific setup and cleanup.

```cpp
struct connected
{
    auto on_enter()
    {
        open_connection();
        return nil::sm::NOOP{};
    }

    auto on_exit()
    {
        close_connection();
        return nil::sm::NOOP{};
    }
};
```

For exception-safe ownership, prefer a member such as `std::unique_ptr` and
RAII.

## Parent and Child States

Put shared behavior in a parent and let children handle specific events.

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

    auto on_event(const cancel&) { return nil::sm::Transit<home>{}; }
};
```

The child gets the event first. `Forward` lets the parent handle it.

## Communicate Between Regions

Emit a typed event when one region needs to notify another.

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

The emitted event is dispatched through the machine, so every active region
can respond to it.

## Handle an Emergency Event

Capture an important event before child regions can consume it.

```cpp
struct emergency_stop {};

struct machine
{
    using captures = nil::xalt::tlist<emergency_stop>;
    using regions = nil::xalt::tlist<worker>;

    auto on_capture(const emergency_stop&)
    {
        return nil::sm::Terminate{};
    }
};
```

Use captures for shutdown, cancellation, or safety events.

## Run Independent Regions

Use orthogonal regions when parts of the system are active together.

```cpp
struct network;
struct user_interface;

struct application
{
    using regions = nil::xalt::tlist<network, user_interface>;
};
```

Each region handles the same event independently, in declaration order.

## Request and Response

Represent the waiting period as its own state.

```cpp
struct idle
{
    using events = nil::xalt::tlist<request>;
    auto on_event(const request& value)
    {
        send(value);
        return nil::sm::Transit<waiting>{};
    }
};

struct waiting
{
    using events = nil::xalt::tlist<response, timeout>;

    auto on_event(const response&) { return nil::sm::Transit<idle>{}; }
    auto on_event(const timeout&) { return nil::sm::Transit<idle>{}; }
};
```

A timer service can post `timeout`; see
[Extensibility](02_EXTENSIBILITY.md) for integration options.

## Test State Changes

A custom API can observe transitions without changing state code.

```cpp
struct Observer
{
    void entered(const char* name);
    void exited(const char* name);
};

// Pass an Observer through a custom API and record on_enter/on_exit.
```

Keep tests focused on one event and one expected state change. The repository's
test suite contains complete mock API examples.

## Practical Rules

- One state should have one clear responsibility.
- Use `Emit` for follow-up events created during dispatch.
- Use `Forward` when a parent owns the decision.
- Use `Defer` only when a future state is guaranteed to handle the event.
- Keep emitted-event chains finite.
- Add external synchronization before calling `post()` from multiple threads.
