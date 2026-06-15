#pragma once

/**
 * This header provides two small, portable, C++14-only helpers shared by all
 * problem types:
 *
 * 1. 'run': a task executor that runs tasks in parallel or sequentially.
 *
 * 2. 'current_memory_megabytes': a portable reader of the current resident set
 *    size (RSS) of the process, used to CAP memory usage at the existing
 *    cooperative stop checkpoints.
 *
 * Both helpers are intentionally dependency-free (only the standard library and
 * platform headers) so they can be included from every 'src/<type>/optimize.cpp'.
 */

#include <chrono>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{

/**
 * Run a set of independent tasks, spawning all of them concurrently.
 *
 * Each task MUST be self-contained and exception-safe: it must not let an
 * exception escape (the callers wrap their work with 'wrapper<>' which stores
 * any exception into a per-task 'std::exception_ptr', so this is already the
 * case).  The tasks are assumed to be independent; the only shared state they
 * touch (the solution pool) is mutex-guarded by the caller.
 */
inline void run(
        const std::vector<std::function<void()>>& tasks,
        bool parallel)
{
    if (tasks.empty())
        return;

    if (!parallel) {
        for (const std::function<void()>& task: tasks)
            task();
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(tasks.size() - 1);
    for (std::size_t i = 0; i < tasks.size() - 1; ++i)
        threads.push_back(std::thread(tasks[i]));
    tasks.back()();
    for (std::thread& thread: threads)
        thread.join();
}

}

#if defined(_WIN32)
#define NOMINMAX
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
 * Background task that monitors memory usage and signals the algorithms to
 * stop when the memory limit is reached.
 *
 * Exits when algorithm_formatter.end_boolean() or parameters.timer.needs_to_end()
 * becomes true (either cooperative stop or time limit).  Sets end_boolean to
 * true when current_memory_megabytes() >= parameters.memory_limit_megabytes.
 */
template<typename AlgorithmFormatter, typename Parameters>
inline void monitor_memory(
        AlgorithmFormatter& algorithm_formatter,
        const Parameters& parameters)
{
    while (!algorithm_formatter.end_boolean()
            && !parameters.timer.needs_to_end()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (algorithm_formatter.end_boolean()
                || parameters.timer.needs_to_end())
            break;
        if (current_memory_megabytes()
                >= (std::size_t)parameters.memory_limit_megabytes)
            algorithm_formatter.end_boolean() = true;
    }
}

/**
 * Overload of 'run' that derives parallelism from 'parameters.optimization_mode'
 * and manages a memory-monitor thread when 'parameters.memory_limit_megabytes > 0'.
 *
 * The monitor is started before the tasks and joined after all tasks finish.
 * Setting 'end_boolean' to true after the tasks complete is safe because all
 * algorithm threads have already returned at that point.
 */
template<typename AlgorithmFormatter, typename Parameters>
inline void run(
        const std::vector<std::function<void()>>& tasks,
        AlgorithmFormatter& algorithm_formatter,
        const Parameters& parameters)
{
    bool parallel = parameters.optimization_mode != OptimizationMode::NotAnytimeSequential;

    std::thread monitor_thread;
    if (parameters.memory_limit_megabytes > 0 && parallel) {
        monitor_thread = std::thread([&algorithm_formatter, &parameters]() {
            monitor_memory(algorithm_formatter, parameters);
        });
    }

    run(tasks, parallel);

    if (monitor_thread.joinable()) {
        algorithm_formatter.end_boolean() = true;
        monitor_thread.join();
    }
}

}
