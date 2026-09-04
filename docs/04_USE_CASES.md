# Small Use Cases

These sketches show when a state machine is useful. Application operations are
left as placeholders.

## Connection protocol

```cpp
struct disconnected
{
    using events = nil::xalt::tlist<connect>;
    auto on_event(const connect&)
    {
        start_connection();
        return nil::sm::TransitTo<connecting>{};
    }
};

struct connecting
{
    using events = nil::xalt::tlist<connected_event>;
    auto on_event(const connected_event&)
    {
        return nil::sm::TransitTo<connected>{};
    }
};
```

Events are accepted only in the states that declare them.

## User interface

```cpp
struct home
{
    using events = nil::xalt::tlist<open_settings>;
    auto on_event(const open_settings&)
    {
        return nil::sm::TransitTo<settings>{};
    }
};

struct settings
{
    using events = nil::xalt::tlist<close_settings>;
    auto on_event(const close_settings&)
    {
        return nil::sm::TransitTo<home>{};
    }
};
```

## Request and response

A request can move the machine into a state that accepts only a response or
timeout:

```cpp
struct waiting
{
    using events = nil::xalt::tlist<response, timeout>;

    auto on_event(const response&) { return nil::sm::TransitTo<idle>{}; }
    auto on_event(const timeout&) { handle_timeout(); return nil::sm::TransitTo<idle>{}; }
};
```

## Parallel work

Use orthogonal regions when independent activities should be active together:

```cpp
struct video_call
{
    using regions = nil::xalt::tlist<network_worker, audio_worker>;
};
```

Each region receives events in declaration order.

## Power states

Lifecycle hooks fit state-scoped hardware changes:

```cpp
struct sleep
{
    auto on_enter() { set_low_power_mode(); return nil::sm::NOOP{}; }
    auto on_exit() { restore_power(); return nil::sm::NOOP{}; }
};
```

## Safety stop

Capture a critical event before a worker sees it:

```cpp
struct controller
{
    using captures = nil::xalt::tlist<stop>;
    using regions = nil::xalt::tlist<worker>;

    auto on_capture(const stop&) { return nil::sm::Terminate{}; }
};
```

## Choosing a model

Use a state machine when valid events and behavior depend on the current mode,
and transitions or lifecycle actions matter. Use an ordinary function or class
when there is no meaningful stateful flow.

See [Patterns](03_PATTERNS.md) for reusable designs and
[Extensibility](02_EXTENSIBILITY.md) for application services.
