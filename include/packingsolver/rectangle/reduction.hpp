#pragma once

#include "packingsolver/rectangle/solution.hpp"

#include "optimizationtools/utils/parameters.hpp"

namespace packingsolver
{
namespace rectangle
{

/** Defined in 'src/rectangle/solution_builder.hpp'; only used here by pointer/reference. */
class SolutionBuilder;

/**
 * Structure passed as parameters of the reduction algorithm.
 *
 * Deliberately does not inherit from 'packingsolver::Parameters<Instance,
 * Solution, Output>' (unlike most other "*Parameters" structs in this
 * codebase): 'Reduction' is wired into 'optimize()' itself (see
 * 'optimize.hpp'/'optimize.cpp'), so inheriting from a type parameterized
 * on 'rectangle::Output' would create a circular include between this
 * header and 'optimize.hpp'. 'optimizationtools::Parameters' already
 * provides everything actually needed here (a 'Timer', 'verbosity_level',
 * ...) without that dependency.
 */
struct ReductionParameters: optimizationtools::Parameters
{
    /** Boolean indicating if the reduction should be performed. */
    bool reduce = true;

    /**
     * Enable/disable the wide/tall item enlargement sub-operation of
     * companion absorption (gates 'Reduction::reduce_group', called for
     * both 'EnlargementCase::Wide' and 'EnlargementCase::Tall' - see
     * 'Reduction''s class-level doc comment). Still subject to
     * 'Reduction::companion_absorption_applies' regardless of this flag.
     */
    bool enlarge_wide_tall_items = true;

    /**
     * Enable/disable the "both" item enlargement sub-operation of
     * companion absorption (gates 'Reduction::reduce_both_groups'). Still
     * subject to 'Reduction::companion_absorption_applies' regardless of
     * this flag.
     */
    bool enlarge_both_items = true;

    /**
     * Enable/disable the full-bin-item dedicated-bin sub-operation of
     * companion absorption (gates 'Reduction::reduce_full_bin_items').
     * Still subject to 'Reduction::companion_absorption_applies'
     * regardless of this flag.
     */
    bool reduce_full_bin_items = true;

    /**
     * Enable/disable the perfect-pair dedicated-bin sub-operation of
     * companion absorption (gates 'Reduction::reduce_perfect_pairs').
     * Still subject to 'Reduction::companion_absorption_applies'
     * regardless of this flag.
     */
    bool reduce_perfect_pairs = true;

    /**
     * Enable/disable merging identical item types (gates
     * 'Reduction::merge_identical_items'). Unlike the four flags above,
     * not subject to 'Reduction::companion_absorption_applies' - see
     * 'Reduction''s class-level doc comment for why this operation's
     * soundness does not depend on the same preconditions.
     */
    bool merge_identical_items = true;

    /** Maximum number of rounds of the outer fixpoint loop. */
    Counter maximum_number_of_rounds = 999;

    /**
     * Size of the tree search queue used for the companion-bin feasibility
     * checks (see 'Reduction').
     */
    NodeId subproblem_queue_size = 32;
};

/**
 * "Packing and removing some items" reduction (Côté, Haouari & Iori 2019,
 * "A Primal Decomposition Algorithm for the Two-dimensional Bin Packing
 * Problem", Section 4.2), plus an identical-item-type merge (see
 * 'merge_identical_items').
 *
 * Only runs at all when 'parameters.reduce' is 'true' - unlike every
 * previous version of this class, there is no shared objective/instance
 * precondition beyond that: this class runs two largely independent
 * operations, each gated by its own, narrower precondition (see
 * 'companion_absorption_applies()' and 'merge_identical_items()'
 * respectively), because their soundness rests on different arguments:
 *
 * - Companion absorption (wide/tall/both, full-bin items, perfect pairs):
 *   only for the 'BinPacking', 'VariableSizedBinPacking' and 'Feasibility'
 *   objectives (every item must be packed there, which is what makes this
 *   operation sound: it only preserves the number of bins used - and the
 *   feasibility of the built solution - because nothing it removes could
 *   ever legitimately be left unpacked, *not* true for 'Knapsack', where an
 *   item may legitimately be left unpacked), only for instances with a
 *   single bin type (the paper's 2D-BPP has one bin size; classifying an
 *   item as "wide"/"tall" depends on which bin's dimensions it is compared
 *   to, and a reduction valid for one bin type isn't obviously valid for
 *   another), and skipped whenever the instance has defects, a finite bin
 *   weight capacity together with non-trivial item weights, a non-'None'
 *   unloading constraint, or resources - every check this part performs
 *   (the companion-bin 'Objective::Feasibility' solves, and the direct
 *   dimension-only matches in 'reduce_full_bin_items'/
 *   'reduce_perfect_pairs') reasons purely about geometry, which none of
 *   these four respect: a defect sitting where a companion needs to go is
 *   invisible to a plain empty-rectangle check; bin weight, unloading
 *   order, and resource capacity are all *whole-bin* properties over every
 *   item that ends up sharing a bin, but a validated-enlarged item's real
 *   companions are invisibly removed from the reduced instance and only
 *   reinserted by 'unreduce_solution' after the downstream solve has
 *   already finished, so that solve can never verify any of the three
 *   still hold once it additionally places other items - never part of the
 *   isolated companion check - into that same bin. See
 *   'companion_absorption_applies()'.
 *
 *   The idea: an item type whose width exceeds half the bin's width (a
 *   "wide" item) can only ever share its row of the bin with items narrow
 *   enough to fit in the leftover width. If *every* narrow-enough item type
 *   can be proven (via an actual 'Objective::Feasibility' solve, small
 *   time/node budget) to fit alongside the wide items, in the leftover
 *   "companion" strips beside them, then those narrow items can be removed
 *   from the instance entirely (their placement is fully determined by
 *   wherever their paired wide item ends up) and the wide items enlarged to
 *   the full bin width - a much smaller equivalent instance. The symmetric
 *   case is applied for items taller than half the bin's height, and a
 *   third case for items that are both wider than half the bin's width
 *   *and* taller than half its height (paired with a full bin instead of a
 *   strip). See 'reduction.cpp' for the exact algorithm (which follows the
 *   paper's sorted, incrementally-growing candidate search, but uses
 *   PackingSolver's own 'Objective::Feasibility' solver as the
 *   packing-check primitive instead of the paper's bespoke greedy
 *   heuristic).
 *
 * - Merging identical item types (see 'merge_identical_items'): unlike
 *   companion absorption, a merged item type's copies stay fully visible
 *   and independently placed in the reduced instance (nothing is hidden
 *   from the downstream solve), so none of the whole-bin arguments above
 *   apply, and nothing about them depends on every item necessarily being
 *   packed either - the downstream solve still sees, and can still
 *   correctly enforce, every copy's true weight/resource
 *   consumption/unloading group/minimum-vs-maximum copies, and defects are
 *   irrelevant to it either way (they constrain *where* an item goes, not
 *   *which* interchangeable copy goes there). This operation therefore
 *   runs for *any* objective, regardless of defects, weight, resources,
 *   unloading constraint, or the number of bin types - as long as its own
 *   per-pair check (rect, plus every property that could make two items
 *   behave differently: orientation, weight, group, eligibility,
 *   mandatory-vs-optional copies, and - per bin type - resource
 *   consumption schedule) finds them truly interchangeable - and, for
 *   'Knapsack' specifically, profit too: every other objective here
 *   ignores profit as a merge criterion (it is never the actual
 *   objective, and 'unreduce_solution' always restores each copy's true
 *   original profit regardless of merging), but 'Knapsack' optimizes
 *   profit directly over a solve that may legitimately leave copies
 *   unplaced, so merging two different-profit item types would report one
 *   uniform profit for every copy and let the solve itself choose a
 *   suboptimal subset on wrong information - restoring true profits
 *   afterwards cannot undo that.
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

    /**
     * Number of dedicated bins set aside outside the reduced instance -
     * holding a single item whose own dimensions already exactly match
     * the bin's (see 'FullBinItem'), a "perfect pair" of two item types
     * that together exactly tile the bin (see 'PerfectPair'), or a "both"
     * group directly captured from a companion-bin check's own solution
     * (see 'BothGroup'). Must be added to any bin-count bound computed on
     * 'instance()' ('bin_packing_bound'; or, scaled by the bin type's
     * cost, 'variable_sized_bin_packing_bound') to recover the bound for
     * the original instance - unlike the wide/tall cases below, whose
     * enlarged item types stay present in 'instance()' and so are already
     * counted by any solve on it, these dedicated bins are entirely
     * absent from 'instance()' and cannot be accounted for by any solve
     * on it alone. Every other bound needs no such translation (in
     * particular 'is_proven_infeasible': these bins' capacity is already
     * subtracted from the reduced instance's own bin type copies in
     * 'reduction_to_instance', so a 'Feasibility' solve on 'instance()'
     * already answers for the original instance directly) - so a reduced
     * instance's 'Output' otherwise already holds the original instance's
     * bounds directly, in 'Output's own field layout (see
     * 'AlgorithmFormatter::update_bounds'), unlike e.g.
     * setcoveringsolver's 'Reduction', whose mandatory sets contribute an
     * extra cost to every bound.
     */
    BinPos number_of_dedicated_bins() const;

