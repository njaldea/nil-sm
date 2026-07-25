# nil/sml

`nil::sml` is a typed state-machine library for hierarchical and orthogonal state composition.

This README is written for consumers of the library and focuses on how to model states and send
events through the public API.

## Overview

`nil::sml` lets you model state machines as plain C++ types:

- states are ordinary structs/classes
- events are ordinary types
- transitions and propagation are returned from `on_event(...)`

The typical entry point is `nil::sml::SM<nil::xalt::tlist<...>>`.

## Quick Start

1. Define event types.
2. Define state types with optional `events` and `regions` lists.
3. Implement `on_event(...)` for handled events.
4. Construct `nil::sml::SM<nil::xalt::tlist<...>>` and call `process_event(...)`.

## State Declaration

The minimum useful shape of a state is:

```cpp
struct idle
{
	using events = nil::xalt::tlist<tick>;
	using regions = nil::xalt::tlist<>;

	auto on_event(const tick&)
	{
		return nil::sml::Discard{};
	}
};
```

## Mental Model

### Hierarchy

A state may own child regions via:

```cpp
using regions = nil::xalt::tlist<ChildA, ChildB, ...>;
```

Child regions receive events before their parent. This naturally forms a hierarchical state
machine.

### Orthogonal Regions

When multiple top-level or child regions exist, they are active simultaneously.

Each region processes the same event independently before the parent decides whether to react.

### Event Propagation

Events are always offered to active child regions before their parent.

Each region resolves to one of four runtime outcomes:

- `Forward`
- `Discard`
- `Transit`
- `Unhandled` (internal)

The parent receives the event if either:

- every child returned `Unhandled`, or
- at least one child returned `Forward`.

Otherwise the event is considered consumed by the children.

Users never return `Unhandled`; it is generated automatically when a state has no matching
reaction for an event.

## Public API You Will Use

### State Machine

- `nil::sml::SM<nil::xalt::tlist<Regions...>, nil::xalt::tlist<Contexts...>, API>`
  - Main entry point for application code.
	- `Regions...` are top-level active regions.
	- `Contexts...` is optional and defaults to an empty context list.
	- `API` is optional and defaults to the built-in API adapter.
  - Event dispatch: `sm.process_event(event)`.

Most users should use the default API adapter. The third template parameter exists to adapt
alternate state interfaces and construction policies.

### Reaction Types

Return one of these from `on_event(...)`:

- `nil::sml::Terminate{}`
	- Terminates the current active region.
- `nil::sml::Forward{}`
  - Event should keep propagating upward.
- `nil::sml::Discard{}`
  - Event is consumed.
- `nil::sml::Transit<NextState>{}` (or `nil::sml::Transit<NextState>()`)
	- `Transit<T>` is a reaction type.
	- Destroys the currently active state in that region and constructs `NextState`.
- `nil::sml::Emit<Payload>(args...)`
	- Enqueue a typed emitted event payload for delivery.

You may also return `std::variant<...>` of allowed reaction types.

### Optional Enter Hook

A state may implement:

```cpp
auto on_enter();
```

Allowed return values are:

- `nil::sml::NOOP{}`
- `nil::sml::Emit<Event>(...)`

`on_enter()` is evaluated when a state instance becomes active (initial construction or transit).

Notes:

- If `on_enter()` emits an event, it is queued and processed by the same FIFO emit flow.
- `on_enter()` does not return `Transit`; state changes still happen via `on_event(...)` or `on_regions_complete()`.

### Optional Exit Hook

A state may implement:

```cpp
auto on_exit();
```

Allowed return values are:

- `nil::sml::NOOP{}`
- `nil::sml::Emit<Event>(...)`

`on_exit()` is evaluated when a state instance is being destroyed (transit, terminate, or machine teardown).

Notes:

- Child regions are destroyed before the parent state's `on_exit()` runs.
- If `on_exit()` emits an event, it is queued and processed by the same FIFO emit flow.

### Optional Completion Hook

A state may implement:

```cpp
auto on_regions_complete();
```

Allowed return values are:

- `nil::sml::NOOP{}`
- `nil::sml::Transit<NextState>{}`
- `nil::sml::Emit<Event>(...)`

This hook is invoked once after every direct child region has terminated.

Notes:

- Completion is implemented internally as a queued event. Users never emit or handle this event directly.
- The completion callback runs once for that completed state instance.
- Completion events are consumed at the targeted state and do not bubble to parents.
- `NOOP` from completion is treated as consumed (`Discard`) internally.

Composite states typically reach completion because each child region eventually returns
`nil::sml::Terminate{}`.

### Emit Semantics

`Emit` is handled internally by `SM` using a FIFO queue.

- Emitted events are queued during processing.
- After the current `on_event(...)` finishes, `SM` drains the queue synchronously.
- Delivery order is FIFO.
- Reentrant chains are supported (for example `e1 -> Emit<e2>`, `e2 -> Emit<e3>`).

