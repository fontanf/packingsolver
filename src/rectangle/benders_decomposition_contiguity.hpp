/**
 * Contiguity-master Benders decomposition
 *
 * A combinatorial Benders decomposition for a single bin (the 'Knapsack' and
 * 'Feasibility' objectives), based on:
 * - Côté, Dell'Amico & Iori (2014), "Combinatorial Benders' Cuts for the
 *   Strip Packing Problem", Operations Research 62(3) - the master/slave
 *   split and the slave's own solution method ('y-check').
 * - Wang, Baldacci, Furini, Wei & Liu (2025), "Exact Algorithms for
 *   Two-Dimensional Knapsack Problems: A Unified Framework with New
 *   Benchmark Results" - adapting that same master/slave split from strip
 *   packing (minimize height, unbounded strip) to a fixed-size bin
 *   ('Knapsack'/'Feasibility'), which is what this file implements.
 *
 * Unlike 'benders_decomposition.cpp' (whose master only sees each bin's
 * total *area*, via a onedimensional relaxation, and whose slave re-derives
 * a full 2D placement from scratch via generic tree search), this master
 * assigns each item an actual x-position, and the slave only has to find
 * y-positions consistent with it - a strictly cheaper problem than full 2D
 * placement.
 *
 * The master (BMP) is an adaptation of the 'P|cont|Cmax' parallel-processor-
 * scheduling relaxation (see 'bar_relaxation.hpp' for the same relaxation
 * used as a continuous LP bound elsewhere in this codebase): the bin is cut
 * into W unit-width columns ("processors"), and each item, in one of its
 * allowed orientations, into as many unit-width slices as its own width,
 * assigned to a run of *consecutive* columns (its 'x' position). Only a
 * necessary condition on the induced 'x'-assignment is enforced (every
 * column's total covering height at most the bin height) - this is what
 * makes it a relaxation: an optimal BMP solution need not be geometrically
 * realizable at all (nothing prevents two items from being assigned
 * overlapping 'x'-columns while one is "on top of" the space the other
 * would also need at a *lower* height than where it ends up, etc.).
 *
 * The BMP itself is solved by 'onedimentional_contiguity/' (a self-contained
 * sibling module: see its own file-level comments), which offers two
 * interchangeable ways to do so, both taking a plain 'rectangle::Instance'
 * (this file's own, or LBD's sub-instance - see below) plus the accumulated
 * no-good cuts, and returning the same result shape:
 * - 'onedimentional_contiguity::milp' - an actual 0-1 MILP (variable
 *   rho[u][x][o] is 1 iff unit 'u', one copy of some item type, is placed at
 *   column 'x' in orientation 'o').
 * - 'onedimentional_contiguity::tree_search' - a combinatorial
 *   branch-and-bound alternative solving the exact same relaxation, in the
 *   spirit of Wang et al. (2025)'s own numerically-exact algorithm (NEA)
 *   using dedicated combinatorial search rather than a MILP solver for
 *   their analogous BMP.
 * 'BendersDecompositionContiguityParameters::use_tree_search' selects
 * between them (defaults to the tree search).
 *
 * The slave (BSP) is the 'y-check' problem: given the BMP's chosen
 * x-position (and orientation) for every selected unit, does a set of
 * y-positions exist making the packing valid (no two items overlapping)?
 * Solved via the niche/skyline-based combinatorial branch-and-bound of
 * Côté, Dell'Amico & Iori (2014), §3 (see 'y_check' in the .cpp).
 *
 * If the BSP proves the BMP's solution infeasible, this same item set is
 * escalated to a Local Benders Decomposition (LBD, Wang et al. 2025 §3.4,
 * see 'run_lbd' in the .cpp) before giving up on it, but only for
 * 'Knapsack': LBMP(I) (their eq. (10a)-(10c) - the same BMP MILP restricted
 * to I, with every unit now mandatory rather than optional) is exactly a
 * fresh, single-bin 'Feasibility' instance over I, so LBD is implemented as
 * a direct recursive call to 'benders_decomposition_contiguity' itself on that
 * sub-instance, rather than as a separate solver - reusing this same
 * BMP/BSP loop (including its own no-good cuts) instead of reimplementing
 * it. For 'Feasibility', I is always every unit in the instance (every item
 * type mandatory there already), never a genuine subset LBD could usefully
 * explore alternate positionings of independently of the outer BMP, so this
 * escalation is skipped for it - see the 'Feasibility' branch's own comment
 * in the .cpp. If LBD finds a feasible pattern for the item set (possibly
 * at different x-positions than the outer BMP's own choice), it is used
 * directly - it necessarily matches the outer BMP's own (already-reported)
 * bound, since profit only depends on which items are selected, so it is
 * optimal. If LBD instead proves that no positioning of the item set is
 * feasible at all, a no-good cut forbidding the whole item set at any
 * position (their §3.1.1 "covering cut") is added and the outer BMP is
 * re-solved.
 *
 * Scope of this implementation (deliberately narrower than either paper -
 * see the header comment in 'benders_decomposition.cpp' for the analogous
 * "deliberately scoped" note there):
 * - Only the 'Knapsack' and 'Feasibility' objectives are supported, and
 *   only for an instance with a single bin type used exactly once
 *   ('number_of_bin_types() == 1' and that bin type's 'copies == 1') -
 *   i.e. the classical (single-bin) 2D Knapsack / 2D Orthogonal Packing
 *   Problem, matching the scope of both papers (their 'BinPacking'-style
 *   generalizations, via repeated single-bin subproblems, are not
 *   implemented here).
 * - The BMP's (and LBMP's) x-position variables are generated from the
 *   meet-in-the-middle (MIM) pattern set of each (unit, orientation)
 *   variant (Côté & Iori 2018, see 'algorithms/meet_in_the_middle.hpp'),
 *   rather than every integer column - this is what keeps the relaxation
 *   tractable on instances with many item types. Neither paper's other
 *   problem reductions (dominance conditions, label-setting initial
 *   bounds), numerically-exact (fixed-point) arithmetic variant, nor its
 *   native G2KP extensions (item conflicts, same-bin/different-bin
 *   constraints) are implemented - only the core BMP/BSP Benders loop plus
 *   LBD. The 'Feasibility' branch's own no-good cut (see above - both a
 *   direct top-level 'Feasibility' call and LBD's own recursive one reach
 *   it) is strengthened via the full Wang et al. (2025) §3.5.2 procedure,
 *   extending Côté et al. (2014): the infeasible unit set is first shrunk
 *   (steps (1)-(3): recursive vertical-line binary split, a two-directional
 *   line sweep, then 12 scans of iterative single-unit removal - see
 *   'shrink_via_split'/'shrink_via_sweep'/'shrink_via_removal' in the .cpp),
 *   then each surviving unit's own forbidden x-window is widened as far as
 *   provably still-infeasible via an LP (step (4), eqs. (18a)-(18d) - see
 *   'build_lifted_cut'), including every one of a unit's own candidates
 *   within that window rather than just the single position originally
 *   used. The 'Knapsack' branch's own no-good cut (added only once LBD has
 *   already proven the *whole* item set infeasible, at every position) uses
 *   their simpler, unshrunk §3.1.1 covering cut instead (see
 *   'onedimentional_contiguity::build_covering_cut') - no further
 *   minimization is meaningful there, since the item set has already been
 *   exhaustively ruled out. The paper's own subset-row-cut-style triplet
 *   penalty (their eq. 5c) is instead supported through packingsolver's
 *   general-purpose 'Resource' mechanism (see
 *   'onedimentional_contiguity::add_resource_constraints' for 'milp', and
 *   'onedimentional_contiguity::BranchingScheme' for 'tree_search') - the
 *   same one already used to express SR cuts elsewhere in this codebase
 *   (wang2025_bin_packing) - rather than as a dedicated G2KP construct.
 * - Resources (see 'Resource') are enforced directly by whichever BMP
 *   algorithm is in use, both hard-capacity ones and 'penalize' ones;
 *   'milp' restricts 'penalize' resources to capacity == 1 (unlike
 *   'onedimensional::add_penalize_resource_constraints' in
 *   'milp_assignment.cpp', which also handles larger integer capacities),
 *   every involved item type's consumption a 'threshold_schedule(N)',
 *   needed to linearize them into a MILP row - 'tree_search' has no such
 *   restriction, since it can track true cumulative consumption directly.
 *   Defects, weight, and eligibility are still not modeled (same
 *   exclusions, and for the same reason, as 'bar_relaxation.hpp' - see its
 *   own header comment).
 */

