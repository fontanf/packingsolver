/**
 * MILP NFP
 *
 * Prove that a set of items doesn't fit in a bin, with a mixed-integer linear
 * program based on the no-fit polygons of the items.
 *
 * The rotations of the items are enumerated. For a rotation of each item:
 * - The reference point of item i must be in its inner-fit rectangle IFP_i:
 *   the positions where its bounding box is in the bounding box of the bin
 *   (minus the item-bin spacing).
 * - For a pair of items (i, j), the relative position d = p_j - p_i must be
 *   in R_ij = IFP_j - IFP_i, and outside the interior of the no-fit polygon
 *   NFP_ij. The region F_ij = R_ij \ int(NFP_ij) is partitioned into convex
 *   pieces, and a binary variable selects the piece containing d. The
 *   constraints of the edges of a piece are relaxed with big-M coefficients
 *   when the piece isn't selected.
 * The set of items doesn't fit if the MILP of every rotation combination is
 * infeasible. These MILPs are solved in parallel.
 *
 * Before solving the MILP of a rotation combination, the core widening may
 * prove it infeasible. The core of an item is the part of its y-range which
 * no other item can reach, given the regions F_ij. Since the rotations are
 * fixed, the horizontal band of the core of an item, across the bin, contains
 * no other item, so the item can be widened to the width of the bin on its
 * core. The cores of different items are disjoint, so the rotation
 * combination is infeasible if the sum of the widened areas exceeds the area
 * of the bin.
 *
 * A rotation combination is also infeasible if one of its sub-combinations
 * without one of the items is. These are checked first: their MILPs are
 * cheaper, and each of them is shared by several combinations. The statuses
 * of the combinations and sub-combinations are kept in the cache, so that
 * the next calls reuse them.
 *
 * The exact fits (for example, an item exactly in a notch of another one)
 * are feasible positions of null area strictly inside the no-fit polygon:
 * line segments and points, returned by shape::no_fit_polygon. Each of them
 * is an additional convex piece of F_ij.
 *
 * The model is a relaxation, so that an infeasible MILP is a proof:
 * - The circular arcs of the regions F_ij are approximated by line segments
 *   outside of them.
 * - The shape of the bin is replaced by its bounding box, and the defects,
 *   the fixed items and the item-item spacing are ignored.
 * - The rotations must be discrete; otherwise, nothing is proven.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

#include <memory>

namespace packingsolver
{
namespace irregular
{

enum class MilpNfpStatus
{
    /** Proven: the items don't fit in the bin. */
    Infeasible,

    /**
     * The relaxation is feasible for some rotation combination, so the items
     * may fit in the bin.
     */
    Feasible,

    /** Not proven, because of the time limit or of a continuous rotation. */
    Unknown,
};

struct MilpNfpOutput
{
    /** Status. */
    MilpNfpStatus status = MilpNfpStatus::Unknown;

    /** Number of rotation combinations considered. */
    Counter number_of_rotation_combinations = 0;

    /**
     * Number of rotation combinations proven infeasible without a MILP,
     * because a pair of items doesn't fit or by the core widening.
     */
    Counter number_of_rotation_combinations_pruned = 0;

    /** Number of MILPs of rotation combinations solved. */
    Counter number_of_milps = 0;

    /** Number of MILPs of sub-combinations solved. */
    Counter number_of_sub_combination_milps = 0;

    /**
     * Number of rotation combinations proven infeasible by one of their
     * sub-combinations.
     */
    Counter number_of_rotation_combinations_pruned_by_sub_combinations = 0;

    /** Maximum number of convex pieces of a pair of items. */
    Counter maximum_number_of_pieces = 0;

    /**
     * If the status is 'Feasible', the positions of the items in a solution
     * of the relaxation (in the original coordinates).
     *
     * They are only a packing up to the relaxation: the shape of the bin,
     * its defects, its fixed items and the item-item spacing are ignored.
     */
    std::vector<SolutionItem> items;
};

/**
 * Data reused between calls of 'milp_nfp' on the same instance: the convex
 * pieces of the pairs of item types in each of their rotations, which are
 * expensive to compute.
 */
class MilpNfpCache
{

public:

    MilpNfpCache();

    ~MilpNfpCache();

    struct Data;

    std::unique_ptr<Data> data_;
};

struct MilpNfpParameters: optimizationtools::Parameters
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;

    /**
     * Number of threads: the MILPs of the rotation combinations are solved
     * in parallel.
     */
    Counter number_of_threads = 4;

    /**
     * Before solving the MILP of a rotation combination, check its
     * sub-combinations without one of the items.
     */
    bool check_sub_combinations = true;

    /** Cache, kept between calls on the same instance (optional). */
    MilpNfpCache* cache = nullptr;
};

/**
 * Try to prove that the items of 'item_type_ids' (one entry per copy) don't
 * fit in a bin of type 'bin_type_id'.
 */
MilpNfpOutput milp_nfp(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemTypeId>& item_type_ids,
        const MilpNfpParameters& parameters = {});

/**
 * Build the solution, with a single bin of type 'bin_type_id', of the
 * positions of the items of a 'Feasible' output (see 'MilpNfpOutput::items'),
 * and return 'true' if it is feasible.
 *
 * The solution is only checked for overlaps, so it isn't built if the items
 * must be spaced or if the bin has fixed items, which the MILP NFP algorithm
 * ignores.
 */
bool milp_nfp_solution(
        const Instance& instance,
        BinTypeId bin_type_id,
        const MilpNfpOutput& output,
        Solution& solution);

}
}