    /**
     * 'true' iff the reduction alone already proves the original instance
     * infeasible: a bin type's copies were exhausted by dedicated bins
     * (see 'FullBinItem'/'PerfectPair') while real items were still left
     * over needing to be packed (see 'reduction_to_instance'). Only ever
     * possible for 'Feasibility' (the only objective with genuinely
     * finite bin type copies in practice); always 'false' otherwise. When
     * 'true', 'instance()' is not a meaningful reduced instance to solve
     * at all - callers must check this first.
     */
    bool proven_infeasible() const { return proven_infeasible_; }

private:

    /**
     * 'true' iff companion absorption (wide/tall/both, full-bin items,
     * perfect pairs) is meaningful for 'instance' - the class-level doc
     * comment's "Companion absorption" paragraph (including the objective
     * check; unlike 'merge_identical_items', this operation is only sound
     * for 'BinPacking'/'VariableSizedBinPacking'/'Feasibility'). Does not
     * check 'parameters.reduce' - the constructor only calls this after
     * already checking it.
     */
    static bool companion_absorption_applies(const Instance& instance);

    /*
     * Private types
     */

    /**
     * Which of the two sub-cases enlarged a given item type - "wide" or
     * "tall" (see 'reduce_group'). The "both" sub-case has no enum value
     * of its own: it needs different enough control flow from wide/tall
     * (see 'reduce_both_groups'/'try_reduce_both_group') that every
     * function below taking 'EnlargementCase' is only ever called with
     * 'Wide'/'Tall' - "both" instead has its own dedicated, differently-
     * named functions (e.g. 'could_fit_both' alongside 'could_fit'), so
     * there is never a "both" branch to keep in sync (or leave
     * unreachable) in any of them.
     */
    enum class EnlargementCase { Wide, Tall };

    /**
     * A companion item removed from the instance because it was proven to
     * always fit alongside a bigger item.
     *
     * 'item_type_id' is in the *original* instance's item type id space:
     * the working representation ('ReductionItemType' below) is a stable,
     * never-reindexed 1:1 copy of the original instance's item types (only
     * ever marking entries 'removed', never inserting/erasing slots), so
     * its own indices already coincide with the original instance's ids.
     */
    struct CompanionItem
    {
        /** Item type id (original instance's id space) of the removed item. */
        ItemTypeId item_type_id;

        /** Position of the bottom-left corner relative to the big item's own bottom-left corner. */
        Point offset;

        /** Whether the removed item is rotated or not. */
        bool rotate;

        /**
         * This companion's own real companions, captured directly (via
         * 'extract_companions') at the moment it was itself absorbed here
         * - empty unless it was already enlarged (on some other axis, in
         * an earlier round) before being absorbed. A companion is no
         * longer excluded from being an already-enlarged item type (see
         * every R-candidate scan's own doc comment for why that
         * exclusion was unsound), so this can legitimately be non-empty
         * and needs to be resolved recursively when placing a solution -
         * see 'place_item_and_companions'.
         *
         * Captured *directly here*, rather than looked up later via
         * 'final_item_types_', because a single item type's own
         * 'companions_by_copy' entries can end up split across several
         * different absorptions (different copies of the same
         * already-enlarged type going to different big items, or
         * different copies within the same big item's own group) - only
         * the specific copy captured for *this* occurrence is correct
         * here; a later, id-based lookup has no way to tell which of
         * several possible entries belongs to which occurrence.
         */
        std::vector<CompanionItem> nested_companions;
    };

    /**
     * Working representation of an item type during the reduction process.
     *
     * Item types are never physically removed from this vector while the
     * reduction is running (only marked 'removed'): this keeps every id
     * stable across the whole (possibly multi-round) process, so that
     * 'CompanionItem::item_type_id' references remain valid regardless of
     * which round removed them, and so that this vector's own indices
     * double as original-instance item type ids throughout. It is only
     * compacted once, at the very end, when building the final reduced
     * 'Instance' (see 'reduction_to_instance').
     *
     * Only ever holds what the reduction process can actually *change*
     * ('rect', via enlargement; 'copies', via a partial 'PerfectPair' -
     * see 'reduce_perfect_pairs') plus the bookkeeping needed to undo it
     * ('removed'/'companions_by_copy'). Every other item type field
     * (profit, group, orientation, ...) never changes during the process,
     * so it is read directly from 'original_instance_' (indexable by the
     * exact same id, per the invariant above) wherever needed, instead of
     * being duplicated here.
     */
    struct ReductionItemType
    {
        bool removed = false;

        /**
         * Item type id (original instance's id space) this one was merged
         * into (see 'merge_identical_items'), or '-1' if it was not merged
         * away. Only ever set alongside 'removed = true', but distinct from
         * every other reason 'removed' can be 'true' (absorbed as a
         * companion, or consumed into a dedicated bin): those hide the item
         * from the reduced instance entirely, whereas a merged-away item's
         * own 'copies' still contribute to the survivor's - see
         * 'reduction_to_instance'.
         */
        ItemTypeId merged_into = -1;

        /** Current (possibly enlarged) dimensions. */
        Rectangle rect;

