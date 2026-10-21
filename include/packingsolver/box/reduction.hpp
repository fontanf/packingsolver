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
     * Enable/disable lifting item dimensions via a subset-sum bound (gates
     * 'Reduction::lift_item_dimensions'). Still subject to
     * 'Reduction::lift_item_dimensions_applies' and
     * 'Reduction::lift_item_dimensions_applies_axis' regardless of this
     * flag.
     */
    bool lift_item_dimensions = true;

    /**
     * Enable/disable trimming the copies of dominated item types (gates
     * 'Reduction::remove_dominated_items'). Still subject to
     * 'Reduction::remove_dominated_items_applies' regardless of this flag.
     */
    bool remove_dominated_items = true;

    /**
     * Enable/disable removing dominated bin types (gates
     * 'Reduction::compute_dominated_bin_types'). Still subject to
     * 'Reduction::remove_dominated_bin_types_applies' regardless of this
     * flag.
     */
    bool remove_dominated_bin_types = true;

    /**
     * Enable/disable reserving a dedicated bin for each copy of an item
     * type filling the bin (gates 'Reduction::reduce_full_bin_items').
     * Still subject to 'Reduction::full_bin_reduction_applies' regardless
     * of this flag.
     */
    bool reduce_full_bin_items = true;

    /**
     * Enable/disable reserving a dedicated bin for pairs of item types
     * filling the bin together (gates 'Reduction::reduce_perfect_pairs').
     * Still subject to 'Reduction::full_bin_reduction_applies' regardless
     * of this flag.
     */
    bool reduce_perfect_pairs = true;
};

