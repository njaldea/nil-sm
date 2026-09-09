# Extensibility

Use a custom API when states need application services, or to observe
construction and lifecycle calls.

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
    using args = nil::xalt::tlist<AppContext>;

    explicit logged_in(AppContext* context)
        : user_id(context->user_id) {}

    int user_id;
};

AppContext context{42};
nil::sm::SM<nil::sm::api::Default<>::template type, logged_in, AppContext> machine{&context};
```

Keep both context objects alive until after the machine is destroyed. The
machine does not copy or delete them.

## Custom hooks

An API is a template supplying `API<State>`. `Coalesce` fills in everything else
from the default API when only one hook needs to change:

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
        return nil::sm::api::Default<Observer>::template type<State>::on_enter(
            state,
            observer
        );
    }
};

Observer observer;
nil::sm::CoalescedSM<LoggingAPI, logged_in> machine{&observer};
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

Custom APIs can replace construction or add logging, profiling, timers, and
allocators — keep changes small and delegate the rest to `api::Default`.
See [Advanced API](05_ADVANCED.md) for signatures and coalescing rules.
