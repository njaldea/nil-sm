# nil/sm

`nil/sm` is a C++20, header-only library for typed hierarchical and orthogonal
state machines. States and events are ordinary C++ types; invalid handlers and
transition targets fail at compile time.

## Start here

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

`post()` is synchronous. The library does not create threads or lock the
machine; use one owner thread or synchronize at the application boundary.

## Documentation

| Read | For |
| --- | --- |
| [Core Guide](docs/01_GUIDE.md) | States, actions, regions, and dispatch |
| [Patterns](docs/03_PATTERNS.md) | Short, reusable designs |
| [Extensibility](docs/02_EXTENSIBILITY.md) | Contexts, hooks, and type erasure |
| [Barriers](docs/07_BARRIER.md) | Structural child-machine boundaries |
| [Formatters](docs/06_FORMAT.md) | PlantUML, Mermaid, DOT, SCXML, and XState |
| [Advanced API](docs/05_ADVANCED.md) | Custom API and compile-time extensions |

## Build

The project requires a C++20 compiler. From a configured build directory:

```sh
ninja -C .build
ctest --test-dir .build --output-on-failure
```

See [AGENTS.md](AGENTS.md) for configuration commands.

## License

See [LICENSE](LICENSE).