        /**
         * Current (possibly reduced) number of copies. Starts at the
         * original instance's own copies and only ever decreases, when a
         * 'PerfectPair' with an unequal-copies partner consumes
         * 'min(copies_1, copies_2)' of it, leaving the item type itself
         * present (not 'removed') with the leftover copies - see
         * 'reduce_perfect_pairs'. Every copies-sensitive computation
         * elsewhere in the class (building a 'try_reduce_group' check
         * sub-instance, 'reduce_full_bin_items', building the final
         * reduced instance, ...) must read this field, never
         * 'original_instance_->item_type(id).copies' directly - offering
         * more copies than truly remain to a validation solve could make
         * an actually-infeasible reduction look feasible.
         */
        ItemPos copies = 0;

        /**
         * For each copy of this item type (in the order copies will be
         * encountered while scanning a reduced solution), the companion
         * items packed alongside it. This is the sole record of whether
         * (and how) this item type was ever enlarged - there is no
         * separate boolean: the whole outer vector is empty iff it never
         * was, and non-empty (always exactly 'copies' entries, each
         * possibly itself empty) from the moment it first is, in
         * 'try_reduce_group''s own 'enlarge()' step (the only place this
         * field is ever set for 'Wide'/'Tall' - "both" never mutates it
         * in place at all, since an item absorbed via 'try_reduce_both_group'
         * is removed outright, not enlarged - see 'BothGroup'). An item
         * can be enlarged by more than one axis in turn (its current
         * 'rect' and already-captured companions from an earlier axis
         * directly feed the next axis's own search - see
         * 'gather_sorted_big_items'), so 'enlarge()' appends to whatever
         * is already here rather than overwriting it, keeping every
         * axis's real companions intact regardless of how many
         * contributed.
         */
        std::vector<std::vector<CompanionItem>> companions_by_copy;
    };

    /**
     * Everything below (through 'gather_sorted_both_big_items') differs
     * between the wide/tall sub-cases (sharing one function each, via a
     * plain switch on 'enlargement_case') and the "both" sub-case (its
     * own, separate, identically-purposed function, with an '_both'
     * suffix) - which used to go through a single, three-way switch per
     * function (or, before that, a 'GroupCaseConfig' struct of per-case
     * lambdas) instead. Neither stayed a good fit once "both" needed
     * genuinely different *control flow* (not just a different formula)
     * from wide/tall for its own companion-bin construction: a three-way
     * switch meant every wide/tall-only reader had to also read (and every
     * wide/tall-only change had to route around) a "both" branch that
     * lived in a completely different calling function, and vice versa.
     * Splitting each into its natural two functions - one shared by
     * 'reduce_group'/'try_reduce_group', one used only by
     * 'reduce_both_groups'/'try_reduce_both_group' - keeps each
     * function's own switch (if it has one at all) covering only the
     * cases that are actually symmetric.
     *
     * 'bin_w'/'bin_h', threaded through every function below, are always
     * the *shrunk* bin dimensions (equation (7), "shrinking the bins":
     * see 'compute_shrunk_bin_sizes') - not the bin's true ones, for the
     * same soundness reason every one of them needs: no achievable
     * combination of the original items ever exceeds the shrunk value, so
     * claiming exactly up to it already accounts for everything that
     * could ever really share a row/column/bin with this item.
     */

    /**
     * 'true' iff item type 'item_type_id' is eligible to be a "big" item
     * for 'enlargement_case' (see 'gather_sorted_both_big_items' for the
     * "both" equivalent, inlined there rather than a function of its own
     * - see its own doc comment for why). Takes the id (alongside the
     * working 'ReductionItemType') since it needs 'Instance::item_type's
     * own 'oriented' field, which 'ReductionItemType' no longer
     * duplicates.
     *
     * Requires the item to be 'oriented': 'Wide'/'Tall''s own enlargement
     * fixes it at a specific declared-orientation position, so a
     * non-oriented item's other orientation would otherwise be silently
     * ignored - see 'reduce_group'.
     */
    bool is_big(
            EnlargementCase enlargement_case,
            ItemTypeId item_type_id,
            const ReductionItemType& item,
            Length bin_w,
            Length bin_h) const;

    /**
     * Strict ordering used to sort big items for 'enlargement_case'
     * non-increasing: 'true' iff 'item_1' should be considered before
     * 'item_2' (see 'gather_sorted_both_big_items' for the "both"
     * equivalent, inlined there rather than a function of its own).
     */
    bool size_greater(
            EnlargementCase enlargement_case,
            const ReductionItemType& item_1,
            const ReductionItemType& item_2) const;

    /**
     * 'true' iff item type 'item_type_id' is worth offering alongside big
     * item 'big_item' under 'enlargement_case' ('Wide'/'Tall' - see
     * 'could_fit_both' for "both").
     *
     * When it returns 'true' for at least one candidate, this is only
     * ever a necessary, not sufficient, geometric pre-filter used to keep
     * 'try_reduce_group''s check candidate set small: the actual
     * 'Objective::Feasibility' solve it builds from it is what actually
     * proves feasibility, so an overly *generous* filter can never cause
     * an unsound reduction there, only a slower/less effective search.
     *
     * But when it comes back 'false' for *every* candidate against a
     * given big item, 'try_reduce_group' also relies on that as *proof*
     * nothing could ever fit at all, with no verifying solve behind that
     * claim (see its own doc comment for 'trivially_feasible') - so an
     * overly *restrictive* filter (missing a combination that actually
     * fits) is unsound in that direction specifically, not merely less
     * effective.
     */
    bool could_fit(
            EnlargementCase enlargement_case,
            const ReductionItemType& big_item,
            ItemTypeId item_type_id,
            const ReductionItemType& item,
            Length bin_w,
            Length bin_h) const;

    /**
     * Same as 'could_fit', for a "both"-big item - see there for the
     * general contract (necessary-not-sufficient pre-filter for
     * 'try_reduce_both_group''s own real solve; proof of "nothing fits at
     * all" for its own 'trivially_feasible').
     *
     * Takes 'big_item_type_id' in addition to 'big_item' (unlike
     * 'could_fit', which only ever needs the *candidate*'s own 'oriented'
     * flag): "both" uniquely also needs the *big* item's own 'oriented'
     * flag, to know whether trying its rotated presentation is even
     * allowed - checked against *every* orientation the big item could
     * actually use, not just its declared form, since under-checking here
     * would let 'trivially_feasible' wrongly "prove" a companion could
     * never fit, when it actually could alongside the big item's rotated
     * presentation - not merely a missed opportunity, but a real
     * unsoundness.
     */
    bool could_fit_both(
            ItemTypeId big_item_type_id,
            const ReductionItemType& big_item,
            ItemTypeId item_type_id,
            const ReductionItemType& item,
            Length bin_w,
            Length bin_h) const;

    /**
     * Dimensions of the companion bin built around 'big_item' for
     * 'enlargement_case'. Only ever 'Wide'/'Tall': "both"'s own companion
     * bin is always just 'bin_w'x'bin_h' directly (see
     * 'try_reduce_both_group'), too trivial a formula to need a function
     * of its own.
     */
    Rectangle companion_bin_dimensions(
            EnlargementCase enlargement_case,
            const ReductionItemType& big_item,
            Length bin_w,
            Length bin_h) const;

