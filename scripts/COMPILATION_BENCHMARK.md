# Compilation Performance Benchmarks

Compile time and peak memory (Max RSS) of `nil-sml` in clean Release builds.

---

## 1. Benchmarks

All four share one state machine ([sandbox/benchmarks/benchmark_sm.hpp](sandbox/benchmarks/benchmark_sm.hpp)) that uses every library feature: nested and orthogonal regions, the full action suite, hooks and captures, and dependency injection.

| # | Source | Measures |
|:---|:---|:---|
| 1 Plain SM | [1_plain.cpp](sandbox/benchmarks/1_plain.cpp) | `DefaultSM<Root>` instantiation and dispatch |
| 2 SM + Validate | [2_validate.cpp](sandbox/benchmarks/2_validate.cpp) | 1 + `static_assert(nil::sm::validate<Root, API>())` |
| 3 Build IR only | [3_ir.cpp](sandbox/benchmarks/3_ir.cpp) | `nil::sm::ir::build<Root, API>()` without a state machine |
| 4 SM + Build IR | [4_sm_ir.cpp](sandbox/benchmarks/4_sm_ir.cpp) | 1 + 3 in one translation unit |

---

## 2. Latest: v1.0.1 vs Current

The current tree compiles the formatters, `Diagnostics` and `Index` once into the `nil-sm-format` static library. `nil/sm.hpp` no longer includes IR, diagnostics or formatter headers, and `validate()` lives in the light `nil/sm/validate.hpp`.

GCC 13.3, `nil-xalt` 1.4.9, each revision built from its own checkout, 3 runs averaged. Compare only within this section.

### Compile-only (`g++ -c`)

Each cell is `wall / user CPU` and peak RSS.

| Benchmark | v1.0.1 | Current | $\Delta$ Time | $\Delta$ Peak RSS |
|:---|:---|:---|---:|---:|
| 1 Plain SM | 2.23 s / 1.98 s<br>294.3 MB | 1.35 s / 1.19 s<br>201.8 MB | -39% | **-92.5 MB (-31%)** |
| 2 SM + Validate | 2.65 s / 2.35 s<br>311.7 MB | 1.77 s / 1.58 s<br>241.8 MB | -33% | **-69.8 MB (-22%)** |
| 3 Build IR only | 1.92 s / 1.70 s<br>275.7 MB | 1.28 s / 1.14 s<br>185.2 MB | -33% | **-90.5 MB (-33%)** |
| 4 SM + Build IR | 2.76 s / 2.49 s<br>319.7 MB | 1.94 s / 1.75 s<br>244.9 MB | -30% | **-74.7 MB (-23%)** |

Benchmark 2 improves least because `validate` still needs `ir.hpp`.

### Full clean build (`ninja`, tests and sandboxes, no viz)

| Revision | Wall | CPU | Peak RSS |
|:---|---:|---:|---:|
| v1.0.1 | 29.46 s | 364.4 s | 447.0 MB |
| Current | 28.48 s (-3%) | 345.4 s (-5%) | 412.1 MB (**-34.9 MB, -7.8%**) |

The gain is small: tests and sandboxes still compile their own template instantiations, and the current tree builds one more library and more tests and sandboxes. Wall time varies about ±8% on this machine, so rely on CPU time and peak RSS.

### Using the formatter and diagnostics headers

A translation unit with `<nil/sm.hpp>`, the formatter and diagnostics headers and an empty `main` (`-c`), before the library split vs now:

| Translation unit | Before | Now |
|:---|:---|:---|
| Headers only | 1.35 s / 1.19 s, 224.0 MB | 0.78 s / 0.67 s, 138.9 MB |
| `sandbox/uml.cpp` | 2.31 s / 2.06 s, 299.0 MB | 1.36 s / 1.19 s, 191.9 MB |

### Where the memory went

Before the split, two changes added the regression (single-run bisect):

