# Compilation Performance Benchmarks

Compilation time and peak memory (Max RSS) metrics for `nil-sml` across clean Release builds and revisions.

---

## 1. Scope of Benchmarks

All four benchmark sandboxes share the exact same state machine topology ([sandbox/benchmark_sm.hpp](sandbox/benchmark_sm.hpp)) exercising all library features:
- **Hierarchical & Orthogonal Regions**: Nested composite states and parallel regions.
- **Full Action Suite**: `TransitTo<T>`, `Forward`, `Discard`, `Defer`, `DeferTo<T>`, `Emit<E>`, `Terminate`, and `std::variant`.
- **Hooks & Captures**: `on_enter()`, `on_exit()`, `on_regions_finalized()`, and `on_capture()`.
- **Dependency Injection**: Ancestor `props`, descendant `args`, and `direct_parent`.

### Benchmark Variants

- **Benchmark 1 (Plain SM)** ([sandbox/benchmark_1_plain.cpp](sandbox/benchmark_1_plain.cpp)):
  - **Scope**: Compiles standalone State Machine instantiation (`DefaultSM<Root>`) and runtime event dispatch.
  - **Focus**: Isolates template instantiation overhead of states, orthogonal/nested regions, lifecycle hooks, and action dispatch.
- **Benchmark 2 (SM + Validate)** ([sandbox/benchmark_2_validate.cpp](sandbox/benchmark_2_validate.cpp)):
  - **Scope**: Compiles State Machine instantiation + compile-time validation via `static_assert(nil::sm::validate<API, Root>())`.
  - **Focus**: Measures compile-time evaluation and constant-expression overhead of graph validation.
- **Benchmark 3 (Build IR only)** ([sandbox/benchmark_3_ir.cpp](sandbox/benchmark_3_ir.cpp)):
  - **Scope**: Compiles standalone IR generation (`nil::sm::ir::build<API, Root>()`).
  - **Focus**: Measures compile-time and runtime cost of graph reflection and metadata introspection without state machine execution.
- **Benchmark 4 (SM + Build IR)** ([sandbox/benchmark_4_sm_ir.cpp](sandbox/benchmark_4_sm_ir.cpp)):
  - **Scope**: Compiles both State Machine instantiation and IR build in the same translation unit.
  - **Focus**: Measures composite template and memory overhead when runtime state machine and IR model coexist.

---

## 2. Feature Benchmark Comparison Matrix (by Commit)

Standalone front-end compilation metrics (`g++ -c` with Release flags, averaged over 3 clean runs). Each cell reports `Avg Wall Time / Avg User CPU` and `Avg Peak RSS`.

| Commit | Date | Benchmark 1<br>(Plain SM) | Benchmark 2<br>(SM + Validate) | Benchmark 3<br>(Build IR only) | Benchmark 4<br>(SM + Build IR) | Notes |
|:---|:---|:---|:---|:---|:---|:---|
| `98a2c6c` | 2026-09-12 | 0.98 s / 0.91 s<br>231.1 MB (236,668 KB) | 1.13 s / 1.00 s<br>267.2 MB (273,612 KB) | 0.71 s / 0.66 s<br>183.7 MB (188,140 KB) | 1.22 s / 1.09 s<br>273.6 MB (280,164 KB) | Baseline (flat vs initial `ff18492` measurement); single-run, not 3-run averaged |
| `9bf9cde` | 2026-09-13 | 0.80 s / 0.71 s<br>199.2 MB (204,009 KB) | 1.01 s / 0.92 s<br>235.1 MB (240,701 KB) | 0.76 s / 0.70 s<br>183.8 MB (188,192 KB) | 1.12 s / 1.01 s<br>240.6 MB (246,352 KB) | Replaced `std::visit` with a manual index-based `visit()` and replaced `std::variant` with `nil::xalt::tagged_union` (trivially-copyable tagged union, no libstdc++ visit/variant class machinery) for the internal `on_event_t`/`on_enter_t`/`on_exit_t`/`on_regions_finalized_t` action types.<br>3-run average; versus `98a2c6c`:<br>- B1 **-31.9 MB (-13.8%)**<br>- B2 **-32.1 MB (-12.0%)**<br>- B3 +0.1 MB (~0%, unaffected — no `SM`/dispatch instantiation)<br>- B4 **-33.0 MB (-12.1%)** |

---

## 3. Parallel Clean Build History (`ninja -C .build`)

Comparable full test suite and sandbox builds with default parallelism (3 clean runs averaged per commit):

| Commit | Date | Key Changes / Scope | Avg Wall Time | Avg CPU Time | Avg Peak RSS | $\Delta$ Peak RSS vs Baseline |
|:---|:---|:---|---:|---:|---:|---:|
| `933d993` | 2026-09-08 | Baseline (`origin/master` barrier layout) | 25.25 s | 195.61 s | 457.6 MB (468,629 KB) | *Baseline* |
| `41d78c4` | 2026-09-13 | Simplified API policy and separate `Unhandled` boundary checks (last commit before dispatch-internals optimization) | 28.73 s | 230.71 s | 456.6 MB (467,512 KB) | **-1.1 MB (-0.24%)** |
| `9bf9cde` | 2026-09-13 | Centralized state validation.<br>Replaced `std::visit` with a manual index-based `visit()`.<br>Replaced `std::variant` with `nil::xalt::tagged_union` for `on_event_t`/`on_enter_t`/`on_exit_t`/`on_regions_finalized_t`. | 30.78 s | 262.87 s | 406.6 MB (416,409 KB) | <br>- **-52.7 MB (-11.5%)** vs `41d78c4`<br>- **-51.0 MB (-11.1%)** vs baseline |

> **Note on Wall Time**: Later revisions include additional sandbox targets, diagram rendering formats, and new test translation units.

---

## 4. Build Environment & Methodology

- **Compiler**: GCC (`g++`)
- **Flags**: `-O3 -DNDEBUG -std=gnu++20 -fno-rtti -flto -Wfatal-errors -Wshadow -Werror -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion`
- **Build System**: Ninja (`.build`)
- **Measurement Tool**: `/usr/bin/time -f "wall=%e user=%U sys=%S maxrss=%M KB"`
- **Reproducing Sandbox Benchmarks**:
  ```bash
  /usr/bin/time -f "wall=%e user=%U sys=%S maxrss=%M KB" \
    g++ -Isrc/publish -isystem .build/vcpkg_installed/x64-linux/include/nil-xalt/1.4.5 \
    -O3 -DNDEBUG -std=gnu++20 -fno-rtti -flto -Wfatal-errors \
    -c sandbox/benchmark_1_plain.cpp -o /dev/null
  ```
