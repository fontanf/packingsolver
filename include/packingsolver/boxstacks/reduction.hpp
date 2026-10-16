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
     * Enable/disable removing the redundant rotations of the item types
     * (gates 'Reduction::remove_redundant_rotations').
     */
    bool remove_redundant_rotations = true;

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

    /**
     * Enable/disable lifting item dimensions (gates
     * 'Reduction::lift_item_dimensions'). Still subject to
     * 'Reduction::lift_item_dimensions_applies' and
     * 'Reduction::lift_item_dimensions_applies_axis' regardless of this
     * flag.
     */
    bool lift_item_dimensions = true;
};

/**
 * Instance reduction (preprocessing): a small subset of
 * 'rectangle::Reduction''s operations (see its own class-level doc comment
 * for the full six-operation version), restricted here to the two whose
 * soundness argument does not depend on anything specific to 2D geometry,
 * plus a redundant-rotation removal and an item dimension lift:
 *
 * - Removing redundant rotations (see 'remove_redundant_rotations'): items
 *   are packed upright, so an item type allows at most rotations 'XYZ' and
 *   'YXZ'. 'YXZ' is redundant if 'x == y' (same footprint as 'XYZ'), and
 *   either rotation is redundant if its rotated footprint fits in no bin
 *   type while the other one does. The rotations kept are a subset of the
 *   original ones, so every solution of the reduced instance is a solution
 *   of the original instance, and every solution of the original instance
 *   can be written with the remaining rotations only. Item types with fixed
 *   copies keep all their rotations. Runs first, so that
 *   'merge_identical_items' also merges item types whose allowed rotations
 *   only differed by redundant ones.
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
 * - Lifting item dimensions (see 'lift_item_dimensions'): rectangle's own
 *   subset-sum lift, applied to the footprints of the stacks. Only applied
 *   to an item type with a single copy whose 'stackability_id' no other
 *   remaining item type shares: such an item is always alone in its
 *   stack, so its footprint is the footprint of its stack, and enlarging
 *   it changes no stacking possibility. Along 'x' or 'y', the bin is first
 *   shrunk to the largest total length of the items not exceeding it
 *   ("shrinking the bins", see 'shrunk_bin_length'); then, if no
 *   combination of the other items can fill the space left beside the
 *   item in the shrunk bin, the item is enlarged to close that gap. Sound
 *   because any solution can be compacted along the axis (every stack
 *   pushed towards the origin while its other coordinate stays fixed): the
 *   position of a stack is then the total length of a chain of stacks
 *   before it, so the items of any chain through the lifted item, other
 *   than itself, fill at most the gap's complement. Counting every other
 *   item on its own, rather than one footprint per stack, only makes the
 *   bound larger, which is safe. Requires the item type to have a single
 *   allowed rotation (growing its 'x' would otherwise also change its
 *   extent along 'y' when rotated). Along 'z', the item is enlarged to the
 *   height of the bin, since its stack contains nothing else. The
 *   compaction moves stacks, and the enlarged footprint changes the
 *   reduced instance's view of some constraints, so:
 *   - nothing is lifted with a single bin type that has defects or fixed
 *     items (see 'lift_item_dimensions_applies');
 *   - nothing is lifted along 'x' with a semi-trailer truck: the axle
 *     weights depend on the 'x' positions and on the 'x' centers of the
 *     stacks;
 *   - with an unloading constraint, nothing is lifted along the axis whose
 *     compaction could break it: 'x' for 'OnlyYMovements' and
 *     'IncreasingX', 'y' for 'OnlyXMovements' and 'IncreasingY'
 *     (compacting along 'x' keeps the 'y' coordinates and the order along
 *     'x' of the stacks whose 'y' ranges intersect, so it keeps the
 *     constraints that only depend on those);
 *   - the footprint is only lifted if the item alone satisfies the maximum
 *     stack density of the bin type with its true footprint, since the
 *     reduced instance checks it against the enlarged one;
 *   - an objective measuring how far the items extend along an axis
 *     ('OpenDimensionX'/'OpenDimensionY', or 'BinPackingWithLeftovers'
 *     with a 'LeftoverMode' involving the axis) never lifts along it.
 *   'unreduce_solution' places the true items in stacks of their true
 *   footprint, at the positions the solve chose for their enlarged
 *   stand-ins. Runs before 'merge_identical_items'; a lifted item type is
 *   never merged, since its 'stackability_id' is unique. See
 *   'lift_item_dimensions_applies()' and
 *   'lift_item_dimensions_applies_axis()'.
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

    /**
     * 'true' iff lifting item dimensions (see 'lift_item_dimensions') is
     * meaningful for 'instance' - a single bin type, without defects or
     * fixed items. Does not check 'parameters.reduce' - the constructor
     * only calls this after already checking it.
     */
    static bool lift_item_dimensions_applies(const Instance& instance);

    /**
     * 'true' iff lifting item dimensions is sound along axis 'axis' ('0'
     * for 'x', '1' for 'y', '2' for 'z') for 'instance' - see the
     * class-level doc comment for the objective, semi-trailer truck and
     * unloading constraint exclusions.
     */
    static bool lift_item_dimensions_applies_axis(
            const Instance& instance,
            int axis);

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
     * process can actually change ('copies', via a negative-profit trim,
     * 'rotations' and 'box') plus the bookkeeping needed to fold a
     * merged-away item type into its survivor ('merged_into'). Every other
     * item type field (profit, weight, ...) never changes during the
     * process, so it is read directly from 'original_instance_' wherever
     * needed.
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
         * Allowed rotations. Starts at the original instance's own
         * rotations and only ever shrinks, via
         * 'remove_redundant_rotations'.
         */
        std::vector<Rotation> rotations;

        /**
         * Current (possibly trimmed) number of copies. Starts at the
         * original instance's own copies and only ever decreases, via
         * 'remove_negative_profit_items'.
         */
        ItemPos copies = 0;

        /**
         * Current (possibly enlarged) box. Starts at the original item
         * type's own box and only ever grows, via 'lift_item_dimensions'.
         */
        Box box;
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
     * Removes the redundant rotations of the item types (see the
     * class-level doc comment). Returns 'true' iff at least one rotation
     * was removed.
     */
    bool remove_redundant_rotations(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * Trims away optional (beyond 'copies_min'), negative-profit copies of
     * every item type - see the class-level doc comment. Returns 'true' iff
     * at least one item type was trimmed.
     */
    bool remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * Upper bound on the largest total length along axis 'axis' ('0' for
     * 'x', '1' for 'y'), not exceeding 'capacity', of the current copies
     * of the not-yet-'removed' item types, each taken with any of its
     * allowed rotations - except, if 'excluded_item_type_id' is not '-1',
     * one copy of that item type.
     */
    Length max_achievable_dimension_sum(
            const std::vector<ReductionItemType>& reduction_item_types,
            Length capacity,
            int axis,
            ItemTypeId excluded_item_type_id) const;

    /**
     * Largest total length along axis 'axis' ('0' for 'x', '1' for 'y') of
     * the remaining items not exceeding the length of the (single) bin
     * type along it ("shrinking the bins", see the class-level doc
     * comment).
     */
    Length shrunk_bin_length(
            const std::vector<ReductionItemType>& reduction_item_types,
            int axis) const;

    /**
     * Enlarges the item types along every axis allowed by
     * 'lift_item_dimensions_applies_axis' - see the class-level doc
     * comment. Returns 'true' iff at least one item type was enlarged.
     */
    bool lift_item_dimensions(
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
     * Builds the final reduced 'Instance' from 'reduction_item_types':
     * copies every bin type over unchanged (no bin-type-level operation in
     * this class), then, for each original item type not entirely trimmed
     * away and not merged into another, adds one item type with the
     * combined copies/'copies_min' of itself and everything merged into it
     * - populating 'reduced_item_origin_runs_' alongside (see its own doc
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