    /**
     * Position, relative to the big item's own bottom-left corner in the
     * final packing, of a companion item placed at 'bl_corner_in_check' in
     * the companion-bin check, for 'enlargement_case'. Only ever
     * 'Wide'/'Tall' (called by 'try_reduce_group'): "both" needs no
     * relative offset at all, since 'try_reduce_both_group' captures and
     * replays the check's own absolute positions directly - see
     * 'BothGroup'.
     */
    Point compute_offset(
            EnlargementCase enlargement_case,
            const ReductionItemType& big_item,
            Point bl_corner_in_check) const;

    /**
     * Not-yet-removed item types eligible as "big" items for
     * 'enlargement_case' ('Wide'/'Tall' - see 'gather_sorted_both_big_items'
     * for "both"), sorted non-increasing. Deliberately does not exclude
     * already-enlarged item types: 'is_big'/'could_fit'/
     * 'companion_bin_dimensions' all read the item's *current* dimensions,
     * so an item already enlarged on one axis (say wide, now spanning the
     * bin's full width) can still be genuinely "big" - or not - on another
     * (tall); its current width simply becomes part of that other axis's
     * own companion-strip search (a strictly more powerful search than
     * using its original width would give, since the strip now spans the
     * bin's full width too). An item already fully enlarged on a given
     * axis (current dimension already at the target) is naturally handled
     * by the existing degenerate-companion-bin path in 'try_reduce_group'
     * (zero/negative area), not by excluding it here.
     */
    std::vector<ItemTypeId> gather_sorted_big_items(
            const std::vector<ReductionItemType>& reduction_item_types,
            EnlargementCase enlargement_case,
            Length bin_w,
            Length bin_h);

    /**
     * Same as 'gather_sorted_big_items', for "both"-big items. Its own
     * "is big"/"strict ordering" logic (the "both" equivalent of
     * 'is_big'/'size_greater') is inlined directly in the implementation
     * rather than split into 'is_big_both'/'size_greater_both' functions
     * of their own: each is only a few lines, with a single call site
     * (this one), so a separate function would only add a name and a
     * jump to follow without a matching gain in clarity.
     *
     * Unlike 'is_big', a non-oriented item is safe to admit as "both"-big:
     * 'try_reduce_both_group' never pre-places or enlarges the big item
     * in place at all - it captures and replays whatever
     * position/rotation the companion-bin check itself found (see
     * 'BothGroup'), so there is no shared, per-type state that would need
     * a single orientation to stay consistent across every copy.
     */
    std::vector<ItemTypeId> gather_sorted_both_big_items(
            const std::vector<ReductionItemType>& reduction_item_types,
            Length bin_w,
            Length bin_h);

    /**
     * 'true' iff 'companions_by_copy' (a candidate big item's
     * not-yet-applied one) holds at least one real, solve-validated
     * companion, as opposed to being entirely empty. Used by
     * 'try_reduce_group' to decide whether a candidate that went through
     * an actual companion-bin solve (alongside other candidates in the
     * same group) ended up with anything assigned to it there - a
     * candidate that didn't is left untouched, since nothing here proves
     * its own strip couldn't have held something under a different
     * grouping. Bypassed entirely when 'try_reduce_group''s own
     * 'trivially_feasible' holds instead (see there): that case proves
     * real emptiness up front, via the candidate scan itself rather than
     * a solve, so every checked big item is enlarged unconditionally.
     */
    static bool has_validated_companions(
            const std::vector<std::vector<CompanionItem>>& companions_by_copy);

    /**
     * Extracts (and removes) the first 'copies_to_consume' entries of
     * 'item.companions_by_copy', for a caller about to consume that many
     * of 'item''s copies into a 'FullBinItem'/'PerfectPair'/'BothGroup'
     * (see whichever's own 'companions'/'companions_by_copy' field, or
     * 'CompanionItem::nested_companions' when the caller is itself
     * absorbing 'item' as a companion) - leaving the rest behind for
     * whatever of 'item''s own copies still remain in the working
     * representation afterwards. Returns 'copies_to_consume' empty
     * placeholder entries if 'item.companions_by_copy' is itself empty
     * (never enlarged; nothing to capture). Requires
     * 'item.companions_by_copy.size() == item.copies' whenever it is
     * non-empty (true by construction: 'try_reduce_group''s own
     * 'enlarge()' step - the only place this field is ever grown, see
     * 'ReductionItemType::companions_by_copy''s own doc comment - keeps
     * it sized to the copies count at that moment, and every caller of
     * this function keeps the two in lockstep by decrementing
     * 'item.copies' by the same 'copies_to_consume' it extracts here).
     */
    static std::vector<std::vector<CompanionItem>> extract_companions(
            ReductionItemType& item,
            ItemPos copies_to_consume);

    /**
     * Shared implementation of the wide/tall sub-cases (see
     * 'reduce_both_groups' for "both", which needs different enough
     * control flow - captured-and-replayed dedicated bins instead of an
     * in-place enlargement - not to share this): follows the paper's
     * sorted, incrementally-growing candidate search (singletons, then
     * growing multi-item groups, restarting after each success), using an
     * 'Objective::Feasibility' solve (small time/node budget) as the
     * packing-check primitive instead of the paper's bespoke greedy
     * heuristic. Returns 'true' iff at least one item type was reduced.
     */
    bool reduce_group(
            std::vector<ReductionItemType>& reduction_item_types,
            const ReductionParameters& parameters,
            EnlargementCase enlargement_case,
            Length bin_w,
            Length bin_h);

    /**
     * Attempt to prove that every not-yet-removed item type satisfying
     * 'could_fit' for some item in 'candidate_big_item_ids' can be packed
     * into the companion bins built around 'candidate_big_item_ids' (one
     * bin type per candidate, 'copies' companion-bin instances each). On
     * success, applies the reduction in place (marking absorbed items
     * removed and appending the newly found companions onto each
     * candidate's own 'companions_by_copy' - a candidate may already hold
     * companions found by an earlier axis, which are kept, not discarded)
     * and returns 'true'. Only ever called for 'EnlargementCase::Wide'/
     * 'Tall' - see 'reduce_both_groups'/'try_reduce_both_group' for the
     * "both" case's own, separate implementation.
     *
     * The R-candidate scan only ever excludes 'removed' item types, never
     * merely already-'enlarged' ones - checking each candidate's
     * *current* dimensions, which stay safe to use even though they may
     * be inflated (an item can only ever be enlarged on an axis where it
     * already qualified as "big" there, and "big" on an axis is exactly
     * what disqualifies it from passing *any* companion check on that
     * same axis - the foundational argument behind wide/tall/both itself
     * - so its current size on that axis fails 'could_fit' for the same
     * reason its original size would have). Excluding an enlarged item
     * type here would mean concluding "nothing could fit" using a
     * narrower candidate pool than the original problem actually offers,
     * which is unsound: an item that only happens to already be spoken
     * for elsewhere in *this* reduction's own bookkeeping could still
     * have been the thing a true optimal solution shares this space with
     * (found via a concrete counterexample: a 7x4 "wide" item and a 2x8
     * "tall" item, in a 10x10 bin, each independently found companionless
     * by their *own* strip check and enlarged to their respective full
     * dimensions - but a 7-wide row and a full-height 2-wide column
     * cannot coexist in one bin, even though the two *original* items
     * did, since the 2x8 item was always a valid, if ultimately
     * non-fitting, width-only candidate for the 7x4 item's own strip;
     * excluding it because it happened to already be enlarged elsewhere
     * let that candidate silently vanish from the scan). If absorbed
     * here, an already-enlarged companion's own real companions (if any)
     * are captured directly into 'CompanionItem::nested_companions' in
     * the "apply" step below, rather than orphaned.
     *
     * When 'candidate_r_ids' comes back completely empty - genuinely
     * nothing, from that same permissive candidate pool, passes
     * 'could_fit' for *any* checked big item - every checked big item is
     * unconditionally, safely enlarged with zero companions
     * ('trivially_feasible' below): equation (8)'s ("enlarging the
     * items", Côté, Haouari & Iori 2019/2021 Section 4.1) "empty group"
     * case, proven here by the scan itself rather than by an actual
     * solve. This never applies to a *degenerate* big item (zero/negative
     * area companion bin - its own dimensions already reached the bin's
     * on that axis): those are excluded from 'checked_big_item_ids' up
     * front and never touched by this function at all, which is
     * important - a degenerate companion bin means "no room to check",
     * not "checked and found empty", so it must never by itself justify
     * enlarging (that would burn the item type on a pure no-op for zero
     * gain, while permanently excluding it from a possibly better later
     * reduction - concretely, two items each already spanning the bin's
     * full height, making their own "tall" companion strip degenerate,
     * that together would have formed a real 'PerfectPair', except one of
     * them got claimed here first, with zero benefit, before
     * 'reduce_perfect_pairs' ever got a chance to see it).
     */
    bool try_reduce_group(
            std::vector<ReductionItemType>& reduction_item_types,
            const ReductionParameters& parameters,
            EnlargementCase enlargement_case,
            Length bin_w,
            Length bin_h,
            const std::vector<ItemTypeId>& candidate_big_item_ids);