### Emit + Transit Behavior

When emission and transitions happen in the same top-level event cycle:

- Child-region emits are queued immediately.
- Any transitions produced while handling the event are applied before queued emitted events are delivered.
- Queued emits are then processed FIFO.

Net effect: emitted follow-up events observe the post-transition active configuration.

## Event Ordering

For each processed event:

1. Child regions process the event.
2. Region transitions/terminations are applied.
3. Completion callbacks (`on_regions_complete`) are queued if needed.
4. Queued emitted events are delivered FIFO.

## Writing States

A state type can declare:

- `using events = nil::xalt::tlist<...>;`
  - Event types this state can react to.
- `using regions = nil::xalt::tlist<...>;`
  - Child regions (for hierarchy/orthogonality).

Both declarations are optional.

If omitted:

- `events` defaults to an empty list.
- `regions` defaults to no child regions.

A state may implement one or more `on_event(...)` member functions.

Then implement one or more handlers:

```cpp
static auto on_event(const MyEvent&) -> nil::sml::Discard;
```

or a variant form:

```cpp
static auto on_event(const MyEvent&) -> std::variant<
	nil::sml::Forward,
	nil::sml::Discard,
	nil::sml::Transit<OtherState>
>;
```

## Examples

## Minimal Example

```cpp
#include <nil/sml.hpp>

struct click {};

struct child
{
	using events = nil::xalt::tlist<click>;

	static auto on_event(const click&)
	{
		return nil::sml::Forward{};
	}
};

struct root
{
	using regions = nil::xalt::tlist<child>;
	using events = nil::xalt::tlist<click>;

	static auto on_event(const click&)
	{
		return nil::sml::Discard{};
	}
};

int main()
{
	nil::sml::SM<nil::xalt::tlist<root>> sm{};
	sm.process_event(click{});
}
```

## Nested Hierarchy Example

```cpp
struct tick {};

struct walking
{
	using events = nil::xalt::tlist<tick>;
	static auto on_event(const tick&) { return nil::sml::Forward{}; }
};

struct movement
{
	using regions = nil::xalt::tlist<walking>;
};

struct idle
{
	using events = nil::xalt::tlist<tick>;
	static auto on_event(const tick&) { return nil::sml::Discard{}; }
};

struct animation
{
	using regions = nil::xalt::tlist<idle>;
};

struct root
{
	using regions = nil::xalt::tlist<movement, animation>;
};
```

## Orthogonal Regions Example

```cpp
struct e1 {};

struct left
{
	using events = nil::xalt::tlist<e1>;
	static auto on_event(const e1&) { return nil::sml::Discard{}; }
};

struct right
{
	using events = nil::xalt::tlist<e1>;
	static auto on_event(const e1&) { return nil::sml::Forward{}; }
};

nil::sml::SM<nil::xalt::tlist<left, right>> sm{};
sm.process_event(e1{});
```

## Transition Example

```cpp
struct e1 {};

struct running;

struct idle
{
	using events = nil::xalt::tlist<e1>;

	static auto on_event(const e1&)
	{
		return nil::sml::Transit<running>{};
	}
};

struct running
{
	using events = nil::xalt::tlist<e1>;

	static auto on_event(const e1&)
	{
		return nil::sml::Discard{};
	}
};
```

## Emit Example

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
		return nil::sml::Discard{};
	}
};

struct e1 {};

struct producer
{
	using events = nil::xalt::tlist<e1>;

	static auto on_event(const e1&)
	{
		return nil::sml::Emit<out_event>(42);
	}
};

nil::sml::SM<nil::xalt::tlist<producer, sink>> sm{};
sm.process_event(e1{});
```

## Emit + Transit Example (Behavior)

```cpp
struct tick {};
struct follow_up {};

struct target
{
	using events = nil::xalt::tlist<follow_up>;

	static auto on_event(const follow_up&)
	{
		return nil::sml::Discard{};
	}
};

struct mover
{
	using events = nil::xalt::tlist<tick>;

	static auto on_event(const tick&)
	{
		return nil::sml::Transit<target>{};
	}
};

struct emitter
{
	using events = nil::xalt::tlist<tick>;

	static auto on_event(const tick&)
	{
		return nil::sml::Emit<follow_up>{};
	}
};

// The transit in mover is applied before follow_up is delivered.
// follow_up is then handled by target.
nil::sml::SM<nil::xalt::tlist<mover, emitter>> sm{};
sm.process_event(tick{});
```

## Compile-Time Validation

Examples of invalid definitions include:

- Returning an unsupported reaction type from `on_event(...)`.
- Returning an unsupported lifecycle-hook type from `on_enter()`, `on_exit()`, or `on_regions_complete()`.
- Returning `Unhandled` explicitly from user code.

## State Lifetime

State objects are constructed when they become active and destroyed when they are replaced by
`Transit` or terminated via `Terminate`.

State instances may therefore contain ordinary member variables to maintain per-state runtime data.

