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
| `ff18492` | 2026-09-12 | 0.93 s / 0.83 s<br>233.1 MB (238,727 KB) | 1.14 s / 1.04 s<br>269.6 MB (276,032 KB) | 0.71 s / 0.64 s<br>186.0 MB (190,473 KB) | 1.22 s / 1.08 s<br>275.8 MB (282,469 KB) | Initial baseline covering all library features |

---

## 3. Parallel Clean Build History (`ninja -C .build`)

Comparable full test suite and sandbox builds with default parallelism (3 clean runs averaged per commit):

| Commit | Date | Key Changes / Scope | Avg Wall Time | Avg CPU Time | Avg Peak RSS | $\Delta$ Peak RSS vs Baseline |
|:---|:---|:---|---:|---:|---:|---:|
| `933d993` | 2026-09-08 | Baseline (`origin/master` barrier layout) | 25.25 s | 195.61 s | 457.6 MB (468,629 KB) | *Baseline* |
| `87fb83d` | 2026-09-09 | Revamp (`direct_parent` & cleanup) | 25.30 s | 206.43 s | 436.2 MB (446,675 KB) | **-21.4 MB (-4.68%)** |
| `cf3d691` | 2026-09-09 | More tests & docs update | 26.63 s | 214.59 s | 467.8 MB (479,019 KB) | **+10.1 MB (+2.22%)** |
| `fd49352` | 2026-09-10 | Added IR validation & error info | 29.37 s | 252.86 s | 474.0 MB (485,344 KB) | **+16.3 MB (+3.57%)** |
| `915cefb` | 2026-09-11 | BSL license update (`v0.0.1`) | 28.16 s | 240.77 s | 473.9 MB (485,233 KB) | **+16.2 MB (+3.54%)** |
| `ff18492` | 2026-09-12 | Fix API requirements & `constexpr` validation | 26.97 s | 210.97 s | 458.0 MB (468,959 KB) | **+0.3 MB (+0.07%)** |

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