    /**
     * A single dedicated bin built directly from a "both" companion-bin
     * feasibility check (see 'try_reduce_both_group'): unlike the
     * wide/tall cases (which enlarge a surviving big item type in place
     * and record its companions relative to it - see
     * 'CompanionItem::offset'), a "both" group is captured and replayed
     * exactly as the check's own solution found it, the same way
     * 'PerfectPair' captures two exactly-fitting item types directly
     * instead of enlarging one of them.
     *
     * Different copies of the same big item type's companion bin can end
     * up with genuinely different contents, or the big item placed with a
     * different rotation (since the big item is offered to the check as
     * an ordinary, freely placeable - and, if non-oriented, freely
     * rotatable - item rather than pre-fixed at a single position; see
     * 'try_reduce_both_group') - so, unlike 'FullBinItem'/'PerfectPair',
     * there is no shared 'copies' count here: each dedicated bin this
     * reduction found is its own separate entry.
     */
    struct BothGroup
    {
        struct PlacedItem
        {
            /** Item type id (original instance's id space). */
            ItemTypeId item_type_id;

            Point bl_corner;

            bool rotate;

            /**
             * This item's own pre-existing real companions, if it was
             * already enlarged (on a different axis, in an earlier
             * round) before being absorbed into this group - captured
             * the same way as 'FullBinItem::companions_by_copy' (see
             * there), but a single copy's worth, since each 'BothGroup'
             * is itself already one specific copy/dedicated bin.
             */
            std::vector<CompanionItem> companions;
        };

        /**
         * Every item placed in this one dedicated bin: the big item
         * itself plus whichever real companions the feasibility check
         * found alongside it.
         */
        std::vector<PlacedItem> items;
    };

    /**
     * "Both" group reduction: unlike 'reduce_group' (shared by the
     * wide/tall cases), each companion-bin feasibility check's own
     * solution is captured and replayed directly as one or more dedicated
     * bins (see 'BothGroup' and 'try_reduce_both_group'), rather than
     * enlarging a surviving big item type in place. Follows the same
     * sorted, incrementally-growing candidate search as 'reduce_group'.
     * Returns 'true' iff at least one dedicated bin was found.
     */
    bool reduce_both_groups(
            std::vector<ReductionItemType>& reduction_item_types,
            const ReductionParameters& parameters,
            Length bin_w,
            Length bin_h);

    /**
     * Attempt to prove that every not-yet-removed item type satisfying
     * 'could_fit_both' for some item in 'candidate_big_item_ids' can be
     * packed, alongside its own big item, into a companion bin of size
     * 'bin_w'x'bin_h' (the "both" case's companion bin is always the full
     * shrunk bin, never a strip). Unlike 'try_reduce_group', the big item
     * is *not* pre-placed as a fixed item: it is offered to the check as
     * an ordinary item, with its own true 'oriented' flag, free to be
     * placed - and, if non-oriented, rotated - however the solve likes.
     *
     * Every candidate's own big item shares one single companion bin type
     * with every other candidate in the group (rather than each getting
     * its own, eligibility-restricted one, as an earlier version of this
     * function did): nothing needs to keep two different candidates'
     * big items from ever landing in the same bin instance, because
     * geometry already guarantees it can't happen - two "both"-big items
     * (each spanning more than half of *both* the bin's dimensions, by
     * definition - see 'gather_sorted_both_big_items') can never
     * simultaneously fit in one 'bin_w'x'bin_h' bin, regardless of where
     * either is placed. So a
     * solved bin instance holds at most one candidate's big item purely
     * by construction, and the interpretation step below can just read
     * off which one (if any) directly from that bin's own contents.
     *
     * On success, each distinct bin the check's own solution built becomes
     * its own 'BothGroup' entry (see there for why no shared 'copies'
     * count is used), and every item placed in it - the big item and
     * every real companion - is removed from the working representation,
     * capturing whatever companions it may already have had of its own.
     * A candidate big item that ends up with zero bin instances actually
     * used (possible, since the shared companion bin type's 'copies_min'
     * is 0 - the solver only needs to use as many bin instances as it
     * takes to place every big item and R-candidate somewhere) is left
     * completely untouched, the same way 'try_reduce_group' leaves an
     * unused candidate alone - though in practice, since every big item
     * is itself mandatory and a bin holds at most one, exactly as many
     * bin instances end up used as there are big item copies offered in
     * total, so this only matters if the check fails to find a solution
     * at all (handled separately, via 'check_solution.full()').
     *
     * Respects the same bin type copies cap as
     * 'reduce_full_bin_items'/'reduce_perfect_pairs', since every group
     * here becomes its own dedicated bin outside the reduced instance's
     * own bin type copies - checked once, over the *total* number of
     * dedicated bins this call would add across every candidate at once,
     * declining the whole batch if it would not fit rather than partially
     * committing: an R-candidate's own copies can end up split across
     * several different big items' companion bins by the check, with no
     * simple way to tell which specific placement belongs to which
     * candidate once only some of them are declined ("sound, just less
     * effective" - as elsewhere in this class).
     *
     * When 'candidate_r_ids' comes back completely empty - genuinely
     * nothing, from the same 'removed'-only-excluded candidate pool,
     * passes 'could_fit_both' for *any* candidate big item - each
     * candidate is independently, unconditionally captured as its own
     * single-item dedicated bin (still subject to the same bin type
     * copies cap, and to determining which orientation actually fits,
     * per candidate):
     * equation (8)'s ("enlarging the items", Côté, Haouari & Iori
     * 2019/2021 Section 4.1) "empty group" case, proven here by the scan
     * itself rather than by an actual solve - no group solve is needed
     * (or possible: "both"'s companion bin is always the full 'bin_w'x
     * 'bin_h' bin, never degenerate, so there is no analogous
     * degenerate/checked-empty distinction to make the way
     * 'try_reduce_group' must for its own strips).
     *
     * Returns 'true' iff at least one dedicated bin was found this way.
     */
    bool try_reduce_both_group(
            std::vector<ReductionItemType>& reduction_item_types,
            const ReductionParameters& parameters,
            Length bin_w,
            Length bin_h,
            const std::vector<ItemTypeId>& candidate_big_item_ids);

