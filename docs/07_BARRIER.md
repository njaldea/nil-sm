# Barrier States

`nil::sm::barrier::State` hides a child machine behind a small provider
interface. It is useful when a large machine should be compiled across
translation units.

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

## Contexts

`barrier::SM` is non-owning. It borrows the host queues and the context slots
passed by the barrier state. The host owns the actual context objects and must
keep them alive while the child exists.

State and API contexts are bridged by `nil::sm::barrier::context_adapter<Parent, Child>`.
Two partial specializations cover the common cases:

- an `adapt_context(Parent*, Child**)` call when one is available;
- value conversion for non-borrowable constructible contexts, such as
  `std::shared_ptr<Derived>` to `std::shared_ptr<Base>`, where the converted
  object is stored in the adapter.

The library provides one `adapt_context` overload, constrained to pairs where
`Parent*` converts to `Child*`, so borrowing needs no user code.

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

Being a non-template, such an overload also wins over the library's borrowing
overload when both apply. Declare it before the barrier state is instantiated;
otherwise the constraint silently resolves to a different default, and two
translation units that disagree violate the one-definition rule.

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

A full specialization always wins over the partial ones. An adapter provides a
constructor receiving `Parent*` and a `context()` function returning the child
context slot. Its storage belongs to the barrier state and lives as long as the
child machine.

## Runtime behavior

Events reaching the barrier are posted to the child. Child transitions,
`DeferTo` transitions, emissions, and termination are applied inside the child.
When the child reports `is_finalized()`, the barrier returns its `FinalizeAction` once;
subsequent events are unhandled.

## Diagrams

`Provider::ir()` is required when the barrier appears in a diagram. It delegates
IR generation to the child machine so the barrier can remain opaque at runtime
while its graph remains visible in output.

See the small examples in `sandbox/barrier_property.cpp` and
`sandbox/barrier_shared_ptr.cpp`, and the complete machine in
`sandbox/tollbooth_barrier/`.
