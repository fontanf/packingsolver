/**
 * Onedimensional-contiguity MILP
 *
 * MILP formulation of the BMP relaxation (Côté, Dell'Amico & Iori (2014) /
 * Wang, Baldacci, Furini, Wei & Liu (2025) - see the file-level comment in
 * '../benders_decomposition_contiguity.hpp' for the full derivation): the
 * bin is cut into W unit-width columns, and each unit of an item, in one
 * of its allowed orientations, is assigned to a run of *consecutive*
 * columns (its 'x' position), subject to a per-column height budget and
 * any accumulated no-good cuts. Solved as an actual 0-1 MILP: variable
 * rho[u][x][o] is 1 iff unit 'u' is placed at column 'x' in orientation 'o'.
 *
 * Self-contained: 'milp' takes a plain 'rectangle::Instance' (single bin
 * type, used exactly once) and a set of no-good cuts, and rebuilds its own
 * unit/candidate model from them on every call - no state is threaded
 * through from the caller. This lets 'tree_search.hpp' (an alternative,
 * combinatorial way to solve the exact same relaxation) share the exact
 * same input/output contract, so
 * '../benders_decomposition_contiguity.cpp''s outer Benders loop can call
 * either one interchangeably.
 */

#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

#include "optimizationtools/utils/parameters.hpp"

namespace packingsolver
{
namespace rectangle
{
namespace onedimentional_contiguity
{

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Candidates ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * A single unit's candidate placement: unit 'copy' of item type
 * 'item_type_id', in orientation 'rotate', placed with its left edge at
 * column 'x' (so it covers columns '[x, x + width - 1]').
 */
struct Candidate
{
    ItemTypeId item_type_id;
    ItemPos copy;
    Length x;
    bool rotate;
    Length width;
    Length height;

    bool operator==(const Candidate& candidate) const
    {
        return item_type_id == candidate.item_type_id
            && copy == candidate.copy
            && x == candidate.x
            && rotate == candidate.rotate;
    }
};

/**
 * Output of 'build_units_and_candidates' below.
 */
struct UnitsAndCandidates
{
    /** Every candidate, one per unit, per allowed orientation, per left-justified column position. */
    std::vector<Candidate> candidates;

    /**
     * Candidate ids of each unit (one physical item copy, flattened out of
     * 'ItemType::copies' so that each copy gets its own, independent set of
     * candidates - matching how the rest of this codebase treats per-copy
     * selection when copies need to be tracked individually, see
     * 'aggregate_units'/'units' in 'benders_decomposition.cpp'), indexed by
     * '[item_type_id][copy]'.
     */
    std::vector<std::vector<std::vector<size_t>>> candidates_by_item_type_and_copy;

    /** Candidate ids of each item type (every copy combined), indexed by 'item_type_id'. */
    std::vector<std::vector<size_t>> candidates_by_item_type;

