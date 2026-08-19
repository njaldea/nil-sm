# nil/sm

A typed state-machine library for hierarchical and orthogonal state composition in C++.

This README covers how to define states, handle events, and use the public API. For advanced topics like custom APIs and lifecycle hooks, see [ADVANCED.md](ADVANCED.md).

---

## Design Goals

- **Hierarchical state machines** — states can own child regions for natural composition
- **Orthogonal regions** — multiple independent regions active simultaneously
- **Strong compile-time validation** — catch unsupported reactions and lifecycle types at compile time
- **Plain C++ state types** — no base classes or intrusive requirements
- **No macros** — library is pure C++
- **No code generation** — works with standard compilers and tools

---

## Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [State Declaration](#state-declaration)
- [Core Concepts](#core-concepts)
- [Public API Reference](#public-api-reference)
- [Common Patterns](#common-patterns)
- [Event Ordering](#event-ordering)
- [Writing States](#writing-states)
- [Examples](#examples)

---

## Overview

`nil::sm` lets you model state machines as plain C++ types:

- **States** are ordinary structs/classes
- **Events** are ordinary types
- **Transitions** and propagation are returned from `on_event(...)`

The main entry point is `nil::sm::SM<API, TopLevelRegions...>`.

## Quick Start

1. Define event types.
2. Define state types with optional `events` and `regions` lists.
3. Implement `on_event(...)` for handled events.
4. Construct `nil::sm::DefaultSM<...>` and call `post(...)`.

## State Declaration

The minimum useful shape of a state is:

```cpp
struct idle
{
	using events = nil::xalt::tlist<tick>;
	using regions = nil::xalt::tlist<>;

	auto on_event(const tick&)
	{
		return nil::sm::Discard{};
	}
};
```

---

## Core Concepts

### Hierarchy

States can own **child regions** to form hierarchical machines:

```cpp
struct parent
{
	using regions = nil::xalt::tlist<ChildA, ChildB>;
};
```

Child regions process events *before* their parent. Events bubble upward unless consumed.

### Orthogonal Regions

Multiple regions at the same level are **active simultaneously** and process events independently:

```cpp
struct root
{
	using regions = nil::xalt::tlist<RegionA, RegionB>;  // Both active
};
```

Each region handles the same event, then the parent decides based on their collective response.

**Ordering:**
- Regions are processed in **declaration order**
- On destruction, regions are destroyed in **reverse declaration order**

### Event Propagation

**Order of dispatch:**
1. Child regions receive the event first
2. Parent receives it if:
   - Every region was `Unhandled`, OR
   - At least one region explicitly `Forward`
3. Otherwise, the event is consumed by children

**Return outcomes:**
| Outcome | Meaning |
|---------|---------|
| `Discard` | Event consumed; stop propagation |
| `Forward` | Let parent (and above) handle it |
| `Transit<S>` | Transition to state `S` |
| `Terminate` | End this region |
| `Unhandled` | No matching handler (auto-generated) |

**Important:** Users never explicitly return `Unhandled`. The library generates this automatically when a state has no handler for an event.

### Event Capture

A state can declare a `captures` list to intercept an event **before** it reaches child regions:

```cpp
struct guard
{
	using captures = nil::xalt::tlist<e_abort>;
	using regions = nil::xalt::tlist<child>;

	static auto on_capture(const e_abort&) { return nil::sm::Terminate{}; }
};
```

**Order of checks:**
1. `on_capture(...)` runs first, before child regions ever see the event
2. If it returns `Unhandled` (event not in `captures`) or `Forward`, the event proceeds to the normal flow — child regions, then this state's own `on_event(...)`
3. Any other return (`Discard`, `Transit<S>`, `Terminate`, `Defer`, `Emit<...>`) short-circuits: child regions never receive the event

**Important:** `Forward` means the opposite of what it means for `on_event`. For `on_event`, `Forward` bubbles the event *up* to the parent. For `on_capture`, `Forward` lets the event fall *down* into the regions instead of intercepting it.

---

## Public API Reference

### State Machine

**`nil::sm::SM<API, Regions...>`**
- Main entry point for application code
- `Regions...` are top-level active regions
- **Methods:**
  - `void post(T event)` — Dispatch an event
- **Template args:**
  - First template parameter `API` selects the state-construction/lifecycle adapter (default: built-in adapter)
- **Constructor:** `SM(state_context_t* ctx, api_context_t* api_ctx)` — takes pointers; the caller owns the lifetime of both context objects

When no context is needed, pass `nullptr` for both:
```cpp
nil::sm::DefaultSM<MyRegion> sm{nullptr, nullptr};
// or with explicit API:
nil::sm::SM<MyAPI, MyRegion> sm{nullptr, nullptr};
```

For states requiring injected context, use `default_api` with a context type — see [State Construction Contexts](#state-construction-contexts).

### API Adapter Contract

Custom API adapters (used via `SM<API, ...>` or `coalesce_api`) follow this hook shape:

- `make(parent, state_contexts, api_contexts, metadata)`
- `on_capture(state, event, api_contexts)`
- `on_event(state, event, api_contexts)`
- `on_enter(state, api_contexts)`
- `on_exit(state, api_contexts)`
- `on_regions_finalized(state, api_contexts)`

Only `make(...)` receives `Metadata` explicitly. If you provide a custom `make` (including via `coalesce_api`), it must include the metadata parameter.

`Metadata` currently contains:
- `region` (`std::size_t`) — region slot index of the state instance

Metadata is stored by the runtime on state instances (via the internal base) and can be surfaced by custom APIs/states when needed.

---

### Reaction Types

Return one of these from `on_event(...)`:

| Type | Meaning |
|------|---------|
| `nil::sm::Discard{}` | Consume event; stop bubbling |
| `nil::sm::Forward{}` | Propagate to parent |
| `nil::sm::Transit<NextState>{}` | Destroy current state, construct `NextState` |
| `nil::sm::Terminate{}` | End this region |
| `nil::sm::Defer{}` | Store event; replay after next `Transit` |
| `nil::sm::Emit<Payload>(args...)` | Queue typed emitted event |
| `std::variant<...>` | Any combination of the above |

---

### Lifecycle Hooks

#### `on_enter()` — Optional

Called when a state becomes active (initial construction or after `Transit`).

**Allowed returns:**
- `nil::sm::NOOP{}` — do nothing
- `nil::sm::Emit<Event>(...)` — queue an event

**Note:** Emitted events from `on_enter()` are queued and flushed at the end of construction, before the first `post()` call.

#### `on_exit()` — Optional

Called when a state is destroyed (during `Transit`, `Terminate`, or machine shutdown).

**Allowed returns:**
- `nil::sm::NOOP{}` — do nothing
- `nil::sm::Emit<Event>(...)` — queue an event

**Note:** Child regions are destroyed *before* the parent's `on_exit()` runs.

---

#### `on_regions_finalized()` — Optional

Called once all direct child regions have terminated.

**Allowed returns:**
- `nil::sm::NOOP{}` — do nothing
- `nil::sm::Transit<NextState>{}` — transition to a new state
- `nil::sm::Emit<Event>(...)` — queue an event

Composite states typically use this to react when all children have finished.

---

## Common Patterns

### Defer Semantics

**`Defer`** postpones the current event and replays it after the next `Transit` in that region.

**Use case:** A state that isn't ready stores events while waiting for setup, then transitions to a state that handles them.

```cpp
struct waiting
{
	using events = nil::xalt::tlist<e_work, e_ready>;
	static auto on_event(const e_work&)  { return nil::sm::Defer{}; }
	static auto on_event(const e_ready&) { return nil::sm::Transit<working>{}; }
};
```

**Semantics:**
- Deferred events are stored per-region
- On `Transit`, the new state receives deferred events in FIFO order
- Deferred events are replayed **before** newly emitted events generated after the transition
- Deferred events are discarded if the region terminates (no transit)

---

### State Construction Contexts

`default_api<T, ContextType>` injects a shared context into every state at construction time.

The SM holds a **pointer** to the context — the caller owns its lifetime:

```cpp
struct MyContext
{
	int value;
};

template <typename T>
using MyAPI = nil::sm::default_api<T, MyContext>;

struct my_state
{
	template <typename Parent>
	my_state(Parent*, MyContext* ctx)
		: value(ctx->value) {}

	int value;
};

MyContext ctx{.value = 42};
nil::sm::SM<MyAPI, my_state> sm{&ctx, nullptr};
```

**Constructor resolution order** (first match wins):
1. `T(parent, ContextType*)` — receive pointer to context
2. `T(parent)` — parent only
3. `T()` — default construction

**Design note:** The state does not need to know the concrete context type if a custom API performs construction. This allows states to remain decoupled from context implementation details.

For advanced context injection patterns, see [ADVANCED.md](ADVANCED.md).

---

### Emit + Transit Behavior

When both emission and transition occur in the same event cycle:

1. Child-region emits are queued immediately
2. Transitions are applied *before* queued emits are delivered
3. Queued emits are then processed FIFO

**Result:** Emitted follow-up events see the post-transition active configuration.

Example:
```cpp
struct emitter
{
	using events = nil::xalt::tlist<tick>;
	static auto on_event(const tick&) { return nil::sm::Emit<follow_up>{}; }
};

struct target
{
	using events = nil::xalt::tlist<follow_up>;
	static auto on_event(const follow_up&) { return nil::sm::Discard{}; }
};

// follow_up is delivered to target, not emitter
nil::sm::DefaultSM<emitter, target> sm{nullptr, nullptr};
sm.post(tick{});
```

---

## Event Ordering

For each `post()` call:

1. **Child processing** — Child regions process the event
2. **Defer storage** — Any `Defer` returns are stored per-region
3. **Transitions** — `Transit` and `Terminate` are applied; deferred events flush into new states
4. **Completion** — `on_regions_finalized()` callbacks fire if children terminated
5. **Emits** — Queued emitted events are delivered FIFO

**Key semantics:** All transitions for the current event are resolved before any emitted events are delivered. This means newly emitted events see the post-transition active configuration.

**Within delivery (step 5):**
- After `on_event(...)` completes, `SM` drains emit queue synchronously
- Delivery is FIFO
- Reentrancy is supported (e.g., `e1 → Emit<e2>`, `e2 → Emit<e3>`)

---

## Writing States

A state is a struct/class with optional declarations:

```cpp
struct MyState
{
	using events = nil::xalt::tlist<EventA, EventB>;  // Optional
	using captures = nil::xalt::tlist<EventC>;  // Optional; checked before regions
	using regions = nil::xalt::tlist<ChildA, ChildB>;  // Optional
	
	// Optional lifecycle hooks
	auto on_enter() { return nil::sm::NOOP{}; }
	auto on_exit() { return nil::sm::NOOP{}; }
	auto on_regions_finalized() { return nil::sm::NOOP{}; }
	
	// Event handlers
	static auto on_capture(const EventC&) { return nil::sm::Terminate{}; }
	static auto on_event(const EventA&) { return nil::sm::Discard{}; }
	static auto on_event(const EventB&) { return nil::sm::Forward{}; }
};
```

**Defaults:**
- If `events` is omitted → empty list
- If `captures` is omitted → empty list (no interception)
- If `regions` is omitted → no children
- If `on_enter()` / `on_exit()` / `on_regions_finalized()` are omitted → not called

**Handler signatures:**
```cpp
static auto on_event(const MyEvent&) -> nil::sm::Discard;
// or
static auto on_event(const MyEvent&) -> std::variant<
	nil::sm::Forward,
	nil::sm::Discard,
	nil::sm::Transit<OtherState>
>;
```

**State data:**
States can hold member variables to maintain per-state runtime data:
```cpp
struct counter
{
	int count = 0;
	
	auto on_event(const increment&) {
		count++;  // Modify state
		return nil::sm::Discard{};
	}
};
```

---

## Compile-Time Validation

The library detects and rejects at compile time:

- Unsupported reaction types from `on_event(...)`
- Unsupported lifecycle return types from `on_enter()`, `on_exit()`, `on_regions_finalized()`
- Explicit `Unhandled` in user code

## Examples

### 1. Minimal Example

Child forwards to parent; parent discards.

```cpp
#include <nil/sm.hpp>

struct click {};

struct child
{
	using events = nil::xalt::tlist<click>;
	static auto on_event(const click&) { return nil::sm::Forward{}; }
};

struct root
{
	using regions = nil::xalt::tlist<child>;
	using events = nil::xalt::tlist<click>;
	static auto on_event(const click&) { return nil::sm::Discard{}; }
};

int main()
{
	nil::sm::DefaultSM<root> sm{nullptr, nullptr};
	sm.post(click{});  // child forwards → root discards
}
```

---

### 2. Transition Example

State change on event.

```cpp
struct e1 {};

struct running;

struct idle
{
	using events = nil::xalt::tlist<e1>;
	static auto on_event(const e1&) { return nil::sm::Transit<running>{}; }
};

struct running
{
	using events = nil::xalt::tlist<e1>;
	static auto on_event(const e1&) { return nil::sm::Discard{}; }
};
```

---

### 3. Hierarchy Example

Multiple orthogonal hierarchies.

```cpp
struct tick {};

struct walking
{
	using events = nil::xalt::tlist<tick>;
	static auto on_event(const tick&) { return nil::sm::Forward{}; }
};

struct movement
{
	using regions = nil::xalt::tlist<walking>;
};

struct idle
{
	using events = nil::xalt::tlist<tick>;
	static auto on_event(const tick&) { return nil::sm::Discard{}; }
};

struct animation
{
	using regions = nil::xalt::tlist<idle>;
};

struct root
{
	using regions = nil::xalt::tlist<movement, animation>;  // Two independent hierarchies
};
```

---

### 4. Orthogonal Regions Example

Two regions at the same level process events independently.

```cpp
struct e1 {};

struct left
{
	using events = nil::xalt::tlist<e1>;
	static auto on_event(const e1&) { return nil::sm::Discard{}; }  // Consumes
};

struct right
{
	using events = nil::xalt::tlist<e1>;
	static auto on_event(const e1&) { return nil::sm::Forward{}; }  // Propagates
};

nil::sm::DefaultSM<left, right> sm{nullptr, nullptr};
sm.post(e1{});  // Both regions process independently
```

---

### 5. Emit Example

State generates a follow-up event.

```cpp
struct out_event
{
	int value;
};

struct sink
{
	using events = nil::xalt::tlist<out_event>;
	static auto on_event(const out_event& payload)
	{
		(void)payload;
		return nil::sm::Discard{};
	}
};

struct e1 {};

struct producer
{
	using events = nil::xalt::tlist<e1>;
	static auto on_event(const e1&) { return nil::sm::Emit<out_event>(42); }
};

nil::sm::DefaultSM<producer, sink> sm{nullptr, nullptr};
sm.post(e1{});  // e1 → emit(out_event{42}) → delivered to sink
```

---

### 6. Defer Example

Event deferred until transition.

```cpp
struct e_save {};
struct e_go   {};

struct receiver
{
	using events = nil::xalt::tlist<e_save>;
	static auto on_event(const e_save&) { return nil::sm::Discard{}; }
};

struct deferrer
{
	using events = nil::xalt::tlist<e_save, e_go>;
	static auto on_event(const e_save&) { return nil::sm::Defer{}; }
	static auto on_event(const e_go&)   { return nil::sm::Transit<receiver>{}; }
};

struct root { using regions = nil::xalt::tlist<deferrer>; };

nil::sm::DefaultSM<root> sm{nullptr, nullptr};
sm.post(e_save{});  // deferred — stored
sm.post(e_go{});    // transit to receiver; e_save flushed and handled
```

---

### 7. Capture Example

Capture intercepts before the child region ever reacts.

```cpp
struct e_intercept {};

struct child
{
	using events = nil::xalt::tlist<e_intercept>;
	static auto on_event(const e_intercept&) { return nil::sm::Discard{}; }
};

struct root
{
	using regions = nil::xalt::tlist<child>;
	using captures = nil::xalt::tlist<e_intercept>;
	static auto on_capture(const e_intercept&) { return nil::sm::Terminate{}; }
};

nil::sm::DefaultSM<root> sm{nullptr, nullptr};
sm.post(e_intercept{});  // captured by root; child::on_event never runs
```

---

## References

- [ADVANCED.md](ADVANCED.md) — Custom APIs, state construction contexts, lifecycle customization
- [src/test/TESTS.md](src/test/TESTS.md) — Comprehensive test reference guide


