#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace rectangle
{

/**
 * A cut (dual-feasible-function cut, no-good cut, or pairwise-
 * incompatibility cut) turned into a onedimensional resource on a specific
 * bin type.
 *
 * A per-item-type consumption is a per-copy schedule (see
 * 'onedimensional::Resource::item_consumptions'), not a single
 * scalar: a dual-feasible-function cut is a plain linear inequality on item
 * counts, so a uniform (length-1) schedule is exact for it; a no-good cut
 * or pairwise-incompatibility cut instead needs "at least N copies of this
 * item type", which a uniform per-unit consumption cannot express (it would
 * only cap the *combined* total of the item types involved, wrongly
 * excluding unrelated combinations using more of one item type and none of
 * another) - achieved exactly via 'threshold_schedule' below.
 */
struct ResourceCut
{
    double capacity;
    std::vector<std::pair<ItemTypeId, std::vector<double>>> consumption;
};

/**
 * Per-copy consumption schedule making an item type's contribution to a
 * resource's total equal to 'min(count, threshold)': 'threshold' ones
 * followed by a single trailing zero. Contribution keeps growing only while
 * count < threshold, and never grows past it - so summing this schedule's
 * contribution over every item type in a selection, and setting the
 * resource's capacity to '(sum of thresholds) - 1', forbids exactly the
 * selection (and any selection using at least as many copies of every item
 * type in it), without excluding anything else.
 */
std::vector<double> threshold_schedule(ItemPos threshold);

struct BendersDecompositionOutput: Output
{
    /** Constructor. */
    BendersDecompositionOutput(const Instance& instance):
        Output(instance) { }

    /** Number of iterations. */
    Counter number_of_iterations = 0;
};

struct BendersDecompositionParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver = mathoptsolverscmake::SolverName::Highs;

    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Use the tree search algorithm to solve the master problem. */
    bool master_problem_use_tree_search = false;

    /** Use the MILP assignment algorithm to solve the master problem. */
    bool master_problem_use_milp_assignment = true;

    /**
     * Maximum number of iterations, in non-'Anytime' optimization modes
     * only (-1: unlimited). 'Anytime' mode instead relies on the timer (or
     * full convergence, i.e. every bin of the master's candidate found
     * geometrically feasible) to stop, since it is expected to keep
     * improving for as long as it is given to run.
     */
    Counter not_anytime_maximum_number_of_iterations = -1;

    /**
     * Maximum number of iterations that actually solve at least one
     * feasibility subproblem, in non-'Anytime' optimization modes only
     * (-1: unlimited). Unlike 'not_anytime_maximum_number_of_iterations',
     * an iteration whose master candidate is cut by a dual-feasible-
     * function inequality before any subproblem is solved (see
     * 'find_most_violated_dual_feasible_function_cut' in
     * 'benders_decomposition.cpp') doesn't count towards this limit, since
     * it never actually pays for a subproblem solve.
     */
    Counter not_anytime_maximum_number_of_subproblem_solves = 0;

    /** Size of the queue for the knapsack subproblem. */
    NodeId subproblem_queue_size = 512;

    /**
     * Maximum number of minimal-infeasible-subset no-good cuts to generate
     * per infeasible bin found (see 'enumerate_minimal_infeasible_subsets'
     * in 'benders_decomposition.cpp'); bounds how much the search for
     * additional cuts can cost per Benders iteration.
     */
    Counter maximum_number_of_no_good_cuts_per_bin = 8;
};

/**
 * Lift a no-good cut via the sequential lifting procedure of Balas (1975)
 * and Wolsey (1975), as adapted by Côté, Haouari & Iori (2021), Section 7.3
 * ("Lifting the Cut", their Algorithm 2): starting from 'S := C' (the
 * original cut's item types, each worth profit 1 per copy up to its
 * threshold), visit every item type not already in the cut, in id order,
 * and compute how many "covering units" it is worth by solving a 2D
 * knapsack that forces one copy of it into the bin alongside the best
 * achievable selection from 'S'. The paper solves that knapsack exactly
 * (2D-KP); this uses the bar relaxation ('bar_relaxation.hpp', Scheithauer
 * 1999) instead, the same substitution the paper itself makes for the same
 * reason (2D-KP is NP-hard, the bar relaxation is not) via its "2D-UKP".
 *
 * Concretely, for candidate j*: let 'cover_size' be '|C|' in the paper's
 * own notation - the *original* cover's size, a fixed constant for the
 * whole procedure, never updated as more items get lifted (only 'S' itself
 * grows - see 'cover_size''s own doc comment below for why getting this
 * backwards is a real, previously-shipped bug: it compounds without
 * bound). Solve a Knapsack instance on this bin type with every item type
 * in 'S' at its tracked (copies, profit) - profit 1 per copy for the
 * original cut's items, or a lifted item's own coefficient for exactly 1
 * copy (see below) - plus j* forced into exactly 1 copy via 'copies_min'
 * (a hard bound, not a profit incentive: bar_relaxation is only a linear
 * relaxation, so a sufficiently high profit only guarantees j* wins the
 * *aggregate* trade-off against everything 'S' could contribute in total -
 * it says nothing about the *marginal*, per-unit-of-shared-capacity
 * trade-off the LP actually optimizes over, which a single very dense item
 * already in 'S', e.g. a previously-lifted one with a large coefficient on
 * a small footprint, could still win, leaving j* selected fractionally
 * below 1 - so 'copies_min' is the only mechanism that actually guarantees
 * j*'s quantity reaches 1 at the optimum), at profit 0 (j*'s own real
 * profit is irrelevant here - only 'S''s achievable profit alongside its
 * forced presence matters). The reported bound is then exactly 'S''s
 * contribution alongside that forced copy - a valid (since the bar
 * relaxation is itself a relaxation) upper bound on the exact 2D-KP value
 * the paper calls for. The lift coefficient is then
 * 'alpha = max(0, cover_size - 1 - that bound)', floored to the nearest
 * integer below it (never above: the true, exact-2D-KP-based coefficient
 * is itself an integer at least as large as any valid upper-bound-based
 * estimate of it, so rounding down can only make this estimate more
 * conservative, never unsound). If alpha > 0, j* joins 'S' - contributing
 * exactly 1 copy at profit 'alpha' to every future candidate's knapsack
 * (not 'alpha' copies at profit 1: unlike the original cut's items, which
 * really do represent that many physical units, j* is a single physical
 * item whose *equivalent covering worth* happens to be 'alpha') - and is
 * appended to the same, single cut being built, with its own row
 * contribution capped at 'alpha' via 'threshold_schedule' exactly as for
 * the original items.
 *
 * Unlike a purely geometric domination check (fits within the footprint of
 * some subset of 'S', therefore forces at least that much room), this
 * needs no notion of "dominates": the relaxation bound already accounts
 * for every item type in 'S' at once, so nothing here is unsound the way
 * merging several independently-computed dominators into one shared row
 * would be. It also generalizes cleanly to non-geometric infeasibility
 * (e.g. a weight or axle-load limit the original cut's infeasibility proof
 * actually turned on): the bar relaxation is a valid upper bound on the
 * exact 2D knapsack value regardless of which other constraints made 'C'
 * infeasible in the first place, since dropping constraints from a
 * relaxation can only raise that bound, never lower it below the truth.
 */
ResourceCut lift_no_good_cut(
        const ResourceCut& original_cut,
        const Instance& instance,
        BinTypeId bin_type_id,
        const BendersDecompositionParameters& parameters);

BendersDecompositionOutput benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters);

}
}
