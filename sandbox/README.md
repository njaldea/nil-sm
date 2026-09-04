# Sandbox

Scratch programs used to exercise `nil::sm` and to measure what it costs to compile.

| target | source | purpose |
|---|---|---|
| `sandbox` | `main.cpp` | small feature scratchpad |
| `sandbox_uml` | `uml.cpp` | feature-by-feature diagram reference |
| `sandbox_tollbooth` | `tollbooth/` | toll booth machine, single translation unit |
| `sandbox_toll_uml` | `tollbooth/uml.cpp` | PlantUML for the toll booth machine |
| `sandbox_tollbooth_libs` | `tollbooth_libs/` | same machine, one library per job |
| `sandbox_tollbooth_slot` | `tollbooth_slot/` | same machine, jobs as opaque child machines |
| `sandbox_tollbooth_barrier` | `tollbooth_barrier/` | same machine, jobs behind `nil::sm::barrier::State` |

## Toll booth

A CLI-driven booth. `session` captures `shutdown` and restarts its own region whenever a job
finishes, so `booth:waiting` comes back and the next job can be picked. Each job is a composite
with orthogonal regions that must all terminate before the job is done.

```
session  (captures shutdown; on_regions_finalized -> Transit<session>)
└── booth:waiting                     select_* -> Transit<job>
    ├── job:startup       selftest chain      | gate arm
    ├── job:collection    lane pipeline       | gate arm | health
    ├── job:shift         cash chain          | audit chain
    └── job:maintenance   diagnostics chain   | gate arm
```

States are reused through templates parameterized on the *result* type rather than the target
state, so `return Next();` accepts `Transit<X>`, `Terminate`, or `Discard`, and chains can be
declared bottom-up with alias templates and no forward declarations:

```cpp
template <typename Job> using gate_closing = step<tag::gate_closing<Job>, Terminate, Terminate>;
template <typename Job> using gate_open    = waiting<tag::gate_open<Job>, ev::gate_close, Transit<gate_closing<Job>>>;
template <typename Job> using gate_opening = step<tag::gate_opening<Job>, Transit<gate_open<Job>>, Terminate>;
template <typename Job> using gate_arm     = waiting<tag::gate_closed<Job>, ev::gate_open, Transit<gate_opening<Job>>>;
```

The `Job` tag keeps each instantiation distinct, which is why the same gate arm chain can be
embedded in three different jobs. `workflow<Tag, Next, Regions...>` is the single composite
abstraction and is used both as a top-level job and as the nested `lane:occupied` sub-machine.

Between them the jobs cover `Transit`, `Terminate`, `Discard`, `Forward`, `Defer`, `Emit`,
captures, `std::variant` results, `on_enter` / `on_exit` / `on_regions_finalized`, state-context
injection, and nesting three levels deep.

Try:

```
printf 'job collection\narrive 1\ntick\nreceipt\npay 7\nok\ndepart\nclose\nok\nreset\nstatus\nquit\n' \
  | .build/bin/sandbox_tollbooth
```

`receipt` is deferred in `lane:await_payment` and replayed in `lane:paid`; `lane:classify` emits
`gate_open`, which the gate region in another orthogonal region reacts to.

## Four assemblies of the same machine

`tollbooth_libs/`, `tollbooth_slot/`, and `tollbooth_barrier/` reuse `tollbooth/common.hpp` and
`tollbooth/jobs.hpp` verbatim, so the machine is identical in all four and only the assembly
differs.

- **`tollbooth`** — everything instantiated in one translation unit.
- **`tollbooth_libs`** — each job is its own `nil::sm::SM` compiled into its own static library and
  exported as `std::unique_ptr<nil::sm::ISM> make_X(booth_context*, trace_context*)`. `main.cpp`
  instantiates no state machine at all. The idle/active decision moves out of the machine and into
  C++, and a job is no longer a nested region.
- **`tollbooth_slot`** — one machine, but `job_slot<Tag, Factory>` is a leaf state that owns an
  opaque child `ISM` and forwards events into it with a single templated `on_event`. The child
  signals completion through `booth_context::job_done` and the slot returns `Terminate`. Keeps one
  CLI-driven machine, but the child no longer participates in the parent's capture/forward chain
  and does not appear in the generated diagram.
- **`tollbooth_barrier`** — one machine, but each job is a `nil::sm::barrier::State<Terminate,
  Provider>` built in its own library, same split as `tollbooth_libs`. Unlike `tollbooth_slot`'s
  hand-rolled `job_slot`, no event list needs to be declared (every event forwards unconditionally)
  and no `job_done` flag is needed (`FinalizeAction` fires from `ISM::is_finalized()`). The job's
  `barrier::SM` borrows the host's `Runtime` instead of owning one, so its own `on_enter`/`Emit`/
  `Defer` cascades broadcast through the whole booth tree exactly like a normal nested region,
  instead of being confined to a separate one. Each job type (e.g. `job::startup_job`) is used
  directly as the `barrier::SM`'s root, exactly as it's used directly inside `session`'s region in
  `tollbooth` - no run-once wrapper needed, since the job already reports its own termination via
  `on_regions_finalized`. Combined with `Provider::ir()` and `build_node`'s flattening
  (`barrier.hpp`), `sandbox_toll_barrier_uml`'s diagram is structurally identical to
  `sandbox_toll_uml`'s; only the underlying `barrier::State` type differs, invisibly.

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
| `tollbooth_slot/main.cpp` (session + slots only) | 0.78s | 207 708 KB |
| `tollbooth_libs/main.cpp` (no SM instantiated) | 0.33s | 121 048 KB |
| `tollbooth_libs/collection.cpp` (worst job) | 1.15s | 282 820 KB |
| `tollbooth_libs/maintenance.cpp` | 0.83s | 224 268 KB |
| `tollbooth_libs/startup.cpp` | 0.81s | 223 708 KB |
| `tollbooth_libs/shift.cpp` | 0.74s | 209 316 KB |

Clean serialized build, link included (`ninja -C .build -j1 <target>`):

| target | wall | maxrss |
|---|---:|---:|
| `sandbox_tollbooth` | 3.47s | 408 276 KB |
| `sandbox_tollbooth_libs` | 5.20s | 284 472 KB |
| `sandbox_tollbooth_slot` | 5.97s | 284 372 KB |
| `sandbox_toll_uml` | 2.79s | 275 040 KB |

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
- `tollbooth_slot` still pays 207 MB in its own TU for the session and slot states, so it is only
  worth it when the type erasure is wanted for its own sake.
- `sandbox_toll_uml` names the same SM type but only instantiates the formatter's compile-time
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
