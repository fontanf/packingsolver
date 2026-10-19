/**
 * Sequential feasibility algorithm
 *
 * Iteratively solves Feasibility sub-problems with a shrinking bin or bin-count
 * until no feasible packing can be found. Supports objectives BinPacking,
 * BinPackingWithLeftovers, OpenDimensionX, OpenDimensionY, and OpenDimensionXY.
 *
 * The caller supplies the inner feasibility solver as a std::function. Mutable
 * state (e.g. warm-start penalties) can be captured in the closure to pass
 * information between consecutive sub-problem calls.
 *
 * Each sub-problem, except the first one, receives a warm start: the previous
 * solution found. It is a solution of the original instance, not of the
 * sub-instance. The bin types of a sub-instance are bin types of the original
 * instance with the same ids, possibly restricted and with fewer copies; for
 * objective BinPackingWithLeftovers, the restricted last bin is a bin type of
 * its own, added after the others, unless it is the only copy of its bin type.
 * The item types of a sub-instance are the item types of the original
 * instance with the same ids. Therefore, the warm start is generally not a
 * feasible solution of the sub-instance: it may use more bins than available,
 * and some items may stick out of the shrunk bins. It is up to the inner
 * solver to repair it.
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

#include <functional>

namespace packingsolver
{
namespace irregular
{

/**
 * Inner feasibility solver.
 *
 * The first argument is the Feasibility sub-instance, the second one the warm
 * start, a solution of the original instance ('nullptr' for the first
 * sub-problem).
 */
using SequentialFeasibilitySolver = std::function<SolutionPool<Instance, Solution>(
        const Instance&,
        const Solution*)>;

struct SequentialFeasibilityOutput: Output
{
    /** Constructor. */
    SequentialFeasibilityOutput(const Instance& instance):
        Output(instance) { }
};

struct SequentialFeasibilityParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /**
     * Ratio by which the open dimension(s) of the best solution found are
     * shrunk to build the next sub-problem.
     */
    double shrink_ratio = 0.01;

    /**
     * Enable the compression: after a sub-problem without feasible solution,
     * instead of stopping, the best solution found is shrunk by a smaller
     * ratio, starting from half of 'shrink_ratio':
     * - if the timer has a time limit, the ratio decreases linearly with the
     *   elapsed time, down to 'compression_minimum_shrink_ratio' at the time
     *   limit (compression phase of Sparrow);
     * - otherwise, it is multiplied by 'compression_shrink_ratio_decay' after
     *   each failure, until it gets below 'compression_minimum_shrink_ratio'.
     *
     * Not available for objective BinPacking, whose number of bins can't be
     * shrunk by a small ratio.
     */
    bool compression = false;

    /**
     * Decay of the shrink ratio after each failure during the compression,
     * without time limit.
     */
    double compression_shrink_ratio_decay = 0.9;

    /** Minimum shrink ratio of the compression. */
    double compression_minimum_shrink_ratio = 1e-5;
};

SequentialFeasibilityOutput sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters = {});

}
}
