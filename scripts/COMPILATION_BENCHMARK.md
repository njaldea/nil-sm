# Compilation Performance Benchmarks

Compilation time and peak memory (Max RSS) metrics for `nil-sml` across clean Release builds and revisions.

---

## 1. Parallel Clean Build History (`ninja -C .build`)

Comparable full test suite and sandbox builds with default parallelism.

| Commit | Date | Key Changes / Scope | Avg Wall Time | Avg CPU Time | Avg Peak RSS | $\Delta$ Peak RSS vs Baseline |
|:---|:---|:---|---:|---:|---:|---:|
| `933d993` | 2026-09-08 | Baseline (`origin/master`) | 23.45 s | 86.64 s | 468.6 MB (468,648 KB) | *Baseline* |
| `dbb5928` | 2026-09-08 | `direct_parent` revamp | 23.46 s | 85.50 s | 447.1 MB (447,063 KB) | **-21.5 MB (-4.6%)** |
| `ff18492` | 2026-09-12 | `constexpr` IR validation & all tests | 27.32 s | 219.99 s | 469.0 MB (469,031 KB) | **+0.4 MB (+0.08%)** |

> **Note on Wall Time**: Later revisions include additional sandbox targets and new test translation units.

---

## 2. Standalone Translation Unit Benchmarks (`g++ -c`)

Isolated front-end compilation time and memory per translation unit without link/LTO overhead:

| Commit | Date | Translation Unit | Key Feature Tested | Wall Time | User CPU Time | Peak RSS |
|:---|:---|:---|:---|---:|---:|---:|
| `ff18492` | 2026-09-12 | `src/test/13_sm_compile_time_diagnostics.cpp` | `static_assert(validate<API, Root>())` | 1.17 s | 1.01 s | 276.9 MB (283,592 KB) |

---

## 3. Single-Threaded Reference Builds (`ninja -j1`)

Isolated single-core builds (sequential compilation & link) to measure per-process ceiling and total sequential CPU work:

| Commit | Date | Scope / Description | Wall Time | User CPU Time | Peak RSS |
|:---|:---|:---|---:|---:|---:|
| `36f1f0b` | 2026-08-29 | Historical baseline (earlier test suite & sandbox) | 58.94 s | — | 468.8 MB (480,044 KB) |
| `ff18492` | 2026-09-12 | Full suite + 6 sandboxes (55 build targets) | 90.70 s | 83.81 s | 458.0 MB (469,008 KB) |

---

## 4. Detailed Run Logs

### Commit `ff18492` (2026-09-12)
3 consecutive clean Release builds (`ninja -C .build -t clean && ninja -C .build`):

| Run | Wall Time | User Time | Sys Time | Peak RSS |
|:---|---:|---:|---:|---:|
| Run 1 | 27.69 s | 204.04 s | 18.58 s | 468,948 KB |
| Run 2 | 27.21 s | 202.92 s | 17.58 s | 468,832 KB |
| Run 3 | 27.05 s | 200.19 s | 16.65 s | 469,312 KB |
| **Average** | **27.32 s** | **202.38 s** | **17.60 s** | **469,031 KB (~458.0 MiB)** |

---

## 5. Build Environment & Methodology

- **Compiler**: GCC (`g++`)
- **Flags**: `-O3 -DNDEBUG -std=gnu++20 -fno-rtti -flto -Wfatal-errors -Wshadow -Werror -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion`
- **Build System**: Ninja (`.build`)
- **Measurement Command**:
  ```bash
  ./scripts/benchmark_build.sh [num_runs]
  # Or directly:
  /usr/bin/time -f "wall=%e user=%U sys=%S maxrss=%M KB" ninja -C .build
  ```