    /** Candidate ids covering each column, indexed by column. */
    std::vector<std::vector<size_t>> candidates_by_column;
};

/**
 * A unit's rotation: everything about it other than its width, which
 * 'build_units_and_candidates' below tracks separately (in parallel, both
 * indexed '[unit_id][rotation_id]') since 'minimal_mim_patterns' wants a
 * bare 'std::vector<std::vector<Length>>' of widths.
 */
struct UnitRotation
{
    bool rotate;
    Length height;
};

/**
 * Build every unit and every 'Candidate' for the given bin - see
 * 'UnitsAndCandidates'.
 *
 * x-positions are reduced to the meet-in-the-middle (MIM) pattern set of
 * every unit, computed jointly across all of its own rotations by
 * 'minimal_mim_patterns' (see 'meet_in_the_middle.hpp', in particular its
 * "Rotations" section: passing each unit's full list of rotation widths,
 * rather than flattening every rotation into its own independent entry, is
 * what lets it treat "this unit, in some rotation" as a single choice when
 * computing every *other* unit's own candidate positions - avoiding the
 * unsound-if-flattened alternative of a unit's two rotations both being
 * available to combine with other units' widths at once), rather than every
 * integer column a rotation's width fits at: this is what keeps the model
 * tractable on instances with many item types, since the number of
 * x-position variables per unit would otherwise grow linearly with the bin
 * width.
 *
 * Deterministic given 'instance' (fixed iteration order over item types,
 * copies and orientations, and a deterministic 'minimal_mim_patterns'), so
 * repeated calls on the same 'instance' always produce the exact same
 * 'candidates' list in the exact same order - the id space 'NoGoodCut::
 * candidate_ids' below relies on this to stay meaningful across the
 * independent rebuilds 'milp'/'tree_search' each perform on every call.
 */
UnitsAndCandidates build_units_and_candidates(
        const Instance& instance,
        const BinType& bin_type);

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Result /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Result of 'milp'/'tree_search' below: solving the BMP/LBMP relaxation for
 * a given rectangle instance (single bin type, used exactly once) subject
 * to a set of no-good cuts.
 */
struct OnedimensionalContiguityResult
{
    enum class Status
    {
        /** A feasible selection of units (possibly empty) was found. */
        Feasible,
        /** The relaxation itself (including all cuts) has no feasible solution. */
        Infeasible,
        /**
         * The search ended (timed out, or - for 'tree_search' in
         * non-'Anytime' mode - its fixed beam width wasn't wide enough to
         * be exhaustive) without finding a feasible selection and without
         * proving there is none.
         */
        Inconclusive,
    };

    /** Default to 'Inconclusive': every genuine 'Feasible'/'Infeasible' result must set this explicitly. */
    Status status = Status::Inconclusive;

    /** Objective value (0 for 'Feasibility'); only meaningful if 'status == Feasible'. */
    double objective_value = 0.0;

