# Advanced API

This page is for custom API authors. Start with [Extensibility](02_EXTENSIBILITY.md)
for normal application integration.

## API contract

`SM<API, Root>` instantiates `API<State>` for every reachable state. A complete
API supplies these aliases:

```cpp
template <typename State>
struct MyAPI
{
    using api_context_t = void;
    using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
    using events_t = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;
    using captures_t = nil::xalt::coalesce_t<State, nil::sm::detail::captures_tag>;
    using args_t = nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>;
    using props_t = nil::xalt::coalesce_t<State, nil::sm::detail::props_tag>;
};
```

The default API supplies `make`, event/capture hooks, and lifecycle hooks. A
custom API may override any of them:

```cpp
template <typename... Args>
static State make(api_context_t*, const nil::sm::Metadata&, Args*...);

template <typename E>
static auto on_event(State&, const E&, api_context_t*);
template <typename E>
static auto on_capture(State&, const E&, api_context_t*);
static auto on_enter(State&, api_context_t*);
static auto on_exit(State&, api_context_t*);
static auto on_regions_finalized(State&, api_context_t*);
```

State construction arguments come from `State::args`. Context objects are passed
by pointer and are never owned by the machine.

## Coalesce

`api::Coalesce<PartialAPI>` fills missing aliases and hooks from `api::Default`.
A partial API should delegate to the default hook when it adds observation but
still wants ordinary state behavior.

The API context is the first machine constructor argument when it is not `void`;
root construction arguments follow it.

## Explicit reachability

By default, reachable states come from child regions and transition targets. A
specialization of `nil::sm::siblings<InitialState>` replaces that discovery with
an explicit ordered list:

```cpp
namespace nil::sm
{
    template <>
    struct siblings<screen_a>
    {
        using type = nil::xalt::tlist<screen_b, screen_c>;
    };
}
```

The specialization must be visible before machine instantiation. The resulting
list controls compile-time state indices; transition targets must be present.
This is intended for generated or externally described graphs.

## Metadata

`Metadata` passed to `make()` contains the state index, region, depth, name,
final flag, barrier flag, and parent metadata. Parent pointers are valid while
the state tree exists.

Prefer small adapters and hooks over replacing the runtime. The machine remains
synchronous and single-threaded by design.
