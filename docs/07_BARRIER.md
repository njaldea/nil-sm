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
one part of that cost into its own translation unit, while the rest of the
machine only sees a small, non-template interface.

## The `Provider` contract

```cpp
// Header, where the barrier state is declared.
NIL_SM_BARRIER_DECLARE(MyProvider);

// Source file, where MyAPI and MyRoot are visible.
NIL_SM_BARRIER_DEFINE(MyProvider, MyAPI, MyRoot)
```

These macros generate the two static methods `Provider` needs:

```cpp
struct MyProvider
{
    static std::unique_ptr<nil::sm::ISM> make(
        nil::sm::barrier::Runtime* runtime,
        const nil::sm::Metadata* parent_metadata
    );

    // Required only if this barrier is rendered in diagrams (see "Diagrams" below).
    static nil::sm::ir::Model ir(const nil::sm::Metadata* parent_metadata);
};

using MyBarrier = nil::sm::barrier::State<nil::sm::Transit<Next>, MyProvider>;
```

- `Provider::make` is declared where `MyBarrier` is used, but only *defined*
  (with the concrete child state graph fully visible) in its own `.cpp`.
- `FinalizeAction` (`Terminate`, `Transit<X>`, or `Emit<T>`) is applied once
  the child reports `is_finalized()`.
- Every event reaching the barrier is forwarded to the child unconditionally
  — no `events`/`captures` list needs to be declared.

## `barrier::SM`

`Provider::make` typically returns a `nil::sm::barrier::SM<API, T>`:

```cpp
static std::unique_ptr<nil::sm::ISM> make(
    nil::sm::barrier::Runtime* runtime,
    const nil::sm::Metadata* parent_metadata
)
{
    return std::make_unique<nil::sm::barrier::SM<MyAPI, MyRoot>>(runtime, parent_metadata);
}
```

Unlike a plain `nil::sm::SM<API, T>`, `barrier::SM` borrows the host's
`Runtime` instead of owning one, so the child's `on_enter`/`Emit`/`Defer`
cascades broadcast through the whole host tree like an ordinary nested
region. The child must use the same API and context types as the host; a
mismatch throws `std::invalid_argument`. The host keeps owning both context
objects and must keep them alive until the child is destroyed.

## What does and doesn't cross the barrier

The barrier posts every event to the child and relays the result, but
`ISM::post()` only preserves `Forward`/`Unhandled` — everything else
(`Discard`, `Defer`, `Terminate`, `Transit`, `Emit`) is already applied
inside the child and collapses to `Discard`.

Once the child reports `is_finalized()`, the barrier returns `FinalizeAction`
for that event, then stops posting to the child and returns `Unhandled` for
every event after, so `FinalizeAction` fires exactly once.

## Diagrams

```cpp
#include <nil/sm/barrier.hpp> // alongside <nil/sm/uml.hpp>
```

`Provider::ir()` must be defined for any barrier state passed to
`nil::sm::puml<SM>()` (or the other formatters): it renders the child's graph
nested inside the barrier's node. When the child has exactly one non-final
root, the barrier's node adopts that root's identity directly, so it renders
exactly like the state it wraps.

See [sandbox/tollbooth_barrier](../sandbox/tollbooth_barrier) for a complete
example.
