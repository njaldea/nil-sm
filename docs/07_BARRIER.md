# Barrier States

`nil::sm::barrier::State` hides a child machine behind a small provider
interface, so a large machine can be split across translation units.

```cpp
#include <nil/sm/barrier.hpp>
```

## Provider

Declare the provider where the barrier state is used:

```cpp
NIL_SM_BARRIER_DECLARE(MyProvider, MyAPI);
```

Define it where the child state type is visible:

```cpp
NIL_SM_BARRIER_DEFINE(MyProvider, MyAPI, MyRoot);
```

The macros provide `make()` and `ir()`. The child is normally a
`nil::sm::barrier::SM<API, Root>`:

```cpp
using MyBarrier = nil::sm::barrier::State<
    nil::sm::Terminate,
    MyProvider>;
```

A provider exposes the context types of its child API through the declaration
macro. The provider factory receives shared queues and erased contexts.

The declared struct isn't `final`. To customize a shared provider per
embedding site — e.g. a display `name`, or a `payload()` hook (see below) —
inherit from it instead of redeclaring:

```cpp
struct MyNamedProvider : MyProvider
{
    static constexpr auto name = "custom-name";
};
```

Every such view shares the base's `id` (a `nil::xalt::type_id<NAME>` baked in
by the declaration), so the IR still groups them as one provider.

## Contexts

`barrier::SM` is non-owning. It borrows the host queues and the context slots
passed by the barrier state. The host owns the actual context objects and must
keep them alive while the child exists.

State and API contexts are bridged by `nil::sm::barrier::context_adapter<Parent, Child>`,
which covers two common cases automatically:

- borrowing, via a built-in `adapt_context(Parent*, Child**)` overload when
  `Parent*` converts to `Child*` — no user code needed;
- value conversion for non-borrowable but constructible contexts, such as
  `std::shared_ptr<Derived>` to `std::shared_ptr<Base>`, where the converted
  object is stored in the adapter.

### Custom bridging

For properties, composition, or unrelated types, declare `adapt_context` next to
the parent context. It is found through argument-dependent lookup, and the child
context slot is an output parameter, so the child type selects the overload:

```cpp
void adapt_context(parent_context* parent, child_context** out)
{
    *out = &parent->child;
}
```

A non-template overload like this wins over the library's borrowing overload
when both apply. Declare it before the barrier state is instantiated —
otherwise a different default is silently chosen, and mismatched translation
units violate the one-definition rule.

This form only borrows, so the parent must own the object. When the child
context has to be assembled and owned by the bridge, specialize the adapter
instead:

```cpp
template <>
struct nil::sm::barrier::context_adapter<parent_context, child_context> final
{
    explicit context_adapter(parent_context* parent)
        : context_value{.log = &parent->log, .retries = parent->config.retries}
    {
    }

    child_context* context()
    {
        return &context_value;
    }

    child_context context_value;
};
```

A full specialization always wins over the partial ones. Every adapter needs a
constructor from `Parent*` and a `context()` accessor; its storage lives as
long as the child machine.

## Runtime behavior

Events reaching the barrier are posted to the child. Child transitions,
`DeferTo` transitions, emissions, and termination are applied inside the child.
When the child reports `is_finalized()`, the barrier returns its `FinalizeAction` once;
subsequent events are unhandled.

## Payload injection

A provider can define a `payload()` hook to hand the child machine data
computed from the parent's API context, without changing `make()`'s
signature or the `NIL_SM_BARRIER_DECLARE`/`DEFINE` macros:

```cpp
struct MyProvider : /* the declared provider */
{
    static MyPayload payload(api_context_t* ctx)
    {
        return MyPayload{ /* built from *ctx */ };
    }
};
```

If `Provider::payload(api_context_t*)` is defined, the barrier state calls it
right after the child machine is constructed and posts the result as
`nil::sm::barrier::EvPayload<MyPayload>{ value }` (aggregate CTAD deduces
`MyPayload`). If `payload()` isn't defined, nothing is posted.

### Only one state needs the payload

If a single state needs the payload before moving on, it just declares
`EvPayload<T>` in its `events` and then returns an action:

```cpp
using events = nil::xalt::tlist<barrier::EvPayload<MyPayload>>;

auto on_event(const barrier::EvPayload<MyPayload>& e)
{
    return Action(); // Emit, Discard, Forward, Defer, DeferTo<T>, TransitTo<T>
}
```

`nil::sm::barrier::WaitForPayload<Next, T>` is a ready-made state for exactly
this: it declares `EvPayload<T>` in its `events` and transits to `Next` on
receipt.

### Multiple states need the payload

When more than one state down the tree needs the payload, the top-level state
must `captures`/`on_capture` it (not `on_event` — a composite's own `on_event`
doesn't run for events routed straight to its active region) and `Forward` it,
so descendants can still receive it. Use `WaitForPayload<Next, T>` as the
initial state of any region that must not progress until the payload has
arrived:

```cpp
struct Root
{
    using regions = nil::xalt::tlist<barrier::WaitForPayload<Next, MyPayload> /*, ... */>;
    using captures = nil::xalt::tlist<barrier::EvPayload<MyPayload>>;

    static auto on_capture(const barrier::EvPayload<MyPayload>& e)
    {
        payload = e.value;
        return Forward{};
    }

    MyPayload payload;
};
```

Ordering: the payload always arrives strictly after the child's own initial
`on_enter` chain has already run (construction and entry happen atomically
inside `Provider::make()`), so it can influence event handling but never the
child's initial `on_enter` or which initial region/state gets entered.

## Compile-time scaling guidance

For large machines, barriers are the primary mechanism to reduce peak compile
memory by splitting independent subgraphs into separate translation units.
This generally lowers per-TU RSS and improves build parallelism, while total
wall clock can rise due to repeated header parsing and extra link work.

## Diagrams

`Provider::ir()` is required when the barrier appears in a diagram. It delegates
IR generation to the child machine so the barrier can remain opaque at runtime
while its graph remains visible in output.

See the small examples in `sandbox/barrier_property.cpp` and
`sandbox/barrier_shared_ptr.cpp`, and the complete machine in
`sandbox/tollbooth_barrier/`.
