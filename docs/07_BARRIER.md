# Barrier States

`nil::sm::barrier::State<FinalizeAction, Provider>` lets you split a large
state machine into smaller pieces compiled in separate translation units,
without changing the machine's runtime behavior or its generated diagrams.

```cpp
#include <nil/sm/barrier.hpp> // not pulled in by <nil/sm.hpp>
```

## Why

Instantiating `State`/`Region`/dispatch tables for a large composite state is
the dominant cost of compiling a `nil::sm` machine (see
[sandbox/README.md](../sandbox/README.md#compile-cost)). A barrier state moves
one part of the graph's instantiation cost into its own translation unit,
while the rest of the machine only sees a small, non-template interface.

## The `Provider` contract

```cpp
struct MyProvider
{
    static std::unique_ptr<nil::sm::ISM> make(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    );

    // Optional: enables diagram rendering (see "Diagrams" below).
    static nil::sm::ir::Model ir(const nil::sm::Metadata* parent_metadata);
};

using MyBarrier = nil::sm::barrier::State<nil::sm::Transit<Next>, MyProvider>;
```

- `Provider::make` is declared where `MyBarrier` is used, but only *defined*
  (with the concrete child state graph fully visible) in its own `.cpp`.
- `FinalizeAction` is applied once the child reports `is_finalized()`. It can
  be `nil::sm::Terminate`, `nil::sm::Transit<X>`, or `nil::sm::Emit<T>` (same
  set `on_regions_finalized()` accepts).
- Every event reaching the barrier is forwarded to the child unconditionally
  — unlike an ordinary state, no `events`/`captures` list needs to be
  declared.

## `barrier::SM`

`Provider::make` typically returns a `nil::sm::barrier::SM<API, T>` instead of
a plain `nil::sm::SM<API, T>`:

```cpp
static std::unique_ptr<nil::sm::ISM> make(
    nil::sm::barrier::Runtime* runtime,
    const nil::sm::Metadata* parent_metadata
)
{
    return std::make_unique<nil::sm::barrier::SM<MyAPI, MyRoot>>(runtime, parent_metadata);
}
```

`barrier::SM` borrows the host's `Runtime` instead of owning one, and never
flushes it — draining stays the host's responsibility. This means the child's
own `on_enter`/`Emit`/`Defer` cascades broadcast through the whole host tree
exactly like an ordinary nested region, instead of being confined to a
separate, invisible one.

## What does and doesn't cross the barrier

- `Forward` and `Unhandled` from the child pass through to the barrier's own
  parent, letting sibling regions still react.
- `Discard`, `Defer`, `Terminate`, `Transit`, and `Emit` from the child are
  never re-surfaced raw: they're either already fully owned/applied inside
  the child (re-emitting them would double-own or double-queue their data),
  or meaningless outside it (a `Transit` target belongs to the child's own
  reachable-state table). They collapse to `Discard` at the barrier.
- Once the child finalizes, the barrier stops forwarding and stops
  re-checking `is_finalized()`, so `FinalizeAction` fires exactly once even
  if further events arrive before the resulting `Transit`/`Terminate`/`Emit`
  is applied.

## Diagrams

```cpp
#include <nil/sm/formatter/barrier.hpp> // alongside <nil/sm/uml.hpp>
```

If `Provider::ir()` is defined, `nil::sm::puml<SM>()` (and the other
formatters) render the child's graph nested inside the barrier's node instead
of an opaque leaf. When the child has exactly one non-final root, the
barrier's own node adopts that root's identity directly — so a barrier
renders exactly like the state it wraps, with no extra layer in the diagram.

See [sandbox/tollbooth_barrier](../sandbox/tollbooth_barrier) for a complete
example: each job is a `barrier::State` built in its own static library, and
`sandbox_toll_barrier_uml`'s output is structurally identical to
`sandbox_toll_uml`'s.
