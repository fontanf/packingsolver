/**
 * Sequential feasibility algorithm for rectangleguillotine.
 *
 * Iteratively solves Feasibility sub-problems with a shrinking bin until no
 * feasible packing can be found. Supports objectives OpenDimensionX,
 * OpenDimensionY, and BinPackingWithLeftovers (single bin).
 *
 * The caller supplies the inner feasibility solver as a std::function.
 */

#pragma once

#include "packingsolver/rectangleguillotine/optimize.hpp"

#include <functional>

namespace packingsolver
{
namespace rectangleguillotine
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
