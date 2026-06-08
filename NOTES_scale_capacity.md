# Plan A: polygon processing capacity — change notes

Branch: `feat/scale-capacity`. Scope: raise the ceiling so the solver handles
larger projects (more part/bin types, more item copies) without silent overflow
or pathological memory blowups. Changes are additive and behavior-preserving for
existing valid inputs. The environment cannot compile (deps are FetchContent and
not present), so changes were made correct-by-construction and matched to the
surrounding style.

## What changed

### 1. Widen ID integer widths (verified, implemented)
File: `include/packingsolver/algorithms/common.hpp`

- `ItemTypeId`: `int16_t` -> `int32_t`  (line 24)
- `GroupId`:    `int16_t` -> `int32_t`  (line 26)
- `BinTypeId`:  `int16_t` -> `int32_t`  (line 36)
- `DefectId`:   `int16_t` -> `int32_t`  (line 38)

Rationale: these aliases are the loop/index/`.size()` types for the number of
item types, bin types and defects. With `int16_t` the count silently truncates
above 32767 (e.g. `number_of_item_types()` returns `int16_t` from a vector
`.size()`), capping the number of distinct part shapes a project can have. They
are pure type aliases used consistently as plain integers, so int16 -> int32 is
value-preserving for every input that fits in int16 (i.e. all current inputs).

Blast-radius verification (all over the project tree, `extern/` excluded):
- No `std::numeric_limits<ItemTypeId/BinTypeId/DefectId/GroupId>` assumptions.
- No binary serialization of these IDs (`reinterpret_cast`/`memcpy`/`fread`/
  `fwrite` — none found).
- No printf/scanf width specifiers (`%hd`/`%hi`/`PRId16`) — code uses
  `std::to_string`, `operator<<`, and nlohmann::json, all width-agnostic.
- No bit-packing of these IDs (no `#pragma pack`, no `<<16`/`0xffff` masks on
  IDs). The `int8_t`/`uint8_t` hits found are unrelated (`new_bin` flags,
  reachability bitmaps, `quality_rules`).
- `std::hash<ItemTypeId>` is used in `boxstacks/branching_scheme.cpp` and
  `box/block.cpp`; `std::hash` has standard specializations for both int16 and
  int32, so this is fine.
- No narrowing casts `(int16_t)` / `static_cast<int16_t>` / `(short)` anywhere
  outside `extern/`.

NOT widened (deliberately left at int16):
- `Depth`, `GuideId` (line 44-45): tree-search internals (search depth / guide
  selector), unrelated to part/bin count and potentially memory-layout
  sensitive in hot-path node structs. Not named by the task; left alone.
- `ItemPos`/`BinPos` already int32/int64; unchanged.

### 2. Input-scale overflow guards (verified, implemented)
File: `src/irregular/instance_builder.cpp`, in `InstanceBuilder::build()`

- Item-count overflow guard (around line 634, before `number_of_items_ +=
  item_type.copies`): throws `std::runtime_error` with the existing
  `FUNC_SIGNATURE + ": " ...` style if adding an item type's `copies` would
  overflow `ItemPos` (int32). Without it the running item total wraps silently
  and corrupts every downstream area/profit/bound computation.

- Bin-materialization guard (around line 723, after `copies == -1` is resolved
  to `number_of_items()` and BEFORE the dense
  `for (BinPos copy = 0; copy < bin_type.copies; ++copy)` loop that pushes one
  entry per bin into `bin_type_ids_` and `previous_bins_area_`): throws if the
  cumulative number of bins to materialize exceeds `100000000`, or would
  overflow that cap when added. This stops a pathological `copies` value from
  exhausting memory before any solving begins. The cap is far above any
  realistic instance and does not affect valid inputs. The infinite-copies
  path (`copies == -1` -> `number_of_items()`) is itself already bounded by the
  item-count guard above.

These two are the only places in the irregular builder where a count drives a
dense allocation or an integer accumulation that can overflow.

## What was investigated and intentionally skipped

### 3. Per-shape vertex / approximation knob (investigated, NOT implemented)
Files: `src/irregular/shape_simplification.cpp`,
`src/irregular/branching_scheme.hpp` (`maximum_approximation_ratio = 0.05`),
`include/packingsolver/irregular/optimize.hpp`
(`initial_maximum_approximation_ratio = 0.20`, factor `0.75`,
`not_anytime_maximum_approximation_ratio = 0.05`).

`shape_simplification()` converts `maximum_approximation_ratio` into a single
global *area* budget (`maximum_approximation_area = ratio * min(total_bin_area,
total_item_area)`) and hands all shapes to the upstream `shape::simplify(...)`.
There is no per-shape vertex cap at this layer. A per-shape vertex bound would
require either (a) modifying upstream `shape::simplify` (lives in the fetched
`fontanf/shape` dependency, not in this repo) or (b) post-filtering vertices
here, which WOULD change results for current inputs by default. Per the task's
"keep behavior identical" constraint, no change was made. Opportunity: expose an
optional `maximum_vertices_per_shape` parameter that, when set (default = no
limit), bounds per-shape complexity for huge polygons — but this needs an
upstream `shape` hook to be done safely and is therefore future work.

### Algorithmic items — future work (explicitly out of scope)
- `periodic_packing.cpp` NFP construction O(n^2)/O(n^3) behavior.
- Trapezoidation complexity in the irregular branching scheme.
These are algorithm rewrites and too risky without a benchmark harness; noted
here, not touched.

## Residual compile risk

- The ID widening could NOT be compiled in this environment. It is a pure
  type-alias change with no narrowing/serialization/packing dependency found,
  so risk is low, but it MUST be compile-verified by the maintainer. The most
  plausible (still unlikely) failure mode is an overload-resolution change where
  some call previously bound to an `int16_t`/`short` overload — none were found
  in-tree, but third-party headers were not exhaustively audited.
- `extern/packingsolver/` in the worktree is a separate, stale checkout with its
  own `build/_deps`; it is NOT part of this project's build (the project's
  `extern/CMakeLists.txt` only does FetchContent). It was intentionally left
  untouched. Only the root `include/` and `src/` trees were edited.

## How to build and test

From the worktree root
(`C:/.../packingsolver-wt-scale`):

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Run the unit tests (CMake builds them by default,
`PACKINGSOLVER_BUILD_TEST=ON`):

```
ctest --test-dir build --output-on-failure
```

Targeted checks for these changes:
- The irregular test suite (`test/irregular/irregular_test.cpp`) should pass
  unchanged — the guards do not trigger on valid instances and the ID widening
  is value-preserving.
- Optional manual overflow check: build an irregular instance with a bin
  `copies` above 100000000 and confirm a clear `std::runtime_error` is thrown by
  `InstanceBuilder::build()` instead of an OOM/silent wrap.
