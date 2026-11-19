# Advanced API

This page is for custom API authors. Start with [Extensibility](02_EXTENSIBILITY.md)
for normal application integration.

## API contract

`SM<API, Root>` instantiates `API::state<State>` for every reachable state. The
policy has one required top-level member: a nested
`template <typename State> struct state`. Hooks are static functions and receive
no per-instance context, so state-specific data reaches a state through its
`args` and `props` ([Extensibility](02_EXTENSIBILITY.md)).

### Required policy members

| Member | Required form | Purpose |
| --- | --- | --- |
| `state` | `template <typename State> struct state` | Per-state policy implementation, instantiated for every reachable state. |

`state<State>` must provide the following aliases and methods. `api::Coalesce`
can supply omitted members from `api::Default`; a policy written from scratch
must provide them all.

| Member | Required form | Return / role |
| --- | --- | --- |
| `instance_t` | Type alias | The instance the machine stores for the state: `State`, or a wrapper around it. `make` returns it. |
| `regions_t` | Type alias | Initial child regions or states. |
| `events_t` | Type alias | Events handled directly by `State`. |
| `captures_t` | Type alias | Events intercepted before child regions. |
| `args_t` | Type alias | Dependencies passed to `make`. |
| `props_t` | Type alias | Properties exposed to descendant states. |
| `make` | `static instance_t make(const Metadata&, Args*...)` | Constructs the stored instance. |
| `on_event` | `template <typename E> static auto on_event(instance_t&, const E&)` | Event action. |
| `on_capture` | `template <typename E> static auto on_capture(instance_t&, const E&)` | Capture action. |
| `on_enter` | `static auto on_enter(instance_t&)` | Entry hook; return `NOOP`, `Emit<E>`, or `Unhandled`. |
| `on_exit` | `static auto on_exit(instance_t&)` | Exit hook; return `NOOP`, `Emit<E>`, or `Unhandled`. |
| `on_regions_finalized` | `static auto on_regions_finalized(instance_t&)` | Completion hook; return `NOOP`, `Terminate`, `TransitTo<T>`, `Emit<E>`, or `Unhandled`. |