/**
 * A redundant-rotation removal (see 'remove_redundant_rotations'), a
 * negative-profit item trim (see 'remove_negative_profit_items'), a
 * dominated-item trim (see 'remove_dominated_items'), a subset-sum item
 * dimension lift (see 'lift_item_dimensions'), an identical-item-type
 * merge (see 'merge_identical_items') plus a dominated-bin-type removal
 * (see 'compute_dominated_bin_types') - the 'box' analogue of
 * 'rectangle::Reduction', without its companion absorption.
 *
 * - Removing redundant rotations (see 'remove_redundant_rotations'): a
 *   rotation of an item type is redundant if another allowed rotation gives
 *   the same rotated box (e.g. 'YXZ' when 'x == y'), or if the rotated box
 *   fits in no bin type while another allowed rotation does. The rotations
 *   kept are a subset of the original ones, so every solution of the
 *   reduced instance is a solution of the original instance, and every
 *   solution of the original instance can be written with the remaining
 *   rotations only. Item types with fixed copies keep all their rotations.
 *   Runs first, so that 'merge_identical_items' also merges item types
 *   whose allowed rotations only differed by redundant ones.
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
 * - Lifting item dimensions via subset sum (see 'lift_item_dimensions'):
 *   the 3D version of rectangle's own operation. Along an axis, the bin
 *   is first shrunk to the largest total length of the items not
 *   exceeding it ("shrinking the bins", see 'shrunk_bin_length'). Then,
 *   for an item type with a single copy, if no combination of the other
 *   items can fill the space left beside it in the shrunk bin, the item
 *   type is enlarged to close that gap. Sound because any solution can be
 *   compacted along the axis (every item pushed towards the origin while
 *   the other two coordinates stay fixed): the position of each item is
 *   then the total length of a chain of items before it, and the items of
 *   any chain through the lifted item, other than itself, fill at most the
 *   gap's complement - so the enlarged item still fits. Only applied to an
 *   item type whose allowed rotations all put the same one of its own
 *   dimensions along the axis (otherwise growing that dimension would also
 *   change its extent along other axes for some rotation), with no fixed
 *   copies. The other item types count in the bound with every length
 *   their allowed rotations give them along the axis. Requires a single
 *   bin type without fixed items (fixed items cannot be moved by the
 *   compaction). An objective measuring how far the items extend along an
 *   axis ('OpenDimensionX'/'Y'/'Z', or 'BinPackingWithLeftovers' with a
 *   'LeftoverMode' involving the axis) never lifts along it.
 *   'reduction_to_instance' writes the enlarged boxes into the reduced
 *   instance, and 'unreduce_solution' places the true items at the
 *   position and rotation the solve chose for their enlarged stand-ins.
 *   Runs in a loop with 'remove_redundant_rotations', since lifting can
 *   make more rotations redundant, and before 'merge_identical_items',
 *   which compares the current (possibly enlarged) boxes. See
 *   'lift_item_dimensions_applies()' and
 *   'lift_item_dimensions_applies_axis()'.
 *
 * - Trimming dominated item types (see 'remove_dominated_items'): only for
 *   'Knapsack', rectangle's own 'remove_dominated_items'. Item type A
 *   dominates item type B when A is at least as profitable, at most as
 *   heavy, never consumes more of any resource, and fits inside B: for
 *   every allowed rotation of B, A has an allowed rotation fitting inside
 *   the rotated box of B. A copy of B can then always be replaced by an
 *   unused copy of A, so B's copies are trimmed down to
 *   'dominated_item_copies_bound' (see 'algorithms/dominated_items.hpp'),
 *   B being removed when it is 0. Per bin type, a bin holds at most its
 *   volume divided by A's items of A and B, or a single one if A and A, A
 *   and B, and B and B are incompatible: no pair of allowed rotations puts
 *   them side by side along some axis, or their weights or a resource
 *   overflow the bin. Uses the rotations left by
 *   'remove_redundant_rotations', and the original boxes (it runs before
 *   'lift_item_dimensions'). Only for fully optional ('copies_min' 0) item
 *   types without fixed copies. See 'remove_dominated_items_applies()'.
 *
 * - Removing dominated bin types (see 'compute_dominated_bin_types'):
 *   rectangle's own 'remove_dominated_bin_types'. Bin type A dominates bin
 *   type B when A is at least as large along every axis, at most as
 *   costly, has at least B's maximum weight, has no resource (and B no
 *   negative-penalty one), and neither has fixed items. B is then left out
 *   of the reduced instance if it has no 'copies_min' and A has enough
 *   copies to replace any use of B on top of its own (the number of items
 *   plus A's own 'copies_min'). Only for 'Knapsack', 'Feasibility' and
 *   'VariableSizedBinPacking' with more than one bin type: with the bin
 *   packing objectives ('BinPacking', 'BinPackingWithLeftovers'), the bin
 *   types must be used in their declared order. 'unreduce_solution' maps
 *   the bin type ids of
 *   the reduced instance back to the original ones. See
 *   'remove_dominated_bin_types_applies()'.
 *
 * - Reserving dedicated bins (see 'reduce_full_bin_items' and
 *   'reduce_perfect_pairs'): rectangle's and onedimensional's own
 *   operations. Compared against the bin shrunk along each axis to the
 *   largest total length of the items not exceeding it (see
 *   'shrunk_bin_length'), using the current (possibly lifted) boxes. An
 *   item whose box equals the shrunk bin can't share its bin with any
 *   other item: along each axis, any other item beside it would make a
 *   total length beyond the shrunk bin. Such an item gets a dedicated bin.
 *   An allowed rotation of the item giving the shrunk bin is enough: the
 *   shrunk length along each axis is at least the length of the item along
 *   it in any rotation fitting the bin, so every such rotation gives the
 *   same box. Two items spanning the shrunk bin along two axes, whose
 *   lengths along the third axis sum to the shrunk bin's, also get a
 *   dedicated bin together: in any solution, the other items of the bin of
 *   each of them are beside it along the third axis, in two slabs spanning
 *   the cross-section of the bin, whose total length is at most the length
 *   of the other item of the pair (shrunk bin); so the items of the two
 *   bins other than the pair fit in a single bin, next to the bin holding
 *   the pair. This requires both items to have a single allowed rotation
 *   (otherwise they could be placed without spanning the two axes). Only
 *   for 'BinPacking' with a single bin type, without fixed items, and
 *   without a weight or resource constraint a hidden reservation could
 *   violate (see 'full_bin_reduction_applies()'). Never reserves more
 *   dedicated bins than the bin type has copies; if they take every copy
 *   while items remain, the instance is infeasible (see
 *   'proven_infeasible()'). The dedicated items are left out of the
 *   reduced instance, the bin type has its 'copies' reduced accordingly,
 *   and 'unreduce_solution' adds the dedicated bins back. Runs in the same
 *   loop as 'lift_item_dimensions', since lifting can make an item fill
 *   the shrunk bin, and a reservation can shrink the bin further.
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

    /**
     * Number of bins reserved as dedicated bins (see
     * 'reduce_full_bin_items'/'reduce_perfect_pairs'), entirely absent from
     * 'instance()' - a caller solving 'instance()' needs to add this back
     * onto any bin-count bound it finds.
     */
    BinPos number_of_dedicated_bins() const;

    /**
     * 'true' iff the reduction alone proves the original instance
     * infeasible: the dedicated bins take every copy of the bin type while
     * items remain. 'instance()' is then not meaningful to solve.
     */
    bool proven_infeasible() const { return proven_infeasible_; }

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
     * meaningful for 'instance' - a single bin type, without fixed items.
     * Does not check 'parameters.reduce' - the constructor only calls this
     * after already checking it.
     */
    static bool lift_item_dimensions_applies(const Instance& instance);

    /**
     * 'true' iff lifting item dimensions is sound along axis 'axis' ('0'
     * for 'x', '1' for 'y', '2' for 'z') for 'instance': 'false' when the
     * objective measures how far the items extend along it - see the
     * class-level doc comment.
     */
    static bool lift_item_dimensions_applies_axis(
            const Instance& instance,
            int axis);

    /**
     * 'true' iff trimming dominated item types (see
     * 'remove_dominated_items') is meaningful for 'instance': only
     * 'Knapsack'.
     */
    static bool remove_dominated_items_applies(const Instance& instance);

    /**
     * 'true' iff removing dominated bin types (see
     * 'compute_dominated_bin_types') is meaningful for 'instance' - see
     * the class-level doc comment.
     */
    static bool remove_dominated_bin_types_applies(const Instance& instance);

    /**
     * 'true' iff reserving dedicated bins (see 'reduce_full_bin_items' and
     * 'reduce_perfect_pairs') is meaningful for 'instance' - see the
     * class-level doc comment.
     */
    static bool full_bin_reduction_applies(const Instance& instance);

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
         * Allowed rotations. Starts at the original instance's own
         * rotations and only ever shrinks, via
         * 'remove_redundant_rotations'.
         */
        std::vector<Rotation> rotations;

        /**
         * Current (possibly enlarged) box. Starts at the original item
         * type's own box and only ever grows, via 'lift_item_dimensions'.
         */
        Box box;

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
     * Removes the redundant rotations of the item types (see the
     * class-level doc comment). Returns 'true' iff at least one rotation
     * was removed.
     */
    bool remove_redundant_rotations(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * Trims away negative-profit optional copies (see the class-level doc
     * comment's first paragraph). Returns 'true' iff at least one item type
     * was trimmed.
     */
    bool remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * Upper bound on the largest total length along axis 'axis', not
     * exceeding 'capacity', of the current copies of the not-yet-'removed'
     * item types, each taken with any of its allowed rotations - except,
     * if 'excluded_item_type_id' is not '-1', one copy of that item type.
     */
    Length max_achievable_dimension_sum(
            const std::vector<ReductionItemType>& reduction_item_types,
            Length capacity,
            int axis,
            ItemTypeId excluded_item_type_id) const;

    /**
     * Largest total length along axis 'axis' of the remaining items not
     * exceeding the length of the (single) bin type along it ("shrinking
     * the bins", see the class-level doc comment).
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
     * 'true' iff item type 'item_type_id_a' dominates item type
     * 'item_type_id_b' - see the class-level doc comment.
     */
    bool item_type_dominates(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_a,
            ItemTypeId item_type_id_b) const;

    /**
     * 'true' iff a copy of item type 'item_type_id_1' and a copy of item
     * type 'item_type_id_2' (possibly the same one) can never be packed
     * together in a bin of type 'bin_type_id' - see the class-level doc
     * comment.
     */
    bool items_incompatible(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2,
            BinTypeId bin_type_id) const;

    /**
     * Upper bound on the number of copies of item type 'item_type_id_b' an
     * optimal solution needs, item type 'item_type_id_a' dominating it,
     * computed with the current copies of both - see the class-level doc
     * comment.
     */
    ItemPos dominated_item_copies_bound(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_a,
            ItemTypeId item_type_id_b) const;

    /**
     * Trims the copies of every fully optional item type dominated by
     * another, not-yet-'removed' one down to 'dominated_item_copies_bound',
     * removing it when that is '0'. Returns 'true' iff at least one item
     * type was trimmed.
     */
    bool remove_dominated_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff bin type 'bin_type_id_a' dominates bin type
     * 'bin_type_id_b' - see the class-level doc comment.
     */
    bool bin_type_dominates(
            BinTypeId bin_type_id_a,
            BinTypeId bin_type_id_b) const;

    /**
     * For each bin type, 'true' iff it is dominated by another bin type
     * with enough copies to replace it - see the class-level doc comment.
     */
    std::vector<bool> compute_dominated_bin_types() const;

    /**
     * Builds the final reduced 'Instance' from 'reduction_item_types',
     * leaving out the bin types marked in 'bin_type_removed', populating
     * 'reduced_item_origin_runs_' and 'reduced_to_original_bin_type_id_'
     * along the way.
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types,
            const std::vector<bool>& bin_type_removed);

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
     * Original instance's bin type id of each of 'instance_''s own bin
     * types - identity unless 'compute_dominated_bin_types' left some out.
     */
    std::vector<BinTypeId> reduced_to_original_bin_type_id_;

    /** A dedicated bin for each copy of an item type filling the bin. */
    struct FullBinItem
    {
        /** Item type id, original instance's id space. */
        ItemTypeId item_type_id;

        /** Rotation giving the box of the bin. */
        Rotation rotation;

        /** Number of dedicated bins. */
        ItemPos copies;
    };

    /** Dedicated bins for pairs of item types filling the bin together. */
    struct PerfectPair
    {
        /** Item type ids, original instance's id space. */
        ItemTypeId item_type_id_1;
        ItemTypeId item_type_id_2;

        /** Rotations of the two items. */
        Rotation rotation_1;
        Rotation rotation_2;

        /** Axis along which the two items are side by side. */
        int axis;

        /** Number of dedicated bins. */
        ItemPos copies;
    };

    /** See 'reduce_full_bin_items'. */
    std::vector<FullBinItem> full_bin_items_;

    /** See 'reduce_perfect_pairs'. */
    std::vector<PerfectPair> perfect_pairs_;

    /** See 'proven_infeasible()'. */
    bool proven_infeasible_ = false;

    /**
     * Reserves a dedicated bin for every copy of every item type whose box,
     * in one of its allowed rotations, equals the shrunk bin. Returns
     * 'true' iff at least one item type was reserved.
     */
    bool reduce_full_bin_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * Reserves a dedicated bin for 'min(copies_1, copies_2)' copies of every
     * pair of item types filling the shrunk bin together (including two
     * copies of the same item type). Returns 'true' iff at least one pair
     * was reserved.
     */
    bool reduce_perfect_pairs(
            std::vector<ReductionItemType>& reduction_item_types);

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
