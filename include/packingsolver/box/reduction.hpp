#pragma once

#include "packingsolver/box/solution.hpp"

#include "optimizationtools/utils/parameters.hpp"

namespace packingsolver
{
namespace box
{

/** Defined in 'src/box/solution_builder.hpp'; only used here by pointer/reference. */
class SolutionBuilder;

/**
 * Structure passed as parameters of the reduction algorithm.
 *
 * Deliberately does not inherit from 'packingsolver::Parameters<Instance,
 * Solution, Output>' (unlike most other "*Parameters" structs in this
 * codebase): 'Reduction' is wired into 'optimize()' itself (see
 * 'optimize.hpp'/'optimize.cpp'), so inheriting from a type parameterized on
 * 'box::Output' would create a circular include between this header and
 * 'optimize.hpp'. 'optimizationtools::Parameters' already provides
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
 * A negative-profit item trim (see 'remove_negative_profit_items') plus an
 * identical-item-type merge (see 'merge_identical_items') - the 'box'
 * analogue of 'rectangle::Reduction', scoped down to just these two
 * operations (rectangle's own companion absorption, subset-sum dimension
 * lifting, dominated-item removal and dominated-bin-type removal are not
 * implemented here).
 *
 * - Trimming negative-profit item types (see
 *   'remove_negative_profit_items'): only for 'Knapsack' - for every other
 *   objective every item type is mandatory regardless of its own profit, so
 *   a negative profit changes nothing about whether it must be packed. For
 *   'Knapsack', copies beyond an item type's own 'copies_min' are optional,
 *   and a negative-profit copy is never worth choosing to place on its own
 *   merits - it only ever lowers the objective, and freeing its
 *   footprint/weight/resource consumption for something else can only help
 *   or be neutral - so such copies are trimmed away outright, leaving only
 *   whatever 'copies_min' still mandates. Skipped entirely whenever any
 *   'penalize' resource anywhere has a negative 'penalty': that is a
 *   one-time profit *bonus* the first time a bin's consumption crosses the
 *   resource's capacity (see 'Resource''s own doc comment in
 *   'algorithms/common.hpp'), so a negative-profit item could still be
 *   worth including if its own consumption helps trigger that crossing - an
 *   indirect benefit this purely per-item check cannot see. See
 *   'remove_negative_profit_items_applies()'.
 *
 * - Merging identical item types (see 'merge_identical_items'): a merged
 *   item type's copies stay fully visible and independently placed in the
 *   reduced instance (nothing is hidden from the downstream solve), so this
 *   runs for *any* objective, as long as its own per-pair check (box
 *   dimensions, allowed rotation set, weight, and per-bin-type resource
 *   consumption schedule) finds the two item types truly interchangeable -
 *   and, for 'Knapsack' specifically, profit too: every other objective
 *   ignores profit as a merge criterion (it is never the actual objective,
 *   and 'unreduce_solution' always restores each copy's true original
 *   profit regardless of merging), but 'Knapsack' optimizes profit directly
 *   over a solve that may legitimately leave copies unplaced, so merging
 *   two different-profit item types would report one uniform profit for
 *   every copy and let the solve itself choose a suboptimal subset on wrong
 *   information - restoring true profits afterwards cannot undo that. Two
 *   item types are only ever merged when both are fully mandatory
 *   ('copies_min == copies') or both fully optional ('copies_min == 0') -
 *   never a mix, since a combined 'copies_min' would then force some
 *   arbitrary subset of the merged copies to be placed, unrelated to which
 *   original type they actually came from.
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
     * stable throughout, so that this vector's own indices double as
     * original-instance item type ids. It is only compacted once, at the
     * very end, when building the final reduced 'Instance' (see
     * 'reduction_to_instance').
     */
    struct ReductionItemType
    {
        bool removed = false;

