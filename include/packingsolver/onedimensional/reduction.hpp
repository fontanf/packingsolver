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

    /**
     * Enable/disable the full-bin-item dedicated-bin sub-operation (gates
     * 'Reduction::reduce_full_bin_items'). Still subject to
     * 'Reduction::full_bin_reduction_applies' regardless of this flag.
     */
    bool reduce_full_bin_items = true;

    /**
     * Enable/disable the perfect-pair dedicated-bin sub-operation (gates
     * 'Reduction::reduce_perfect_pairs'). Still subject to
     * 'Reduction::full_bin_reduction_applies' regardless of this flag.
     */
    bool reduce_perfect_pairs = true;

    /**
     * Enable/disable the Martello-Toth dominant-set dedicated-bin
     * sub-operation (gates 'Reduction::reduce_dominant_sets'). Still
     * subject to 'Reduction::full_bin_reduction_applies' regardless of this
     * flag.
     */
    bool reduce_dominant_sets = true;

    /**
     * Enable/disable growing an eligible item type's own declared length to
     * close a leftover margin no other item type could ever fill (gates
     * 'Reduction::lift_item_lengths'). Still subject to
     * 'Reduction::lift_item_lengths_applies' regardless of this flag.
     */
    bool lift_item_lengths = true;

    /**
     * Enable/disable reasoning against the shrunk bin length (see
     * 'Reduction::shrunk_bin_length') instead of the bin's true one, in
     * 'Reduction::reduce_full_bin_items', 'Reduction::reduce_perfect_pairs',
     * 'Reduction::reduce_dominant_sets' and 'Reduction::lift_item_lengths'.
     */
    bool shrink_bin = true;

    /**
     * Enable/disable removing dominated bin types (gates
     * 'Reduction::compute_dominated_bin_types'). Still subject to
     * 'Reduction::remove_dominated_bin_types_applies' regardless of this
     * flag.
     */
    bool remove_dominated_bin_types = true;
};

