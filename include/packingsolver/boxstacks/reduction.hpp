#pragma once

#include "packingsolver/boxstacks/solution.hpp"

#include "optimizationtools/utils/parameters.hpp"

namespace packingsolver
{
namespace boxstacks
{

/** Defined in 'src/boxstacks/solution_builder.hpp'; only used here by pointer/reference. */
class SolutionBuilder;

/**
 * Structure passed as parameters of the reduction algorithm.
 *
 * Deliberately does not inherit from 'packingsolver::Parameters<Instance,
 * Solution, Output>' (unlike most other "*Parameters" structs in this
 * codebase): 'Reduction' is wired into 'optimize()' itself (see
 * 'optimize.hpp'/'optimize.cpp'), so inheriting from a type parameterized on
 * 'boxstacks::Output' would create a circular include between this header
 * and 'optimize.hpp'. 'optimizationtools::Parameters' already provides
 * everything actually needed here (a 'Timer', 'verbosity_level', ...)
 * without that dependency.
 */
struct ReductionParameters: optimizationtools::Parameters
{
    /** Boolean indicating if the reduction should be performed. */
    bool reduce = true;

    /**
     * Enable/disable trimming negative-profit item types down to their own
     * 'copies_min' (gates 'Reduction::remove_negative_profit_items'). Still
     * subject to 'Reduction::remove_negative_profit_items_applies'
     * regardless of this flag - see there for its own precondition.
     */
    bool remove_negative_profit_items = true;

    /**
     * Enable/disable merging identical item types (gates
     * 'Reduction::merge_identical_items').
     */
    bool merge_identical_items = true;
};

/**
 * Instance reduction (preprocessing): a small subset of
 * 'rectangle::Reduction''s operations (see its own class-level doc comment
 * for the full six-operation version), restricted here to the two whose
 * soundness argument does not depend on anything specific to 2D geometry:
 *
 * - Trimming negative-profit item types (see
 *   'remove_negative_profit_items'): only for 'Knapsack' - every other
 *   objective needs every copy of every item type packed regardless, so a
 *   negative profit changes nothing about whether it must be packed. For
 *   'Knapsack', copies beyond an item type's own 'copies_min' are optional,
 *   and a negative-profit copy is never worth choosing to place on its own
 *   merits - it only ever lowers the objective, and freeing its
 *   footprint/weight for something else can only help or be neutral - so
 *   such copies are trimmed away outright, leaving only whatever
 *   'copies_min' still mandates. Unlike 'rectangle::Reduction''s own
 *   version of this operation, there is no 'penalize'-resource exception
 *   to skip here: 'boxstacks' bin/item types carry no resource-consumption
 *   mechanism at all (unlike 'rectangle'), so no indirect profit bonus can
 *   ever make a negative-profit copy worth keeping.
 *
 * - Merging identical item types (see 'merge_identical_items'): a merged
 *   item type's copies stay fully visible and independently placed in the
 *   reduced instance (nothing is hidden from the downstream solve), so this
 *   runs for *any* objective, as long as its own per-pair check (dimensions,
 *   plus every property that could make two items behave differently:
 *   allowed rotations, weight, group, stackability id, nesting height,
 *   maximum stackability, maximum weight above, and mandatory-vs-optional
 *   copies) finds them truly interchangeable - and, for 'Knapsack'
 *   specifically, profit too (every other objective ignores profit as a
 *   merge criterion, since 'unreduce_solution' always restores each copy's
 *   true original profit regardless of merging - but 'Knapsack' optimizes
 *   profit directly over a solve that may legitimately leave copies
 *   unplaced, so merging two different-profit item types would report one
 *   uniform profit for every copy and let the solve choose a suboptimal
 *   subset on wrong information).
 *
 * Unlike 'rectangle::Reduction', neither operation here can ever prove the
 * instance infeasible or hide items in a dedicated bin outside the reduced
 * instance, so this class has no 'proven_infeasible()'/
 * 'number_of_dedicated_bins()' equivalents - 'instance()' is always a
 * meaningful (possibly identical, if 'parameters.reduce' is 'false' or
 * neither operation found anything) reduced instance to solve directly.
 */
class Reduction
{

public:

    /** Constructor. */
    Reduction(
            const Instance& instance,
            const ReductionParameters& parameters = {});

    /** Get the reduced instance. */
    const Instance& instance() const { return instance_; }

    /** Unreduce a solution of the reduced instance. */
    Solution unreduce_solution(
            const Solution& solution) const;

private:

