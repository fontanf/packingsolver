# Plan B: resource limits

This change-set lets a user CAP thread/CPU usage and CAP memory, with the
DEFAULT being UNLIMITED. When the user sets nothing, behavior is identical to
before.

Two new optimize parameters per problem type (all six):

- `Counter number_of_threads = 0;` — 0 means "use all cores" (spawn every worker
  thread at once, the historical behavior). N>0 caps concurrent worker threads.
- `Counter memory_limit_megabytes = 0;` — 0 means "unlimited". N>0 stops the
  search once resident set size (RSS) reaches N MiB.

## Shared layer

New header `include/packingsolver/algorithms/thread_pool.hpp` (header-only,
C++14, no C++17 features). It provides:

### `run_in_waves(const std::vector<std::function<void()>>& tasks, Counter n)`

Bounded-concurrency executor for a set of independent tasks.

- `n <= 0` (default): spawns ALL tasks at once in their own `std::thread` and
  joins them — byte-for-byte the previous `std::vector<std::thread>` spawn/join
  loop. This is what guarantees the default path is unchanged.
- `n > 0`: runs the tasks in successive "waves" of at most `n` threads, joining
  each wave before starting the next. Simple, deterministic, no semaphores.

Each task is required to be exception-safe (never throw). All call sites satisfy
this because every task body is the existing `wrapper<F, f>(exception_ptr, ...)`
helper from `algorithms/common.hpp`, which already catches any exception into a
per-task `std::exception_ptr` stored in the function-local
`std::forward_list<std::exception_ptr> exception_ptr_list`. After
`run_in_waves` returns, each optimize function still walks that list and
`std::rethrow_exception`s the first non-null pointer, exactly as before.

Why this is behavior-equivalent to the old code when uncapped: the tasks were
already independent; the only shared mutable state they touch is the solution
pool, which is mutex-guarded (`AlgorithmFormatter::update_solution`, and the
`SolutionPool`), and the per-task `std::exception_ptr` (each task writes only its
own node; `std::forward_list` never invalidates references to existing nodes on
`push_front`, so the references captured eagerly at task-creation time stay
valid).

### `std::size_t current_memory_megabytes()`

Portable current-RSS reader:

- `_WIN32`: `GetProcessMemoryInfo` / `PROCESS_MEMORY_COUNTERS.WorkingSetSize`
  (requires linking **psapi** — see CMake note below).
- `__APPLE__`: `task_info(MACH_TASK_BASIC_INFO)` → `resident_size`.
- else (Linux/other): parse `/proc/self/statm` (resident pages × page size).

Returns 0 if it cannot be determined.

### `bool memory_limit_reached(Counter memory_limit_megabytes)`

Returns false immediately when the limit is 0 (default) — so it is a no-op on
the default path. Otherwise compares `current_memory_megabytes()` against the
limit. It is only called at the existing cooperative stop checkpoints (the
coarse anytime loops), never per search node, so it is cheap.

## How the caps are wired

### Thread cap
Each `src/<type>/optimize.cpp` collected its workers as
`std::vector<std::thread>` and spawned them with
`threads.push_back(std::thread(wrapper<...>, std::ref(exception_ptr_list.front()), ...))`.
Each such spawn/join loop was refactored to instead collect
`std::vector<std::function<void()>> tasks` of lambdas that call the same
`wrapper<...>(exception_ptr, ...)`, and the join loop was replaced by
`run_in_waves(tasks, parameters.number_of_threads)`. Argument-passing semantics
were preserved: things the old code passed to `std::thread` by value
(`ibs_parameters_list[i]`, branching-scheme parameters, the new-solution
callback) are captured by value; things passed by `std::ref`
(`branching_schemes[i]`, `instance`, `parameters`, `algorithm_formatter`, the
`exception_ptr` node) are captured by reference. The `NotAnytimeSequential`
branch (which ran inline, single-threaded) is left exactly as it was.

### Memory cap
At the coarse anytime stop checkpoints — the loops that re-grow the queue size
and check `if (parameters.timer.needs_to_end()) break;` — a check was added:

```cpp
if (memory_limit_reached(parameters.memory_limit_megabytes)) {
    algorithm_formatter.end_boolean() = true;
    break;
}
```

