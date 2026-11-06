/**
 * Global nonlinear programming
 *
 * Solve an instance with the 'Feasibility' objective and a single bin
 * exactly, with continuous rotations, by solving a nonconvex quadratically
 * constrained program to global optimality with SCIP's spatial
 * branch-and-bound.
 *
 * Model, in coordinates where the bin fits in the unit square:
 * - Each item i has the position (x_i, y_i) of its center (the average of
 *   the vertices of its shapes) and its rotation (c_i, s_i), with
 *   c_i^2 + s_i^2 = 1, so that a point v of the item, relatively to its
 *   center, is at (x_i + c_i v_x - s_i v_y, y_i + s_i v_x + c_i v_y): it is
 *   linear in the variables of the item.
 * - The allowed rotations of an item type are a union of ranges. The angles
 *   of a range of width w and middle m are the points of the unit circle
 *   with c cos(m) + s sin(m) >= cos(w / 2). With several ranges, a binary
 *   variable selects the range, and the constraints of the other ranges
 *   are relaxed.
 * - The bin must be convex: the vertices of the convex hulls of the shapes
 *   of each item are in the bin, shrunk by the item-bin spacing (linear
 *   constraints).
 * - The items are decomposed into convex parts. For each pair of parts of
 *   different items, a separating line of normal (a, b) and offset d is a
 *   variable: the vertices of the first part satisfy a x + b y <= d, the
 *   vertices of the second part a x + b y >= d (bilinear constraints). Two
 *   convex parts don't overlap iff such a line exists. Without item-item
 *   spacing, the normal satisfies a^2 + b^2 >= 1, since a separating line
 *   remains one when its coefficients are scaled; with a spacing, it is a
 *   unit vector and the half-spacing is subtracted on each side.
 * - Symmetries: the identical items are sorted by their x-coordinates; if
 *   the bin is centrally symmetric and the items of the subset can be
 *   rotated by any angle, the packing rotated by 180 degrees around the
 *   center of the bin is a packing, so the first item is in the left half
 *   of the bin.
 *
 * As in the tree search on relative positions, the bound of a subset of the
 * items is a bound for all of them: the pairs of items are solved first, by
 * decreasing area, then the increasing subsets of the largest items, until
 * a subset is proven infeasible. The solution of the subset with all the
 * items is a solution of the instance if the items don't overlap.
 *
 * The results are exact up to the tolerances of SCIP: a packing where the
 * items overlap by less than its feasibility tolerance (1e-6 relatively to
 * the size of the bin) may be missed, so the infeasibility proofs hold up
 * to this tolerance. The solutions are checked before being returned.
 *
 * Unsupported: other objectives, several bins, non-convex bins, defects,
 * fixed items, mirrored rotations and circular arcs.
 *
 * Requires SCIP (the CMake option 'PACKINGSOLVER_USE_SCIP').
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

namespace packingsolver
{
namespace irregular
{

struct GlobalNonlinearProgrammingOutput: Output
{
    /** Constructor. */
    GlobalNonlinearProgrammingOutput(const Instance& instance):
        Output(instance) { }

    /** Number of subsets of items solved. */
    Counter number_of_subsets = 0;

    /** Number of nodes of the branch-and-bound of SCIP. */
    Counter number_of_nodes = 0;
};

struct GlobalNonlinearProgrammingParameters: packingsolver::Parameters<Instance, Solution, Output>
{
};

/**
 * Return 'true' iff the algorithm is available (SCIP is used) and supports
 * the instance.
 */
bool global_nonlinear_programming_supports(
        const Instance& instance);

GlobalNonlinearProgrammingOutput global_nonlinear_programming(
        const Instance& instance,
        const GlobalNonlinearProgrammingParameters& parameters = {});

}
}
