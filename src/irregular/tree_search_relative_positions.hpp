/**
 * Tree search on relative positions
 *
 * Prove that a set of items doesn't fit in a bin, with a tree search on the
 * relative positions of the pairs of items, based on their no-fit polygons.
 *
 * For a rotation of each item:
 * - The reference point of item i must be in its inner-fit rectangle IFP_i:
 *   the positions where its bounding box is in the bounding box of the bin
 *   (minus the item-bin spacing).
 * - For a pair of items (i, j), the relative position d = p_j - p_i must be
 *   in R_ij = IFP_j - IFP_i, and outside the interior of the no-fit polygon
 *   NFP_ij. The region F_ij = R_ij \ int(NFP_ij) is partitioned into convex
 *   pieces, and d must be in one of them.
 *
 * The problem is solved by a depth-first tree search, which fixes the
 * rotation of an item or the piece of a pair at each node. The positions of
 * a node are bounded by two difference-bound matrices, one for each axis, on
 * the reference points of the items. The propagation alternates the closure
 * of the matrices (a shortest path computation) with the filtering of the
 * rotations and of the pieces:
 * - the rotations of an item whose inner-fit rectangle doesn't intersect the
 *   bounds of its position are removed, and the bounds are shrunk to the
 *   union of the inner-fit rectangles of the remaining ones;
 * - the pieces which don't intersect the box of the relative positions of
 *   their pair are removed, and the box is shrunk to the bounding box of the
 *   remaining pieces clipped to it;
 * - the rotations of an item which no remaining piece of one of its pairs
 *   supports are removed.
 * A node is infeasible if an item has no rotation left, if a pair has no
 * piece left, or if a matrix has a negative cycle. Once each item has a
 * single rotation and each pair a single piece, the problem is a linear
 * program. The search first fixes the rotations of the items, the largest
 * ones first, and then branches on the pair with the fewest pieces left. The
 * first levels of the tree are expanded breadth-first, and
 * their subtrees are explored in parallel.
 *
 * The core bound may prove a node infeasible. The core of an item is the part
 * of its y-range which no other item can reach, given the pieces and the
 * positions of the node. The horizontal band of the core of an item, across
 * the bin, contains no other item, so the item can be widened to the width of
 * the bin on its core. The cores of different items are disjoint, so the
 * node is infeasible if the sum of the widened areas exceeds the area of the
 * bin. If the rotation of an item isn't fixed, the smallest widened area of
 * its remaining rotations is taken.
 *
 * For the open dimension objectives, the tree search minimizes the open
 * dimension instead, and gives a bound. The end of the strip along the open
 * axis is another node of the difference-bound matrix of this axis, which
 * the items must precede, so that the closure gives a bound on it at each
 * node: the tree search is a branch-and-bound, in which a node is pruned if
 * its bound isn't smaller than the best value found so far, and the linear
 * program of a leaf minimizes the open dimension. The core bound uses the
 * cores of the items along the fixed axis, with the best value found so far
 * as the length of the strip.
 *
 * The exact fits (for example, an item exactly in a notch of another one)
 * are feasible positions of null area strictly inside the no-fit polygon:
 * line segments and points, returned by shape::no_fit_polygon. Each of them
 * is an additional convex piece of F_ij.
 *
 * The program is a relaxation, so that an infeasible program is a proof:
 * - The circular arcs of the regions F_ij are approximated by line segments
 *   outside of them.
 * - The shape of the bin is replaced by its bounding box, and the defects,
 *   the fixed items and the item-item spacing are ignored.
 * - The rotations must be discrete; otherwise, nothing is proven.
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace irregular
{

struct TreeSearchRelativePositionsOutput: Output
{
    /** Constructor. */
    TreeSearchRelativePositionsOutput(const Instance& instance):
        Output(instance) { }

    /** Number of nodes of the tree searches. */
    Counter number_of_nodes = 0;

    /** Number of linear programs solved at the leaves of the tree searches. */
    Counter number_of_linear_programs = 0;

    /** Maximum number of convex pieces of a pair of items. */
    Counter maximum_number_of_pieces = 0;
};

struct TreeSearchRelativePositionsParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** Solver of the linear programs. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;

    /** Number of threads, which explore subtrees in parallel. */
    Counter number_of_threads = 4;

    /**
     * For the open dimension objectives, upper bound on the open dimension
     * (in the original coordinates): only the solutions with a smaller one
     * are searched. By default, the size of the bin.
     */
    LengthDbl open_dimension_upper_bound = std::numeric_limits<LengthDbl>::infinity();

    /**
     * For the 'Feasibility' objective, solve the increasing subsets of the
     * largest items one after the other (see 'tree_search_relative_positions').
     * If 'false', only the set of all the items is solved, e.g. when all its
     * subsets are already known to fit.
     */
    bool growing_subsets = true;
};

/**
 * Solve an instance with the 'Feasibility' objective and a single bin, or
 * with an open dimension objective.
 *
 * The bound of a subset of the items is a bound for all of them, so the
 * increasing subsets of the largest items are solved one after the other.
 *
 * For the 'Feasibility' objective, the instance is proven infeasible as soon
 * as a subset is, and this subset is the infeasible subset of the output;
 * otherwise, the positions of the solution of the relaxation
 * with all the items are a solution if the items don't overlap.
 *
 * For the open dimension objectives, the bounds of the subsets are computed
 * until the time limit. The positions of the solution of the relaxation with
 * all the items are a solution if they don't overlap.
 *
 * The fixed items are ignored.
 */
TreeSearchRelativePositionsOutput tree_search_relative_positions(
        const Instance& instance,
        const TreeSearchRelativePositionsParameters& parameters = {});

}
}