The machine stores an `instance_t`, and every hook receives it. `instance_t` is usually
`State`; an API can make it a wrapper to carry extra data, see
[Wrapping the state](#wrapping-the-state). `api::Coalesce` takes `instance_t` from the
partial API, falling back to `State`, so a partial API whose `make` returns a wrapper must
declare it as `instance_t`.

Event and capture methods return the usual action types: `TransitTo<T>`,
`Terminate`, `Forward`, `Discard`, `Defer`, `DeferTo<T>`, or `Emit<E>` (including
permitted `std::variant` combinations). Optional lifecycle hooks may be absent
from a state, but the policy must produce `Unhandled` for an absent hook.

### Complete API definition

```cpp
struct MyAPI
{
    template <typename State>
    struct state
    {
        using instance_t = State;
        using regions_t = nil::xalt::coalesce_t<State, nil::sm::detail::regions_tag>;
        using events_t = nil::xalt::coalesce_t<State, nil::sm::detail::events_tag>;
        using captures_t = nil::xalt::coalesce_t<State, nil::sm::detail::captures_tag>;
        using args_t = nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>;
        using props_t = nil::xalt::coalesce_t<State, nil::sm::detail::props_tag>;

        template <typename... Args>
        static instance_t make(const nil::sm::Metadata&, Args*... args)
        {
            return State(args...);
        }

        template <typename E>
        static auto on_event(State& s, const E& e)
        {
            return s.on_event(e);
        }

        template <typename E>
        static auto on_capture(State& s, const E& e)
        {
            return s.on_capture(e);
        }

        static auto on_enter(State& s)
        {
            return s.on_enter();
        }

        static auto on_exit(State& s)
        {
            return s.on_exit();
        }

        static auto on_regions_finalized(State& s)
        {
            return s.on_regions_finalized();
        }
    };
};
```

State construction dependencies ([Extensibility:
Contexts and state arguments](02_EXTENSIBILITY.md#contexts-and-state-arguments))
are borrowed by pointer and are never owned by the machine.

## Partial APIs

`api::Coalesce<PartialAPI>` fills missing aliases and hooks from `api::Default`.
Aliases the partial API declares (`instance_t`, `args_t`, ...) take precedence, so a
partial API can add an `args_t` entry and wrap the state to carry what the entry
resolves to. Authors need only define the aliases or hooks they wish to customize:

```cpp
struct LoggingAPI
{
    template <typename State>
    struct state
    {
        template <typename E>
        static auto on_event(State& s, const E& e)
        {
            std::cout << "Handling event\n";
            return s.on_event(e);
        }
    };
};

using MySM = nil::sm::SM<nil::sm::api::Coalesce<LoggingAPI>, RootState>;
```

## Wrapping the state

By default the machine stores the state type itself: `instance_t` is `State`. An API can
make `instance_t` a **wrapper** around the state to attach extra data to every state
instance, such as a logger, a tracer, a metrics sink, or a test mock.

### Why it exists

Hooks are static functions and receive no per-instance context, so an API has nowhere to
keep data that belongs to one machine. The wrapper fills that gap without globals:

- the API adds the type to `args_t`, so it is resolved like any other dependency, from
  the root arguments or from a parent's `props`;
- `make` receives it as a pointer and stores it in the wrapper;
- every hook receives the wrapper and can use the stored data.

Use it for cross-cutting concerns that every state shares. It is not meant for state
behaviour; put that in the state itself.

### Contract

- `instance_t` is what `make` returns. When it is not `State`, it is treated as a wrapper.
- `args_t` lists what `make` receives. Add the service type to it, and pass the service as
  a root argument to the machine so it resolves for every state, including nested states
  and barrier children.
- Every hook takes `instance_t&`. `api::Default` hooks take `State&`, so a wrapper that
  owns the state forwards to them with the wrapped state.
- `props` accessors are applied to the state, not to the wrapper. If `instance_t` is not a
  `State` (or a type derived from it), the library reads the state from the wrapper's
  `state` member, so name that member `state`.
- `Fin` and `final` states are wrapped as well. They cannot be derived from, so use an
  owning wrapper (a `state` member) rather than inheritance.

### Example

```cpp
struct Logger
{
    void log(std::string_view message);
};

template <typename State>
struct logged
{
    template <typename... Args>
    explicit logged(Logger* init_logger, Args*... args)
        : state{args...}
        , logger(init_logger)
    {
    }

    State state;
    Logger* logger;
};

struct LoggingAPI
{
    template <typename State>
    struct state
    {
        using default_state_t = nil::sm::api::Default::state<State>;
        using instance_t = logged<State>;
        using args_t = nil::xalt::tlist_join_t<
            nil::xalt::tlist<Logger>,
            nil::xalt::coalesce_t<State, nil::sm::detail::args_tag>>;

        // The service is the first arg; the rest are the state's own args.
        static instance_t make(const nil::sm::Metadata&, Logger* logger, auto*... args)
        {
            return instance_t(logger, args...);
        }

        static auto on_enter(instance_t& instance)
        {
            instance.logger->log("enter");
            return default_state_t::on_enter(instance.state);
        }

        static auto on_exit(instance_t& instance)
        {
            return default_state_t::on_exit(instance.state);
        }

        static auto on_regions_finalized(instance_t& instance)
        {
            return default_state_t::on_regions_finalized(instance.state);
        }

        template <typename E>
        static auto on_event(instance_t& instance, const E& event)
        {
            return default_state_t::on_event(instance.state, event);
        }

        template <typename E>
        static auto on_capture(instance_t& instance, const E& event)
        {
            return default_state_t::on_capture(instance.state, event);
        }
    };
};

Logger logger;
nil::sm::SM<nil::sm::api::Coalesce<LoggingAPI>, Root, Logger> machine{&logger};
```

With `api::Coalesce`, the partial API must declare every hook the wrapper changes the type
of. Since `Default` hooks take `State&`, define all five hooks as above, or a hook the
partial API omits will be forwarded to `Default` with the wrapper and fail to compile.

### Notes

- The service type becomes an argument of every state, so diagram validation needs it too:
  `nil::sm::validate<Root, API, Logger>()` and `nil::sm::ir::make_root_props<Logger>()`.
- The machine borrows the service like any other root argument; keep it alive until the
  machine is destroyed.
- `make` must return exactly `instance_t`; a different type is rejected at compile time.

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
