# Plan C — thread/async robustness + cross-OS hardening

Branch: `feat/async-robustness`. C++14 only. No algorithm changes; correctness/robustness hardening only.

All findings below were verified against the actual source before any change was made.
The upstream scan was partly self-contradictory (notably it confused the
`AlgorithmFormatter`'s `std::mutex mutex_` with the `SolutionPool`); each item is
reported as REAL or NOISE with code evidence.

---

## Finding 1 — `new_solution_callback` invoked while holding the mutex — REAL, FIXED

### Evidence
In every `src/*/algorithm_formatter.cpp`, the `update_*` methods used manual
`mutex_.lock()` / `mutex_.unlock()` and called the user callback **inside** the
critical section, e.g. (rectangle, original):

```cpp
mutex_.lock();
...
int new_best = output_.solution_pool.add(solution);
if (new_best == 1) {
    print(s);
    output_.json["IntermediaryOutputs"].push_back(output_.to_json());
    parameters_.new_solution_callback(output_);   // <-- heavy I/O under lock
    ... // optimality check, end_ = true
}
mutex_.unlock();
```

The callback really does file I/O. See `src/rectangle/main.cpp` (and every other
`main.cpp`):

```cpp
parameters.new_solution_callback = [json_output_path, certificate_path](
        const packingsolver::Output<Instance, Solution>& output) {
    if (!json_output_path.empty())
        output.write_json_output(json_output_path);             // disk write
    if (!certificate_path.empty())
        output.solution_pool.best().write(certificate_path);    // disk write
};
```

Because all worker threads funnel every new solution through this single mutex,
each thread is blocked on another thread's **disk I/O** (JSON + certificate
writes that grow with `IntermediaryOutputs`). This serializes the workers and is
the most plausible source of the "CPU lock-up / poor scaling" symptom.

There is also a latent **exception-safety** bug in the original manual locking:
if `to_json()`, `push_back`, the callback, or `print()` throws, the `mutex_` is
never unlocked → permanent deadlock the next time any thread calls `update_*`.

### Fix
For every `update_*` method in all six formatters
(`box`, `boxstacks`, `irregular`, `onedimensional`, `rectangle`,
`rectangleguillotine`):

1. Replaced manual `mutex_.lock()/unlock()` with RAII
   `std::unique_lock<std::mutex> lock(mutex_);` — exception-safe; the mutex is
   always released even if something throws.
2. Kept **all** shared-state mutation (`solution_pool.add`, JSON `push_back`,
   optimality check, `end_ = true`, and `print()` to the log stream) under the
   lock — ordering and log interleaving are preserved.
3. Take a **copy** of `output_` into a local `output_snapshot` under the lock,
   then `lock.unlock()` and invoke `new_solution_callback(output_snapshot)`
   **outside** the critical section. The callback (and its disk I/O) no longer
   blocks the other workers, and it observes a stable snapshot rather than a
   reference that could be mutated concurrently.

`print()` was deliberately left inside the lock: it is fast formatted output to
the shared log stream, and keeping it under the lock preserves correct,
non-interleaved log lines (a behavior guarantee). Only the heavy file-I/O
callback was moved out.

### Why the copy is safe / cheap enough
`packingsolver::Output<Instance, Solution>` is copyable: its members are a
`nlohmann::json` (base class), a `SolutionPool` (a `std::set<Solution>` plus two
`Solution`s), and PODs. `Solution` holds a `const Instance*` (pointer, not a
reference — see `include/packingsolver/*/solution.hpp`), so both copy-construction
and copy-assignment are implicitly available. The snapshot is constructed with
`Output<Instance, Solution> output_snapshot(instance_);` using the existing
`Output(const Instance&)` constructor and then assigned `= output_` only on the
path that actually fires the callback.

The type is written fully-qualified as `packingsolver::Output<Instance, Solution>`
to avoid any ambiguity with the **derived** `rectangle::Output` /
`box::Output` / … defined in each `optimize.hpp` (those derive from
`packingsolver::Output<Instance, Solution>`). `output_` itself is declared as the
base template type in the formatter, so slicing is not a concern — the snapshot is
the same static type as `output_`.

### Residual risk
- Minor overhead: an empty `Output` (one `Solution` inserted into the pool) is
  default-constructed on every `update_*` call even when no callback fires. This
  is small and bounded; it buys clarity and exception-safety.
- The snapshot copy itself is real work on the hot path, but it now happens under
  the lock *instead of* the much larger disk I/O, so net contention drops
  sharply.
- Not compiled here (no toolchain in the worktree). The edits are mechanical and
  uniform across all six files; see build/test steps below.

---

## Finding 2 — `end_` termination flag is a plain `bool` read unlocked by other threads — REAL race, NOT "fixed" (documented; cannot fix without external API change)

### Evidence
`include/packingsolver/*/algorithm_formatter.hpp`:

```cpp
bool& end_boolean() { return end_; }   // returns a non-const reference
...
bool end_ = false;
std::mutex mutex_;
```

`end_` is **written** under `mutex_` (inside `update_*`) but **read without any
lock** in two ways:

