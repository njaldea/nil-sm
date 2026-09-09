# Sandbox

Small programs used to exercise `nil::sm` and measure compile cost.

| target | source | purpose |
|---|---|---|
| `sandbox` | `main.cpp` | small feature scratchpad |
| `sandbox_tollbooth` | `tollbooth/` | toll booth machine, single translation unit |
| `sandbox_tollbooth_uml` | `tollbooth/uml.cpp` | PlantUML for the toll booth machine |
| `sandbox_tollbooth_barrier` | `tollbooth_barrier/` | same machine, jobs behind `nil::sm::barrier::State` |
| `sandbox_barrier_property` | `barrier_property.cpp` | barrier state context selected from a parent property |
| `sandbox_barrier_shared_ptr` | `barrier_shared_ptr.cpp` | shared_ptr barrier context converted to a base |
| `sandbox_barrier_state_uml` | `barrier_state_uml.cpp` | one barrier state used twice, with root and standalone PlantUML output |

## Toll booth

A CLI-driven booth. `session` captures `shutdown` and restarts its own region whenever a job
finishes, so `booth:waiting` comes back and the next job can be picked. Each job is a composite
with orthogonal regions that must all terminate before the job is done.

```
session  (captures shutdown; on_regions_finalized -> TransitTo<session>)
└── booth:waiting                     select_* -> TransitTo<job>
    ├── job:startup       selftest chain      | gate arm
    ├── job:collection    lane pipeline       | gate arm | health
    ├── job:shift         cash chain          | audit chain
    └── job:maintenance   diagnostics chain   | gate arm
```

States are reused through templates parameterized on the *result* type rather than the target
state, so `return Next();` accepts `TransitTo<X>`, `Terminate`, or `Discard`, and chains can be
declared bottom-up with alias templates and no forward declarations:

```cpp
template <typename Job> using gate_closing = step<tag::gate_closing<Job>, Terminate, Terminate>;
template <typename Job> using gate_open    = waiting<tag::gate_open<Job>, ev::gate_close, TransitTo<gate_closing<Job>>>;
template <typename Job> using gate_opening = step<tag::gate_opening<Job>, TransitTo<gate_open<Job>>, Terminate>;
template <typename Job> using gate_arm     = waiting<tag::gate_closed<Job>, ev::gate_open, TransitTo<gate_opening<Job>>>;
```

The `Job` tag keeps each instantiation distinct, which is why the same gate arm chain can be
embedded in three different jobs. `workflow<Tag, Next, Regions...>` is the single composite
abstraction and is used both as a top-level job and as the nested `lane:occupied` sub-machine.

Between them the jobs cover `TransitTo`, `Terminate`, `Discard`, `Forward`, `Defer`, `Emit`,
captures, `std::variant` results, `on_enter` / `on_exit` / `on_regions_finalized`, state-context
injection, and nesting three levels deep.

Try:

```
printf 'job collection\narrive 1\ntick\nreceipt\npay 7\nok\ndepart\nclose\nok\nreset\nstatus\nquit\n' \
  | .build/bin/sandbox_tollbooth
```

`receipt` is deferred in `lane:await_payment` and replayed in `lane:paid`; `lane:classify` emits
`gate_open`, which the gate region in another orthogonal region reacts to.

## Barrier context adapters

`barrier_property.cpp` shows a barrier state context selected from a property of the parent
context. `barrier_shared_ptr.cpp` shows a parent-owned `std::shared_ptr<Derived>` converted to a
barrier context of type `std::shared_ptr<Base>`.

State and API adapters are separate template parameters. The barrier state supplies the child machine
and its context types; the barrier owns adapter storage for the child lifetime.

`repl::loop` and `repl::feed` are shared. `repl::feed` is written against anything exposing
`post<E>()`, so it accepts a concrete `SM<...>` and a `nil::sm::ISM` without change.

## Compile cost

gcc (`/usr/bin/c++`), Release flags:
`-Isrc/publish -isystem <nil-xalt> -O3 -DNDEBUG -std=gnu++20 -fno-rtti -flto`,
measured with `/usr/bin/time -f 'wall=%es maxrss=%MKB'`.

Where the memory goes, compile only (`-c`, no link):

| translation unit | wall | maxrss |
|---|---:|---:|
| `#include <nil/sm.hpp>` + empty `main` | 0.31s | 107 956 KB |
| `#include "jobs.hpp"` + empty `main` (types declared, never instantiated) | 0.29s | 113 784 KB |
| + instantiate `SM<Coalesce<tracing_api>, booth>` | 2.41s | 403 964 KB |
| full TU (SM + REPL) | 2.36s | 406 272 KB |

Per assembly, compile only (`-c`, no link):

| translation unit | wall | maxrss |
|---|---:|---:|
| `tollbooth/main.cpp` | 2.37s | 406 220 KB |

Clean serialized build, link included (`ninja -C .build -j1 <target>`):

| target | wall | maxrss |
|---|---:|---:|
| `sandbox_tollbooth` | 3.47s | 408 276 KB |
| `sandbox_tollbooth_uml` | 2.79s | 275 040 KB |

Findings:

- Declaring the state types is nearly free (+6 MB). Instantiating the SM costs **+290 MB**, so
  essentially all of the cost is template instantiation of `State` / `Region` / dispatch tables.
- LTO is not the driver for a single-TU target. The same TU without `-flto` peaks *higher*
  (421 012 KB, because codegen runs in-process), and linking adds under 2 MB on top of the compile
  peak. The "LTO dominates" effect only shows up in the whole-project build with ~30 TUs.
- Peak RSS is set by the single worst translation unit, so splitting jobs across TUs drops the
  ceiling from 408 MB to 284 MB — the limit becomes the collection job alone.
- Total wall time goes *up* (3.5s → 5.2s / 6.0s) because the shared headers are reparsed per TU and
  there are more LTO objects to link. The win is peak memory and parallelism, not total work.
- `sandbox_tollbooth_uml` names the same SM type but only instantiates the formatter's compile-time
  traversal, not the runtime `State` / `Region` tree, and peaks 130 MB lower — a useful control
  when trying to reduce instantiation cost.

### Reproducing

```sh
./configure/gcc -ts
XALT=$(echo .build/vcpkg_installed/x64-linux/include/nil-xalt/*)
FLAGS="-Isrc/publish -isystem $XALT -O3 -DNDEBUG -std=gnu++20 -fno-rtti -flto"

# one translation unit
/usr/bin/time -f 'wall=%es maxrss=%MKB' c++ $FLAGS -c sandbox/tollbooth/main.cpp -o /tmp/tb.o

# one target, link included
ninja -C .build -t clean sandbox_tollbooth
/usr/bin/time -f 'wall=%es maxrss=%MKB' ninja -C .build -j1 sandbox_tollbooth
```