    /**
     * A single item type whose own dimensions (in some allowed
     * orientation) already exactly match the bin's: it fills a bin by
     * itself, so nothing else could ever share a bin with it (see
     * 'reduce_full_bin_items'). Set aside as 'copies' dedicated bins the
     * same way as a 'PerfectPair', just with one item per bin instead of
     * two.
     *
     * In practice, such an item is already "both"-big (an exact match
     * trivially satisfies the "both"-big condition on both axes - see
     * 'gather_sorted_both_big_items') with no room left for any companion
     * (its own dimensions already consume the entire companion bin), so
     * 'try_reduce_both_group' - which runs earlier in the same round, in
     * the constructor's own outer fixpoint loop - already captures it
     * directly via its own 'trivially_feasible' path (see there) before
     * 'reduce_full_bin_items' ever gets a turn.
     * 'reduce_full_bin_items' remains as the direct, explicit dimension
     * check regardless - simpler and more obviously correct to state on
     * its own terms than reasoning transitively through "both"'s own
     * companion-search machinery, even if the latter now reaches the same
     * conclusion first in the common case.
     */
    struct FullBinItem
    {
        /** Item type id (original instance's id space). */
        ItemTypeId item_type_id;

        /** Whether the item is rotated to match the bin. */
        bool rotate;

        /** Number of dedicated bins (one per copy of the item type). */
        BinPos copies;

        /**
         * Companions of 'item_type_id' itself, one entry per copy
         * consumed here, captured (moved out of the working
         * 'ReductionItemType::companions_by_copy') at the moment this
         * item type was claimed - empty entries if it was never enlarged.
         * An item type reaching this point can already carry real,
         * solve-validated companions of its own (see
         * 'reduce_full_bin_items''s own doc comment for why that is no
         * longer excluded): those companions still need to end up
         * somewhere in the final solution, so they travel with this
         * record instead of being silently dropped, and 'unreduce_solution'
         * places them recursively alongside 'item_type_id'.
         */
        std::vector<std::vector<CompanionItem>> companions_by_copy;
    };

    /**
     * For every not yet removed item type whose *current* dimensions (in
     * some allowed orientation) exactly match 'bin_w'x'bin_h', removes it
     * from the instance and records it in 'full_bin_items_'. Returns
     * 'true' iff at least one item type was reduced.
     *
     * 'bin_w'/'bin_h' are the *shrunk* bin dimensions (equation (7),
     * "shrinking the bins": see 'compute_shrunk_bin_sizes'), not the
     * bin's true ones - the largest achievable combination of *original*
     * item widths/heights not exceeding the true bin's. If an item's width already equals
     * that maximum, no other item's width can be small enough to add
     * anything to it without exceeding the maximum itself - a
     * contradiction - so the true bin's remaining margin on that axis is
     * guaranteed permanently unusable by anything else, symmetrically for
     * height, jointly covering the entire L-shaped leftover region around
     * the item. This is why a single "current dimensions" check suffices
     * here, unlike an earlier version of this function, which separately
     * checked an item's *current* dimensions against the *true* bin
     * *and* its *original* dimensions against the *shrunk* bin as two
     * independent regimes: once every enlargement target in the class
     * uses the same shrunk value ('bin_w'/'bin_h', passed uniformly to
     * every wide/tall/both helper - see 'is_big' and friends above), an
     * item that was never touched already has current == original
     * dimensions, and an item that *was* enlarged already has its current
     * dimensions reflecting that same shrunk target - so checking current
     * dimensions against the (now uniformly shrunk) 'bin_w'/'bin_h'
     * covers both cases at once.
     *
     * An item type here may already carry companions - with zero of them
     * (companionlessly, on one axis only, which may now newly bring it to
     * an exact match on both), or with real, solve-validated companions
     * of its own (see 'FullBinItem::companions_by_copy', which captures
     * and carries them along instead of orphaning them - unlike an even
     * earlier version of this function, which excluded already-enlarged
     * item types entirely to sidestep that; see the git history for why
     * that exclusion turned out to be the wrong fix).
     *
     * Sound for the same reason a 'PerfectPair' is: such an item can
     * never share a bin with anything else in any solution (it already
     * fills the entire bin by itself, taking into account permanently
     * unusable margin, and taking into account its own real companions -
     * if any - which are captured and travel with it), so setting it
     * aside in its own dedicated bin never requires more bins than that
     * solution already used. Respects the same bin type copies cap as
     * 'reduce_perfect_pairs' (shared via 'number_of_dedicated_bins'), for
     * the same reason.
     */
    bool reduce_full_bin_items(
            std::vector<ReductionItemType>& reduction_item_types,
            Length bin_w,
            Length bin_h);

    /**
     * A "perfect pair": two item types that, together (each in some
     * allowed orientation), exactly tile the bin via a single guillotine
     * split - side by side, both spanning the bin's full height, widths
     * summing to its full width; or stacked, both spanning its full
     * width, heights summing to its full height. Unlike the
     * wide/tall/both cases (which enlarge a surviving item type and
     * remove its companions), *both* item types of a perfect pair are
     * removed from the instance entirely: nothing else could ever share
     * a bin with either of them (see 'reduce_perfect_pairs'), so the
     * pair is set aside as 'copies' fixed, dedicated bins instead of
     * being handed to the underlying solver at all. Reinstated as extra
     * bins in 'unreduce_solution', and folded into the reduced
     * instance's bin type copies in 'reduction_to_instance'.
     */
    struct PerfectPair
    {
        /**
         * First item type id (original instance's id space); placed at a
         * dedicated bin's bottom-left corner, never rotated. Both item
         * types of a pair are required to be 'oriented' (see
         * 'reduce_perfect_pairs''s own doc comment for why), so there is
         * no rotated form to ever record for either one.
         */
        ItemTypeId item_type_id_1;

        /** Second item type id (original instance's id space); never rotated either. */
        ItemTypeId item_type_id_2;

        /** Position of the second item's bottom-left corner. */
        Point offset_2;

        /** Number of dedicated bins (one per copy of the pair; both item types have this many copies). */
        BinPos copies;

        /**
         * Companions of 'item_type_id_1', captured the same way as
         * 'FullBinItem::companions_by_copy' (see there) - empty entries
         * if it was never enlarged. One entry per copy consumed here.
         */
        std::vector<std::vector<CompanionItem>> item_1_companions_by_copy;

        /** Companions of 'item_type_id_2', same as 'item_1_companions_by_copy' but for the second item type. */
        std::vector<std::vector<CompanionItem>> item_2_companions_by_copy;
    };

