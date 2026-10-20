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
};

SequentialFeasibilityOutput sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters = {});

}
}
