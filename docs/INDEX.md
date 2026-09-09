# Documentation Map

Use the pages in this order:

1. [Core Guide](01_GUIDE.md) — the state-machine model and dispatch rules.
2. [Patterns](03_PATTERNS.md) — common designs in short examples.
3. [Extensibility](02_EXTENSIBILITY.md) — application contexts, hooks, and type erasure.
4. [Barriers](07_BARRIER.md) — structural child-machine boundaries.
5. [Formatters](06_FORMAT.md) — diagram output.
6. [Advanced API](05_ADVANCED.md) — custom API authors and compile-time extensions.

## Rules at a glance

- States and events are ordinary C++ types.
- `post()` is synchronous and the machine is not thread-safe.
- `SM` borrows context and root-argument objects; the caller owns them.
- Use `args` for state construction dependencies and `props` for explicit member exposure.
- Use `captures` for events that must run before child regions.