    /**
     * "Perfect pair" reduction: for every pair of distinct, not yet
     * removed item types - either side may already carry companions, with
     * zero of them (companionlessly) or with real, solve-validated
     * companions of its own, which are captured into
     * 'PerfectPair::item_1_companions_by_copy'/'item_2_companions_by_copy'
     * rather than orphaned (see 'reduce_full_bin_items''s own doc comment
     * for the same point spelled out for a single item) - consumes
     * 'min(copies_1, copies_2)' copies from each side and records the
     * pair in 'perfect_pairs_' if their *current* dimensions form a
     * 'PerfectPair' against 'bin_w'x'bin_h'.
     *
     * 'bin_w'/'bin_h' are the *shrunk* bin dimensions, exactly as for
     * 'reduce_full_bin_items' - see that function's own doc comment for
     * why a single "current dimensions" check against the (uniformly
     * shrunk, throughout the class) bin dimensions now suffices here too,
     * where an earlier version of this function tried two independent
     * regimes (current dimensions against the true bin, or original
     * dimensions against the shrunk bin) separately.
     *
     * Returns 'true' iff at least one pair was reduced.
     *
     * Sound by a "slide together" exchange argument distinct from the
     * wide/tall/both cases' own: since one item of the pair spans
     * 'bin_w'/'bin_h' by construction, it is forced to do so in *every*
     * bin it could ever be placed in (its own height already equals the
     * bin's, taking into account the same permanently-unusable margin
     * argument as 'reduce_full_bin_items''s own), so whatever shares a
     * bin with it is confined to one or two same-height strips together
     * summing to exactly the other item's own width - which can always be
     * reassembled, by simple translation, into a single bin alongside
     * whatever shared the *other* item's own bin. So setting them aside
     * together in one dedicated bin, wherever they individually end up in
     * any solution, never requires more bins than that solution already
     * used.
     *
     * Both item types of a pair are required to be 'oriented' - unlike
     * 'reduce_full_bin_items', this cannot soundly be relaxed for a
     * non-oriented item. That function's own rotation handling is safe
     * because matching *both* bin dimensions at once forces a non-square
     * bin's declared (non-rotated) form to be geometrically infeasible in
     * that same bin - the item is *effectively* oriented there regardless
     * of the flag. A perfect pair only matches *one* bin dimension, which
     * leaves the item's other dimension unconstrained: its declared form
     * typically remains perfectly valid too, so nothing forces it into
     * the "spans this dimension" role the "slide together" argument above
     * depends on - a true optimal solution could legitimately place it
     * the other way, sharing its bin with something this reduction never
     * considered. Committing such an item to a dedicated bin here could
     * then overstate the bins truly required, corrupting not just the
     * primal solution but 'bin_packing_bound' itself (this reduction's
     * dedicated-bin count is added directly onto it - see
     * 'optimize()' - so it must never be an overcount).
     *
     * Pairs 'min(copies_1, copies_2)' copies (rather than requiring equal
     * copies) by consuming that many from 'ReductionItemType::copies' on
     * *both* sides: whichever side reaches zero is marked 'removed', but a
     * side with more copies than its partner keeps its leftover copies
     * and stays present in the reduced instance as an ordinary
     * (non-removed) item type - e.g. a pair with 2 and 4 copies fixes 2
     * dedicated bins and removes the 2-copy item type entirely, leaving
     * the other with 2 copies still to be packed normally. Never reserves
     * more dedicated bins than the bin type actually has copies for
     * (skipping a pair that would otherwise overrun it, leaving both item
     * types for the underlying solver instead) - this keeps the
     * subtraction in 'reduction_to_instance' from ever going negative, so
     * no separate infeasibility bookkeeping is needed here.
     */
    bool reduce_perfect_pairs(
            std::vector<ReductionItemType>& reduction_item_types,
            Length bin_w,
            Length bin_h);

    /**
     * Merge every group of pairwise-'items_mergeable' item types in
     * 'reduction_item_types' into a single survivor (the lowest item type
     * id in the group) with the group's combined copies - see the
     * class-level doc comment's "Merging identical item types" paragraph
     * for why this is sound regardless of defects/weight/resources/
     * unloading constraint/number of bin types. Unlike companion
     * absorption, a merged-away item type's own 'copies'/
     * 'companions_by_copy' are left untouched (only 'removed' and
     * 'merged_into' are set) - 'reduction_to_instance' reads them directly
     * when it later expands the survivor back into a per-copy origin list
     * (see 'CopyOrigin'), so there is nothing to consolidate here.
     *
     * Returns 'true' iff at least one merge happened (unlike the other
     * 'reduce_*' methods, this is not currently used to drive a fixpoint
     * loop - merging item types never changes any 'rect'/'copies' value
     * that could make a previously-failed companion-absorption check
     * succeed - but is still reported for consistency and in case a future
     * change relies on it).
     */
    bool merge_identical_items(
            std::vector<ReductionItemType>& reduction_item_types);

    /**
     * 'true' iff item types 'item_type_id_1' and 'item_type_id_2' are
     * interchangeable: same current (possibly already-enlarged) 'rect', and
     * every original-instance property that could make them behave
     * differently downstream - orientation, weight, group, eligibility,
     * and, for every bin type/resource, the exact same per-copy
     * consumption schedule (an empty/absent schedule only matches another
     * empty/absent one, even though both mean "zero consumption" - a
     * missed merge opportunity in that corner case, never an unsound one).
     * Does *not* compare profit, except for 'Knapsack' - see this method's
     * own doc comment in 'reduction.cpp' for why a mismatch is harmless
     * for every other objective this class handles, but not for
     * 'Knapsack', where profit is the actual objective and copies are
     * optional.
     *
     * Also requires each candidate's own current (post-companion-
     * absorption) copies to be either entirely mandatory (its effective
     * 'copies_min' equals its 'copies' - see 'effective_copies_min') or
     * entirely optional (effective 'copies_min' is 0) - a mix, or a
     * genuinely partial requirement (0 < copies_min < copies, only
     * possible for 'Knapsack', the one objective companion absorption
     * never touches) would make 'reduction_to_instance''s per-copy origin
     * list (see 'CopyOrigin') pick an arbitrary split between the two
     * original item types that may not respect either one's own true
     * minimum - see 'reduction.cpp' for a concrete counterexample.
     *
     * Does not look at 'removed'/'merged_into' - callers are responsible
     * for only comparing candidates that are not already merged away.
     */
    bool items_mergeable(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2) const;

    /**
     * 'item_type_id''s own minimum copies requirement, adjusted for
     * whatever companion absorption has already consumed from its
     * 'copies' (mirrors how 'reduction_to_instance' already adjusts a bin
     * type's own 'copies_min' by 'number_of_dedicated_bins' - see there):
     * 'max(0, original.copies_min - (original.copies -
     * reduction_item_types[item_type_id].copies))'. For every objective
     * except 'Knapsack', 'original.copies_min' always equals
     * 'original.copies' (see 'InstanceBuilder::build'), so this always
     * equals 'reduction_item_types[item_type_id].copies' exactly - the
     * "still fully mandatory" case 'items_mergeable' and
     * 'reduction_to_instance' both rely on. For 'Knapsack' (the one
     * objective companion absorption never touches, so nothing is ever
     * consumed from its 'copies'), this always equals
     * 'original.copies_min' directly.
     */
    ItemPos effective_copies_min(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id) const;

    /** Build the final reduced 'Instance' from the working representation. */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types);

    struct ShrunkBinSizes
    {
        /** Shrunk bin width (W*, equation (7)). */
        Length bin_width;

