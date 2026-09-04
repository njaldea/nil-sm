# Extensibility

Start with the built-in API. Use a custom API when states need application
services or when you want to observe construction and lifecycle calls.

## Contexts

Context types are object types. `SM` is non-owning and receives their addresses.
Use `void` when a context is not needed.

```cpp
struct AppContext
{
    int user_id;
};

struct logged_in
{
    explicit logged_in(auto*, AppContext* context)
        : user_id(context->user_id) {}

    int user_id;
};

AppContext context{42};
nil::sm::SM<nil::sm::api::Default<AppContext>::template type, logged_in> machine{
    &context,
    nullptr
};
```

Keep both context objects alive until after the machine is destroyed. The
machine does not copy or delete them.

## Custom hooks

An API is a template that supplies `API<State>`. The easiest way to customize
one hook is to use `Coalesce` and delegate everything else to the default API:

```cpp
struct Observer
{
    void entered(const void* state_id);
};

template <typename State>
struct LoggingAPI
{
    using api_context_t = Observer;

    static auto on_enter(State& state, Observer* observer)
    {
        if constexpr (!std::is_same_v<State, nil::sm::Fin>)
            observer->entered(nil::xalt::type_id<State>);
        return nil::sm::api::Default<void, Observer>::type<State>::on_enter(
            state,
            observer
        );
    }
};

Observer observer;
nil::sm::CoalescedSM<LoggingAPI, logged_in> machine{nullptr, &observer};
```

Use `Default` directly when only context types are needed. Use `Coalesce` when
only selected hooks are overridden. The complete hook contract is in
[Advanced API](05_ADVANCED.md).

## Timers and threads

Timers are application services. Schedule a callback that posts a typed timeout
event, and cancel it before the state is destroyed.

The state machine is synchronous and not thread-safe. A common integration is:

1. Other threads enqueue application events.
2. One owner thread drains that queue.
3. Only that thread calls `post()`.

## Allocation and other services

Custom APIs can replace state construction or add logging, profiling, timers,
and allocators. Keep those changes small and delegate to `api::Default` for
behavior you do not need to change.

Continue to [Advanced API](05_ADVANCED.md) for signatures and coalescing rules.
