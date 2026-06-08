#pragma once

/**
 * Plan B: resource limits.
 *
 * This header provides two small, portable, C++14-only helpers shared by all
 * problem types:
 *
 * 1. 'run_in_waves': a bounded-concurrency task executor used to CAP the number
 *    of worker threads spawned by the 'optimize' functions.  When the cap is 0
 *    (the default) it preserves the historical behavior exactly: every task is
 *    spawned in its own 'std::thread' at once and then joined.  When the cap is
 *    strictly positive it executes the (independent) tasks in successive
 *    "waves" of at most 'number_of_threads' concurrent threads, joining each
 *    wave before starting the next.
 *
 * 2. 'current_memory_megabytes': a portable reader of the current resident set
 *    size (RSS) of the process, used to CAP memory usage at the existing
 *    cooperative stop checkpoints.
 *
 * Both helpers are intentionally dependency-free (only the standard library and
 * platform headers) so they can be included from every 'src/<type>/optimize.cpp'.
 */

#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{

/**
 * Run a set of independent tasks with a bounded number of them running
 * concurrently.
 *
 * Each task MUST be self-contained and exception-safe: it must not let an
 * exception escape (the callers wrap their work with 'wrapper<>' which stores
 * any exception into a per-task 'std::exception_ptr', so this is already the
 * case).  The tasks are assumed to be independent; the only shared state they
 * touch (the solution pool) is mutex-guarded by the caller.
 *
 * @param tasks               The tasks to run.
 * @param number_of_threads   Maximum number of tasks running concurrently.
 *                            0 (the default) means "unlimited": spawn all tasks
 *                            at once, which reproduces the historical behavior
 *                            byte-for-byte.
 */
inline void run_in_waves(
        const std::vector<std::function<void()>>& tasks,
        Counter number_of_threads)
{
    if (tasks.empty())
        return;

    // Unlimited (default): spawn everything at once and join, exactly like the
    // previous 'std::vector<std::thread>' spawn/join loops.
    if (number_of_threads <= 0) {
        std::vector<std::thread> threads;
        threads.reserve(tasks.size());
        for (const std::function<void()>& task: tasks)
            threads.push_back(std::thread(task));
        for (std::thread& thread: threads)
            thread.join();
        return;
    }

    // Capped: run the tasks in waves of at most 'number_of_threads' threads.
    Counter wave_size = number_of_threads;
    for (std::size_t begin = 0; begin < tasks.size(); begin += (std::size_t)wave_size) {
        std::size_t end = begin + (std::size_t)wave_size;
        if (end > tasks.size())
            end = tasks.size();
        std::vector<std::thread> threads;
        threads.reserve(end - begin);
        for (std::size_t i = begin; i < end; ++i)
            threads.push_back(std::thread(tasks[i]));
        for (std::thread& thread: threads)
            thread.join();
    }
}

}

#if defined(_WIN32)
// Prevent <windows.h> from defining the 'min'/'max' macros, which would clash
// with the solver's heavy use of std::min/std::max, and trim the include.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace packingsolver
{

/**
 * Return the current resident set size (physical memory in use) of the calling
 * process, in mebibytes.  Returns 0 if it cannot be determined.
 *
 * This is meant to be sampled cheaply at the existing cooperative stop
 * checkpoints, not called for every search node.
 */
inline std::size_t current_memory_megabytes()
{
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return (std::size_t)(pmc.WorkingSetSize / (1024 * 1024));
    return 0;
#elif defined(__APPLE__)
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(
                mach_task_self(),
                MACH_TASK_BASIC_INFO,
                (task_info_t)&info,
                &count) == KERN_SUCCESS) {
        return (std::size_t)(info.resident_size / (1024 * 1024));
    }
    return 0;
#else
    // Linux / other: parse '/proc/self/statm'.  The second field is the
    // resident set size expressed in pages.
    std::FILE* file = std::fopen("/proc/self/statm", "r");
    if (file == nullptr)
        return 0;
    unsigned long size_pages = 0;
    unsigned long resident_pages = 0;
    int read = std::fscanf(file, "%lu %lu", &size_pages, &resident_pages);
    std::fclose(file);
    if (read != 2)
        return 0;
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        page_size = 4096;
    return (std::size_t)(((unsigned long long)resident_pages
                * (unsigned long long)page_size) / (1024ULL * 1024ULL));
#endif
}

/**
 * Return true if a memory limit is set ('memory_limit_megabytes' > 0) and the
 * current resident set size has reached or exceeded it.
 *
 * When 'memory_limit_megabytes' is 0 (the default) this always returns false,
 * so the historical behavior is preserved.
 */
inline bool memory_limit_reached(
        Counter memory_limit_megabytes)
{
    if (memory_limit_megabytes <= 0)
        return false;
    return current_memory_megabytes() >= (std::size_t)memory_limit_megabytes;
}

}
