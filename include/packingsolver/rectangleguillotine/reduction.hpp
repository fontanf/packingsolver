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
};

/**
 * A two-operation subset of 'rectangle::Reduction' (see there for the full,
 * six-operation version this mirrors): a negative-profit item trim (see
 * 'remove_negative_profit_items') and an identical-item-type merge (see
 * 'merge_identical_items'). The other four rectangle operations (companion
 * absorption, subset-sum-based item dimension lifting, dominated-item
 * removal, dominated-bin-type removal) all reason about geometry in ways
 * that assume a solution is a free 2D placement - not obviously true of a
 * guillotine cut tree, whose stage structure and cut-count/cut-distance
 * constraints these two operations never touch - so they are deliberately
 * out of scope here.
 *
 * Both operations run for any objective except where noted, with no
 * shared precondition beyond 'parameters.reduce' - see
 * 'remove_negative_profit_items_applies()' and 'items_mergeable()' for
 * their own, independent preconditions.
 *
 * Only ever needs one pass (unlike 'rectangle::Reduction''s fixpoint
 * loop): neither operation here ever changes an item type's own
 * dimensions or exposes a new merge/trim opportunity for the other -
 * trimming a negative-profit item type down to its 'copies_min' can only
 * ever narrow a later merge candidate's mandatory-vs-optional status
 * (never flip a type that was already fully mandatory or fully optional
 * into the other category - see 'effective_copies_min'), and merging
 * never changes any type's own profit or copies_min.
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
     * Working representation of an item type during the reduction
     * process. Unlike 'rectangle::Reduction::ReductionItemType', carries
     * no 'rect' or companion bookkeeping - neither operation here ever
     * changes an item type's own dimensions or hides another item type
     * behind it.
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
     * Trims away copies of negative-profit item types beyond their own
     * 'copies_min' (see 'ReductionParameters::remove_negative_profit_items'
     * and 'remove_negative_profit_items_applies'). Mirrors
     * 'rectangle::Reduction::remove_negative_profit_items'.
     */
    void remove_negative_profit_items(
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
     * types must be alone in their own stack
     * ('instance.stack_size(stack_id) == 1'). A stack is a sequence of
     * item types that must be cut/produced in a fixed order
     * ('ItemType::stack_pos'); a stack shared with other item types has
     * an ordering a merge could silently disturb (which of two
     * interchangeable-but-distinct types ends up at which stack
     * position is no longer arbitrary once other, non-interchangeable
     * types share the same stack). Restricting merges to item types that
     * are the sole occupant of their own stack sidesteps this entirely:
     * there is nothing else in the stack whose order could be disturbed,
     * and folding two singleton stacks into one (the survivor keeps its
     * own original 'stack_id', just with more copies) leaves each
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
     * populating 'reduced_copy_origins_' along the way. Mirrors
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
     * Where one particular copy of a reduced instance item type came
     * from: the corresponding item type id in the *original* instance,
     * and that original item type's own local copy index. Mirrors
     * 'rectangle::Reduction::CopyOrigin'.
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
     * 'CopyOrigin'). Every entry names the same original item type id
     * unless that reduced item type absorbed one or more others via
     * 'merge_identical_items', in which case its copies are the
     * concatenation, in order, of every merged-together original item
     * type's own copy range - any consistent order works, since merged
     * item types are interchangeable by construction (see
     * 'items_mergeable'). Indexed by the reduced instance's own item type
     * ids: populated once, by 'reduction_to_instance'. Mirrors
     * 'rectangle::Reduction::reduced_copy_origins_'.
     */
    std::vector<std::vector<CopyOrigin>> reduced_copy_origins_;

};

}
}
