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

## Headers

`<nil/sm.hpp>` is the light, header-only core. Everything else is opt-in. The headers whose
code is compiled once into a library need that library linked, so your own translation units
do not compile them.

| Include | Provides | Link | Use it for |
| --- | --- | --- | --- |
| `<nil/sm.hpp>` | `SM`, `DefaultSM`, actions, `TransitTo`, barriers (`barrier.hpp`), compile-time state checks | `nil::sm` | Running a state machine |
| `<nil/sm/barrier.hpp>` | Barrier definitions and runtime (already in `<nil/sm.hpp>`) | `nil::sm` | Only when you want barriers without the rest |
| `<nil/sm/ir.hpp>` | `ir::build<Root, API>()`, `ir::Model`, `ir::make_root_props<...>()` | `nil::sm` | Inspecting the state graph at runtime |
| `<nil/sm/validate.hpp>` | `validate(model)`, `validate<Root, API, RootProps...>()` | `nil::sm` | Pass/fail check that every state gets its args, usable in `static_assert` |
| `<nil/sm/format/diagnostics.hpp>` | `Diagnostics`, `Issue`, `ChildReport` (also includes `validate.hpp`, `format/index.hpp`, `ir.hpp`) | `nil::sm-format` | Finding out which state is missing which arg |
| `<nil/sm/format/index.hpp>` | `Index`, an id-to-node table over `ir::Model` | `nil::sm-format` | Looking a state up by id (viewers, tooling) |
| `<nil/sm/format/uml.hpp>` | `puml`, `mermaid`, `dot`, `scxml`, `xstate`, `json` formatters | `nil::sm-format` | Generating diagrams or the JSON IR |
| `<nil/sm/format/{puml,mermaid,dot,scxml,xstate,json}.hpp>` | One formatter each | `nil::sm-format` | A single output format |
| `<nil/sm/viz.hpp>` | `viz::run(model, root_props, port)` | `nil::sm-viz` (links `nil::sm-format`) | The browser viewer (configure with `-v`) |

`<nil/sm.hpp>` does not include the IR, diagnostics, or formatter headers. Code that
needs them must include them directly and link the library from the table.

From an installed package, request the component to get the target:

```cmake
find_package(nil-sm CONFIG REQUIRED COMPONENTS format)  # nil::sm-format
find_package(nil-sm CONFIG REQUIRED COMPONENTS viz)     # nil::sm-viz, and nil::sm-format with it
target_link_libraries(app PRIVATE nil::sm nil::sm-format)
```

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