    /**
     * 'true' iff trimming negative-profit item types (see
     * 'remove_negative_profit_items') is meaningful for 'instance' - only
     * 'Knapsack', and only when no 'penalize' resource anywhere has a
     * negative 'penalty'. Does not check 'parameters.reduce' - the
     * constructor only calls this after already checking it.
     */
    static bool remove_negative_profit_items_applies(const Instance& instance);

    /*
     * Private types
     */

    /**
     * Working representation of an item type during the reduction process.
     *
     * Item types are never physically removed from this vector while the
     * reduction is running (only marked 'removed'): this keeps every id
     * stable throughout, and this vector's own indices double as
     * original-instance item type ids. Only ever holds what the reduction
     * process can actually change ('copies', via a negative-profit trim)
     * plus the bookkeeping needed to fold a merged-away item type into its
     * survivor ('merged_into'). Every other item type field (profit,
     * rotations, weight, ...) never changes during the process, so it is
     * read directly from 'original_instance_' wherever needed.
     */
    struct ReductionItemType
    {
        bool removed = false;

        /**
         * Item type id (original instance's id space) this one was merged
         * into (see 'merge_identical_items'), or '-1' if it was not merged
         * away. Only ever set alongside 'removed = true', but distinct from
         * the negative-profit-trim reason 'removed' can also be 'true' for:
         * a merged-away item type's own 'copies' still contribute to its
         * survivor's - see 'reduction_to_instance'.
         */
        ItemTypeId merged_into = -1;

        /**
         * Current (possibly trimmed) number of copies. Starts at the
         * original instance's own copies and only ever decreases, via
         * 'remove_negative_profit_items'.
         */
        ItemPos copies = 0;
    };

    /**
     * Minimum number of copies of item type 'item_type_id' that must still
     * be packed, given its current (possibly already-trimmed) 'copies' -
     * mirrors 'rectangle::Reduction::effective_copies_min'. For 'Knapsack',
     * simply 'min(original copies_min, current copies)' (a negative-profit
     * trim never goes below the type's own true 'copies_min', so the
     * 'min' never actually tightens anything in practice - it only guards
     * the invariant). For every other objective, every copy is normally
     * mandatory, but this class never actually shrinks 'copies' for a
     * non-'Knapsack' instance (see 'remove_negative_profit_items_applies'),
     * so this always simply returns the original 'copies_min' there.
     */
    ItemPos effective_copies_min(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id) const;

    /**
     * Trims away optional (beyond 'copies_min'), negative-profit copies of
     * every item type - see the class-level doc comment. Returns 'true' iff
     * at least one item type was trimmed.
     */
    bool remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff item types 'item_type_id_1' and 'item_type_id_2' are truly
     * interchangeable and safe to merge - see the class-level doc comment's
     * "Merging identical item types" paragraph for the full criteria list
     * and the reasoning behind the mandatory/optional consistency
     * requirement.
     */
    bool items_mergeable(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2) const;

    /**
     * Merges every group of pairwise-'items_mergeable' item types in
     * 'reduction_item_types' (first one found in each group survives;
     * 'removed'/'merged_into' are set on the rest - 'reduction_to_instance'
     * reads them directly). Returns 'true' iff at least one merge happened.
     */
    bool merge_identical_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * Where one particular copy of a reduced instance item type came from:
     * the corresponding item type id in the *original* instance, and that
     * original item type's own local copy index.
     */
    struct CopyOrigin
    {
        /** Item type id, original instance's id space. */
        ItemTypeId item_type_id;

        /** 'item_type_id''s own local copy index (0-indexed). */
        ItemPos copy_index;
    };

    /**
     * Builds the final reduced 'Instance' from 'reduction_item_types':
     * copies every bin type over unchanged (no bin-type-level operation in
     * this class), then, for each original item type not entirely trimmed
     * away and not merged into another, adds one item type with the
     * combined copies/'copies_min' of itself and everything merged into it
     * - populating 'reduced_copy_origins_' alongside (see its own doc
     * comment).
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types);

    /*
     * Private attributes
     */

    /** The original, unreduced instance. */
    const Instance* original_instance_;

    /** The reduced instance. */
    Instance instance_;

    /**
     * For each item type of the reduced instance, one entry per copy (in
     * the order copies will be encountered while scanning a reduced
     * solution) giving where that specific copy came from (see
     * 'CopyOrigin'). Every entry names the same original item type id
     * unless that reduced item type absorbed one or more others via
     * 'merge_identical_items', in which case its copies are the
     * concatenation, in order, of every merged-together original item
     * type's own copy range - any consistent order works, since merged
     * item types are interchangeable by construction (see
     * 'items_mergeable'). Indexed by the reduced instance's own item type
     * ids: populated once, in 'reduction_to_instance'.
     */
    std::vector<std::vector<CopyOrigin>> reduced_copy_origins_;

};

}
}
