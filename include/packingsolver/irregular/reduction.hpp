#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "optimizationtools/utils/parameters.hpp"

namespace packingsolver
{
namespace irregular
{

/** Defined in 'src/irregular/solution_builder.hpp'; only used here by pointer/reference. */
class SolutionBuilder;

/**
 * Structure passed as parameters of the reduction algorithm.
 *
 * Deliberately does not inherit from 'packingsolver::Parameters<Instance,
 * Solution, Output>' (unlike most other "*Parameters" structs in this
 * codebase): 'Reduction' is wired into 'optimize()' itself (see
 * 'optimize.hpp'/'optimize.cpp'), so inheriting from a type parameterized
 * on 'irregular::Output' would create a circular include between this
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
     * regardless of this flag.
     */
    bool remove_negative_profit_items = true;

    /**
     * Enable/disable merging identical item types (gates
     * 'Reduction::merge_identical_items').
     */
    bool merge_identical_items = true;
};

/**
 * Instance reduction (preprocessing) for problem type 'irregular'.
 *
 * A cut-down port of 'rectangle::Reduction' (see
 * 'include/packingsolver/rectangle/reduction.hpp' for the general
 * rationale behind both operations below) restricted to the two operations
 * that generalize to every problem type - unlike rectangle, this class
 * does not implement companion absorption, subset-sum-based item dimension
 * lifting, dominated-item/bin-type removal, or full-span item pinning:
 *
 * - Trimming negative-profit item types (see
 *   'remove_negative_profit_items'): only for 'Knapsack' - for every other
 *   objective every item type is mandatory regardless of its own profit.
 *   Copies beyond an item type's own 'copies_min' are optional, and a
 *   negative-profit copy is never worth choosing to place on its own
 *   merits, so such copies are trimmed away outright. Skipped entirely
 *   whenever any 'penalize' resource anywhere has a negative 'penalty':
 *   that is a one-time profit *bonus* the first time a bin's consumption
 *   crosses the resource's capacity, so a negative-profit item could still
 *   be worth including if its own consumption helps trigger that crossing
 *   - an indirect benefit this purely per-item check cannot see. See
 *   'remove_negative_profit_items_applies()'.
 *
 * - Merging identical item types (see 'merge_identical_items'): a merged
 *   item type's copies stay fully visible and independently placed in the
 *   reduced instance, so this operation runs for *any* objective, as long
 *   as its own per-pair check (identical shapes/quality rules, identical
 *   allowed rotations, weight/eligibility/group equivalents this problem
 *   type does not have, and - per bin type - identical resource
 *   consumption schedule) finds two item types truly interchangeable -
 *   and, for 'Knapsack' specifically, profit too (see 'items_mergeable').
 *   Item types are compared for exact shape equality (see
 *   'shape::operator==(const ShapeWithHoles&, const ShapeWithHoles&)') -
 *   a conservative check: two item types with congruent but differently
 *   represented shapes are missed, only costing a missed reduction
 *   opportunity, never correctness.
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

    /**
     * Working representation of an item type during the reduction process.
     *
     * Item types are never physically removed from this vector while the
     * reduction is running (only marked 'removed'): this keeps every id
     * stable, and this vector's own indices double as original-instance
     * item type ids throughout. Only holds what the reduction process can
     * actually change ('copies') plus the bookkeeping needed to undo it
     * ('removed'/'merged_into') - every other item type field (profit,
     * shapes, ...) never changes here, so it is read directly from
     * 'original_instance_' wherever needed.
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
         * Current (possibly reduced) number of copies. Starts at the
         * original instance's own copies and only ever decreases, via
         * 'remove_negative_profit_items'.
         */
        ItemPos copies = 0;
    };

    /**
     * Effective minimum number of copies of item type 'item_type_id' still
     * required, given its current (possibly already-trimmed) 'copies' -
     * see 'remove_negative_profit_items''s own use for why this differs
     * from the original instance's own 'copies_min' for non-'Knapsack'
     * objectives (this operation never actually shrinks copies there, so
     * the distinction is only ever exercised for 'Knapsack' in practice,
     * but the same formula as 'rectangle::Reduction' is kept for
     * consistency and future-proofing against any operation added later
     * that also shrinks copies outside 'Knapsack').
     */
    ItemPos effective_copies_min(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id) const;

    /**
     * Trims every negative-profit item type's copies down to its own
     * 'effective_copies_min' (removing it entirely if that is 0). Returns
     * 'true' iff at least one item type was trimmed.
     */
    bool remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff item types 'item_type_id_1' and 'item_type_id_2' are
     * truly interchangeable and safe to merge - see the class-level doc
     * comment's "Merging identical item types" paragraph.
     */
    bool items_mergeable(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2) const;

    /**
     * Merges every group of pairwise-'items_mergeable' item types (all but
     * the lowest-id survivor of each group get 'removed'/'merged_into'
     * set). Returns 'true' iff at least one item type was merged away.
     */
    bool merge_identical_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * Builds the final reduced 'Instance' from the working representation,
     * populating 'reduced_item_origin_runs_' alongside it.
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types);

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
