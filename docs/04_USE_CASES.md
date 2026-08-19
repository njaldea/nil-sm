# Small Use Cases

These examples show where a state machine fits. They intentionally leave
business code such as `send_request()` out of the examples.

Read the [Guide](01_GUIDE.md) first if `Transit` or `regions` are new to you.

## Network Connection

A protocol can make invalid steps obvious:

```cpp
struct connect {};
struct connected_event {};
struct disconnect {};

struct disconnected
{
    using events = nil::xalt::tlist<connect>;
    auto on_event(const connect&)
    {
        start_connection();
        return nil::sm::Transit<connecting>{};
    }
};

struct connecting
{
    using events = nil::xalt::tlist<connected_event>;
    auto on_event(const connected_event&)
    {
        return nil::sm::Transit<connected>{};
    }
};

struct connected
{
    using events = nil::xalt::tlist<disconnect>;
    auto on_event(const disconnect&)
    {
        return nil::sm::Transit<disconnected>{};
    }
};
```

Data events do not need to be accepted until the machine reaches
`connected`.

## User Interface

A screen flow can be modeled with one region:

```cpp
struct open_settings {};
struct close_settings {};

struct home
{
    using events = nil::xalt::tlist<open_settings>;
    auto on_event(const open_settings&)
    {
        return nil::sm::Transit<settings>{};
    }
};

struct settings
{
    using events = nil::xalt::tlist<close_settings>;
    auto on_event(const close_settings&)
    {
        return nil::sm::Transit<home>{};
    }
};
```

The screen state owns the navigation rules instead of spreading them across
button callbacks.

## Game Character

A character can keep data in a state and change behavior by transitioning:

```cpp
struct jump {};
struct land {};

struct idle
{
    using events = nil::xalt::tlist<jump>;
    auto on_event(const jump&) { return nil::sm::Transit<airborne>{}; }
};

struct airborne
{
    using events = nil::xalt::tlist<land>;
    auto on_event(const land&) { return nil::sm::Transit<idle>{}; }
};
```

Add health or animation data as members when that data belongs to the current
state.

## Request and Response

A request handler can make the waiting state explicit:

```cpp
struct request {};
struct response {};
struct timeout {};

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
    auto on_event(const timeout&) { handle_timeout(); return nil::sm::Transit<idle>{}; }
};
```

A timer service can post `timeout`; see
[Extensibility](02_EXTENSIBILITY.md).

## Parallel Work

Use regions when independent parts of a system are active together:

```cpp
struct network_worker {};
struct audio_worker {};

struct video_call
{
    using regions = nil::xalt::tlist<network_worker, audio_worker>;
};
```

Both regions are active. They can respond to the same event independently.

## Embedded Power States

Lifecycle hooks are useful when state changes control hardware:

```cpp
struct sleep
{
    auto on_enter()
    {
        set_low_power_mode();
        return nil::sm::NOOP{};
    }

    auto on_exit()
    {
        restore_power();
        return nil::sm::NOOP{};
    }
};
```

The state machine describes the mode; the hardware functions remain in the
application.

## Parser Stages

A parser can make its stages explicit:

```cpp
struct source_ready
{
    using events = nil::xalt::tlist<lex>;
    auto on_event(const lex&) { return nil::sm::Transit<tokens_ready>{}; }
};

struct tokens_ready
{
    using events = nil::xalt::tlist<parse>;
    auto on_event(const parse&) { return nil::sm::Transit<ast_ready>{}; }
};
```

Events for later stages are not handled until parsing reaches those stages.

## Safety Stop

Capture a critical event before a child worker sees it:

```cpp
struct stop {};

struct controller
{
    using captures = nil::xalt::tlist<stop>;
    using regions = nil::xalt::tlist<worker>;

    auto on_capture(const stop&)
    {
        return nil::sm::Terminate{};
    }
};
```

This is useful for emergency stop, shutdown, and cancellation paths.

## Choosing a Model

Use a state machine when:

- The object has a small set of meaningful modes.
- The valid events depend on the current mode.
- Transitions and lifecycle behavior matter.
- You want invalid transitions checked during compilation.

Use a normal function or class when there is no meaningful stateful flow. A
state machine is a modeling tool, not a requirement for every decision.

For more design ideas, see [Patterns](03_PATTERNS.md). For custom
services, see [Extensibility](02_EXTENSIBILITY.md).
