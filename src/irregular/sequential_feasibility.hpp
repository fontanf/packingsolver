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
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

#include <functional>

namespace packingsolver
{
namespace irregular
{

using SequentialFeasibilitySolver = std::function<SolutionPool<Instance, Solution>(const Instance&)>;

struct SequentialFeasibilityOutput: Output
{
    /** Constructor. */
    SequentialFeasibilityOutput(const Instance& instance):
        Output(instance) { }
};

struct SequentialFeasibilityParameters: packingsolver::Parameters<Instance, Solution, Output>
{
};

SequentialFeasibilityOutput sequential_feasibility(
        const Instance& instance,
        const SequentialFeasibilitySolver& solver,
        const SequentialFeasibilityParameters& parameters = {});

}
}
