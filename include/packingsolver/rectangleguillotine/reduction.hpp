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
 * A subset of 'rectangle::Reduction' (see there for the full version this
 * mirrors): a negative-profit item trim (see
 * 'remove_negative_profit_items'), a dominated-item trim (see
 * 'remove_dominated_items'), a subset-sum item dimension lift adapted to
 * cut trees (see 'lift_item_dimensions'), an identical-item-type merge
 * (see 'merge_identical_items'), and a dominated-bin-type removal (see
 * 'compute_dominated_bin_types'). Rectangle's companion absorption
 * reasons about geometry in ways that assume a solution is a free 2D
 * placement - not obviously true of a guillotine cut tree - so it is
 * deliberately out of scope here.
 *
 * Trimming dominated item types (see 'remove_dominated_items'): only for
 * 'Knapsack', rectangle's own 'remove_dominated_items'. Item type A
 * dominates item type B when A is at least as profitable, never consumes
 * more of any resource, and fits inside B: for every orientation of B, A
 * has an orientation at most as large along both axes. A copy of B can
 * then always be replaced by an unused copy of A, so B's copies are
 * trimmed down to 'dominated_item_copies_bound' (see
 * 'algorithms/dominated_items.hpp'), B being removed when it is 0. Per bin
 * type, a bin holds at most its area (without its trims) divided by A's
 * items of A and B, or a single one if A and A, A and B, and B and B are
 * incompatible (they can't be put side by side, with a cut thickness
 * between them, in the bin without its trims, or a resource overflows).
 * Putting A where B was is the same rewrite of the cut tree as shrinking a
 * lifted item in 'unreduce_solution': it requires the same cut type and
 * cut constraints as lifting (see 'items_can_shrink'), A must be as large
 * as B along an axis items can't shrink along (see
 * 'items_can_shrink_along'), and B must fit the maximum distances between
 * cuts (its node can become a plate). Also requires A and B alone in their
 * stacks, fully optional ('copies_min' 0), and without fixed copies.
 *
 * Removing dominated bin types (see 'compute_dominated_bin_types'):
 * rectangle's own 'remove_dominated_bin_types'. Bin type A dominates bin
 * type B when A is at least as large along both axes, has the same trims,
 * is at most as costly, has no resource (and B no negative-penalty one),
 * and neither has defects or fixed items. Any cut tree of B then fits in A:
 * the extra length along each axis becomes a waste at the end of each
 * node, at the same depth (an item node spanning B becomes a plate holding
 * it and a waste). This can create small wastes and turn item nodes into
 * plates, so with a minimum waste length, a minimum or maximum distance
 * between cuts, or a maximum number of cuts, A must be exactly as large as
 * B. B is then left out of the reduced instance if it has no 'copies_min'
 * and A has enough copies to replace any use of B on top of its own (the
 * number of items plus A's own 'copies_min'). Only for 'Knapsack',
 * 'Feasibility' and 'VariableSizedBinPacking' with more than one bin type:
 * with the bin packing objectives ('BinPacking',
 * 'BinPackingWithLeftovers', 'BinPackingCuttingCost'), the bin types must
 * be used in their declared order. 'unreduce_solution' maps the bin type ids of the reduced instance back
 * to the original ones. See 'remove_dominated_bin_types_applies()'.
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
 * Reserving dedicated bins (see 'reduce_full_bin_items' and
 * 'reduce_perfect_pairs'): rectangle's own operations, compared against
 * the bin shrunk along each axis to the largest total length of the items
 * not exceeding it, every length (bin included) being increased by the cut
 * thickness, as for lifting. An item filling the shrunk bin can't share its
 * bin with any other item: it gets a dedicated bin. Two oriented items
 * spanning the shrunk bin along one axis, filling it together along the
 * other one, also get a dedicated bin together: in a solution, compacted
 * along the axis of the pair (see the lifting paragraph above), the other
 * items of the bin of each of them are beside it, in subtrees spanning the
 * bin along the other axis, whose total length is at most the length of
 * the other item of the pair; the subtrees of the two bins fit in a single
 * bin, as first-stage nodes if the first stage is along the axis of the
 * pair, as second-stage nodes of a single first-stage node otherwise
 * (never deeper than they were). Only for 'BinPacking' with a single bin
 * type without trims, defects, fixed items or resources, with the cut
 * constraints of 'items_can_shrink' (the item of a dedicated bin can be
 * smaller than the bin, leaving a waste), and for items alone in their
 * stacks (see 'full_bin_reduction_applies()'). Never reserves more
 * dedicated bins than the bin type has copies; if they take every copy
 * while items remain, the instance is infeasible (see
 * 'proven_infeasible()'). The dedicated items are left out of the reduced
 * instance, the bin type has its 'copies' reduced accordingly, and
 * 'unreduce_solution' adds the dedicated bins back, with the true
 * dimensions of the items. Run after lifting, which can make an item fill
 * the shrunk bin.
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
     * 'true' iff an item can be replaced by a smaller one in its node of a
     * cut tree, the rest of the tree being rewritten as in
     * 'unreduce_solution' (see the class-level doc comment): a cut type
     * among 'NonExact', 'Exact' and 'Roadef2018', no minimum distance
     * between cuts, no minimum waste length, no maximum number of cuts,
     * and no defects.
     */
    static bool items_can_shrink(const Instance& instance);

    /**
     * 'true' iff, 'items_can_shrink' holding, an item can be shrunk along
     * axis 'axis' ('0' for 'x', '1' for 'y'): any axis for 'NonExact',
     * the axis of the last-stage cuts for 'Exact', the axis of the extra
     * cuts for 'Roadef2018', both requiring a fixed first stage
     * orientation.
     */
    static bool items_can_shrink_along(
            const Instance& instance,
            int axis);

    /**
     * 'true' iff a node of size 'rect' fits the maximum distances between
     * 1-cuts and 2-cuts, along every axis they may apply to - required
     * when shrinking an item turns its node into a plate.
     */
    static bool fits_maximum_distances(
            const Instance& instance,
            const Rectangle& rect);

    /**
     * 'true' iff trimming dominated item types (see
     * 'remove_dominated_items') is meaningful for 'instance': only
     * 'Knapsack', with 'items_can_shrink'.
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
     * removing it when that is '0'.
     */
    void remove_dominated_items(
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
     * Builds the reduced 'Instance' from the working representation,
     * leaving out the bin types marked in 'bin_type_removed', populating
     * 'reduced_item_origin_runs_' and 'reduced_to_original_bin_type_id_'
     * along the way. Mirrors 'rectangle::Reduction::reduction_to_instance',
     * minus everything about dedicated bins.
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types,
            const std::vector<bool>& bin_type_removed);

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
     * Original instance's bin type id of each of 'instance_''s own bin
     * types - identity unless 'compute_dominated_bin_types' left some out.
     */
    std::vector<BinTypeId> reduced_to_original_bin_type_id_;

    /** A dedicated bin for each copy of an item type filling the bin. */
    struct FullBinItem
    {
        /** Item type id, original instance's id space. */
        ItemTypeId item_type_id;

        /** Number of dedicated bins. */
        ItemPos copies;
    };

    /** Dedicated bins for pairs of item types filling the bin together. */
    struct PerfectPair
    {
        /** Item type ids, original instance's id space. */
        ItemTypeId item_type_id_1;
        ItemTypeId item_type_id_2;

        /** Axis along which the two items are side by side ('0' for 'x'). */
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
     * The bin shrunk along each axis (see the class-level doc comment),
     * each length being increased by the cut thickness.
     */
    Rectangle shrunk_bin(
            const std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * Reserves a dedicated bin for every copy of every item type filling
     * the shrunk bin. Returns 'true' iff at least one item type was
     * reserved.
     */
    bool reduce_full_bin_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * Reserves a dedicated bin for 'min(copies_1, copies_2)' copies of every
     * pair of oriented item types filling the shrunk bin together
     * (including two copies of the same item type). Returns 'true' iff at
     * least one pair was reserved.
     */
    bool reduce_perfect_pairs(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * True dimensions of the original item type 'item_type_id', in the
     * orientation of the reduced item type (see 'swapped_item_types_').
     */
    Rectangle true_rect(ItemTypeId item_type_id) const;

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
