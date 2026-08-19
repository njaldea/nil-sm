# Documentation Map

Start with the [root README](../README.md), then read the numbered documents
in order. Each document has one job.

## Reading Order

1. [Guide](01_GUIDE.md) explains states, events, actions, regions, and lifecycle hooks.
2. [Extensibility](02_EXTENSIBILITY.md) explains how to add application services.
3. [Patterns](03_PATTERNS.md) shows short solutions to common problems.
4. [Use Cases](04_USE_CASES.md) shows how the library fits different domains.
5. [Advanced](05_ADVANCED.md) documents the complete custom API contract.

Most users only need the README, the core guide, and a few patterns.

## Core Ideas

- A **state** is an ordinary C++ type.
- An **event** is an ordinary C++ type.
- An event handler returns an **action** such as `Transit`, `Discard`, or `Forward`.
- A state can contain child **regions**.
- Multiple regions can be active at the same time.
- `post()` is synchronous and must be called by one thread at a time unless the application adds synchronization.

## Built-In Features

- Hierarchical states
- Orthogonal regions
- Compile-time validation
- Event capture
- Event deferral and emission
- Lifecycle hooks
- Mermaid, PlantUML, SCXML, XState, and Graphviz output

## Application-Provided Features

The library does not choose an allocator, timer, logger, profiler, or threading
model. Supply those through a custom API when your application needs them.
This keeps the core small and lets the application own these policies.

## Common Questions

### How do I add a timer?

Inject a timer service through a custom API. The timer should eventually post a
typed timeout event to the state machine. See [Extensibility](02_EXTENSIBILITY.md).

### Can two regions handle the same event?

Yes. Orthogonal regions process events independently. See
[Guide](01_GUIDE.md).

### Are context pointers owned by the machine?

No. The caller owns them and must keep them alive until after the machine is
destroyed.

### Is the machine thread-safe?

No. `post()` is synchronous and the library does not lock internally. Use an
external queue or lock when other threads produce events.

### Where are the tests?

The test suite is under [`src/test/`](../src/test/).
