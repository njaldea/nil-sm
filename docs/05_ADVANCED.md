# Advanced API

This page is a reference for custom API authors. Most applications only need
[Extensibility](02_EXTENSIBILITY.md).

## API shape

`SM<API, Root>` instantiates `API<State>` for every reachable state. It must
provide these aliases:

```cpp
template <typename State>
struct MyAPI
{
    using state_t = State;
    using api_context_t = void;
    using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
    using events_t = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;
    using captures_t = nil::xalt::coalesce_t<State, nil::sm::detail::captures_tag>;
    using args_t = nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>;
    using provides_t = nil::xalt::coalesce_t<State, nil::sm::detail::provides_tag>;
};
```

The built-in API supplies all behavior. A custom API may provide:

```cpp
template <typename... Args>
static state_t make(api_context_t*, const nil::sm::Metadata&, Args*...);

template <typename E>
static auto on_event(state_t&, const E&, api_context_t*);
template <typename E>
static auto on_capture(state_t&, const E&, api_context_t*);
static auto on_enter(state_t&, api_context_t*);
static auto on_exit(state_t&, api_context_t*);
static auto on_regions_finalized(state_t&, api_context_t*);
```

`api::Default` constructs as `State(args...)`. Event and capture hooks may return `Discard`, `Forward`,
`Defer`, `DeferTo<T>`, `TransitTo<T>`, `Emit<E>`, or a variant of valid actions.
Lifecycle hooks have a smaller set of valid actions. See the [Guide](01_GUIDE.md)
for the behavior of each action.

## Context ownership

`api_context_t` describes the API context object. Constructor args are declared per
state via `using args = nil::xalt::tlist<...>;` and are resolved from SM root args:

```cpp
AppContext app;
Observer observer;
nil::sm::SM<MyAPI, Root, AppContext> machine{&observer, &app};
```

`SM` does not own, copy, or destroy API context or root-arg objects.

## Coalesce

`Coalesce<PartialAPI>` fills missing aliases and hooks from `api::Default`:

```cpp
template <typename State>
struct PartialAPI
{
    using api_context_t = Observer;

    static auto on_enter(State& state, Observer* observer)
    {
        observer->entered(nil::xalt::type_id<State>);
        return nil::sm::api::Default<Observer>::template type<State>::on_enter(
            state,
            observer
        );
    }
};

nil::sm::CoalescedSM<PartialAPI, Root> machine{&observer};
```

A partial API should delegate to `api::Default` when it still wants normal
state behavior.

## Runtime metadata

The `Metadata` argument to `make` identifies the state’s region, depth, name,
and parent metadata. It is useful for logging and tracing. Metadata parent
pointers are valid while the state tree is alive.

## Keep it small

Prefer a custom adapter or hook over replacing the whole runtime. The state
machine is synchronous and single-threaded by design; external queues and
locks belong at the application boundary.

`post(detail::Event)` is a low-level escape hatch. Use typed `post<E>()` unless
you already own a valid erased event payload and matching id/deleter/cloner.
