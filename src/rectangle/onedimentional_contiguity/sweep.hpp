/**
 * Onedimensional-contiguity sweep
 *
 * Solves the BMP relaxation of 'milp.hpp' for the 'Feasibility' objective,
 * without no-good cuts: is there a run of consecutive columns for every unit
 * of every item type such that the heights of the units covering each column
 * fit in the height of the bin?
 *
 * Depth-first search on the starts of the units: in a solution, the units
 * can be pushed to the left, one at a time, in the order of their positions,
 * until each one starts at 0 or at the end of another one. The starts are
 * processed from left to right; at the current start, either another unit
 * starts there (the units starting at the same start being chosen in a fixed
 * order, the identical units in the order of their copies), or the start is
 * closed. A node is pruned if the area of the remaining units exceeds the
 * free space of the columns from the current start on, not counting the
 * columns where the lowest remaining unit doesn't fit.
 *
 * Unlike 'milp' and 'tree_search', the positions aren't restricted to the
 * meet-in-the-middle patterns, and the search is specialized for the
 * feasibility question, which makes it much faster to prove that the
 * relaxation is infeasible. A unit which can be rotated has its smallest
 * length as width and height, which keeps a relaxation.
 */

#pragma once

#include "rectangle/onedimentional_contiguity/milp.hpp"

namespace packingsolver
{
namespace rectangle
{
namespace onedimentional_contiguity
{

struct SweepParameters: optimizationtools::Parameters
{
    /**
     * Maximum number of operations; -1 for no limit.
     *
     * An operation is the processing of a column. If the search reaches this
     * limit, its result is 'Inconclusive'.
     */
    Counter maximum_number_of_operations = -1;
};

struct SweepOutput
{
    /**
     * 'Feasible' if the relaxation has a solution, 'Infeasible' if it
     * doesn't, 'Inconclusive' if the search reached its limits.
     */
    OnedimensionalContiguityResult::Status status
        = OnedimensionalContiguityResult::Status::Inconclusive;

    /** Number of operations. */
    Counter number_of_operations = 0;
};

/**
 * Solve the relaxation of an instance with the 'Feasibility' objective and a
 * single bin.
 */
SweepOutput sweep(
        const Instance& instance,
        const SweepParameters& parameters);

}
}
}