- A larger `diagnostics.hpp` plus a new `index.hpp` included by the benchmark header: about +43 MB on all four benchmarks, including Benchmark 3, which has no state machine.
- `nil/sm.hpp` exporting diagnostics, IR and UML: a further ~+58 MB on Benchmarks 1 and 3, ~+39 MB on 2 and 4.

### Caveats

- One machine, one toolchain; clang and other GCC versions were not checked.
- The benchmark sources changed since the Section 3 rows (for example an `as_parent` prop), so small differences against them are not due to the library alone.

---

## 3. History (earlier toolchain)

### Compile-only (`g++ -c`, 3-run average unless noted)

| Commit | Date | B1 | B2 | B3 | B4 | Notes |
|:---|:---|:---|:---|:---|:---|:---|
| `98a2c6c` | 2026-09-12 | 0.98 s / 0.91 s<br>231.1 MB | 1.13 s / 1.00 s<br>267.2 MB | 0.71 s / 0.66 s<br>183.7 MB | 1.22 s / 1.09 s<br>273.6 MB | Baseline (single run) |
| `9bf9cde` | 2026-09-13 | 0.80 s / 0.71 s<br>199.2 MB | 1.01 s / 0.92 s<br>235.1 MB | 0.76 s / 0.70 s<br>183.8 MB | 1.12 s / 1.01 s<br>240.6 MB | Manual index-based `visit()` and `nil::xalt::tagged_union` replace `std::visit` and `std::variant` in the internal action types. B1 -31.9 MB (-13.8%), B2 -32.1 MB (-12.0%), B3 unchanged, B4 -33.0 MB (-12.1%) |
| `52a732f` | 2026-09-13 | 0.80 s / 0.69 s<br>199.2 MB | 0.98 s / 0.92 s<br>234.8 MB | 0.72 s / 0.66 s<br>183.7 MB | 1.09 s / 1.02 s<br>240.6 MB | `nil-cmake` 0.0.6 (`-flto=auto`) and `nil-xalt` 1.4.8; flat, as expected for compile-only runs with no link step |

### Full clean build (`ninja -C .build`, 3-run average)

| Commit | Date | Change | Wall | CPU | Peak RSS |
|:---|:---|:---|---:|---:|---:|
| `933d993` | 2026-09-08 | Baseline | 25.25 s | 195.61 s | 457.6 MB |
| `41d78c4` | 2026-09-13 | Simplified API policy, separate `Unhandled` checks | 28.73 s | 230.71 s | 456.6 MB |
| `9bf9cde` | 2026-09-13 | `visit()` and `tagged_union` (see above) | 30.78 s | 262.87 s | 406.6 MB (**-11.1%** vs baseline) |
| `52a732f` | 2026-09-13 | `-flto` → `-flto=auto` | 10.89 s | 248.06 s | 406.2 MB |

`52a732f` cut wall time by 64.6% for about the same CPU time: GCC passed plain `-flto` to the link step, so LTRANS ran serially regardless of `-j`. That also shows the `9bf9cde` wall-time regression was this bug, not `tagged_union`. Later revisions add sandboxes, formats and tests, so wall times are not comparable across the whole table.

---

## 4. Methodology

- **Compiler**: GCC (`g++`)
- **Flags**: `-O3 -DNDEBUG -std=gnu++20 -fno-rtti -flto -Wfatal-errors -Wshadow -Werror -Wall -Wextra -Wpedantic -pedantic-errors -Wconversion -Wsign-conversion`
- **Build system**: Ninja (`.build`)
- **Measurement**: `/usr/bin/time -f "wall=%e user=%U sys=%S maxrss=%M KB"`
- **Reproducing a compile-only benchmark**:
  ```bash
  /usr/bin/time -f "wall=%e user=%U sys=%S maxrss=%M KB" \
    g++ -Isrc/publish -isystem .build/vcpkg_installed/x64-linux/include/nil-xalt/<version> \
    -O3 -DNDEBUG -std=gnu++20 -fno-rtti -flto -Wfatal-errors \
    -c sandbox/benchmarks/1_plain.cpp -o /dev/null
  ```
- **Reproducing a full build**: `scripts/benchmark_build.sh 3`
