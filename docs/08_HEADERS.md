# Headers

`<nil/sm.hpp>` is the light, header-only core. Everything else is opt-in. The headers whose
code is compiled once into a library need that library linked, so your own translation units
do not compile them.

| Include | Provides | Link | Use it for |
| --- | --- | --- | --- |
| `<nil/sm.hpp>` | `SM`, `DefaultSM`, actions, `TransitTo`, barriers, compile-time state checks | `nil::sm` | Running a state machine |
| `<nil/sm/format/uml.hpp>` | `puml`, `mermaid`, `dot`, `scxml`, `xstate`, `json` formatters (also brings in `ir.hpp` and the diagnostics headers) | `nil::sm-format` | Generating diagrams or the JSON IR |
| `<nil/sm/viz.hpp>` | `viz::run(model, root_props, port)` | `nil::sm-viz` (links `nil::sm-format`) | The browser viewer (configure with `-v`) |

`<nil/sm.hpp>` does not include the IR, diagnostics, or formatter headers. Code that
needs them must include them directly and link the library from the table.

## Fine-grained headers

Most code does not need these. Include one when you want only that piece, for example
`validate.hpp` in a `static_assert` without pulling in the formatters, or `ir.hpp` to walk
the state graph yourself.

| Include | Provides | Link |
| --- | --- | --- |
| `<nil/sm/barrier.hpp>` | Barrier definitions and runtime (already in `<nil/sm.hpp>`) | `nil::sm` |
| `<nil/sm/ir.hpp>` | `ir::build<Root, API>()`, `ir::Model`, `ir::make_root_props<...>()` | `nil::sm` |
| `<nil/sm/validate.hpp>` | `validate(model)`, `validate<Root, API, RootProps...>()`, usable in `static_assert` | `nil::sm` |
| `<nil/sm/format/diagnostics.hpp>` | `Diagnostics`, `Issue`, `ChildReport`: which state is missing which arg | `nil::sm-format` |
| `<nil/sm/format/index.hpp>` | `Index`, an id-to-node table over `ir::Model` | `nil::sm-format` |
| `<nil/sm/format/{puml,mermaid,dot,scxml,xstate,json}.hpp>` | One formatter each, for a single output format | `nil::sm-format` |

## Linking

From an installed package, request the component to get the target:

```cmake
find_package(nil-sm CONFIG REQUIRED COMPONENTS format)  # nil::sm-format
find_package(nil-sm CONFIG REQUIRED COMPONENTS viz)     # nil::sm-viz, and nil::sm-format with it
target_link_libraries(app PRIVATE nil::sm nil::sm-format)
```
