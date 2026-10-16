#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include "optimizationtools/utils/parameters.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

/** Defined in 'src/rectangleguillotine/solution_builder.hpp'; only used here by pointer/reference. */
class SolutionBuilder;

/**
 * Structure passed as parameters of the reduction algorithm.
 *
 * Deliberately does not inherit from 'packingsolver::Parameters<Instance,
 * Solution, Output>' (unlike most other "*Parameters" structs in this
 * codebase): 'Reduction' is wired into 'optimize()' itself (see
 * 'optimize.hpp'/'optimize.cpp'), so inheriting from a type parameterized
 * on 'rectangleguillotine::Output' would create a circular include between
 * this header and 'optimize.hpp'. 'optimizationtools::Parameters' already
 * provides everything actually needed here (a 'Timer', 'verbosity_level',
 * ...) without that dependency. Mirrors 'rectangle::ReductionParameters'.
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
     * 'Reduction::merge_identical_items'). Runs for any objective - see
     * 'Reduction::items_mergeable''s own doc comment.
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
};

/**
 * A three-operation subset of 'rectangle::Reduction' (see there for the
 * full, six-operation version this mirrors): a negative-profit item trim
 * (see 'remove_negative_profit_items'), a subset-sum item dimension lift
 * adapted to cut trees (see 'lift_item_dimensions'), and an
 * identical-item-type merge (see 'merge_identical_items'). The other
 * rectangle operations (companion absorption, dominated-item removal,
 * dominated-bin-type removal) reason about geometry in ways that assume a
 * solution is a free 2D placement - not obviously true of a guillotine cut
 * tree - so they are deliberately out of scope here.
 *
 * The trim and the merge run for any objective except where noted, with
 * no shared precondition beyond 'parameters.reduce' - see
 * 'remove_negative_profit_items_applies()' and 'items_mergeable()' for
 * their own, independent preconditions. All three operations share the
 * same stack-alone restriction (an item type sharing a stack with others
 * is left untouched) - see 'items_mergeable()''s own doc comment for why,
 * and 'remove_negative_profit_items()''s own for why trimming needs it
 * too, not just merging.
 *
 * Lifting item dimensions (see 'lift_item_dimensions'): along an axis, if
 * no combination of the other items can fill the space left beside an
 * item type with a single copy, the item type is enlarged to close that
 * gap - rectangle's own operation, against the bin shrunk to the largest
 * total length of the items not exceeding it ("shrinking the bins"). For
 * a cut tree, the argument is a compaction of the tree along the axis:
 * every node delimited along the axis is pushed back against its previous
 * sibling, and every node is shrunk to its content. The tree structure
 * (depths, number of cuts) is unchanged, and the extent of the root along
 * the axis is then the largest total length, plus one cut thickness
 * between consecutive items, of a chain of items. Hence every length below
 * is increased by the cut thickness (bin lengths included). After
 * compaction, the items of any chain through the lifted item, other than
 * itself, fill at most the gap's complement, so the enlarged item still
 * fits. Enlarging the item may require trimming its siblings in the
 * stacking direction, which depends on the cut type:
 * - 'NonExact': the extra trimming cut absorbs it, any axis can be lifted;
 * - 'Exact': only the axis of the last-stage cuts can be lifted (an item of
 *   the last stage fills its node along the other axis, as its siblings
 *   do), and only with a fixed first stage orientation;
 * - 'Roadef2018': only the axis of the extra cuts can be lifted (two items
 *   sharing an extra cut fill their node along the other axis), and only
 *   with a fixed first stage orientation;
 * - 'Homogenous': nothing is lifted.
 * The compaction can make a distance between two cuts or a waste smaller,
 * and 'unreduce_solution' adds a cut separating the true item from the
 * provably empty margin, so nothing is lifted with a minimum distance
 * between 1-cuts or 2-cuts, a minimum waste length, or a maximum number of
 * 1-cuts or 2-cuts. A maximum distance between 1-cuts (resp. 2-cuts) is
 * handled as a second capacity along the axes the 1-cuts (resp. 2-cuts)
 * may be along: the item is lifted to the smaller of the two bounds, and
 * only if it fits the maximum distance along both axes (since
 * 'unreduce_solution' may turn its node into a plate subject to it). Also
 * requires a single bin type without defects or fixed items, no trims when
 * the cut thickness is not zero, an oriented item type alone in its stack
 * without fixed copies, and an objective that only depends on which items
 * and bins are used ('Knapsack', 'BinPacking', 'VariableSizedBinPacking',
 * 'Feasibility'), or 'OpenDimensionX'/'OpenDimensionY' for the other axis.
 * 'unreduce_solution' rewrites the cut tree around each enlarged item: if
 * its node is delimited along the lifted axis, the node is shrunk, and the
 * freed length goes to the first waste after it (the siblings in between
 * are moved back), or to a new waste at the end of its parent;
 * otherwise, the true item goes into a child node, followed by a waste; if
 * the node is already at the deepest level allowed, its parent, which then
 * only contains the item and a waste, is shrunk instead. See
 * 'lift_item_dimensions_applies()' and
 * 'lift_item_dimensions_applies_axis()'.
 *
 * Only ever needs one pass (unlike 'rectangle::Reduction''s fixpoint
 * loop): trimming a negative-profit item type down to its 'copies_min'
 * can only ever narrow a later merge candidate's mandatory-vs-optional
 * status (never flip a type that was already fully mandatory or fully
 * optional into the other category - see 'effective_copies_min'), merging
 * never changes any type's own profit or copies_min, and lifting only
 * changes the dimensions of oriented item types with a single copy, which
 * the redundant rotation removal does not look at. Lifting runs before
 * merging, which compares the current (possibly enlarged) dimensions.
 *
 * Before both operations, redundant rotations are removed (see
 * 'remove_redundant_rotations'). The rotation of a non-oriented item type is
 * redundant if the item type is a square, or if the rotated item fits in no
 * bin type (without its trims) while the item does: the item type becomes
 * oriented. If instead only the rotated item fits, the item type becomes
 * oriented with its width and height swapped. A leaf of a cut tree accepts
 * its item in either orientation, so every solution of the reduced instance
 * is a solution of the original instance, and every solution of the
 * original instance can be written with the remaining rotations only. Item
 * types with fixed copies are left untouched. This also lets
 * 'merge_identical_items' merge item types which only differed by a
 * redundant rotation.
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
     * 'Knapsack' (every other objective needs every copy of every item
     * type packed regardless of its own profit, so a negative profit
     * changes nothing about whether it must be packed), and only when no
     * 'penalize' resource anywhere has a negative 'penalty': that is a
     * one-time profit *bonus* the first time a bin's consumption crosses
     * the resource's capacity (see 'Resource''s own doc comment in
     * 'algorithms/common.hpp'), so a negative-profit item could still be
     * worth including if its own consumption helps trigger that crossing -
     * an indirect benefit this purely per-item check cannot see. Mirrors
     * 'rectangle::Reduction::remove_negative_profit_items_applies'. Does
     * not check 'parameters.reduce' - the constructor only calls this
     * after already checking it.
     */
    static bool remove_negative_profit_items_applies(const Instance& instance);

    /**
     * 'true' iff lifting item dimensions (see 'lift_item_dimensions') is
     * meaningful for 'instance' - see the class-level doc comment. Does not
     * check 'parameters.reduce' - the constructor only calls this after
     * already checking it.
     */
    static bool lift_item_dimensions_applies(const Instance& instance);

    /**
     * 'true' iff lifting item dimensions is sound along axis 'axis' ('0'
     * for 'x', '1' for 'y') for 'instance' - see the class-level doc
     * comment for the objective and cut type restrictions.
     */
    static bool lift_item_dimensions_applies_axis(
            const Instance& instance,
            int axis);

    /**
     * Working representation of an item type during the reduction
     * process. Unlike 'rectangle::Reduction::ReductionItemType', carries
     * no companion bookkeeping - no operation here hides another item
     * type behind it.
     */
    struct ReductionItemType
    {
        bool removed = false;

        /**
         * Item type id (original instance's id space) this one was merged
         * into (see 'merge_identical_items'), or '-1' if it was not merged
         * away. Only ever set alongside 'removed = true'.
         */
        ItemTypeId merged_into = -1;

        /**
         * Dimensions. Start at the original instance's own dimensions, can
         * be swapped, via 'remove_redundant_rotations', and then only ever
         * grow, via 'lift_item_dimensions'.
         */
        Rectangle rect;

        /**
         * Whether 'remove_redundant_rotations' swapped the dimensions.
         */
        bool swapped = false;

        /**
         * Whether the item type is oriented. Starts at the original
         * instance's own value, and can only ever become 'true', via
         * 'remove_redundant_rotations'.
         */
        bool oriented = false;

        /**
         * Current (possibly trimmed) number of copies. Starts at the
         * original instance's own copies and only ever decreases, via
         * 'remove_negative_profit_items'.
         */
        ItemPos copies = 0;
    };

    /**
     * Minimum number of copies of 'item_type_id' that must still be
     * placed, given its current (possibly already-trimmed) 'copies' -
     * mirrors 'rectangle::Reduction::effective_copies_min'. For
     * 'Knapsack', 'original_item_type.copies_min' capped by whatever
     * 'copies' genuinely remain (never below current 'copies', which
     * would be unsatisfiable). For every other objective, 'copies_min'
     * net of however many copies were already trimmed away (in practice
     * 'remove_negative_profit_items' never runs there - see
     * 'remove_negative_profit_items_applies' - so this reduces to plain
     * 'copies_min', but the formula stays honest regardless).
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
     * Trims away copies of negative-profit item types beyond their own
     * 'copies_min' (see 'ReductionParameters::remove_negative_profit_items'
     * and 'remove_negative_profit_items_applies'). Mirrors
     * 'rectangle::Reduction::remove_negative_profit_items', with one
     * addition specific to this problem type: leaves item types that share
     * a stack with others untouched, for the same reason 'items_mergeable'
     * restricts merging to item types alone in their own stack - trimming
     * (or fully removing) a stack-sharing item type's copies here would
     * just as silently weaken or erase the "fully place the preceding item
     * type before this stack position" requirement
     * ('Solution::stacks_feasible_' checks in rectangleguillotine's own
     * 'solution.cpp') for whichever item type comes right after it in the
     * same stack. An item type alone in its own stack has no such
     * requirement, so it is always safe to trim.
     */
    void remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * Upper bound on the largest total of the lengths along axis 'axis'
     * ('0' for 'x', '1' for 'y'), each increased by 'offset', not exceeding
     * 'capacity', of the current copies of the not-yet-'removed' item
     * types, each taken in any orientation it allows - except, if
     * 'excluded_item_type_id' is not '-1', one copy of that item type.
     */
    Length max_achievable_dimension_sum(
            const std::vector<ReductionItemType>& reduction_item_types,
            Length capacity,
            int axis,
            Length offset,
            ItemTypeId excluded_item_type_id) const;

    /**
     * Enlarges the item types along every axis allowed by
     * 'lift_item_dimensions_applies_axis' - see the class-level doc
     * comment. Returns 'true' iff at least one item type was enlarged.
     */
    bool lift_item_dimensions(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff item types 'item_type_id_1' and 'item_type_id_2' are
     * interchangeable and can be merged into one - same dimensions, same
     * 'oriented', identical resource-consumption schedule on every bin
     * type, and - for 'Knapsack' only - same profit (every other
     * objective ignores profit as a merge criterion: it is never the
     * actual objective there, and 'unreduce_solution' always restores
     * each copy's true original profit regardless of merging); plus both
     * fully mandatory or both fully optional, never a mix (see
     * 'rectangle::Reduction::items_mergeable''s own doc comment for the
     * concrete counterexample showing why a mixed merge is unsound).
     * Mirrors 'rectangle::Reduction::items_mergeable', minus the
     * 'weight'/'group_id'/'eligibility_id' criteria ('ItemType' here has
     * none of those fields).
     *
     * Additional precondition specific to this problem type: both item
     * types must be alone in their own stack - no *other* item type
     * contributes to it, checked as 'instance.stack_size(stack_id) ==
     * item_type.copies' (not '== 1': 'stack_size' counts every physical
     * copy in the stack, so a lone item type with more than one copy of
     * its own must still pass). A stack is a sequence of
     * item types that must be cut/produced in a fixed order
     * ('ItemType::stack_pos'); a stack shared with other item types has
     * an ordering a merge could silently disturb (which of two
     * interchangeable-but-distinct types ends up at which stack
     * position is no longer arbitrary once other, non-interchangeable
     * types share the same stack). Restricting merges to item types that
     * are the sole occupant of their own stack sidesteps this entirely:
     * there is nothing else in the stack whose order could be disturbed,
     * and folding two singleton stacks into one (the survivor keeps its
     * own stack, just with more copies; the stacks are renumbered
     * consecutively in the reduced instance) leaves each
     * stack's own internal order - trivial, one item type - untouched.
     * Item types sharing a stack with others are simply never merged;
     * nothing more sophisticated is attempted here for now.
     */
    bool items_mergeable(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2) const;

    /**
     * Merges every pairwise-'items_mergeable' item type into the first
     * (lowest id) survivor of its group. Mirrors
     * 'rectangle::Reduction::merge_identical_items'.
     */
    void merge_identical_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * Builds the reduced 'Instance' from the working representation,
     * populating 'reduced_item_origin_runs_' along the way. Mirrors
     * 'rectangle::Reduction::reduction_to_instance', minus everything
     * about dedicated bins / bin type removal (out of scope for this
     * two-operation class): every bin type is copied over unchanged, in
     * its original order and with its original copies.
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types);

    /** Original instance. */
    const Instance* original_instance_;

    /** Reduced instance. */
    Instance instance_;

    /**
     * For each item type of the original instance, its dimensions in the
     * reduced instance (see 'ReductionItemType::rect').
     */
    std::vector<Rectangle> reduced_rects_;

    /**
     * For each item type of the original instance, whether its dimensions
     * were swapped (see 'ReductionItemType::swapped').
     */
    std::vector<bool> swapped_item_types_;

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
