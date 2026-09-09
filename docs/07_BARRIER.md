# Barriers

A barrier is a structural region state that owns a child `barrier::SM`. It is
useful when a large state machine should be split across translation units.
The barrier descriptor is compile-time metadata; it is not a user state object.

```cpp
#include <nil/sm/barrier.hpp>
```

## Declare and define

Declare the descriptor where the barrier is used:

```cpp
NIL_SM_BARRIER_DECLARE(MyBarrierState, MyAPI);
```

Define it where the child root is visible:

```cpp
NIL_SM_BARRIER_DEFINE(MyBarrierState, ChildRoot);
```

Use it as an ordinary region:

```cpp
using MyBarrier = nil::sm::barrier::State<
    nil::sm::Terminate,
    MyBarrierState>;

struct Root
{
    using regions = nil::xalt::tlist<MyBarrier>;
};
```

The first template argument is the action applied when the child finalizes.
Use `Terminate` for normal completion. Use `TransitTo<OtherBarrier>` to chain
barriers directly.

The descriptor supplies the child machine's API, stable identity, `make()`, and
`ir()`. It is never instantiated. Put user hooks on the ordinary owning state:
`on_enter`, `on_exit`, `captures`, event handlers, and
`on_regions_finalized`.

## Data and contexts

Child states can use ordinary `args` and `props` dependencies. Property lookups
can pass through the barrier to the owning state; `direct_parent` stops at the
boundary.

The child API context is adapted from the parent with
`nil::sm::barrier::context_adapter<Parent, Child>`:

- compatible pointer types are borrowed automatically;
- constructible child contexts are stored by the adapter;
- unrelated types can provide an ADL-found `adapt_context(Parent*, Child**)`;
- a full `context_adapter` specialization handles custom ownership.

The host owns borrowed context objects and must keep them alive while the child
machine exists.

## Runtime contract

Events enter the child machine first. A child `Forward` reaches the owning state;
a child `Terminate` finalizes the barrier region and applies the configured
completion action. The owner then receives its normal `on_regions_finalized()`
event when appropriate.

Child states are isolated from `direct_parent` access to the host. The barrier
itself has no user lifecycle hooks and does not own a descriptor instance.

## Diagrams

`MyBarrierState::ir()` exposes the child graph as a separate barrier definition.
The host graph shows the barrier as a leaf and includes its completion transition.
Use [Formatters](06_FORMAT.md) for output types and `.barriers` views.
