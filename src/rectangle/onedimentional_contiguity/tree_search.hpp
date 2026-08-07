/**
 * Onedimensional-contiguity tree search
 *
 * A combinatorial branch-and-bound alternative to 'milp.hpp', solving the
 * exact same BMP/LBMP relaxation (see the file-level comments there and in
 * '../benders_decomposition_contiguity.hpp'): assign every unit of every
 * item type a run of consecutive columns, subject to a per-column height
 * budget, any accumulated no-good cuts, and resource constraints - without
 * ever y-checking or otherwise reasoning about y-coordinates at all.
 *
 * Branches over units in a fixed order (by item type, then copy - the same
 * order 'build_units_and_candidates' produces): each unit is either placed
 * at one of its own (meet-in-the-middle-restricted) candidate positions, or
 * - only when the objective is 'Knapsack' - skipped. A leaf is reached once
 * every unit has been decided; a leaf is only 'valid' if every item type's
 * own 'copies_min' was reached by then (checked once, at the leaf, rather
 * than pruned proactively during branching - see 'BranchingScheme::child_tmp').
 *
 * Follows the same 'treesearchsolver'-driven, 'BranchingScheme' pattern as
 * every other tree search in this codebase (see e.g.
 * 'onedimensional::BranchingScheme'), but scoped down for this narrower use
 * case: no 'AlgorithmFormatter'/'SolutionPool'/multi-guide diversification
 * (this is an internal building block, called repeatedly - once per
 * iteration of 'benders_decomposition_contiguity''s own Benders loop - not a
 * top-level, user-facing algorithm), and no dominance-based pruning for now
 * ('comparable' always returns 'false': pruning relies on 'bound' and the
 * per-insertion height/resource/no-good-cut checks alone).
 */

#pragma once

#include "rectangle/onedimentional_contiguity/milp.hpp"

#include "optimizationtools/utils/utils.hpp"

namespace packingsolver
{
namespace rectangle
{
namespace onedimentional_contiguity
{

class BranchingScheme
{

public:

    struct Insertion
    {
        /** Unit decided by this insertion (index into the branching order). */
        size_t unit_id;

        /** 'true' iff the unit is placed (as opposed to skipped - 'Knapsack' only). */
        bool place;

        /** Candidate id the unit is placed at; only meaningful when 'place'. */
        size_t candidate_id;

        bool operator==(const Insertion& insertion) const;
        bool operator!=(const Insertion& insertion) const { return !(*this == insertion); }
    };

    struct Node
    {
        /** Id of the node. */
        NodeId id = -1;

        /** Pointer to the parent of the node, 'nullptr' if the node is the root. */
        std::shared_ptr<Node> parent = nullptr;

        /** Unit decided to reach this node. */
        size_t unit_id = 0;

        /** 'true' iff 'unit_id' was placed (as opposed to skipped). */
        bool placed = false;

        /** Candidate id 'unit_id' was placed at; only meaningful when 'placed'. */
        size_t candidate_id = 0;

        /** Number of units decided so far (placed or skipped). */
        ItemPos number_of_units_decided = 0;

        /** Number of copies of each item type selected so far. */
        std::vector<ItemPos> item_type_selected_copies;

        /** Height used so far, indexed by column. */
        std::vector<Length> h_used;

        /** Profit so far ('Knapsack' only; always 0 for 'Feasibility'). */
        double profit = 0.0;

        /** Resource consumption so far, indexed by resource_id. */
        std::vector<double> resource_consumption;

        /** Number of each no-good cut's own candidates already selected, indexed by cut id. */
        std::vector<ItemPos> cut_counts;

        /**
         * 'true' unless this is a leaf whose selected units don't reach
         * every item type's own 'copies_min' - see 'child_tmp'. Always
         * 'true' before the leaf is reached.
         */
        bool valid = true;
    };

    struct Parameters
    {
        /** Guide. */
        GuideId guide_id = 0;
    };

    /** Constructor. */
    BranchingScheme(
            const Instance& instance,
            const std::vector<NoGoodCut>& cuts,
            const Parameters& parameters);

    /** Get instance. */
    inline const Instance& instance() const { return instance_; }

    /** Get parameters. */
    inline const Parameters& parameters() const { return parameters_; }

    /*
     * Branching scheme methods
     */

    const std::vector<Insertion>& insertions(
            const std::shared_ptr<Node>& parent) const;

    Node child_tmp(
            const std::shared_ptr<Node>& parent,
            const Insertion& insertion) const;

    std::shared_ptr<Node> child(
            const std::shared_ptr<Node>& parent,
            const Insertion& insertion) const
    {
        return std::shared_ptr<Node>(new Node(child_tmp(parent, insertion)));
    }

    const std::shared_ptr<Node> root() const;

