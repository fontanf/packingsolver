#pragma once

/**
 * Helpers to choose the number of threads of the HiGHS solves.
 *
 * HiGHS keeps one task scheduler per calling thread. It is created by the
 * first solve in that thread, with the number of threads given by the
 * 'threads' option of that solve, and kept until the thread exits. Two issues
 * follow:
 * - A later solve in the same thread fails if its 'threads' option is
 *   nonzero and differs from the size of the scheduler. columngenerationsolver
 *   solves its linear programs with 'threads' set to 1, so a solve with
 *   another number of threads must not leave a scheduler behind in a thread
 *   that later runs a column generation.
 * - When a thread holding a scheduler with worker threads exits, HiGHS
 *   detaches the workers. On glibc < 2.43, this crashes the process (see
 *   https://github.com/ERGO-Code/HiGHS/issues/3826).
 *
 * So:
 * - linear programs are solved with a single thread ('highs_use_single_thread');
 *   the simplex is serial, so this costs nothing;
 * - MILPs are solved inside a 'HighsMilpThreads' scope.
 */

#ifdef HIGHS_FOUND

#include "packingsolver/algorithms/common.hpp"

#include "Highs.h"

namespace packingsolver
{

/**
 * Solve with a single thread: the scheduler then starts no worker threads.
 */
inline void highs_use_single_thread(Highs& highs)
{
    highs.setOptionValue("threads", 1);
}

/**
 * Scope of a MILP solve with HiGHS.
 *
 * In 'NotAnytimeSequential' mode, the solve uses a single thread, like every
 * other HiGHS solve of the run.
 *
 * Otherwise, the solve keeps the default number of threads, and the
 * scheduler of the calling thread is reset, waiting for its workers to end,
 * when the scope starts and when it ends:
 * - when it starts, so that the solve doesn't reuse a single-thread scheduler
 *   left by a previous solve;
 * - when it ends, so that no scheduler with worker threads is left in the
 *   thread, and the workers are joined rather than detached.
 *
 * The scope must be created after the 'Highs' object and before 'run()'.
 */
class HighsMilpThreads
{

public:

    HighsMilpThreads(
            Highs& highs,
            OptimizationMode optimization_mode):
        reset_scheduler_(optimization_mode != OptimizationMode::NotAnytimeSequential)
    {
        if (reset_scheduler_) {
            Highs::resetGlobalScheduler(true);
        } else {
            highs_use_single_thread(highs);
        }
    }

    ~HighsMilpThreads()
    {
        if (reset_scheduler_)
            Highs::resetGlobalScheduler(true);
    }

    HighsMilpThreads(const HighsMilpThreads&) = delete;
    HighsMilpThreads& operator=(const HighsMilpThreads&) = delete;

private:

    /** 'true' iff the scheduler is reset at the start and at the end. */
    bool reset_scheduler_;

};

}

#endif