        /** Shrunk bin height (equation (7)'s height variant). */
        Length bin_height;
    };

    /**
     * "Shrinking the bins" (Côté, Haouari & Iori, 2019/2021, Section 4.1,
     * equation (7)), following Alvarez-Valdes et al. (2009): the bin's own
     * width can be shrunk down to the largest achievable combination of
     * item widths that does not exceed it - no combination of items can
     * ever use more of the bin's width than that regardless, so treating
     * the bin as this narrower "shrunk" width changes nothing about which
     * combinations of items can validly share a row, while tightening any
     * test compared against the bin's width (e.g. "wide" item
     * thresholds). Symmetric for height. Only meaningful for instances
     * with a single bin type and finite item copies (an infinite-copies
     * item type can already saturate any capacity on its own, and can't be
     * flattened into individual units below); returns 'false' and leaves
     * the bin's own (unchanged) dimensions in 'shrunk_bin_sizes' for every
     * other case; returns 'true' when equation (7) actually ran.
     *
     * Computed over 'reduction_item_types''s *current* rect/copies,
     * skipping already-'removed' item types entirely - not over
     * 'original_instance_''s original, full population. This is
     * deliberately recomputed every round (see the constructor's own
     * comment at the call site) rather than once, upfront: a 'removed'
     * item type (absorbed as a companion, consumed into a dedicated bin,
     * ...) is no longer independently available to combine with anything
     * else, so excluding it only ever lowers (or leaves unchanged) the
     * genuinely achievable combination on either axis - it does not
     * understate it, even though the item itself has not physically left
     * the bin: its own contribution is either already folded into a
     * survivor's current (possibly enlarged) dimensions, which this
     * function does still count via that survivor's own current entry,
     * or it is captured inside a dedicated bin that is entirely outside
     * this bound's concern (dedicated bins are never shared with
     * anything else - see 'number_of_dedicated_bins'). Once an item type
     * is the *only* one left, this bound naturally collapses to exactly
     * its own current dimensions - correctly proving nothing else
     * remains that could ever share a bin with it, which
     * 'reduce_full_bin_items' below then acts on.
     *
     * Computed via 'multiplechoicesubsetsumsolver' (as in
     * 'boxstacks::TreeSearch''s own "lift length" computation): one group
     * per remaining item copy, containing one candidate value per
     * orientation the item type is allowed to present on this axis (its
     * declared dimension, plus its rotated one too if not 'oriented') -
     * the solver picks *at most* one candidate per group (a group can
     * contribute nothing at all, i.e. that copy sits out of this
     * particular combination), maximizing the total not exceeding the
     * bin's true dimension on that axis. This is what lets a non-oriented
     * item participate soundly, unlike a plain (single-choice) subset sum:
     * each axis's computation independently picks whichever orientation
     * of a given copy contributes more, without needing that choice to
     * also be consistent with what the *other* axis's own (separately
     * computed) combination picked for the same copy - the two
     * computations never claim their respective maxima are
     * simultaneously achievable by one real arrangement, only that
     * neither individually is ever exceeded by anything real, which is
     * all equation (7) needs.
     */
    bool compute_shrunk_bin_sizes(
            const std::vector<ReductionItemType>& reduction_item_types,
            ShrunkBinSizes& shrunk_bin_sizes) const;

    /**
     * Maximum achievable combination of not-yet-'removed' item widths (or
     * heights) not exceeding 'capacity' - the inner multiple-choice
     * subset-sum maximization of equation (7), shared by
     * 'compute_shrunk_bin_sizes''s width/height calls, via
     * 'multiplechoicesubsetsumsolver'. Takes 'original_instance' alongside
     * 'reduction_item_types' only for each item type's own 'oriented'
     * flag, which 'ReductionItemType' does not duplicate.
     */
    static Length max_achievable_dimension_sum(
            const std::vector<ReductionItemType>& reduction_item_types,
            const Instance& original_instance,
            Length capacity,
            bool width_axis);

    /*
     * Private attributes
     */

    /** Original instance. */
    const Instance* original_instance_ = nullptr;

    /** Reduced instance. */
    Instance instance_;

    /** See 'proven_infeasible()'. */
    bool proven_infeasible_ = false;

    /**
     * "Full bin item" reservations found while building 'instance_' (see
     * 'reduce_full_bin_items'/'FullBinItem'). Indexed by discovery order,
     * not by item type id.
     */
    std::vector<FullBinItem> full_bin_items_;

    /**
     * "Perfect pair" reservations found while building 'instance_' (see
     * 'reduce_perfect_pairs'/'PerfectPair'). Indexed by discovery order,
     * not by item type id.
     */
    std::vector<PerfectPair> perfect_pairs_;

    /**
     * "Both" group reservations found while building 'instance_' (see
     * 'reduce_both_groups'/'BothGroup'). Indexed by discovery order, not
     * by item type id; each entry is already its own single dedicated
     * bin (unlike 'full_bin_items_'/'perfect_pairs_', there is no
     * separate 'copies' multiplier to expand).
     */
    std::vector<BothGroup> both_groups_;

    /**
     * The reduction's final bookkeeping for *every* original item type
     * ('companions_by_copy' is empty for item types that were left
     * untouched), survivor in the reduced instance or not - unlike a
     * removed/consumed item type, which never appears in the reduced
     * instance at all, this still needs to be reachable so
     * 'unreduce_solution' can recursively resolve a companion (of a
     * 'FullBinItem', a 'PerfectPair', or another companion) that turns
     * out to have had real companions of its own.
     *
     * Indexed by the *original* instance's own item type ids (a direct
     * copy of the working representation's own indexing - see
     * 'ReductionItemType''s own doc comment): populated once, when the
     * working representation is finalized into the reduced instance (see
     * 'reduction_to_instance').
     */
    std::vector<ReductionItemType> final_item_types_;

    /**
     * Where one particular copy of a reduced instance item type came from:
     * the corresponding item type id in the *original* instance, and that
     * original item type's own local copy index (needed to index its
     * 'companions_by_copy', if any - see 'ReductionItemType''s own doc
     * comment).
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
     * ids: populated once, alongside 'final_item_types_' (see
     * 'reduction_to_instance').
     */
    std::vector<std::vector<CopyOrigin>> reduced_copy_origins_;

    /**
     * Places 'item_type_id' (original instance's id space) at 'bl_corner'
     * (with 'rotate') into bin 'bin_pos' of 'solution_builder', then
     * recursively places each of 'companions' at its own recorded offset
     * relative to 'bl_corner', and, for any companion whose own
     * 'CompanionItem::nested_companions' is non-empty (an already-enlarged
     * item type absorbed as someone else's companion - see
     * 'CompanionItem''s own doc comment for why this is legitimate and
     * not merely a defensive possibility), that companion's own nested
     * companions too, and so on to arbitrary depth. Each 'CompanionItem'
     * carries everything needed for its own subtree directly - no lookup
     * into 'final_item_types_' needed here (that is only for the
     * *top-level* item being placed, whose own companions come from
     * 'final_item_types_' at the call site instead - see
     * 'unreduce_solution').
     */
    void place_item_and_companions(
            SolutionBuilder& solution_builder,
            BinPos bin_pos,
            ItemTypeId item_type_id,
            Point bl_corner,
            bool rotate,
            const std::vector<CompanionItem>& companions) const;

};

}
}
