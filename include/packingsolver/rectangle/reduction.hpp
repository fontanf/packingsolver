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
     *
     * Also gates 'Reduction::reduce_full_span_items' ("Pinning full-span
     * items" in the class-level doc comment) - that operation's own
     * boundary case, so it shares this same flag rather than a separate
     * one of its own; still subject to its own
     * 'Reduction::reduce_full_span_items_applies' regardless of this flag.
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

    /**
     * Enable/disable lifting item dimensions via a 1D subset-sum bound
     * (gates 'Reduction::lift_item_dimensions'). Still subject to
     * 'Reduction::lift_item_dimensions_applies' regardless of this flag -
     * see there for how its precondition differs from companion
     * absorption's.
     */
    bool lift_item_dimensions = true;

    /**
     * Enable/disable trimming negative-profit item types down to their own
     * 'copies_min' (gates 'Reduction::remove_negative_profit_items').
     * Still subject to 'Reduction::remove_negative_profit_items_applies'
     * regardless of this flag - see there for its own precondition.
     */
    bool remove_negative_profit_items = true;

    /**
     * Enable/disable removing dominated item types (gates
     * 'Reduction::remove_dominated_items'). Still subject to
     * 'Reduction::remove_dominated_items_applies' regardless of this flag
     * - see there for its own precondition.
     */
    bool remove_dominated_items = true;

    /**
     * Enable/disable removing dominated bin types (gates
     * 'Reduction::compute_dominated_bin_types'). Still subject to
     * 'Reduction::remove_dominated_bin_types_applies' regardless of this
     * flag - see there for its own precondition.
     */
    bool remove_dominated_bin_types = true;

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
 * 'merge_identical_items'), a companionless, subset-sum-based item
 * dimension lift (see 'lift_item_dimensions'), a negative-profit item
 * trim (see 'remove_negative_profit_items'), a dominated-item removal
 * (see 'remove_dominated_items'), and a dominated-bin-type removal (see
 * 'compute_dominated_bin_types').
 *
 * Only runs at all when 'parameters.reduce' is 'true' - unlike every
 * previous version of this class, there is no shared objective/instance
 * precondition beyond that: this class runs six largely independent
 * operations, each gated by its own, narrower precondition (see
 * 'companion_absorption_applies()', 'merge_identical_items()',
 * 'lift_item_dimensions_applies()', 'remove_negative_profit_items_applies()',
 * 'remove_dominated_items_applies()' and
 * 'remove_dominated_bin_types_applies()' respectively), because their
 * soundness rests on different arguments:
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
 * - Lifting item dimensions via subset sum (see 'lift_item_dimensions'):
 *   for a given item type and axis, a cheaper, more broadly applicable
 *   sibling of companion absorption's own wide/tall enlargement, using a
 *   purely 1D 'multiplechoicesubsetsumsolver' bound (the same primitive
 *   'compute_shrunk_bin_sizes' already uses to shrink the *bin*, applied
 *   here per *item* instead) rather than an actual 2D
 *   'Objective::Feasibility' companion-bin solve: run *after* shrinking
 *   the bin each round (see the constructor's own comment), against the
 *   bin's *shrunk* dimension rather than its true one - itself already
 *   the tightest sound substitute for it throughout this class - if no
 *   achievable combination of the *other* item types can ever reach the
 *   shrunk remaining margin beside this item, that margin is provably,
 *   permanently empty regardless of which items end up sharing the bin,
 *   so the item's own dimension can be grown to close it - with zero real
 *   companions attached (unlike wide/tall, this never removes or hides
 *   any other item type from the reduced instance). Because nothing is
 *   ever hidden this way, none of companion absorption's weight/resource/
 *   unloading-constraint exclusions apply here, and - like
 *   'merge_identical_items' - nothing about it depends on every item
 *   necessarily being packed, so it runs for *any* objective, on either
 *   axis independently, with one exception: an objective that directly
 *   measures how far items extend along a given axis
 *   ('Objective::OpenDimensionX'/'OpenDimensionY', or
 *   'Objective::BinPackingWithLeftovers' with a 'LeftoverMode' that
 *   depends on that axis - 'X'/'Y' for their own axis, 'Area' for both)
 *   must never lift on that axis. Lifting only ever grows an item type's
 *   *declared* dimension in the reduced instance the downstream solve
 *   actually runs on ('reduction_to_instance' carries 'item.rect' over
 *   as-is) - 'unreduce_solution' then places the true, original-sized
 *   item back at whatever position the solve chose for its enlarged
 *   stand-in, leaving the provably-unusable margin as genuine empty space
 *   in the final solution, exactly as intended. That is harmless for an
 *   objective that only cares about *whether* everything fits (or how
 *   many bins that takes): the margin was never usable by anything else
 *   regardless. But an objective that scores a solution by exactly how
 *   far along an axis its items reach cannot tell the two apart: the
 *   downstream solve reasons - and makes placement/objective-value
 *   decisions - against the *enlarged* footprint, potentially settling
 *   for a worse arrangement than the true, smaller footprint would have
 *   allowed, since it believes reach along that axis is already
 *   unavoidable when it is not. See 'lift_item_dimensions_applies_axis()'.
 *   It does still need a single bin type (the bin's own true dimension
 *   must be unambiguous) and no defects (a defect sitting inside the
 *   claimed "provably empty" margin would be invisible to this purely 1D
 *   argument, the same blind spot as companion absorption's own geometric
 *   checks) - see 'lift_item_dimensions_applies()'.
 *
 * - Trimming negative-profit item types (see
 *   'remove_negative_profit_items'): only for 'Knapsack' - for every other
 *   objective every item type is mandatory regardless of its own profit
 *   (see the "Companion absorption" paragraph above), so a negative
 *   profit changes nothing about whether it must be packed. For
 *   'Knapsack', copies beyond an item type's own 'copies_min' are
 *   optional, and a negative-profit copy is never worth choosing to place
 *   on its own merits - it only ever lowers the objective, and freeing
 *   its footprint/weight/resource consumption for something else can only
 *   help or be neutral - so such copies are trimmed away outright,
 *   leaving only whatever 'copies_min' still mandates. Skipped entirely
 *   whenever any 'penalize' resource anywhere has a negative 'penalty':
 *   that is a one-time profit *bonus* the first time a bin's consumption
 *   crosses the resource's capacity (see 'Resource''s own doc comment in
 *   'algorithms/common.hpp'), so a negative-profit item could still be
 *   worth including if its own consumption helps trigger that crossing -
 *   an indirect benefit this purely per-item check cannot see. See
 *   'remove_negative_profit_items_applies()'.
 *
 * - Removing dominated item types (see 'remove_dominated_items'): also
 *   only for 'Knapsack', for the same reason as above - every other
 *   objective needs every copy of every item type packed regardless, so
 *   there is never a genuine choice between two item types to begin with.
 *   Item type A dominates item type B when A's own profit is at least
 *   B's, A's footprint fits within every orientation B's own 'oriented'
 *   flag allows (see 'item_type_fits_footprint_of', shared with
 *   'benders_decomposition.cpp''s own item-type precedence computation,
 *   which relies on the same underlying exchange argument - "swap one
 *   dominated copy for one dominating copy, which always fits in the same
 *   freed space and never lowers profit" - just for a softer purpose, a
 *   precedence hint into a relaxation, not an outright removal), A's own
 *   weight and resource consumption never exceed B's, A and B share the
 *   same 'group_id' (so the swap cannot disturb unloading order - see
 *   'items_mergeable''s own use of the same requirement), and A is
 *   eligible for every bin type B is (a superset - checked, alongside
 *   geometric fit, via 'Instance::item_type_fits_bin_type', exactly as
 *   'benders_decomposition.cpp' does). Unlike a plain precedence hint,
 *   removing B *outright* additionally needs A and B to be provably
 *   *incompatible* - unable to ever both be packed into the same bin (see
 *   'items_provably_incompatible'), derived from geometry (the same
 *   pigeonhole argument 'benders_decomposition.cpp''s own
 *   'items_incompatible' uses for its no-good cuts), weight, or resource
 *   consumption (their combined minimum possible consumption already
 *   exceeding a non-'penalize' resource's capacity). Plain dominance
 *   alone is not enough for an outright removal the way it is for a soft
 *   precedence hint: 2D packing is not an exclusive-choice problem, so A
 *   being pointwise at least as good as B does not by itself mean a
 *   solution could never still profitably use *both* - in different
 *   bins, or side by side in the same one - only provable incompatibility
 *   rules that out, turning it into a genuine either/or choice that
 *   dominance can then resolve in A's favor. Only ever applied to item
 *   types with 'copies_min' 0 (fully optional) - a partially-or-fully
 *   mandatory item type cannot simply be swapped away. See
 *   'remove_dominated_items_applies()'.
 *
 * - Removing dominated bin types (see 'compute_dominated_bin_types'): the
 *   bin-type analogue of the previous paragraph, but *not* restricted to
 *   'Knapsack' - unlike an item, a bin is never itself "optional
 *   inventory" competing with others for a role; the argument is instead
 *   about *bin choice*, which stays meaningful for 'VariableSizedBinPacking',
 *   'Feasibility' and 'Knapsack' (no optimal solution for any of them
 *   ever includes a genuinely empty bin, so preferring a strictly-as-
 *   good-or-better bin type wherever a worse one might have been used is
 *   always at least as good) - but *not* for 'BinPacking': unlike every
 *   other objective here, its bin types must be used in the exact order
 *   they are declared, each one's own copies fully exhausted before the
 *   next is ever touched (see 'rectangle::tree_search.cpp''s own
 *   'BranchingScheme' constructor, and 'onedimensional''s own
 *   'Solution::bin_type_order_feasible()', explicitly scoped to
 *   'BinPacking' alone - the order is a genuine, enforced constraint on
 *   solution validity only there). Removing a bin type would shift every
 *   later one's position in that fixed sequence, making it available
 *   *earlier* than the original problem ever allowed - not an equivalent
 *   substitution, a different problem entirely, regardless of how
 *   thoroughly one bin type dominates another. Bin type A dominates bin
 *   type B when A's own dimensions are at least as big as B's in both
 *   axes (no rotation - bins, unlike items, are never placed rotated) -
 *   the opposite direction from item dominance, where the *smaller* one
 *   wins: a bin needs to be big enough to hold anything the one it
 *   replaces could, so a smaller bin is strictly less capable, never a
 *   safe substitute - A's cost is at most B's, A's own maximum weight is
 *   at least B's, and A is eligible for every eligibility id B is (a
 *   superset, the bin-type analogue of the same requirement in "Removing
 *   dominated item types" above). Both bin types must be defect-free, have
 *   no fixed items, and not be semi-trailer trucks - each is a genuinely
 *   complex per-bin-type feature this class does not attempt to compare or
 *   reconcile across two different bin types, matching this class's
 *   existing precedent elsewhere (see e.g. 'companion_absorption_applies()').
 *   Resources get a narrower, still-sound rule instead of an outright ban:
 *   'resource_id' carries no meaning across two different bin types (it is
 *   just the next index into that one bin type's own 'resources' vector -
 *   see 'InstanceBuilder::add_bin_type_resource()'), so A's own resources,
 *   whatever they are, can never be reconciled against B's and always block
 *   dominance; but a bin type with *no* resources at all is unrestricted
 *   along every resource dimension, so B having resources A lacks is fine
 *   by itself - except when one of B's resources is 'penalize' with a
 *   negative 'penalty': a one-time profit bonus for crossing that
 *   resource's capacity (see 'Resource''s own doc comment, and the same
 *   exclusion in "Removing negative-profit items" above) that A, having no
 *   matching resource, could never replicate - losing access to it would
 *   make A strictly worse than B for some solutions, so that blocks
 *   dominance too even though A itself is otherwise unrestricted. Unlike
 *   item dominance, this needs
 *   no separate incompatibility argument - two *bins* are never
 *   simultaneously "used" the way two items can share one bin, so bin-
 *   type choice genuinely is an exclusive, one-at-a-time decision - but
 *   it does need A's own copies to be "enough" that its supply could
 *   never run out before every genuine use of B ever would have:
 *   conservatively, 'instance.number_of_items()' (no optimal solution for
 *   any of these objectives ever uses more non-empty bins than there are
 *   items to put in them, regardless of bin type mix), unlike the
 *   equivalent bound for items (see "Removing dominated item types"
 *   above), which is unsatisfiable there because it would have to cover
 *   B's own copies too - here, 'copies_A' and 'number_of_items()' are
 *   different quantities (bin copies vs. total item copies), so no such
 *   self-inclusion contradiction arises. Only ever applied to bin types
 *   with 'copies_min' 0 (fully optional) - a partially-or-fully mandatory
 *   bin type cannot simply be dropped. See
 *   'remove_dominated_bin_types_applies()'.
 *
 * - Pinning full-span items (see 'reduce_full_span_items'): Clautiaux,
 *   Carlier and Moukrim (2007, "A new exact method for the two-dimensional
 *   orthogonal packing problem", Proposition 1) show that an item type
 *   whose height (resp. width) exactly equals the bin's own can always be
 *   shifted to a corner without loss of generality - so it can be pinned
 *   there and removed, shrinking the bin by its width (resp. height) for
 *   every item still to be placed. Gated by the same
 *   'parameters.enlarge_wide_tall_items' flag as the wide/tall companion-
 *   absorption cases above, since it is that same idea's own boundary
 *   case: an item already spanning its *entire* target axis has a
 *   companion region of exactly zero area, so - unlike the general
 *   wide/tall search, which must still prove a non-degenerate strip is
 *   empty via an actual candidate scan - nothing could ever occupy it
 *   regardless of what other items exist, no search needed. Only for
 *   'Feasibility' (a single fixed bin, matching the source paper's own
 *   2OPP setting exactly - 'OpenDimensionX'/'OpenDimensionY' would need a
 *   different mechanism, adding the pinned dimension back onto the open-
 *   dimension bound instead of shrinking a fixed one, not yet implemented
 *   here) with exactly one bin type *and* exactly one bin copy (unlike
 *   every dedicated-bin case above, which can freely claim any one of
 *   several available copies, this operation shrinks *the* bin directly,
 *   only ever unambiguous with exactly one instance of it to shrink), plus
 *   every one of companion absorption's own exclusions (defects, weight,
 *   resources, unloading constraint) for the identical reason: the pinned
 *   item is still hidden from the reduced instance entirely, only
 *   reinserted once the downstream solve has already finished, so none of
 *   those whole-bin arguments are any safer here than they are there. Runs
 *   last in each round (after full-bin items and perfect pairs), so a
 *   plain, no-op-for-anything-else pin never preempts either of those
 *   strictly more valuable reductions for the same item type earlier in
 *   the very same round. See 'reduce_full_span_items_applies'.
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

    /**
     * 'true' iff lifting item dimensions via subset sum (see
     * 'lift_item_dimensions') is meaningful for 'instance' at all,
     * regardless of axis - the class-level doc comment's "Lifting item
     * dimensions via subset sum" paragraph: only for instances with a
     * single bin type (the bin's own true dimension must be unambiguous -
     * like companion absorption's own precondition) and no defects (a
     * defect sitting inside the claimed "provably empty" margin would be
     * invisible to this purely 1D argument). Unlike
     * 'companion_absorption_applies', not restricted by weight, resources,
     * or unloading constraint - see the class-level doc comment for why
     * none of those apply here. Objective restrictions are per-axis, not
     * covered here - see 'lift_item_dimensions_applies_axis()'. Does not
     * check 'parameters.reduce' - the constructor only calls this after
     * already checking it.
     */
    static bool lift_item_dimensions_applies(const Instance& instance);

    /**
     * 'true' iff lifting item dimensions (see 'lift_item_dimensions') is
     * sound on the given axis ('width_axis' true for 'x', false for 'y')
     * for 'instance' - the class-level doc comment's "Lifting item
     * dimensions via subset sum" paragraph's own exception: 'false' when
     * the objective directly measures how far items extend along this
     * axis, i.e. 'Objective::OpenDimensionX' (for the 'x' axis) /
     * 'OpenDimensionY' (for 'y'), or 'Objective::BinPackingWithLeftovers'
     * with a 'LeftoverMode' that depends on this axis ('LeftoverMode::X'
     * for 'x', 'LeftoverMode::Y' for 'y', 'LeftoverMode::Area' for
     * either). Independent of 'lift_item_dimensions_applies()' - both
     * must hold for lifting to actually run on a given axis (see
     * 'lift_item_dimensions').
     */
    static bool lift_item_dimensions_applies_axis(
            const Instance& instance,
            bool width_axis);

    /**
     * 'true' iff trimming negative-profit item types (see
     * 'remove_negative_profit_items') is meaningful for 'instance' - the
     * class-level doc comment's "Trimming negative-profit item types"
     * paragraph: only 'Knapsack', and only when no 'penalize' resource
     * anywhere has a negative 'penalty'. Does not check 'parameters.reduce'
     * - the constructor only calls this after already checking it.
     */
    static bool remove_negative_profit_items_applies(const Instance& instance);

    /**
     * 'true' iff removing dominated item types (see
     * 'remove_dominated_items') is meaningful for 'instance' - the
     * class-level doc comment's "Removing dominated item types" paragraph:
     * only 'Knapsack'. Does not check 'parameters.reduce' - the
     * constructor only calls this after already checking it.
     */
    static bool remove_dominated_items_applies(const Instance& instance);

    /**
     * 'true' iff removing dominated bin types (see
     * 'compute_dominated_bin_types') is meaningful for 'instance' - the
     * class-level doc comment's "Removing dominated bin types" paragraph:
     * only when there is more than one bin type (nothing to compare
     * otherwise), and never for 'BinPacking' (its bin types must be used
     * in declared order - a hard solution-validity constraint this
     * operation would break by shifting later bin types' positions in
     * that fixed sequence). Unlike every other operation in this class,
     * not restricted to a *specific* objective - it excludes exactly one
     * ('BinPacking') rather than allowing exactly one or a few. Does not
     * check 'parameters.reduce' - the constructor only calls this after
     * already checking it.
     */
    static bool remove_dominated_bin_types_applies(const Instance& instance);

    /**
     * 'true' iff pinning full-span items (see 'reduce_full_span_items')
     * is meaningful for 'instance' - the class-level doc comment's
     * "Pinning full-span items" paragraph: only 'Feasibility'
     * ('OpenDimensionX'/'OpenDimensionY' need a materially different
     * mechanism - adding the pinned width/height back onto the open-
     * dimension bound afterwards, rather than shrinking a fixed bin - not
     * yet implemented here), and, on top of every one of
     * 'companion_absorption_applies''s own exclusions (defects, weight,
     * resources, unloading constraint, single bin type - for the exact
     * same reasons: shrinking the bin still removes this item type from
     * the reduced instance entirely, only reinserting it once the
     * downstream solve has already finished, so none of those whole-bin
     * arguments are any safer here than for companion absorption itself),
     * exactly one bin *copy*: unlike companion absorption's own dedicated
     * bins (which can freely claim any one of several available bin
     * copies), this operation shrinks *the* bin directly, which is only
     * ever unambiguous when there is exactly one bin instance to shrink -
     * matching the source paper's own setting (a single fixed bin, not a
     * bin-packing problem with several copies of it). Does not check
     * 'parameters.reduce' - the constructor only calls this after already
     * checking it.
     */
    static bool reduce_full_span_items_applies(const Instance& instance);

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
     * A single copy of an item type pinned to a corner and removed by
     * 'reduce_full_span_items' because it was found to span one whole
     * *current* axis of the single bin (its height equal to the bin's
     * current true height, or its width equal to the bin's current true
     * width) - Clautiaux, Carlier and Moukrim (2007, "A new exact method
     * for the two-dimensional orthogonal packing problem", Proposition 1)
     * show such an item can always be moved to a corner without loss of
     * generality, so the bin can be shrunk by exactly its matched
     * dimension for every item still to be placed. Unlike 'FullBinItem'
     * (which needs *both* axes to match, and is set aside in its own
     * dedicated bin, since nothing else could ever share a bin with it
     * regardless), a full-span item only ever claims one axis: the *other*
     * axis of the bin still has room for every other surviving item type,
     * so this item must stay in the *same* single bin as everything else
     * - recorded here with its own fixed absolute position instead, for
     * 'unreduce_solution' to place directly into the reduced solution's
     * one bin. See 'reduce_full_span_items_applies' for why this never
     * needs to consider more than one bin instance.
     */
    struct FullSpanItem
    {
        /** Item type id (original instance's id space). */
        ItemTypeId item_type_id;

        /** Bottom-left corner, in the reduced instance's single bin's own coordinate system. */
        Point bl_corner;

        /** Whether the item is rotated (only possible if 'oriented' is false and the rotated form is what matched). */
        bool rotate;

        /**
         * This copy's own companions, captured (moved out of the working
         * 'ReductionItemType::companions_by_copy') at the moment it was
         * pinned here - empty if the item type was never enlarged. See
         * 'FullBinItem::companions_by_copy' for the identical reasoning.
         */
        std::vector<CompanionItem> companions;
    };

    /**
     * 'number_of_dedicated_bins()' plus, if 'full_span_items_' is
     * non-empty, one more: the *one* bin instance its pins implicitly
     * share with the regular reduced-instance solve (see
     * 'reduce_full_span_items_applies' - only ever meaningful with
     * exactly one bin copy in the first place). That shared bin is not
     * itself one of 'number_of_dedicated_bins()''s own "outside the
     * reduced instance" bins (see its own doc comment) - it is the same
     * bin 'instance()''s own, possibly-shrunk bin type represents - but it
     * is still unavailable for 'reduce_full_bin_items'/
     * 'reduce_perfect_pairs'/'try_reduce_both_group' to separately claim
     * as one of *their* dedicated bins: doing so would silently need two
     * bin instances (one dedicated, one shared) from a pool 'reduce_full_
     * span_items_applies' guarantees has only one. Used in place of a bare
     * 'number_of_dedicated_bins()' at every one of those three functions'
     * own cap checks - not at 'reduction_to_instance''s own bin-copies
     * subtraction for the regular solve, which must stay exactly
     * 'number_of_dedicated_bins()' there: the regular solve's own single
     * remaining bin copy is exactly the shrunk bin the full-span pins
     * already share, not a bin it additionally needs reserved for it.
     */
    BinPos reserved_bin_count() const;

    /**
     * For every not yet removed item type with at least one copy whose
     * *current* dimensions (in some allowed orientation) exactly span
     * 'true_bin_rect_''s current height or width, pins one such copy to
     * the bin's current origin, records it in 'full_span_items_', and
     * shrinks 'true_bin_rect_'/advances 'true_bin_origin_' along the
     * matched axis by that copy's own dimension along it - repeating
     * (across item types, and across multiple copies of the same one)
     * until a full pass finds nothing new, since shrinking the bin on one
     * axis can newly make some other item type's *other* axis match too
     * (Proposition 1's own "if there exists such an item" framing, applied
     * iteratively).
     *
     * "Current dimensions" deliberately includes an item type already
     * grown by 'lift_item_dimensions' or companion absorption earlier the
     * same round, not just an item still at its own original size:
     * whatever either one added beyond the item's true footprint is
     * margin already proven permanently unusable by anything else
     * remaining (lifting's own subset-sum argument, or a companion-bin
     * check's own real, placed companions), so consuming the whole grown
     * span from 'true_bin_rect_' changes nothing about what could ever
     * still fit elsewhere - it was never truly available. A companion-
     * enlarged copy's real companions travel with it via
     * 'FullSpanItem::companions' and are placed relative to wherever this
     * pin ends up, the same way 'place_item_and_companions' already
     * handles every other case in this class.
     *
     * Unlike every other operation in this class,
     * 'true_bin_rect_'/'true_bin_origin_' are this bin's own *true*
     * current dimensions/origin - not the heuristic, non-physical 'bin_w'/
     * 'bin_h' shrunk bound the rest of this class reasons against (see
     * 'compute_shrunk_bin_sizes') - because this is the one operation that
     * actually changes the reduced instance's own real bin, rather than
     * only using a tighter bound internally. Returns 'true' iff at least
     * one copy was pinned this call.
     *
     * Gated by the same 'parameters.enlarge_wide_tall_items' flag as the
     * wide/tall companion-absorption cases (see 'reduce_group'): this is
     * that same idea's own boundary case - an item already occupying its
     * *entire* target axis, with a companion region of exactly zero area,
     * needs no companion search or feasibility solve at all (unlike
     * 'try_reduce_group', whose own degenerate-companion-bin exclusion
     * exists so a *pure no-op* enlargement never preempts a more valuable
     * 'reduce_full_bin_items'/'reduce_perfect_pairs' match found later in
     * the same round - not a concern here, since this function only runs
     * after both of those have already had their turn this round, and
     * removing this item's own axis - rather than merely marking it
     * "enlarged" in place - is never a no-op). It cannot share
     * 'try_reduce_group''s own code directly: enlarging grows an
     * *item*'s footprint in place and hides its (searched-for) companions,
     * which stay valid to reinsert into whichever bin the enlarged item
     * ends up in; this instead shrinks the *bin* itself and fixes an
     * absolute origin, which only ever makes sense with exactly one bin
     * instance to shrink (see 'reduce_full_span_items_applies').
     */
    bool reduce_full_span_items(
            std::vector<ReductionItemType>& reduction_item_types);

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
     * Trims every negative-profit item type's own 'copies' down to its
     * 'effective_copies_min' (marking it 'removed' outright when that is
     * 0) - see the class-level doc comment's "Trimming negative-profit
     * item types" paragraph for why this is sound. Run once, upfront (not
     * part of the main per-round loop): an item type's profit never
     * changes, so there is nothing to re-check across rounds. Returns
     * 'true' iff at least one item type was trimmed or removed.
     */
    bool remove_negative_profit_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff item type 'item_type_id_a' dominates item type
     * 'item_type_id_b' - see the class-level doc comment's "Removing
     * dominated item types" paragraph for the exact criteria and the
     * underlying exchange argument. Always reads original-instance
     * properties (profit, 'rect', weight, group, eligibility, resource
     * consumption) directly from 'original_instance_', never from
     * 'reduction_item_types' - unlike 'items_mergeable', which compares
     * *current* (possibly already-lifted) 'rect' because a merge only
     * needs the two survivors to behave identically going forward,
     * dominance instead needs a fact that is true of the *original*
     * items regardless of what any other operation in this class has
     * since done to their working 'rect' (a lifted item's enlarged
     * declared footprint does not correspond to any real placement
     * argument - see 'lift_item_dimensions''s own doc comment - so basing
     * a removal on it would not be justified).
     *
     * The resource comparison is deliberately conservative: it compares
     * the *maximum* value anywhere in A's own consumption schedule
     * against the *minimum* value anywhere in B's, for each bin
     * type/resource, rather than trying to align schedules index-by-index
     * (copies of A and B are not generally interchangeable one-for-one at
     * the same copy index) - this can only ever miss a genuinely sound
     * removal, never wrongly permit an unsound one.
     */
    bool item_type_dominates(
            ItemTypeId item_type_id_a,
            ItemTypeId item_type_id_b) const;

    /**
     * 'true' iff item types 'item_type_id_1' and 'item_type_id_2'
     * (possibly the same one twice) provably cannot both be packed into
     * any single bin: 'true' whenever they are pairwise incompatible (see
     * the free function of the same purpose in 'reduction.cpp') in
     * *every* bin type either one could ever be placed in (a bin type
     * neither is eligible for is irrelevant; a bin type only one is
     * eligible for already keeps them apart there on its own). Used by
     * 'remove_dominated_items' to justify an outright removal: dominance
     * alone is not enough on its own - see that method's own doc comment
     * for why.
     */
    bool items_provably_incompatible(
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2) const;

    /**
     * Marks item type B 'removed' whenever some other, not-yet-'removed'
     * item type A both 'item_type_dominates' it and is
     * 'items_provably_incompatible' with it - see the class-level doc
     * comment's "Removing dominated item types" paragraph for why *both*
     * are needed together: dominance alone only says A is never a worse
     * choice than B *for a single shared role* - it says nothing about
     * whether a solution could still profitably use *both*, in different
     * bins or side by side in the same one, since 2D packing is not an
     * exclusive-choice problem the way substituting one item for another
     * in a 1D knapsack is. Provable incompatibility closes that gap: it
     * guarantees A and B can never both appear in the same bin at all, so
     * wherever a solution would have used B, only A could have been used
     * instead anyway - a genuine either/or choice, which dominance alone
     * then correctly resolves in A's favor. Only ever considers item
     * types with 'copies_min' 0 (fully optional) as removal candidates -
     * a partially-or-fully mandatory item type cannot simply be dropped.
     * A dominator's own copies are never adjusted (unlike
     * 'merge_identical_items', nothing is folded into it). Naturally
     * self-resolves a fully-tied pair (A and B mutually dominating and
     * incompatible with each other) without removing both: whichever one
     * is checked first, while the other is still available as its own
     * dominator, gets removed, after which the survivor's own now-removed
     * former dominator no longer qualifies as one. Run once, upfront (not
     * part of the main per-round loop) alongside
     * 'remove_negative_profit_items', for the same reason: nothing this
     * method reads about an item type ever changes across rounds. Returns
     * 'true' iff at least one item type was removed.
     */
    bool remove_dominated_items(
            std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff bin type 'bin_type_id_a' dominates bin type
     * 'bin_type_id_b' - see the class-level doc comment's "Removing
     * dominated bin types" paragraph for the exact criteria. Always reads
     * 'original_instance_' directly (bin types are never mutated by
     * anything else in this class, unlike item types' own working
     * 'rect').
     */
    bool bin_type_dominates(
            BinTypeId bin_type_id_a,
            BinTypeId bin_type_id_b) const;

    /**
     * For every bin type, 'true' iff it is dominated (see
     * 'bin_type_dominates') by some other bin type with "enough" copies -
     * see the class-level doc comment's "Removing dominated bin types"
     * paragraph for the exact "enough copies" bound and why, unlike the
     * equivalent bound for items, it is actually satisfiable here. Only
     * ever considers bin types with 'copies_min' 0 (fully optional) as
     * removal candidates. Naturally self-resolves a fully-tied pair (A
     * and B mutually dominating each other) without marking both
     * dominated, the same way 'remove_dominated_items' does: whichever
     * one is checked first, while the other is not yet marked dominated,
     * gets marked, after which the survivor's own now-dominated former
     * dominator no longer qualifies as one. Returned as a plain
     * 'bool' vector (indexed by 'original_instance_''s own bin type ids)
     * rather than mutating a working representation, unlike every
     * per-item operation above - bin types need no per-round working
     * state at all (nothing else in this class ever changes a bin type's
     * own properties the way 'lift_item_dimensions' changes an item
     * type's 'rect'), so 'reduction_to_instance' is the only consumer,
     * and it needs a simple removed/not-removed answer per bin type, not
     * an evolving one.
     */
    std::vector<bool> compute_dominated_bin_types() const;

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
     * 'reduction_to_instance' both rely on.
     *
     * For 'Knapsack', this formula does not apply: companion absorption
     * never runs there (so a shrunk 'copies' can only mean
     * 'remove_negative_profit_items' trimmed away copies that were simply
     * never placed at all, not consumed elsewhere - unlike the
     * subtraction above assumes), so this instead returns
     * 'min(original.copies_min, reduction_item_types[item_type_id].copies)'
     * - the requirement stays exactly 'original.copies_min', only capped
     * (never actually tightened in practice, since that trim only ever
     * stops exactly at 'copies_min' itself) so it can never exceed what
     * genuinely remains.
     */
    ItemPos effective_copies_min(
            const std::vector<ReductionItemType>& reduction_item_types,
            ItemTypeId item_type_id) const;

    /**
     * Build the final reduced 'Instance' from the working representation.
     * 'bin_type_removed', indexed by 'original_instance_''s own bin type
     * ids, marks which bin types 'compute_dominated_bin_types' found
     * dominated (an empty vector, the default, means none - every caller
     * that does not run that operation passes this default rather than a
     * same-size all-'false' vector).
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types,
            const std::vector<bool>& bin_type_removed = {});

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
     *
     * 'excluded_item_type_id' (default '-1', meaning "none") leaves one
     * copy of that item type's own group out of the candidate pool -
     * used by 'lift_item_dimensions_axis' to ask "what could the *other*
     * items contribute alongside a single occurrence of this one",
     * without otherwise duplicating this whole function. Excluding only a
     * single copy (not the whole item type) is deliberately conservative
     * when the item type has further copies of its own: those remaining
     * copies stay in the pool and so may still be combined into the
     * bound, which can only ever make it *larger* (never smaller) than
     * the true per-row achievable amount, since realistically each of the
     * item's own copies needs a row of its own and could not
     * simultaneously also be part of another row's own companions - a
     * safe direction, matching every other bound in this class ("sound,
     * just less effective" - see e.g. 'reduce_perfect_pairs''s own doc
     * comment for the same pattern elsewhere).
     */
    static Length max_achievable_dimension_sum(
            const std::vector<ReductionItemType>& reduction_item_types,
            const Instance& original_instance,
            Length capacity,
            bool width_axis,
            ItemTypeId excluded_item_type_id = -1);

    /**
     * Companionless per-item, per-axis enlargement via a 1D multiple-
     * choice subset-sum bound - see the class-level doc comment's
     * "Lifting item dimensions via subset sum" paragraph for the general
     * argument. For every not-yet-'removed' item type and axis: computes,
     * via 'max_achievable_dimension_sum' (leaving one copy of the item
     * type itself out of the pool), the maximum combination every *other*
     * not-yet-'removed' item type could contribute not exceeding the
     * bin's *shrunk* dimension on that axis ('bin_w'/'bin_h', from this
     * same round's own 'compute_shrunk_bin_sizes' call - see the
     * constructor's own comment for why shrinking runs first and this
     * uses its result rather than the bin's true dimension) minus the
     * item's own current dimension there ('capacity'); if that falls
     * short of 'capacity', grows the item's own dimension by exactly the
     * shortfall, closing the gap precisely (never further - see this
     * method's own doc comment in 'reduction.cpp' for why growing it any
     * further would not be provably safe).
     *
     * Skips an item type entirely (on the axis where this would apply)
     * when either the item type itself, or any *other* not-yet-'removed'
     * item type, has infinite ('< 0') copies: an infinite-copies item
     * type could always be repeated as many times as needed to help fill
     * any finite capacity, so 'max_achievable_dimension_sum' could not
     * soundly bound what it might contribute (its own single-choice
     * "leave one out" adjustment already handles finite copies safely -
     * see its own doc comment - but does not extend to the infinite
     * case), and the item type itself having infinite copies makes "grow
     * this one item type's dimension" a poor fit regardless (every copy
     * would grow identically, so an unbounded number of enlarged copies
     * could end up claiming unboundedly more of the bin than any single
     * occurrence ever needed to prove empty).
     *
     * Also requires the item type being grown to be 'oriented': directly
     * growing 'rect.x'/'rect.y' only means anything if the item is fixed
     * at that declared orientation - see this method's own doc comment in
     * 'reduction.cpp' for the concrete corruption a non-oriented item
     * would suffer otherwise. Same precondition 'is_big' already requires
     * for 'Wide'/'Tall'.
     *
     * Also requires the item type to have exactly 1 remaining copy: since
     * growing 'rect.x'/'rect.y' applies uniformly to every copy of the
     * type at once, but 'max_achievable_dimension_sum''s "leave one out"
     * adjustment only proves the bound for a *single* occurrence, several
     * copies would each need their own row/column with their own claim on
     * the same limited pool of other items - one shared bound cannot back
     * every copy simultaneously (see this method's own doc comment in
     * 'reduction.cpp' for a concrete counterexample).
     *
     * Each axis is independently gated by 'lift_item_dimensions_applies_axis'
     * first - an objective that measures reach along that axis directly
     * (e.g. 'OpenDimensionX') must never have items lifted on it, even
     * when lifting is otherwise sound (see that method's own doc comment,
     * and the class-level "Lifting item dimensions via subset sum"
     * paragraph, for why).
     *
     * Returns 'true' iff at least one item type was enlarged, on either
     * axis.
     */
    bool lift_item_dimensions(
            std::vector<ReductionItemType>& reduction_item_types,
            Length bin_w,
            Length bin_h) const;

    /**
     * Single-axis half of 'lift_item_dimensions' (see there for the
     * general argument and the infinite-copies exclusion). 'width_axis'
     * selects which of 'rect.x'/'rect.y' is being grown; 'bin_dimension'
     * is the bin's own *shrunk* width or height ('bin_w'/'bin_h', from
     * this round's own 'compute_shrunk_bin_sizes' call), matching
     * whichever axis is selected. Returns 'true' iff at least one item
     * type was enlarged on this axis.
     */
    bool lift_item_dimensions_axis(
            std::vector<ReductionItemType>& reduction_item_types,
            Length bin_dimension,
            bool width_axis) const;

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
     * "Full span item" reservations found while building 'instance_' (see
     * 'reduce_full_span_items'/'FullSpanItem'). Indexed by discovery
     * order, not by item type id.
     */
    std::vector<FullSpanItem> full_span_items_;

    /**
     * The single bin's own *true* current dimensions/origin (in the
     * *original* instance's own coordinate system), updated in place by
     * 'reduce_full_span_items' every time it pins and removes a copy -
     * unlike every other bound this class computes (e.g. 'bin_w'/'bin_h'
     * from 'compute_shrunk_bin_sizes'), this is not a heuristic used only
     * internally: it is the real dimensions the reduced instance's own
     * single bin type is built with, and the real origin every
     * 'FullSpanItem::bl_corner' is expressed against.
     * Initialized from the original instance's own single bin type
     * whenever 'reduce_full_span_items_applies' holds (untouched, and
     * never read, otherwise).
     */
    Rectangle true_bin_rect_{0, 0};

    /** See 'true_bin_rect_'. */
    Point true_bin_origin_{0, 0};

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
     * For each bin type of the reduced instance, the original instance's
     * own bin type id it corresponds to (the bin-type analogue of
     * 'reduced_copy_origins_', just without any per-copy structure to
     * track - a bin type, unlike an item type, is never split, merged, or
     * enlarged, so a single id per entry is enough). Identity (entry 'i'
     * equals 'i') unless 'compute_dominated_bin_types' removed some bin
     * types, in which case the reduced instance's own bin type ids are
     * simply original ids with every dominated one skipped, in order.
     * Indexed by the reduced instance's own bin type ids: populated once,
     * alongside 'reduced_copy_origins_' (see 'reduction_to_instance'), and
     * used by 'unreduce_solution' to translate a solved solution's own
     * bin type ids back to 'original_instance_''s id space.
     */
    std::vector<BinTypeId> reduced_to_original_bin_type_id_;

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
