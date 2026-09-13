# Advanced API

This page is for custom API authors. Start with [Extensibility](02_EXTENSIBILITY.md)
for normal application integration.

## API contract

`SM<API, Root>` instantiates `API::state<State>` for every reachable state. The
policy has two required top-level members: `context_t` and a nested
`template <typename State> struct state`. The older name `api_context_t` is no
longer part of the public contract; use `context_t`.

### Required policy members

| Member | Required form | Purpose |
| --- | --- | --- |
| `context_t` | `using context_t = Context;` (`void` if unused) | Context type passed to hooks and the factory. |
| `state` | `template <typename State> struct state` | Per-state policy implementation, instantiated for every reachable state. |

`state<State>` must provide the following aliases and methods. `api::Coalesce`
can supply omitted members from `api::Default`; a policy written from scratch
must provide them all.

| Member | Required form | Return / role |
| --- | --- | --- |
| `regions_t` | Type alias | Initial child regions or states. |
| `events_t` | Type alias | Events handled directly by `State`. |
| `captures_t` | Type alias | Events intercepted before child regions. |
| `args_t` | Type alias | Dependencies passed to the `State` constructor. |
| `props_t` | Type alias | Properties exposed to descendant states. |
| `make` | `static State make(context_t*, const Metadata&, Args*...)` | Constructs `State`. |
| `on_event` | `template <typename E> static auto on_event(State&, const E&, context_t*)` | Event action. |
| `on_capture` | `template <typename E> static auto on_capture(State&, const E&, context_t*)` | Capture action. |
| `on_enter` | `static auto on_enter(State&, context_t*)` | Entry hook; return `NOOP`, `Emit<E>`, or `Unhandled`. |
| `on_exit` | `static auto on_exit(State&, context_t*)` | Exit hook; return `NOOP`, `Emit<E>`, or `Unhandled`. |
| `on_regions_finalized` | `static auto on_regions_finalized(State&, context_t*)` | Completion hook; return `NOOP`, `Terminate`, `TransitTo<T>`, `Emit<E>`, or `Unhandled`. |

Event and capture methods return the usual action types: `TransitTo<T>`,
`Terminate`, `Forward`, `Discard`, `Defer`, `DeferTo<T>`, or `Emit<E>` (including
permitted `std::variant` combinations). Optional lifecycle hooks may be absent
from a state, but the policy must produce `Unhandled` for an absent hook.

### Complete API definition

```cpp
struct MyAPI
{
    using context_t = void;

    template <typename State>
    struct state
    {
        using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
        using events_t = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;
        using captures_t = nil::xalt::coalesce_t<State, nil::sm::detail::captures_tag>;
        using args_t = nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>;
        using props_t = nil::xalt::coalesce_t<State, nil::sm::detail::props_tag>;

        template <typename... Args>
        static State make(context_t*, const nil::sm::Metadata&, Args*... args)
        {
            return State(args...);
        }

        template <typename E>
        static auto on_event(State& s, const E& e, context_t*)
        {
            return s.on_event(e);
        }

        template <typename E>
        static auto on_capture(State& s, const E& e, context_t*)
        {
            return s.on_capture(e);
        }

        static auto on_enter(State& s, context_t*)
        {
            return s.on_enter();
        }

        static auto on_exit(State& s, context_t*)
        {
            return s.on_exit();
        }

        static auto on_regions_finalized(State& s, context_t*)
        {
            return s.on_regions_finalized();
        }
    };
};
```

Context objects (`context_t*`) and state construction dependencies ([Extensibility:
Contexts and state arguments](02_EXTENSIBILITY.md#contexts-and-state-arguments))
are borrowed by pointer and are never owned by the machine.

## Partial APIs

`api::Coalesce<PartialAPI>` fills missing aliases and hooks from `api::Default`.
Authors need only define the aliases or hooks they wish to customize:

```cpp
struct LoggingAPI
{
    template <typename State>
    struct state
    {
        template <typename E>
        static auto on_event(State& s, const E& e, void*)
        {
            std::cout << "Handling event\n";
            return s.on_event(e);
        }
    };
};

using MySM = nil::sm::SM<nil::sm::api::Coalesce<LoggingAPI>, RootState>;
```

When `context_t` is not `void`, the context pointer is passed as the first
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