1. Directly in the `optimize.cpp` driver loops, e.g.
   `if (algorithm_formatter.end_boolean()) break;`
2. Indirectly via the external timer. Every `optimize.cpp` passes the address:
   `ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());`

The external timer (`optimizationtools/utils/timer.hpp`, fetched via CMake
FetchContent, **not editable here**) stores and dereferences it as a raw
`const bool*`:

```cpp
Timer& add_end_boolean(const bool* end) { end_.push_back(end); return *this; }
...
bool needs_to_end() const {
    for (const bool* end : end_)
        if (*end) return true;            // unsynchronized read from worker threads
    ...
}
private:
    std::vector<const bool*> end_;
```

So a plain `bool` is written under a mutex and read concurrently without one →
a data race / UB per the C++ memory model (works in practice on all mainstream
CPUs because aligned `bool` load/store is atomic, but it is still UB and a real
defect).

### Why it was NOT changed to `std::atomic<bool>`
The obvious fix — make `end_` a `std::atomic<bool>` — **breaks the external API**:

- `add_end_boolean` takes `const bool*`. `&end_boolean()` would become
  `std::atomic<bool>*`, which does **not** convert to `const bool*`.
- `std::atomic<bool>` is not guaranteed layout-compatible with `bool`, so even a
  reinterpret-cast would be non-portable, and the timer reads it as a plain
  `bool` anyway (a non-atomic read of an atomic object is itself UB).