    std::vector<std::shared_ptr<Node>> children(
            const std::shared_ptr<Node>& parent) const;

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        if (node_1->number_of_units_decided != node_2->number_of_units_decided)
            return node_1->number_of_units_decided > node_2->number_of_units_decided;
        if (node_1->profit != node_2->profit)
            return node_1->profit > node_2->profit;
        // Final tiebreaker: without this, two distinct nodes that happen to
        // tie on every guide criterion above compare as "equal" under the
        // strict weak ordering 'treesearchsolver' uses to key its queue
        // (a 'std::set'), silently collapsing them into one and dropping
        // the other from the search - matches every other 'operator()' in
        // this codebase (e.g. 'rectangle::BranchingScheme::operator()').
        return node_1->id < node_2->id;
    }

    inline bool leaf(
            const std::shared_ptr<Node>& node) const
    {
        return node->number_of_units_decided == (ItemPos)units_.size();
    }

    bool bound(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    bool better(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    inline bool equals(
            const std::shared_ptr<Node>&,
            const std::shared_ptr<Node>&) const
    {
        return false;
    }

    /*
     * Dominances - disabled for now (see the file-level comment above):
     * 'comparable' always 'false', so 'dominates'/'node_hasher' are never
     * actually invoked by 'treesearchsolver', but must still exist to
     * satisfy its 'BranchingScheme' interface.
     */

    inline bool comparable(const std::shared_ptr<Node>&) const { return false; }

    struct NodeHasher
    {
        inline bool operator()(
                const std::shared_ptr<Node>&,
                const std::shared_ptr<Node>&) const
        {
            return false;
        }

        inline std::size_t operator()(
                const std::shared_ptr<Node>& node) const
        {
            return std::hash<ItemPos>()(node->number_of_units_decided);
        }
    };

    inline NodeHasher node_hasher() const { return NodeHasher(); }

    inline bool dominates(
            const std::shared_ptr<Node>&,
            const std::shared_ptr<Node>&) const
    {
        return false;
    }

    /*
     * Outputs
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        ss << node->profit;
        return ss.str();
    }

    /** Convert a (leaf) node into an 'OnedimensionalContiguityResult'. */
    OnedimensionalContiguityResult to_result(
            const std::shared_ptr<Node>& node) const;

private:

    /** Instance. */
    const Instance& instance_;

    /** No-good cuts. */
    const std::vector<NoGoodCut>& cuts_;

    /** Parameters. */
    Parameters parameters_;

    /** Bin height. */
    Length bin_height_;

    /** Units and candidates, built once from 'instance_'. */
    UnitsAndCandidates units_and_candidates_;

    /** Units, in branching order: '[unit_id] = (item_type_id, copy)'. */
    std::vector<std::pair<ItemTypeId, ItemPos>> units_;

    /**
     * For each candidate id, the ids (into 'cuts_') of every no-good cut it
     * belongs to - built once so 'insertions'/'child_tmp' don't have to
     * scan every cut for every candidate.
     */
    std::vector<std::vector<size_t>> candidate_to_cuts_;

    /**
     * Suffix sum of remaining units' own item type profit, indexed by
     * 'unit_id': 'profit_suffix_[u]' is the sum, over every unit from 'u'
     * to the last one (inclusive), of its own item type's profit - a
     * (loose) upper bound on how much profit placing every one of them
     * could still add, used by 'bound' ('Knapsack' only).
     */
    std::vector<double> profit_suffix_;

    mutable Counter node_id_ = 0;

    mutable std::vector<Insertion> insertions_;

};

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node);

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// tree_search ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/** Parameters for 'tree_search'. */
struct TreeSearchParameters: optimizationtools::Parameters
{
    /** Guide. */
    GuideId guide_id = 0;

    /**
     * Optimization mode.
     *
     * 'Anytime' (the default) progressively grows the search queue
     * (beam width), same as every other tree search in this codebase.
     * 'NotAnytimeSequential'/'NotAnytimeDeterministic' instead jump
     * straight to a fixed queue size ('not_anytime_queue_size') and run
     * it in a single pass - matching the fact that this algorithm is
     * called repeatedly, as an inner-loop building block of
     * 'benders_decomposition_contiguity''s own Benders loop, rather than
     * as a standalone, user-facing search.
     */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /**
     * Maximum size of the search queue (beam width) in
     * 'NotAnytimeSequential'/'NotAnytimeDeterministic' mode - see
     * 'optimization_mode'. Not used in 'Anytime' mode.
     */
    NodeId not_anytime_queue_size = 1024;
};

OnedimensionalContiguityResult tree_search(
        const Instance& instance,
        const std::vector<NoGoodCut>& cuts,
        const TreeSearchParameters& parameters);

}
}
}
