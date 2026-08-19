# nil/sm

`nil/sm` is a C++20 header-only library for building typed state machines. It helps you describe which states an object can be in, which events it accepts, and what should happen when an event arrives.

## What It Provides

- Plain C++ structs and classes as states
- Compile-time checking of event handlers and transitions
- Hierarchical states, where a state can contain child regions
- Orthogonal regions, where multiple regions are active at once
- Synchronous event dispatch through `post()`
- Lifecycle hooks such as `on_enter()` and `on_exit()`
- Event actions such as `Transit`, `Discard`, `Forward`, `Defer`, `Emit`, and `Terminate`
- Optional output in Mermaid, PlantUML, SCXML, XState, and Graphviz formats

The library is intentionally small. Timers, logging, memory pools, and other application-specific services can be supplied through the API template parameter instead of being built into the state machine core.

## Quick Example

An event is an ordinary C++ type. A state lists the events it handles and returns an action from its handler.

```cpp
#include <nil/sm.hpp>

struct start {};
struct stop {};

struct running;

struct stopped
{
	using events = nil::xalt::tlist<start>;

	static auto on_event(const start&)
	{
		return nil::sm::Transit<running>{};
	}
};

struct running
{
	using events = nil::xalt::tlist<stop>;

	static auto on_event(const stop&)
	{
		return nil::sm::Transit<stopped>{};
	}
};

int main()
{
	nil::sm::DefaultSM<stopped> machine{nullptr, nullptr};

	machine.post(start{}); // stopped -> running
	machine.post(stop{});  // running -> stopped
}
```

`post()` is synchronous: it does not return until the event and any resulting transitions have finished. The default machine is intended to be used by one thread at a time. Add an external queue or lock when integrating it with a multi-threaded application.

## The Basic Model

There are three pieces to remember:

1. **States** describe the current mode of an object.
2. **Events** describe something that happened.
3. **Actions** describe how the state machine responds.

For example, a media player might have `stopped`, `playing`, and `paused` states. Events such as `play`, `pause`, and `stop` move the player between those states.

## Documentation

Read the documents in this order:

1. [Guide](docs/01_GUIDE.md) - Core features and event actions
2. [Extensibility](docs/02_EXTENSIBILITY.md) - Custom APIs and dependency injection
3. [Patterns](docs/03_PATTERNS.md) - Solutions to common design problems
4. [Use Cases](docs/04_USE_CASES.md) - Examples from different domains
5. [Advanced](docs/05_ADVANCED.md) - Advanced API customization

The complete navigation page is the [documentation map](docs/INDEX.md).

## Design Choices

### Plain C++ types

States do not inherit from a library base class. You can use ordinary structs, member variables, constructors, and methods.

### Compile-time validation

The library checks event handlers, action types, and transition targets while compiling. A mistake usually appears as a compiler error instead of a runtime failure.

### Synchronous dispatch

Each call to `post()` finishes before the next statement runs. There is no internal worker thread or lock. This keeps event ordering predictable and lets the application choose its own threading model.

### API-based customization

The `API` template parameter controls state construction and lifecycle dispatch. Use it to provide contexts, timers, logging, profiling, or custom allocation without adding those policies to the core library.

## Building

The project requires a C++20 compiler. From a configured build directory:

```sh
ninja -C .build
.build/bin/sm_test
```

See [AGENTS.md](AGENTS.md) for the repository's configure and test commands.

## License

See [LICENSE](LICENSE).
