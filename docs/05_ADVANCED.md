# Advanced API

This page is for custom API authors. Start with [Extensibility](02_EXTENSIBILITY.md)
for normal application integration.

## API contract

`SM<API, Root>` instantiates `API<State>` for every reachable state. A complete
API template supplies the following type aliases and static methods.

### Type aliases

| Alias | Description | Default / Typical definition |
| --- | --- | --- |
| `api_context_t` | Context type passed to hooks and factory (`void` if none). | `void` |
| `regions_t` | Initial child regions/states owned by `State`. | `nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>` |
| `events_t` | Events handled directly by `State`. | `nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>` |
| `captures_t` | Events intercepted before child regions process them. | `nil::xalt::coalesce_t<State, nil::sm::detail::captures_tag>` |
| `args_t` | Dependency types required to construct `State`. | `nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>` |
| `props_t` | Properties exposed by `State` to descendant states. | `nil::xalt::coalesce_t<State, nil::sm::detail::props_tag>` |

### Static methods

| Method | Signature | Description |
| --- | --- | --- |
| `make` | `template <typename... Args>`<br>`static State make(api_context_t*, const Metadata&, Args*...)` | Factory function constructing the state instance. |
| `on_event` | `template <typename E>`<br>`static auto on_event(State&, const E&, api_context_t*)` | Handles standard event dispatch on the state. |
| `on_capture` | `template <typename E>`<br>`static auto on_capture(State&, const E&, api_context_t*)` | Intercepts events before child regions receive them. |
| `on_enter` | `static auto on_enter(State&, api_context_t*)` | Lifecycle hook called when entering the state. |
| `on_exit` | `static auto on_exit(State&, api_context_t*)` | Lifecycle hook called when exiting the state. |
| `on_regions_finalized` | `static auto on_regions_finalized(State&, api_context_t*)` | Lifecycle hook called when all child regions complete. |

### Complete API definition

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

    template <typename... Args>
    static State make(api_context_t*, const nil::sm::Metadata&, Args*... args)
    {
        return State(args...);
    }

    template <typename E>
    static auto on_event(State& s, const E& e, api_context_t*)
    {
        return s.on_event(e);
    }

    template <typename E>
    static auto on_capture(State& s, const E& e, api_context_t*)
    {
        return s.on_capture(e);
    }

    static auto on_enter(State& s, api_context_t*)
    {
        return s.on_enter();
    }

    static auto on_exit(State& s, api_context_t*)
    {
        return s.on_exit();
    }

    static auto on_regions_finalized(State& s, api_context_t*)
    {
        return s.on_regions_finalized();
    }
};
```

State construction arguments come from `State::args`. Context objects are passed
by pointer and are never owned by the machine.

## Author responsibilities

When implementing a custom API from scratch (without `api::Coalesce`), the author is responsible for:

### 1. Return type validity

Hook and dispatch methods must return types satisfying the runtime concepts (defined in `<nil/sm/concepts.hpp>`):

| Hook / Dispatch method | Permitted return types (or `std::variant` of them) | Validated by concept | Details |
| --- | --- | --- | --- |
| `on_event`<br>`on_capture` | `TransitTo<T>`, `Terminate`, `Forward`, `Discard`, `Defer`, `DeferTo<T>`, `Emit<E>` | `concepts::match::action`<br>`concepts::has_valid_on_event`<br>`concepts::has_valid_on_capture` | See [Core Guide: Actions](01_GUIDE.md#2-actions) |
| `on_enter`<br>`on_exit` | `NOOP`, `Emit<E>`, or `Unhandled` (when absent) | `concepts::match::hook`<br>`concepts::has_valid_on_enter`<br>`concepts::has_valid_on_exit` | See [Core Guide: Lifecycle](01_GUIDE.md#5-lifecycle) |
| `on_regions_finalized` | `NOOP`, `Terminate`, `TransitTo<T>`, `Emit<E>`, or `Unhandled` (when absent) | `concepts::match::finalized`<br>`concepts::has_valid_on_regions_finalized` | See [Core Guide: Lifecycle](01_GUIDE.md#5-lifecycle) |

### 2. Handling absent optional hooks

Optional lifecycle methods (`on_enter`, `on_exit`, `on_regions_finalized`) may not be defined on every state. Custom APIs must detect their presence (e.g. via `concepts::has_on_enter<State>`) and return `nil::sm::Unhandled{}` when absent.

### 3. Borrow semantics and lifetime

Context objects (`api_context_t*`) and state construction dependencies ([Extensibility: Contexts and state arguments](02_EXTENSIBILITY.md#contexts-and-state-arguments)) are borrowed by pointer. The caller/host must guarantee their lifetime exceeds the machine's lifetime.

## Coalescing partial APIs

`api::Coalesce<PartialAPI>` fills missing aliases and hooks from `api::Default`.
Authors need only define the aliases or hooks they wish to customize:

```cpp
template <typename State>
struct LoggingAPI
{
    template <typename E>
    static auto on_event(State& s, const E& e, void*)
    {
        std::cout << "Handling event\n";
        return s.on_event(e);
    }
};

using MySM = nil::sm::SM<nil::sm::api::Coalesce<LoggingAPI>, RootState>;
```

When `api_context_t` is not `void`, the context pointer is passed as the first
machine constructor argument, followed by root `args`.

## Metadata

`Metadata` passed to `make()` contains runtime and structural inspection data:

- `index`: State index in the flattened state table.
- `region`: Region index within the parent state.
- `depth`: Nesting depth from root (`0` for root).
- `name`: Demangled or reflective name string.
- `is_final`: Whether the state terminates its region.
- `is_barrier`: Whether the state represents a barrier boundary.
- `parent`: Pointer to parent state metadata (`nullptr` at root).

## Explicit reachability

By default, reachable states are discovered from child regions and transition targets.
A specialization of `nil::sm::siblings<InitialState>` replaces discovery with an
explicit ordered list:

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
list controls compile-time state indices; all transition targets must be present.