The timer lives in `optimizationtools`, which this change-set is not allowed to
modify. Per the task instruction ("Confirm the timer API accepts the address of
an atomic; if not, document the limitation instead of forcing it"), the flag was
**left as a plain `bool`** and the limitation is documented here.

### Precise recommendation (requires an upstream/external change — out of scope here)
The correct, portable fix must be made in `optimizationtools`:

1. Change the timer to store `std::vector<const std::atomic<bool>*>` and read
   with `end->load(std::memory_order_relaxed)`; OR
2. Add an overload `add_end_boolean(const std::atomic<bool>*)`.

Then, in this repo: make `end_` a `std::atomic<bool>`, change
`end_boolean()` to return `std::atomic<bool>&` (or split into a
`bool end() const { return end_.load(); }` getter for the direct
`if (algorithm_formatter.end_boolean())` reads plus an `end_address()` for the
timer). All `optimize.cpp` direct reads and the `add_end_boolean(...)` calls
would be updated accordingly. This is a coordinated multi-repo change and is
deliberately not attempted here.

### Residual risk
The race remains (benign on mainstream hardware, UB by the standard). It is the
biggest residual concurrency risk in this change-set and is gated on an external
library change.

---

## Finding 3 — `SolutionPool::add` thread-safety — NOISE (scan was wrong); NO change

### Evidence
`include/packingsolver/algorithms/common.hpp`: `SolutionPool` has **no** mutex.
The scan's claim that it had a `std::mutex mutex_` member was a confusion with
`AlgorithmFormatter::mutex_`. It also contains a misleading dead comment
`// Check again after mutex lock.` that duplicates the size check — left in place
(the task forbids removing dead/commented code).

However, `SolutionPool::add` does **not need** its own mutex, because every caller
already serializes access:

- The shared pool is `output_.solution_pool`, mutated **only** inside
  `AlgorithmFormatter::update_solution`, which holds `mutex_` around the
  `solution_pool.add(...)` call. (Confirmed in all six formatters.)
- The other `solution_pool.add(...)` call sites in `optimize.cpp`
  (e.g. `outputs[i].solution_pool.add(...)` in the `NotAnytimeDeterministic`
  path) operate on **per-thread, thread-local** `outputs[i]` objects — one
  distinct `Output` per worker index `i`, never shared between threads.
- `post_process.cpp` calls run single-threaded after the parallel phase.

So adding a mutex to `SolutionPool` would be redundant locking with no
correctness benefit and a (small) performance cost. **No change made.** The scan
was wrong on this item.

---

## Finding 4 — threads can outlive the stack on an exception before the join loop — REAL (latent), FIXED

### Evidence
Each `optimize.cpp` (and the nested `optimize_tree_search` helpers) does:

```cpp
std::vector<std::thread> threads;
std::forward_list<std::exception_ptr> exception_ptr_list;
... // loop: exception_ptr_list.push_front(...); threads.push_back(std::thread(...));
for (Counter i = 0; i < (Counter)threads.size(); ++i)
    threads[i].join();
for (... exception_ptr ...) if (exception_ptr) std::rethrow_exception(...);
```

If an exception is thrown **after** one or more threads are already in `threads`
but **before** the explicit join loop runs — e.g. `std::bad_alloc` from
`exception_ptr_list.push_front`, from a `std::stringstream`/lambda allocation, or
from `threads.push_back` / `std::thread` construction failing — the
`std::vector<std::thread>` is destroyed while still holding **joinable** threads,
which calls `std::terminate()` (hard crash, no cleanup). This is a genuine
robustness defect (rare: only under allocation / thread-spawn failure).

### Fix
Added a tiny RAII helper in `include/packingsolver/algorithms/common.hpp`:

```cpp
struct ThreadJoinGuard {
    std::vector<std::thread>& threads;
    ThreadJoinGuard(std::vector<std::thread>& threads): threads(threads) { }
    ~ThreadJoinGuard() {
        for (std::thread& thread : threads)
            if (thread.joinable())
                thread.join();
    }
};
```

and declared one guard immediately after every
`std::vector<std::thread> threads;` (12 sites across all six `optimize.cpp`
files):

```cpp
std::vector<std::thread> threads;
ThreadJoinGuard thread_join_guard(threads);
```

`common.hpp` now also `#include <thread>` and `#include <vector>` (it is reached
by every `optimize.cpp` via `instance.hpp`; all `optimize.cpp` already
`#include <thread>` too).

### Why behavior is preserved
On the normal path the existing explicit join loop joins every thread first, so by
the time the guard's destructor runs, no thread is `joinable()` → the destructor
is a no-op. The guard only acts on the exceptional unwind path, turning a
`std::terminate()` into an orderly "join then propagate" (the stored
`exception_ptr` rethrow / the in-flight exception continues to propagate after the
threads are joined). No double-join (guarded by `joinable()`), no ordering change
on success.

### Residual risk
- Not compiled here. `ThreadJoinGuard` is in `namespace packingsolver`; every
  `optimize.cpp` has `using namespace packingsolver;`, so the unqualified name
  resolves. Verified no other `std::thread` usage exists outside these guarded
  vectors.
- If a thread's work itself never returns (hangs), the guard would block in
  `join()` during unwinding — but that identical blocking already exists in the
  normal join loop, so this is not a new failure mode.

---

## Finding 5 — cross-OS / SIGINT / stdout-stderr flushing — NOISE for this repo; documented

### Evidence
- `set_sigint_handler()` is defined in the external `optimizationtools` Timer
  (`Timer& set_sigint_handler();`), **not** in this repo — not editable here.
- This repo's code contains **no** platform `#ifdef`s, no `fork`/`popen`, no
  `_WIN32` / `__linux__` / `__APPLE__` branches (grep over `src` and `include`
  finds none except the unrelated `_USE_MATH_DEFINES` / `M_PI` guard).
- All driver/log output uses `std::endl`, which flushes. The header banner,
  per-improvement `print()` rows, and final statistics therefore flush as they
  are written. Errors go to `std::cerr` (unbuffered by default) with `std::endl`.
- The Python wrapper captures stdout/stderr and kills the process to cancel;
  process termination and stream teardown are handled by the OS identically on
  Windows/macOS/Linux. There is no Windows-vs-POSIX assumption in this repo's
  own code to fix.

**No change made.** Any improvement to SIGINT handling or output buffering would
have to be made in `optimizationtools`, which is out of scope. If the wrapper ever
needs a guaranteed flush on hard-kill, the recommendation is for the wrapper to
send SIGINT/terminate gracefully and let the existing `std::endl`-flushed log
drain, rather than `SIGKILL`.

---

## Summary of files changed

- `include/packingsolver/algorithms/common.hpp`
  - added `#include <thread>`, `#include <vector>`
  - added `struct ThreadJoinGuard` (RAII join-on-unwind helper)
- `src/{box,boxstacks,irregular,onedimensional,rectangle,rectangleguillotine}/algorithm_formatter.cpp`
  - all `update_*` methods: manual lock → `std::unique_lock`; callback (file I/O)
    moved out of the critical section via an `output_snapshot` copy
- `src/{box,boxstacks,irregular,onedimensional,rectangle,rectangleguillotine}/optimize.cpp`
  - one `ThreadJoinGuard thread_join_guard(threads);` after each
    `std::vector<std::thread> threads;` (12 sites total)

No header public signatures changed; no algorithm logic changed; no
dead/commented code removed.

---

## Build & test steps (cannot be run in this worktree — no toolchain)

From the worktree root
`C:/Users/Asus/Documents/VsCode/Mountain/Korustan-Metal/packingsolver-wt-async`:

```shell
# Configure + build (Release). FetchContent pulls boost, optimizationtools,
# shape, HiGHS, etc. on first configure.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix install
```

Tests (the project's CMake fetches googletest when `PACKINGSOLVER_BUILD_TEST`
is on):

```shell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPACKINGSOLVER_BUILD_TEST=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Targeted sanity run (any solver binary), e.g. rectangleguillotine:

```shell
./install/bin/packingsolver_rectangleguillotine \
    --items <instance>_items.csv --bins <instance>_bins.csv \
    --objective bin-packing-with-leftovers \
    --certificate sol.csv --output out.json --time-limit 5 --verbosity-level 2
```

What to verify after building:
1. Results/objective values are unchanged vs the previous build (behavior
   preservation) on a handful of `data/` instances across all solvers.
2. With `--output` + `--certificate` and multiple algorithms enabled (the default
   Anytime mode that spawns threads), CPU utilization across cores is no longer
   pinned/serialized on the callback's disk writes — wall-clock improvement is
   most visible when the output/certificate paths are set and the instance yields
   many intermediary solutions.
3. No deadlock if a callback throws (it won't with the default callbacks, but the
   `std::unique_lock` now guarantees release).
