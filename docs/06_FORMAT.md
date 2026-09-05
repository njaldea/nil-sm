# Diagram Formatters

Include the formatter header only when diagrams are needed:

```cpp
#include <nil/sm/uml.hpp>
```

The formatters build a diagram from the same compile-time state graph used by
the machine. No machine instance is required:

```cpp
using MySM = nil::sm::DefaultSM<Root>;

std::cout << nil::sm::puml<MySM>();
std::cout << nil::sm::mermaid<MySM>();
std::cout << nil::sm::dot<MySM>();
std::cout << nil::sm::scxml<MySM>();
std::cout << nil::sm::xstate<MySM>();
```

Supported output:

| Format | Wrapper |
| --- | --- |
| PlantUML | `nil::sm::puml<SM>` |
| Mermaid | `nil::sm::mermaid<SM>` |
| Graphviz | `nil::sm::dot<SM>` |
| SCXML | `nil::sm::scxml<SM>` |
| XState | `nil::sm::xstate<SM>` |

The graph includes states reachable through `regions`, `TransitTo`, and
termination. Captures and lifecycle actions are represented in the output.

## Barriers

A barrier provider must expose `ir()` when its graph is rendered. The provider
can be declared with `NIL_SM_BARRIER_DECLARE` and defined with
`NIL_SM_BARRIER_DEFINE`; see [Barriers](07_BARRIER.md).

For runnable examples, see the retained sandbox targets:
`sandbox_tollbooth_uml` and `sandbox_tollbooth_barrier_uml`.