        /**
         * Item type id (original instance's id space) this one was merged
         * into (see 'merge_identical_items'), or '-1' if it was not merged
         * away. Only ever set alongside 'removed = true', but distinct from
         * the "trimmed to zero copies" reason 'removed' can also be
         * 'true' for: a merged-away item type's own 'copies' still
         * contribute to the survivor's - see 'reduction_to_instance'.
         */
        ItemTypeId merged_into = -1;

        /**
         * Current (possibly reduced) number of copies. Starts at the
         * original instance's own copies and only ever decreases, via
         * 'remove_negative_profit_items' trimming it down to
         * 'effective_copies_min'.
         */
        ItemPos copies = 0;
    };

    /**
     * Minimum number of copies of item type 'item_type_id' that must still
     * be packed, given its current (possibly already trimmed) 'copies' -
     * 'original_instance_->item_type(item_type_id).copies_min' for
     * 'Knapsack' (capped by 'copies', which only ever matters as an
     * invariant guard - 'remove_negative_profit_items' never trims below
     * it), or, for every other objective, 'copies_min' minus however many
     * copies have already been trimmed away, floored at 0.
     */
    ItemPos effective_copies_min(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id) const;

    /**
     * Trims away negative-profit optional copies (see the class-level doc
     * comment's first paragraph). Returns 'true' iff at least one item type
     * was trimmed.
     */
    bool remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff item types 'item_type_id_1' and 'item_type_id_2' are
     * truly interchangeable and safe to merge (see the class-level doc
     * comment's second paragraph).
     */
    bool items_mergeable(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2) const;

    /**
     * Merges every group of pairwise-'items_mergeable' item types in
     * 'reduction_item_types' (marking every non-survivor 'removed', with
     * 'merged_into' set). Returns 'true' iff at least one merge happened.
     */
    bool merge_identical_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * Builds the final reduced 'Instance' from 'reduction_item_types',
     * populating 'reduced_item_origin_runs_' along the way.
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types);

    /*
     * Private attributes
     */

    /** Original instance. */
    const Instance* original_instance_;

    /** Reduced instance. */
    Instance instance_;

    /**
     * One contiguous run of a reduced item type's copies that all came
     * from the same original item type - the survivor's own copies form
     * one run, and each item type merged into it (see
     * 'merge_identical_items') contributes one more, in discovery order.
     * A reduced item type that was never merged at all has exactly one
     * run, spanning all of its own copies.
     */
    struct OriginRun
    {
        /** Item type id, original instance's id space. */
        ItemTypeId item_type_id;

        /** Number of consecutive copies drawn from 'item_type_id'. */
        ItemPos count;
    };

    /**
     * For each item type of the reduced instance, its own ordered list of
     * 'OriginRun's (see there). Indexed by the reduced instance's own item
     * type ids: populated once, in 'reduction_to_instance'.
     *
     * This is a run-length-encoded alternative to naming an original item
     * type id for every individual copy: since every merge only ever
     * concatenates whole *ranges* of original copies (never interleaves
     * them - see 'items_mergeable'), a handful of runs always suffice
     * regardless of how many copies a type actually has, which is what
     * lets 'unreduce_solution' resolve even a many-thousand-copy pattern
     * in time proportional to the number of *runs* actually crossed, not
     * to the number of copies itself.
     */
    std::vector<std::vector<OriginRun>> reduced_item_origin_runs_;

    /**
     * Consumes one copy from the end of 'runs' (a working copy of one
     * reduced item type's own entry in 'reduced_item_origin_runs_'),
     * returning the original item type id it came from, and dropping that
     * last run once it is exhausted. Any consistent consumption order
     * works, since a reduced item type's copies are interchangeable by
     * construction - consuming from the end lets the caller use a plain
     * 'pop_back()' instead of erasing the front (which would have to
     * shift every remaining run down).
     */
    static ItemTypeId consume_one_origin(
            std::vector<OriginRun>& runs);

};

}
}
