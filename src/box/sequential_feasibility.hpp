/**
 * Sequential feasibility algorithm for box.
 *
 * Iteratively solves Feasibility sub-problems with a shrinking bin or bin
 * count until no feasible packing can be found. Supports objectives
 * BinPacking, BinPackingWithLeftovers, OpenDimensionX, OpenDimensionY and
 * OpenDimensionZ.
 *
 * For objective BinPackingWithLeftovers, the last bin is shrunk along the
 * first dimension of the leftover mode (along x for modes 'X', 'XY', 'XZ' and
 * 'XYZ', along y for modes 'Y' and 'YZ', along z for mode 'Z'); it is a bin
 * type of its own, added after the others.
 *
 * The caller supplies the inner feasibility solver as a std::function.
 */

#pragma once

#include "packingsolver/box/optimize.hpp"

#include <functional>

namespace packingsolver
{
namespace box
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
