# Extending the State Machine

The core library handles state transitions and event dispatch. Your application
can provide the parts that are specific to it, such as logging, timers, or
memory allocation.

For the core API, read the [Guide](01_GUIDE.md). For the complete template
contract, read [Advanced](05_ADVANCED.md).

## The API Parameter

The main type is:

```cpp
nil::sm::SM<API, Regions...>
```

`API` tells the machine how to construct states and call their hooks. The
library provides a default API, so you only need a custom API when you want to
add application behavior.

## Contexts

A context is an object shared with states. The machine stores a pointer to it;
it does not own the object.

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

Keep `context` alive until after `machine` is destroyed.

## Logging

A custom API can call an observer before delegating to the default behavior.
The important idea is to wrap one hook and leave the rest unchanged.

```cpp
struct Logger
{
    void entered(const char* state_name);
};

template <typename State>
struct LoggingAPI : nil::sm::default_api<State, void, Logger>
{
    using base = nil::sm::default_api<State, void, Logger>;
    using api_context_t = Logger;

    static auto on_enter(State& state, Logger* logger)
    {
        if (logger != nullptr)
            logger->entered(nil::xalt::type_name_v<State>);

        return base::on_enter(state, logger);
    }
};
```

The exact observer type and name formatting are application choices. The
library does not require a logging framework.

## Timers

Timers should produce ordinary events. A state can start a timer in
`on_enter()` and cancel it in `on_exit()`.

```cpp
struct waiting
{
    TimerService* timers;
    TimerId timer = 0;

    auto on_enter()
    {
        timer = timers->schedule(5s, [this] { post(timeout{}); });
        return nil::sm::NOOP{};
    }

    auto on_exit()
    {
        timers->cancel(timer);
        return nil::sm::NOOP{};
    }
};
```

The callback mechanism depends on your timer service. Make sure the callback
cannot use the state after `on_exit()` has run; cancel the timer before the
state is destroyed.

## Memory Pools

Override the API's state-construction hook and allocate states from your pool.
This is an advanced optimization; start with the default heap allocation.

```cpp
template <typename State>
struct PooledAPI
{
    // Keep the normal aliases and hooks from the default API.
    // Override make(...) to acquire storage from your pool.
};
```

See [Advanced](05_ADVANCED.md) for the required hook signatures.

## Posting From Other Threads

The state machine itself is single-threaded. A safe application design is:

1. Other threads put events into a thread-safe queue.
2. One owner thread removes events from the queue.
3. Only that owner thread calls `machine.post(event)`.

This keeps the state machine simple while allowing the surrounding application
to use multiple threads.

## Practical Rules

- Start with `DefaultSM` and add a custom API only when needed.
- Delegate to `default_api` instead of rewriting normal behavior.
- Document who owns every context pointer.
- Keep timer callbacks from outliving their states.
- Keep logging and profiling hooks lightweight.
- Treat custom allocation as an optimization, not a starting requirement.