    /** Selected units, i.e. the chosen placement of every selected unit; only meaningful if 'status == Feasible'. */
    std::vector<Candidate> selected_units;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// No-good cuts /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * A no-good cut to add to the relaxation, in one of two shapes (see the
 * 'build_positional_no_good_cut'/'build_covering_cut' builders below and
 * Wang et al. (2025)'s own §3/§3.1.1 for the derivation of each):
 * - Positional (their eq. 8): one candidate per unit - the exact (x,
 *   orientation) each selected unit used - so the row only forbids
 *   reproducing that exact full combination again; a different position
 *   for even a single unit already satisfies it.
 * - Covering (their §3.1.1 "covering cut"): every one of each selected
 *   unit's own candidates (every x, every orientation), not just the one
 *   used, so the row forbids that unit *ever* being selected together with
 *   the others again, at any position.
 * Both share the same row shape once built - 'candidate_ids' at
 * coefficient 1 each, upper-bounded by 'upper_bound' - only the exact set
 * of candidates differs; 'upper_bound' is always 'number of distinct units
 * involved - 1' (not 'candidate_ids.size() - 1', which only coincides with
 * it for a positional cut).
 *
 * 'candidate_ids' reference the canonical id space 'build_units_and_candidates'
 * assigns for the instance the cut was built for (see its own doc comment) -
 * meaningful across independent rebuilds of the same instance, which is what
 * lets a single 'std::vector<NoGoodCut>' accumulated by the outer Benders
 * loop be replayed unchanged into either 'milp' or 'tree_search'.
 */
struct NoGoodCut
{
    std::vector<size_t> candidate_ids;
    ItemPos upper_bound;
};

/**
 * Positional no-good cut (Wang et al. 2025 eq. 8): forbids reproducing
 * 'selected_candidate_ids' exactly - same units, at the same positions.
 */
NoGoodCut build_positional_no_good_cut(
        const std::vector<size_t>& selected_candidate_ids);

/**
 * Covering no-good cut (Wang et al. 2025 §3.1.1): forbids the unit set
 * behind 'selected_units' from ever being selected together again, at *any*
 * positions. Only sound once every positioning of that exact unit set has
 * been proven infeasible - see 'benders_decomposition_contiguity.cpp''s own
 * 'Knapsack' branch, reached only after LBD returns 'Infeasible'.
 *
 * Rebuilds its own 'UnitsAndCandidates' from 'instance' (see
 * 'build_units_and_candidates') to look up each selected unit's own full
 * candidate list ('candidates_by_item_type_and_copy[item_type_id][copy]') -
 * not just the single one it was selected at.
 */
NoGoodCut build_covering_cut(
        const Instance& instance,
        const std::vector<Candidate>& selected_units);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////// MILP //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Add resource-capacity rows (see 'Resource') to 'model', for every
 * resource of the instance's own (single) bin type.
 *
 * Resource consumption only depends on which units are selected, never on
 * their (x, orientation), so - exactly like the onedimensional-relaxation
 * Benders decomposition's own master (see 'build_master_instance' in
 * 'benders_decomposition.cpp': "Resources are enforced directly by the
 * master's own MILP this way: the geometric slave subproblem never needs to
 * know about them at all") - this belongs entirely at the relaxation level,
 * not in 'y_check': a plain linear row over the same per-candidate
 * variables the model already uses for everything else.
 * 'presence(item_type_id, copy)' below (the sum of that unit's own
 * candidate variables, across every (x, orientation) - the same grouping
 * the "each unit selected at most once" row already uses) stands in for a
 * single 0/1 "is this unit present" variable: the existing "at most one of
 * its own candidates" row already keeps it in {0, 1}.
 *
 * A hard-capacity resource ('!penalize') becomes a single row per resource:
 *     sum_{item_type_id, copy} consumption(item_type_id, copy) * presence(item_type_id, copy) <= capacity
 * the same coefficient applying to every one of a unit's own candidates,
 * since consumption doesn't depend on (x, orientation).
 *
 * A 'penalize' resource instead only supports capacity == 1 (unlike
 * 'onedimensional::add_penalize_resource_constraints' in
 * 'milp_assignment.cpp', which also handles larger integer capacities -
 * see its own doc comment there for the full derivation and validation
 * this mirrors for the capacity == 1 case only), every involved item
 * type's consumption a 'threshold_schedule(N)': one binary "excess"
 * variable 'psi', with objective coefficient '-penalty', and a pairwise
 * clique row 'presence(u) + presence(v) - psi <= 1' for every pair of units
 * {u, v} the resource involves - the same Jepsen et al. (2008)/Wang et al.
 * (2025 eq. 5c) linearization, over 'presence(u)' here instead of a single
 * 0/1 variable per unit. Skipped entirely for 'Feasibility': a 'penalize'
 * resource never blocks packing and there is no profit to penalize there.
 */
void add_resource_constraints(
        const Instance& instance,
        const UnitsAndCandidates& units_and_candidates,
        mathoptsolverscmake::MathOptModel& model);

/** Parameters for 'milp'. */
struct MilpParameters: optimizationtools::Parameters
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver = mathoptsolverscmake::SolverName::Highs;

    /**
     * Optimization mode.
     *
     * Threaded through from the caller for interface symmetry with
     * 'TreeSearchParameters' (so callers can set it uniformly regardless
     * of 'BendersDecompositionContiguityParameters::use_tree_search'); has
     * no effect on 'milp' itself, which always solves the relaxation to
     * (LP-)optimality or until the timer ends either way.
     */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;
};

/**
 * Build and solve the BMP/LBMP relaxation as a 0-1 MILP directly assigning
 * every selected unit an (x, orientation) - see the file-level comment
 * above and in 'benders_decomposition_contiguity.hpp'.
 */
OnedimensionalContiguityResult milp(
        const Instance& instance,
        const std::vector<NoGoodCut>& cuts,
        const MilpParameters& parameters);

}
}
}