/**
 * A deliberately small subset of rectangle's own 'Reduction' (see
 * 'packingsolver/rectangle/reduction.hpp' for the full six-operation
 * version and the detailed soundness arguments this one reuses): the two
 * operations that generalize cleanly across every problem type, with no
 * geometry-specific reasoning at all, plus the two dedicated-bin
 * reservation operations from rectangle's own "companion absorption"
 * family that carry over essentially unchanged once "footprint" becomes
 * plain length (no rotation, no companions - a 1D item has nothing else to
 * absorb) -
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
 * - Reserving a dedicated bin for a single item type whose own length
 *   exactly matches the bin's (see 'reduce_full_bin_items'), or for a pair
 *   of item types whose lengths exactly sum to it (see
 *   'reduce_perfect_pairs'): either way, the bin is provably full, so no
 *   downstream solve placing anything else can ever do better than
 *   reserving it outright - matching rectangle's own 'reduce_full_bin_items'/
 *   'reduce_perfect_pairs', minus the rotation/companion bookkeeping 1D
 *   has no use for. Only for 'BinPacking', and only when
 *   'full_bin_reduction_applies' holds: a single bin type (so the
 *   dedicated bin's own type is unambiguous), no per-bin weight capacity
 *   or resource that a hidden reservation could silently violate (see
 *   'Instance::weight_matters'/'resources_matter'), and no item type
 *   anywhere in the instance with a 'nesting_length', a finite
 *   'maximum_weight_after', or a 'maximum_stackability' that could ever
 *   bind (below the instance's total number of items). These last three
 *   apply to *every* item type, not just the reserved ones: a reservation
 *   is justified by an exchange argument that moves the *other* items of
 *   an optimal solution between bins, which is only valid if every item
 *   takes exactly its own length wherever it goes, whatever shares its
 *   bin. (E.g. bin length 10, items 6 and 4, and two items of length 7
 *   with nesting length 3: reserving the perfect pair {6, 4} leaves the
 *   two 7s needing a bin each, 3 bins in total, while {6, 7} and {4, 7}
 *   only need 2.) Like
 *   'remove_negative_profit_items'/'merge_identical_items', never touches
 *   an item type involved in a precedence, for the same reason: reserving
 *   some (or all) of its copies into a dedicated bin removes them from the
 *   reduced instance's own copy count the same way trimming does, which a
 *   precedence elsewhere (e.g. 'milp_assignment') still resolves against
 *   that original item type id. Reserved item types are entirely absent
 *   from the reduced instance - not merely reduced in 'copies' like
 *   'remove_negative_profit_items' - so 'unreduce_solution' reinstates
 *   them as their own dedicated bins, and the reduced instance's own bin
 *   type has its 'copies' folded down by however many were reserved (see
 *   'reduction_to_instance'). See 'full_bin_reduction_applies()'.
 *
 * - Reserving a dedicated bin for an item together with a *dominant* set
 *   of other items (see 'reduce_dominant_sets'): the Martello-Toth
 *   reduction procedure (Martello and Toth, "Knapsack problems:
 *   algorithms and computer implementations", 1990, section 8.4.1). A set
 *   'F' of items fitting beside item 'j' dominates another set 'T' fitting
 *   beside it when 'T' can be partitioned into subsets, each mapped to a
 *   distinct item of 'F' no shorter than the subset's total length: some
 *   optimal solution then puts exactly 'F' beside 'j' (swap each subset of
 *   'T' with its mapped item of 'F'). With 'C' the free copies fitting
 *   beside 'j', 'a' the longest of them and 'b' the longest other one
 *   fitting beside both 'j' and 'a', three sufficient rules are checked,
 *   in order:
 *   - all of 'C' fits beside 'j': 'F = C' (it contains every candidate);
 *   - 'a' exactly fills the space beside 'j', or no two copies of 'C' fit
 *     together beside 'j': 'F = {a}';
 *   - no three copies of 'C' fit together beside 'j', and no two copies of
 *     'C' longer than 'b' do either: 'F = {a, b}' (any candidate is a
 *     single copy, no longer than 'a', or a pair '{x, y}', 'x >= y', with
 *     'x <= a' and 'y <= b').
 *   Items are processed by decreasing length, and the whole pass repeats
 *   until no more bin is fixed (fixing a bin only ever shrinks 'C' for the
 *   others). Subject to the same preconditions as 'reduce_full_bin_items'/
 *   'reduce_perfect_pairs' (see 'full_bin_reduction_applies'), and every
 *   reserved item must pass 'full_bin_reduction_item_ok'. See
 *   'reduce_dominant_sets()'.
 *
 * - Growing an eligible item type's own declared length to close a
 *   leftover margin that no achievable combination of the *other* item
 *   types could ever fill (see 'lift_item_lengths'), bounded with a
 *   'subsetsumsolver' solve over the *other* item types' own lengths.
 *   Unlike the dedicated-bin reservations above, it hides nothing from
 *   the reduced instance. If no achievable combination of them can ever reach the
 *   margin left over beside this item, that margin is provably,
 *   permanently empty regardless of what else ends up sharing the bin, so
 *   the item's own declared length (in the reduced instance the
 *   downstream solve actually runs on - 'reduction_to_instance' carries
 *   it over via 'InstanceBuilder::set_item_type_length') can be grown to
 *   close it, with nothing removed or hidden the way every reservation
 *   above hides its absorbed item types; 'unreduce_solution' then places
 *   the true, original-length item back wherever the solve chose to put
 *   its grown stand-in, leaving the provably-unusable margin as genuine
 *   empty space in the final solution. Because nothing is hidden, none of
 *   the weight/resource/precedence exclusions above apply here, and this
 *   runs for *any* objective except 'BinPackingWithLeftovers' (whose own
 *   leftover-length measurement is exactly what this reduction's growth
 *   would corrupt) and 'Knapsack'. Only ever applied to an item type that is the sole
 *   occupant of its own final copy count - own 'copies' exactly '1' *and*
 *   no other item type merged into it (see 'lift_item_lengths' for why
 *   growing a type with more than one final copy would be unsound) - and
 *   only ever applied to an item type with a neutral (zero/infinite)
 *   'nesting_length'/'maximum_weight_after' itself. Every other item type
 *   still counts towards the achievable-margin bound, a nesting one at its
 *   smallest possible footprint ('length - nesting_length'), so that the
 *   bound never underestimates what could truly fill the margin. Runs *after*
 *   'merge_identical_items' - unlike the dedicated-bin reservations
 *   above, which run *before* it (see 'lift_item_lengths' for why the
 *   ordering matters here). Requires a single bin type (the bin's own
 *   length must be unambiguous) with no fixed items (a fixed item sitting
 *   inside the claimed "provably empty" margin would be invisible to this
 *   purely 1D argument, the same blind spot as the dedicated-bin
 *   reservations' own checks) - see 'lift_item_lengths_applies()'.
 *
 * - Shrinking the bin (see 'shrunk_bin_length'): rectangle's own
 *   'compute_shrunk_bin_sizes' (equation (7), "shrinking the bins"). No
 *   bin can ever hold more than the largest achievable combination of the
 *   remaining items' lengths, so every operation above that reasons
 *   against the bin's length ('reduce_full_bin_items',
 *   'reduce_perfect_pairs', 'reduce_dominant_sets', 'lift_item_lengths')
 *   uses that shrunk length instead - recomputed before each of them from
 *   the items still remaining (and at every pass of
 *   'reduce_dominant_sets'), since every reservation can only shrink it
 *   further. The reduced instance itself keeps the bin's true length.
 *   Uses the same subset-sum bound as 'lift_item_lengths' (with the same
 *   nesting-aware footprints), so it is only ever computed when one of
 *   those operations already applies, i.e. with a single bin type and no
 *   fixed items.
 *
 * - Removing dominated bin types (see 'compute_dominated_bin_types'):
 *   rectangle's own 'remove_dominated_bin_types'. Bin type A dominates bin
 *   type B when A is at least as long, at most as costly, has at least B's
 *   maximum weight, supports every eligibility id B does, has no resource
 *   (and B no negative-penalty one), and neither has fixed items. B is
 *   then left out of the reduced instance if it has no 'copies_min' and A
 *   has enough copies to replace any use of B on top of its own (the
 *   number of items plus A's own 'copies_min'): any solution using B has
 *   an equivalent-or-better one using A instead. Only for 'Knapsack',
 *   'Feasibility' and 'VariableSizedBinPacking' with more than one bin
 *   type: 'BinPacking' uses bin types in their declared order (see
 *   'Solution::bin_type_order_feasible()'), and
 *   'BinPackingWithLeftovers'/'Default' measure waste, which a longer
 *   substitute bin can only increase. 'unreduce_solution' maps the
 *   reduced instance's bin type ids back to the original ones. See
 *   'remove_dominated_bin_types_applies()'.
 *
 * For a classical knapsack instance (see 'is_classical_knapsack': a
 * single-bin 'Knapsack' instance with no constraint beyond lengths and
 * copies), only 'remove_negative_profit_items' is applied: 'optimize()'
 * solves it exactly with 'knapsacksolver::dynamic_programming_primal_dual',
 * which already performs every useful reduction itself, far more
 * efficiently. That algorithm also runs on any other single-bin knapsack,
 * but only as a relaxation (for its bound and a quick feasible solution),
 * so every applicable reduction is still applied there.
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
     * 'reduce_full_bin_items'/'reduce_perfect_pairs'/
     * 'reduce_dominant_sets'),
     * entirely absent from 'instance()' - a caller solving 'instance()'
     * needs to add this back onto any bin-count bound it finds, to
     * translate it into the original instance's own bound.
     */
    BinPos number_of_dedicated_bins() const;

    /**
     * 'true' iff the reduction alone already proves the original instance
     * infeasible: dedicated-bin reservations exhausted the single bin
     * type's own (finite) 'copies' while real items were still left over
     * to pack (only possible for 'BinPacking', whose bin copies are
     * otherwise unbounded in practice - see 'reduction_to_instance'). When
     * 'true', 'instance()' is not meaningful to solve at all.
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
     * 'true' iff 'reduce_full_bin_items'/'reduce_perfect_pairs' are
     * meaningful for 'instance' - see this class's own doc comment. Does
     * not check 'parameters.reduce' - the constructor only calls this
     * after already checking it.
     */
    static bool full_bin_reduction_applies(const Instance& instance);

    /**
     * 'true' iff 'lift_item_lengths' is meaningful for 'instance' - see
     * this class's own doc comment. Does not check 'parameters.reduce' -
     * the constructor only calls this after already checking it.
     */
    static bool lift_item_lengths_applies(const Instance& instance);

    /**
     * 'true' iff removing dominated bin types (see
     * 'compute_dominated_bin_types') is meaningful for 'instance' - see
     * this class's own doc comment. Does not check 'parameters.reduce' -
     * the constructor only calls this after already checking it.
     */
    static bool remove_dominated_bin_types_applies(const Instance& instance);

    /**
     * 'true' iff 'instance' is a single-bin 'Knapsack' instance with no
     * weight constraint, resource, fixed item, precedence, nesting length,
     * 'maximum_weight_after', binding 'maximum_stackability', 'copies_min'
     * or ineligible item type - i.e. one that
     * 'knapsacksolver::dynamic_programming_primal_dual' solves exactly, for
     * which only 'remove_negative_profit_items' is applied (see this
     * class's own doc comment).
     */
    static bool is_classical_knapsack(const Instance& instance);

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

        /**
         * Current (possibly grown) length. Starts at the original item
         * type's own length and only ever grows, via 'lift_item_lengths' -
         * every other operation reads the item type's length straight from
         * 'original_instance_' instead, since 'lift_item_lengths' is the
         * only one of them that ever needs to run after it, and it always
         * runs last (see this class's own doc comment for why).
         */
        Length length = 0;
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
     * A dedicated-bin reservation for every copy of a single item type
     * whose own length exactly matches the bin's - see
     * 'reduce_full_bin_items'.
     */
    struct FullBinItem
    {
        /** Item type id, original instance's id space. */
        ItemTypeId item_type_id;

        /** Number of dedicated bins reserved this way for this item type. */
        ItemPos copies;
    };

    /**
     * A dedicated-bin reservation for a pair of item types whose lengths
     * exactly sum to the bin's - see 'reduce_perfect_pairs'. Both item
     * types may be the same one (two of its own copies filling a bin
     * together).
     */
    struct PerfectPair
    {
        /** First item type id, original instance's id space. */
        ItemTypeId item_type_id_1;

        /** Second item type id, original instance's id space. */
        ItemTypeId item_type_id_2;

        /** Number of dedicated bins reserved this way for this pair. */
        ItemPos copies;
    };

    /**
     * 'true' iff item type 'item_type_id' is eligible for
     * 'original_instance_''s own (single, per 'full_bin_reduction_applies')
     * bin type and involved in no precedence - the two conditions common
     * to 'reduce_full_bin_items', 'reduce_perfect_pairs' and
     * 'reduce_dominant_sets' for every item they reserve (which each
     * additionally check their own length-specific conditions on top of
     * this).
     */
    bool full_bin_reduction_item_ok(
            ItemTypeId item_type_id) const;

    /**
     * Reserves a dedicated bin for every copy of every item type whose own
     * length exactly matches 'bin_length' (the (single) bin type's, shrunk
     * or not - see 'shrunk_bin_length'), up to however many
     * of that bin type's own 'copies' remain unreserved (see
     * 'number_of_dedicated_bins'). Returns 'true' iff at least one item
     * type was reserved this way.
     */
    bool reduce_full_bin_items(
            std::vector<ReductionItemType>& reduction_item_types,
            Length bin_length);

    /**
     * Reserves a dedicated bin for 'min(copies_1, copies_2)' copies of
     * every pair of item types whose lengths exactly sum to 'bin_length'
     * (see 'reduce_full_bin_items') - including a single item type paired
     * with itself, two
     * copies at a time - up to however many of that bin type's own
     * 'copies' remain unreserved. A type with more copies than its partner
     * keeps its own leftover copies as an ordinary item type in the
     * reduced instance. Returns 'true' iff at least one pair was reserved
     * this way.
     */
    bool reduce_perfect_pairs(
            std::vector<ReductionItemType>& reduction_item_types,
            Length bin_length);

    /**
     * A batch of identical dedicated-bin reservations for an item together
     * with a dominant set of other items - see 'reduce_dominant_sets'.
     */
    struct DominantSet
    {
        /**
         * Item type id of every item of the dedicated bin (one entry per
         * copy, the item it was built for first), original instance's id
         * space.
         */
        std::vector<ItemTypeId> item_type_ids;

        /** Number of dedicated bins reserved with this exact content. */
        BinPos copies;
    };

    /**
     * Looks for a set of free copies dominating every set that could share
     * a bin with one copy of 'item_type_id' (see this class's own doc
     * comment for the three rules checked). On success, returns 'true' and
     * fills 'dominant_set' with one entry per copy (not including the copy
     * of 'item_type_id' itself). 'sorted_item_type_ids' lists the item
     * types by decreasing length.
     */
    bool find_dominant_set(
            const std::vector<ReductionItemType>& reduction_item_types,
            const std::vector<ItemTypeId>& sorted_item_type_ids,
            ItemTypeId item_type_id,
            Length bin_length,
            std::vector<ItemTypeId>& dominant_set) const;

    /**
     * Martello-Toth reduction procedure: reserves a dedicated bin for each
     * item that has a dominant set (see 'find_dominant_set') together with
     * it, up to however many of the (single) bin type's own 'copies'
     * remain unreserved. Returns 'true' iff at least one dedicated bin was
     * reserved this way.
     */
    bool reduce_dominant_sets(
            std::vector<ReductionItemType>& reduction_item_types,
            bool shrink_bin);

    /**
     * Largest length achievable by summing some subset of the current
     * copies of every item type - except, if 'excluded_item_type_id' is not
     * '-1', one copy of that item type - without exceeding 'capacity'. An
     * item type with a nonzero 'nesting_length' counts at its smallest
     * possible footprint ('length - nesting_length'), so the result is
     * always an upper bound on what is truly achievable. A thin
     * wrapper around 'subsetsumsolver::dynamic_programming_bellman_word_ram'.
     */
    Length max_achievable_length_sum(
            const std::vector<ReductionItemType>& reduction_item_types,
            Length capacity,
            ItemTypeId excluded_item_type_id) const;

    /**
     * Grows the declared length of every eligible item type (own 'copies'
     * exactly '1', no other item type merged into it, neutral
     * 'nesting_length'/'maximum_weight_after') up to the largest value that
     * still leaves only a provably-unreachable margin beside it - see this
     * class's own doc comment. Returns 'true' iff at least one item type
     * was grown this way.
     */
    bool lift_item_lengths(
            std::vector<ReductionItemType>& reduction_item_types,
            Length bin_length);

    /**
     * Largest length achievable by any combination of the remaining items
     * without exceeding the (single) bin type's own length - see this
     * class's own doc comment.
     */
    Length shrunk_bin_length(
            const std::vector<ReductionItemType>& reduction_item_types) const;

    /**
     * 'true' iff bin type 'bin_type_id_a' dominates bin type
     * 'bin_type_id_b' - see this class's own doc comment.
     */
    bool bin_type_dominates(
            BinTypeId bin_type_id_a,
            BinTypeId bin_type_id_b) const;

    /**
     * For each bin type, 'true' iff it is dominated by another bin type
     * with enough copies to replace it - see this class's own doc comment.
     */
    std::vector<bool> compute_dominated_bin_types() const;

    /**
     * Builds the final reduced 'Instance' from the working representation,
     * populating 'reduced_item_origin_runs_' along the way.
     */
    Instance reduction_to_instance(
            const std::vector<ReductionItemType>& reduction_item_types,
            const std::vector<bool>& bin_type_removed);

    /*
     * Private attributes
     */

    /** The original, non-reduced instance. */
    const Instance* original_instance_;

    /** The reduced instance. */
    Instance instance_;

    /** See 'reduce_full_bin_items'. */
    std::vector<FullBinItem> full_bin_items_;

    /** See 'reduce_perfect_pairs'. */
    std::vector<PerfectPair> perfect_pairs_;

    /** See 'reduce_dominant_sets'. */
    std::vector<DominantSet> dominant_sets_;

    /**
     * Original instance's bin type id of each of 'instance_''s own bin
     * types - identity unless 'compute_dominated_bin_types' left some out.
     */
    std::vector<BinTypeId> reduced_to_original_bin_type_id_;

    /** See 'proven_infeasible()'. */
    bool proven_infeasible_ = false;

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
     * lets 'unreduce_solution' below resolve even a many-thousand-copy
     * pattern in time proportional to the number of *runs* actually
     * crossed, not to the number of copies itself.
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
