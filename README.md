# nil/sm

`nil/sm` is a small C++20 header-only library for typed hierarchical state
machines. States are ordinary C++ types, events are ordinary types, and the
compiler checks handlers and transition targets.

## Quick start

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
        return nil::sm::TransitTo<running>{};
    }
};

struct running
{
    using events = nil::xalt::tlist<stop>;

    static auto on_event(const stop&)
    {
        return nil::sm::TransitTo<stopped>{};
    }
};

int main()
{
    nil::sm::DefaultSM<stopped> machine;
    machine.post(start{});
    machine.post(stop{});
}
```

`post()` is synchronous. The machine does not create threads or perform
locking; call it from one owner thread or add synchronization around it.

## The model

1. A state describes the current mode.
2. An event describes something that happened.
3. An action decides how the machine responds.

Common actions are `TransitTo<T>`, `Terminate`, `Discard`, `Forward`, `Defer`,
and `Emit<E>`. States can contain one or more child regions. Multiple regions
are active together.

## Documentation map

| Document | Use it for |
| --- | --- |
| [Guide](docs/01_GUIDE.md) | States, events, regions, and actions |
| [Patterns](docs/03_PATTERNS.md) | Short reusable designs |
| [Extensibility](docs/02_EXTENSIBILITY.md) | Contexts, observers, timers, and threading |
| [Advanced API](docs/05_ADVANCED.md) | Complete API customization reference |
| [Formatters](docs/06_FORMAT.md) | Diagram output |
| [Barriers](docs/07_BARRIER.md) | Splitting a machine across translation units |

## Build

The project requires a C++20 compiler. From a configured build directory:

```sh
ninja -C .build
ctest --test-dir .build --output-on-failure
```

See [AGENTS.md](AGENTS.md) for configure and test commands.

## License

See [LICENSE](LICENSE).