Setting `algorithm_formatter.end_boolean() = true` is the existing global stop
signal: every worker registers `timer.add_end_boolean(&algorithm_formatter.end_boolean())`,
so flipping it stops all cooperating threads. The same checkpoint is also wired
into the irregular tree-search worker iteration loop (the most impactful loop for
the user's product).

### CLI
Each `src/<type>/main.cpp` gained two options matching the existing
short-name-less style:

```
("threads,", po::value<Counter>(), "Maximum number of threads (default: all cores)")
("memory-limit-mb,", po::value<Counter>(), "Memory limit in mebibytes (default: unlimited)")
```

mapped right after `optimization-mode`:

```cpp
if (vm.count("threads"))
    parameters.number_of_threads = vm["threads"].as<Counter>();
if (vm.count("memory-limit-mb"))
    parameters.memory_limit_megabytes = vm["memory-limit-mb"].as<Counter>();
```

When the flags are absent, the params stay 0 → unlimited → current behavior.

## Per-type coverage status

| Type                 | Param | Thread cap (optimize.cpp) | Memory cap | CLI | CMake psapi |
|----------------------|-------|---------------------------|------------|-----|-------------|
| irregular            | yes   | yes (both spawn loops)    | yes (TS worker loop + SSK + DS) | yes | yes |
| rectangle            | yes   | yes (TS loop + dispatch)  | yes (SVC + DS loops) | yes | yes |
| box                  | yes   | yes (TS, TS-maximal-spaces, dispatch) | yes (SSK + SVC loops) | yes | yes |
| boxstacks            | yes   | yes (single TS loop)      | partial (no anytime growth loop; relies on thread cap + timer) | yes | yes |
| onedimensional       | yes   | yes (TS loop + dispatch)  | yes (SSK + SVC loops) | yes | yes |
| rectangleguillotine  | yes   | yes (TS loop + dispatch)  | yes (SSK + DS loops) | yes | yes |

### Intentionally NOT modified
`src/rectangleguillotine/column_generation_2.cpp` spawns at most 2 threads
(vertical + horizontal) using its own `ColumnGeneration2Parameters` type, which
does not carry `number_of_threads`. It was left as-is to keep behavior
byte-identical and avoid touching an unrelated parameter struct; it spawns a
fixed, bounded 2 threads regardless of the cap. (Note: it also has a pre-existing
upstream quirk where both threads share the single front() exception_ptr node —
not introduced or fixed here.)

### Memory-cap coverage caveat
The fine-grained beam-search inner loop lives in the external `treesearchsolver`
dependency (`iterative_beam_search_2`), which only checks `timer.needs_to_end()`
and cannot be modified from this repo. The memory cap therefore takes effect at
the next coarse checkpoint (end of an IBS run / next queue-growth iteration), not
mid-beam. For very large single beams this means RSS can overshoot the limit
before the search yields. This is the documented residual.

## How defaults preserve current behavior

- Both params default to `0`. `run_in_waves(tasks, 0)` takes the unlimited branch
  that spawns all tasks at once and joins — identical to the old loop.
- `memory_limit_reached(0)` short-circuits to `false`, so the added checkpoints
  are no-ops.
- CLI flags are optional; when absent the params remain 0.
- The `NotAnytimeSequential` inline paths are untouched.

## Residual compile risk

1. **psapi linkage (Windows).** `GetProcessMemoryInfo` needs `psapi`. Each
   per-type `CMakeLists.txt` now adds `if (WIN32) target_link_libraries(<lib>
   PUBLIC psapi) endif()`. If a consumer links the static libs directly with a
   custom build, they must also link psapi.
2. **Header include order.** `thread_pool.hpp` includes `<windows.h>` then
   `<psapi.h>` only under `_WIN32`. `<windows.h>` can be heavy / macro-polluting
   (e.g. `min`/`max`); it is included after the standard headers and after
   `common.hpp` to minimize interference, but if a future TU is sensitive to
   `windows.h` macros this could need `WIN32_LEAN_AND_MEAN` / `NOMINMAX`.
3. **Lambda capture of reference locals.** The refactor captures
   `BranchingScheme& branching_scheme` (a reference local) by reference; this is
   valid C++14 (binds to the referent vector element, which outlives
   `run_in_waves`). Confirmed the branching-scheme vectors are not mutated during
   the spawn loop.
4. Could not compile in this environment (no toolchain / submodules here), so the
   above is by-construction review, not a verified build.

## Fix: deferred-task concurrency regression

The thread-cap refactor (above) replaced the old per-algorithm `std::thread`
spawn-during-loop with collecting lambdas into `tasks` and running them later via
`run_in_waves`. To keep one algorithm on the main thread, each top-level
`optimize(...)` algorithm-selection block had an extra guard
`&& last_algorithm != N`, where `last_algorithm` was the highest-enabled
algorithm index. That index was computed even in anytime modes, so in Anytime
mode the highest-enabled algorithm took the `else` (inline) path and ran
*before* `run_in_waves` — consuming the whole anytime time budget. The remaining
algorithms were only collected into `tasks` and did not start until
`run_in_waves` ran after the loop, by which point no time was left. Net effect:
in Anytime mode only one algorithm effectively ran. (Originally the others were
real `std::thread`s spawned during the loop, so they ran concurrently with the
inline one — no regression.)

Fix, applied consistently in all top-level `optimize(...)` selection blocks
(`src/{box,irregular,onedimensional,rectangle,rectangleguillotine}/optimize.cpp`):
each guard

```cpp
if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential
        && last_algorithm != N) {
```

became

```cpp
if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
```

so EVERY enabled algorithm now goes through `tasks` in non-sequential modes and
they all start together in `run_in_waves`. The `else`/inline path is taken only
in `NotAnytimeSequential` mode, where everything correctly runs inline and
sequentially. This made the `last_algorithm` variable unused, so its declaration
was removed in each of those five files (verified by grep: no remaining
references). `boxstacks/optimize.cpp` never had this pattern — its single
guide×direction beam-search loop already collects every unit as a task with no
inline-during-loop unit — so it was left unchanged.

The per-strategy worker loops (e.g. the guide×direction loop in
`optimize_tree_search`) were already correct: they push every unit into `tasks`
and only run inline under `NotAnytimeSequential`. They were not touched. The
`exception_ptr_list` rethrow after `run_in_waves` is unchanged.

With `number_of_threads == 0` (default) `run_in_waves` spawns all tasks at once,
so all enabled algorithms run concurrently for the full budget — identical
results to the original; the only difference vs the original is that the
previously-inline algorithm now runs in its own thread while the main thread
waits inside `run_in_waves`. With `number_of_threads > 0` they run in joined
waves of that size.

Also included here: `include/packingsolver/algorithms/thread_pool.hpp` now
defines `NOMINMAX` and `WIN32_LEAN_AND_MEAN` before `#include <windows.h>` on
Windows, preventing the `min`/`max` macros from clashing with the solver's heavy
`std::min`/`std::max` use (this resolves residual compile risk #2 above).

## Build + test steps

From the worktree root
`C:/Users/Asus/Documents/VsCode/Mountain/Korustan-Metal/packingsolver-wt-resource`:

```bash
# Configure (fetches submodule deps via CMake/Bazel as usual)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DPACKINGSOLVER_BUILD_MAIN=ON
cmake --build build --config Release
```

On Windows the per-type libraries now link `psapi` automatically.

Smoke tests (defaults = unchanged behavior):

```bash
# Default run (unlimited threads + memory) — must match pre-change output
./build/.../packingsolver_irregular -i data/<instance> -t 5

# Cap threads to 2
./build/.../packingsolver_irregular -i data/<instance> -t 5 --threads 2

# Cap memory to 512 MiB
./build/.../packingsolver_irregular -i data/<instance> -t 60 --memory-limit-mb 512
```

Repeat for `packingsolver_rectangle`, `packingsolver_box`,
`packingsolver_boxstacks`, `packingsolver_onedimensional`,
`packingsolver_rectangleguillotine`. Verify `--threads N` reduces concurrent
thread count (e.g. via Task Manager / `top`) and `--memory-limit-mb N` ends the
run once RSS approaches the limit, while a run with neither flag is identical to
the previous build.
```
