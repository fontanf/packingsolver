/**
 * Sequential strips / one-dimensional algorithm
 *
 * Two-phase heuristic for the BinPacking and BinPackingWithLeftovers
 * objectives (single bin type):
 * - Phase 1: generate a set of strips by solving the strip packing problem
 *   (all items packed, minimize the total width) with the
 *   column_generation_strips algorithm.
 * - Phase 2: solve a onedimensional bin packing problem where the items are
 *   the strips generated in phase 1 (item length = strip width, bin length =
 *   bin width) to decide how strips are grouped into bins.
 *
 * The final solution is then reconstructed by placing, in each bin, the
 * strips assigned to it side by side.
 */

#pragma once

#include "packingsolver/rectangleguillotine/optimize.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct SequentialStripsOnedimensionalOutput: Output
{
    /** Constructor. */
    SequentialStripsOnedimensionalOutput(const Instance& instance):
        Output(instance) { }
};

struct SequentialStripsOnedimensionalParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::CLP;
};

const SequentialStripsOnedimensionalOutput sequential_strips_onedimensional(
        const Instance& instance,
        const SequentialStripsOnedimensionalParameters& parameters = {});

}
}
