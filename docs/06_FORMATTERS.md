# Diagram Formatters

The library can render a state machine's structure to several diagram formats
directly from its C++ types, with no separate schema or code generation step.

## Enabling it

Formatters live in a separate header from the core state machine so that
projects which don't need diagrams don't pay for it:

```cpp
#include <nil/sm/uml.hpp> // pulls in dot, mermaid, puml, scxml, xstate
```

`<nil/sm.hpp>` alone does **not** include this header.

## Usage

Each format is a small streamable wrapper keyed on your `SM<...>` type. Stream
it to any `std::ostream`:

```cpp
using MySM = nil::sm::DefaultSM<Root>;

std::cout << nil::sm::puml<MySM>();
std::cout << nil::sm::mermaid<MySM>();
std::cout << nil::sm::dot<MySM>();
std::cout << nil::sm::scxml<MySM>();
std::cout << nil::sm::xstate<MySM>();
```

No instance of the state machine is needed — the diagram is derived purely
from the types (`API`, the root state, and everything reachable from it via
`regions`, `Transit<...>`, and `on_regions_finalized`).

See [sandbox/uml.cpp](../sandbox/uml.cpp) for a runnable example covering
hierarchical states, orthogonal regions, transitions, parent bubbling, defer,
and event capture; run it with:

```sh
.build/bin/sandbox_uml puml
.build/bin/sandbox_uml mermaid
.build/bin/sandbox_uml dot
.build/bin/sandbox_uml scxml
.build/bin/sandbox_uml xstate
```

## Supported formats

| Format | Function | Notes |
|---|---|---|
| PlantUML | `nil::sm::puml<SM>()` | `@startuml` state diagram, orthogonal regions rendered as `--`-separated compartments. |
| Mermaid | `nil::sm::mermaid<SM>()` | `stateDiagram-v2`, orthogonal regions rendered the same way as PlantUML. |
| Graphviz | `nil::sm::dot<SM>()` | `digraph`, composite states become cluster subgraphs; regions get their own initial/termination pseudo-nodes. |
| SCXML | `nil::sm::scxml<SM>()` | Orthogonal regions become `<parallel>` with one `<state>` wrapper per region so each region's completion has an unambiguous final id. |
| XState | `nil::sm::xstate<SM>()` | JSON matching XState v5 machine config; orthogonal regions become `"type": "parallel"` with `"region_N"` keys. |

## What gets rendered

- **States**: every state reachable from the root, transitively, through
  `regions`, `Transit<Target>` (from `on_event`/`on_capture`/
  `on_regions_finalized`), and the implicit "final" (`Fin`) target of
  `Terminate`.
- **Initial pseudostate**: each region's first reachable state is flagged as
  its initial state; formatters render this as `[*] --> state`, an
  `initial="..."` attribute, etc.
- **Transitions**: one per `on_event`/`on_capture` handler whose result can
  produce `Transit`, `Terminate`, `Emit`, or `Defer`. `Terminate` targets the
  region's implicit final pseudostate (`[*]`).
- **Actions**: `on_enter`, `on_exit`, and `on_regions_finalized` are annotated
  on the state if they can produce an `Emit`.
- **Captures**: rendered the same as events, tagged so they're
  distinguishable from normal `on_event` transitions (`[c]` in PlantUML/
  Mermaid/Graphviz).

## How it works internally

All five formatters share one format-neutral intermediate representation
(`nil::sm::ir::Model` / `nil::sm::ir::Node`, in
[formatter/ir.hpp](../src/publish/nil/sm/formatter/ir.hpp)), built once by
`nil::sm::formatter::detail::build_ir<API, Root>()`
([formatter/detail.hpp](../src/publish/nil/sm/formatter/detail.hpp)) by
walking the same compile-time reachability graph the runtime dispatcher uses.
Each formatter (`dot.hpp`, `mermaid.hpp`, `puml.hpp`, `scxml.hpp`,
`xstate.hpp`) only implements `render(std::ostream&, const ir::Model&)` — to
add a new output format, build against the IR rather than the state machine's
templates directly.

State ids in the output (e.g. `ST_1a2b3c4d5e6f7890`) are a stable hash of each
state's position in the hierarchy (ancestor region/state indices) plus its
name, not its address — safe to diff across runs and builds.