#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace rectangle
{

struct BendersDecompositionContiguityOutput: Output
{
    /** Constructor. */
    BendersDecompositionContiguityOutput(const Instance& instance):
        Output(instance) { }

    /** Number of iterations. */
    Counter number_of_iterations = 0;
};

struct BendersDecompositionContiguityParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /**
     * Whether to solve the BMP relaxation via
     * 'onedimentional_contiguity::tree_search' (the default) or
     * 'onedimentional_contiguity::milp' - see the file-level comment above.
     */
    bool use_tree_search = true;

    /**
     * MILP solver for the master problem - used for 'milp' itself (only
     * when 'use_tree_search' is 'false'), and always for the no-good cut
     * lifting LP ('build_lifted_cut' in the .cpp, part of the
     * 'Feasibility' branch's own cut strengthening, independent of
     * 'use_tree_search').
     */
    mathoptsolverscmake::SolverName master_problem_milp_solver = mathoptsolverscmake::SolverName::Highs;

    /** Guide for the master problem tree search (only used when 'use_tree_search' is 'true'). */
    GuideId master_problem_tree_search_guide_id = 0;

    /**
     * Size of the queue for the master problem tree search in
     * 'NotAnytimeSequential'/'NotAnytimeDeterministic' mode - see
     * 'onedimentional_contiguity::TreeSearchParameters::optimization_mode'.
     * Only used when 'use_tree_search' is 'true' and 'optimization_mode'
     * is not 'Anytime'.
     */
    NodeId master_problem_tree_search_not_anytime_queue_size = 1024;

    /** Maximum number of iterations. */
    Counter maximum_number_of_iterations = -1;

    /**
     * Seed for the random unit-removal orderings of the no-good cut
     * strengthening procedure (Wang et al. 2025 §3.5.2, step (3) - see
     * 'shrink_via_removal' in 'benders_decomposition_contiguity.cpp').
     */
    Seed seed = 0;
};

/**
 * Only supported for the 'Knapsack' and 'Feasibility' objectives, and only
 * for an instance with a single bin type used exactly once (see the
 * file-level comment above).
 */
BendersDecompositionContiguityOutput benders_decomposition_contiguity(
        const Instance& instance,
        const BendersDecompositionContiguityParameters& parameters);

}
}
