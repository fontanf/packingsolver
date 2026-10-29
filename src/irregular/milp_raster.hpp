/**
 * MILP raster
 *
 * The goal of the MILP raster algorithm is to find a packing solution by
 * rasterizing the bin and solving a mixed-integer linear program.
 *
 * The items are placed at the points of a grid, and occupy every cell they
 * intersect, which gives a restriction of the problem: a feasible solution.
 * The MILP is solved for decreasing cell sizes.
 *
 * In parallel, a relaxation of the problem is solved on the same grids,
 * which gives a bound:
 * - Placement (item type, rotation, cell k) means that the bottom-left corner
 *   of the bounding box of the item is anywhere in cell k. Several copies may
 *   have the same placement.
 * - The coefficient of a placement in a cell is a lower bound on the area of
 *   the cell covered by the item, wherever the item is in cell k: the item is
 *   partitioned into convex parts, and for a convex part, the minimum is
 *   reached when the item is at a corner of cell k (the square root of the
 *   area of the intersection of two convex shapes is concave in their
 *   relative position, from the Brunn-Minkowski inequality); so the
 *   coefficient is the sum over the convex parts of the minimum of their
 *   coverage over the 4 corners.
 * - The capacity of a cell is the area of the cell inside the bin minus the
 *   area of the fixed items in it.
 * The defects and the item-item spacing are ignored, which keeps the
 * relaxation valid. The relaxation is only computed if every allowed
 * rotation of the item types is a single angle.
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace irregular
{

struct MilpRasterOutput: Output
{
    /** Constructor. */
    MilpRasterOutput(const Instance& instance):
        Output(instance) { }
};

struct MilpRasterParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;

    /**
     * Maximum number of cell sizes: the algorithm solves the MILP of a cell
     * size, then halves it, until the timer ends or this number of cell sizes
     * is reached.
     *
     * -1 means no limit.
     */
    Counter maximum_number_of_cell_sizes = -1;

    /**
     * Maximum number of nodes of the branch-and-bound of the MILP solver,
     * for each cell size.
     *
     * -1 means no limit.
     */
    Counter maximum_number_of_nodes = -1;

    /** Compute a bound by solving the relaxation (see above). */
    bool use_bound = true;

    /**
     * Maximum number of non-zero coefficients of the model of the
     * relaxation: the smaller cell sizes are not tried once it is reached.
     *
     * -1 means no limit.
     */
    Counter bound_maximum_number_of_nonzeros = 1000000;

    /**
     * Optimization mode.
     *
     * In modes 'Anytime' and 'NotAnytime', the relaxation is solved in
     * another thread; otherwise, the relaxation of a cell size is solved
     * after the restriction of the same cell size.
     */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;
};

MilpRasterOutput milp_raster(
        const Instance& instance,
        const MilpRasterParameters& parameters = {});

}
}
