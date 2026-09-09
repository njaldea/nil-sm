# Formatters

Include the formatter header only when diagrams are needed:

```cpp
#include <nil/sm/uml.hpp>
```

Formatters use the compile-time state graph. They do not need a machine instance.
Stream the `.root` view:

```cpp
using MySM = nil::sm::DefaultSM<Root>;

std::cout << nil::sm::puml<MySM>().root;
std::cout << nil::sm::mermaid<MySM>().root;
std::cout << nil::sm::dot<MySM>().root;
std::cout << nil::sm::scxml<MySM>().root;
std::cout << nil::sm::xstate<MySM>().root;
```

| Format | Type |
| --- | --- |
| PlantUML | `nil::sm::puml<SM>` |
| Mermaid | `nil::sm::mermaid<SM>` |
| Graphviz | `nil::sm::dot<SM>` |
| SCXML | `nil::sm::scxml<SM>` |
| XState | `nil::sm::xstate<SM>` |

The graph includes reachable states, transitions, captures, lifecycle actions,
termination, and structural barrier completion. Barrier child graphs are exposed
through the formatter's `.barriers` range.

For runnable examples, see `sandbox_uml`, `sandbox_barrier_state_uml`, and
`sandbox_tollbooth_barrier_uml`.
