# Documentation Map

Start here:

1. [README](../README.md) for a quick overview and build commands.
2. [Core Guide](01_GUIDE.md) for states, events, actions, and regions.
3. [Patterns](03_PATTERNS.md) for short reusable designs.
4. [Use cases](04_USE_CASES.md) for small domain examples.

Use these when needed:

- [Extensibility](02_EXTENSIBILITY.md) for contexts, hooks, timers, and threads.
- [Advanced API](05_ADVANCED.md) for custom API contracts.
- [Formatters](06_FORMAT.md) for diagrams.
- [Barriers](07_BARRIER.md) for split translation units and context adapters.

## Current rules

- States and events are ordinary C++ types.
- `post()` is synchronous and the library is not thread-safe.
- `SM` is non-owning; context objects belong to the caller.
- Unspecified context types are `void`, not pointer types.
- Use `TransitTo`, `Discard`, `Forward`, `Defer`, `Emit`, and `Terminate` as
  appropriate.
