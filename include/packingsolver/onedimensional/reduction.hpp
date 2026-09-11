#pragma once

#include "packingsolver/onedimensional/solution.hpp"

#include "optimizationtools/utils/parameters.hpp"

namespace packingsolver
{
namespace onedimensional
{

/** Defined in 'src/onedimensional/solution_builder.hpp'; only used here by pointer/reference. */
class SolutionBuilder;

/**
 * Structure passed as parameters of the reduction algorithm.
 *
 * Deliberately does not inherit from 'packingsolver::Parameters<Instance,
 * Solution, Output>' (unlike most other "*Parameters" structs in this
 * codebase): 'Reduction' is wired into 'optimize()' itself (see
 * 'optimize.hpp'/'optimize.cpp'), so inheriting from a type parameterized on
 * 'onedimensional::Output' would create a circular include between this
 * header and 'optimize.hpp'. 'optimizationtools::Parameters' already
 * provides everything actually needed here (a 'Timer', 'verbosity_level',
 * ...) without that dependency.
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
     * 'Reduction::merge_identical_items'). Runs for any objective - see
     * 'Reduction''s class-level doc comment.
     */
    bool merge_identical_items = true;
};

/**
 * A deliberately small subset of rectangle's own 'Reduction' (see
 * 'packingsolver/rectangle/reduction.hpp' for the full six-operation
 * version and the detailed soundness arguments this one reuses): only the
 * two operations that generalize cleanly across every problem type, with no
 * geometry-specific reasoning at all -
 *
 * - Trimming negative-profit item types (see
 *   'remove_negative_profit_items'): only for 'Knapsack' - for every other
 *   objective every item type is mandatory regardless of its own profit, so
 *   a negative profit changes nothing about whether it must be packed. For
 *   'Knapsack', copies beyond an item type's own 'copies_min' are optional,
 *   and a negative-profit copy is never worth choosing to place on its own
 *   merits, so such copies are trimmed away outright, leaving only whatever
 *   'copies_min' still mandates. Skipped entirely whenever any 'penalize'
 *   resource anywhere has a negative 'penalty' (a one-time profit *bonus*
 *   the first time a bin's consumption crosses the resource's capacity -
 *   see 'Resource''s own doc comment in 'algorithms/common.hpp' - so a
 *   negative-profit item could still be worth including if its own
 *   consumption helps trigger that crossing, an indirect benefit this
 *   purely per-item check cannot see), and, for this problem type
 *   specifically, whenever the item type is involved (as either side) in an
 *   item type precedence (see 'Precedence' in 'instance.hpp'): a precedence
 *   is resolved by 'InstanceBuilder::build()' from both sides' own item
 *   type ids, so trimming one side's copies away entirely here could
 *   silently drop or weaken a precedence relied on elsewhere (e.g.
 *   'milp_assignment') - out of scope for this reduction, which never
 *   touches or interacts with that mechanism. See
 *   'remove_negative_profit_items_applies()'.
 *
 * - Merging identical item types (see 'merge_identical_items'): a merged
 *   item type's copies stay fully visible and independently placed in the
 *   reduced instance (nothing is hidden from the downstream solve), so this
 *   runs for *any* objective, as long as its own per-pair check (length,
 *   plus every property that could make two items behave differently:
 *   weight, nesting length, maximum stackability, maximum weight after,
 *   eligibility, resource consumption schedule, mandatory-vs-optional
 *   copies) finds them truly interchangeable - and, for 'Knapsack'
 *   specifically, profit too, for the same reason as rectangle's own
 *   version (see there): 'Knapsack' optimizes profit directly over a solve
 *   that may legitimately leave copies unplaced, so merging two
 *   different-profit item types would report one uniform profit for every
 *   copy and let the solve itself choose a suboptimal subset on wrong
 *   information. Also never merges an item type involved in a precedence
 *   (either side), for the same reason 'remove_negative_profit_items'
 *   excludes them: a precedence names a specific item type id, and merging
 *   either side into another type would silently change which item type it
 *   actually constrains. See 'items_mergeable()'.
 *
 * For every excluded case, this class no-ops entirely: 'instance()' returns
 * a copy of the original instance, and 'unreduce_solution' is the identity
 * function.
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
     * stable throughout, and this vector's own indices double as original-
     * instance item type ids. Only compacted once, at the very end, when
     * building the final reduced 'Instance' (see 'reduction_to_instance').
     */
    struct ReductionItemType
    {
        bool removed = false;

        /**
         * Item type id (original instance's id space) this one was merged
         * into (see 'merge_identical_items'), or '-1' if it was not merged
         * away.
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
     * be placed, given its current (possibly already trimmed) 'copies' -
     * see rectangle's own 'effective_copies_min' (identical logic, no
     * geometry involved).
     */
    ItemPos effective_copies_min(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id) const;

    /**
     * Trims every negative-profit item type's copies down to its own
     * 'effective_copies_min', removing it outright if that is '0'. Returns
     * 'true' iff at least one item type was trimmed.
     */
    bool remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff item types 'item_type_id_1' and 'item_type_id_2' are
     * interchangeable (see this class's own doc comment).
     */
    bool items_mergeable(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2) const;

    /**
     * Merges every group of pairwise-'items_mergeable' item types (summing
     * their copies into whichever one is encountered first in each group -
     * an arbitrary but consistent choice, safe since merged item types are
     * interchangeable by construction). Returns 'true' iff at least one
     * merge happened.
     */
    bool merge_identical_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * Builds the final reduced 'Instance' from the working representation,
     * populating 'reduced_copy_origins_' along the way.
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types);

    /*
     * Private attributes
     */

    /** The original, non-reduced instance. */
    const Instance* original_instance_;

    /** The reduced instance. */
    Instance instance_;

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
     * For each item type of the reduced instance, one entry per copy (in
     * the order copies will be encountered while scanning a reduced
     * solution) giving where that specific copy came from (see
     * 'CopyOrigin'). Every entry names the *same* original item type id
     * unless that reduced item type absorbed one or more others via
     * 'merge_identical_items', in which case its copies are the
     * concatenation, in order, of every merged-together original item
     * type's own copy range - any consistent order works, since merged
     * item types are interchangeable by construction (see
     * 'items_mergeable'). Indexed by the reduced instance's own item type
     * ids: populated once, in 'reduction_to_instance' (identity - one
     * entry per own copy, nothing merged in - when nothing was actually
     * reduced).
     */
    std::vector<std::vector<CopyOrigin>> reduced_copy_origins_;

};

}
}
