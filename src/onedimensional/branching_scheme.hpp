#pragma once

#include "packingsolver/onedimensional/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace onedimensional
{

/**
 * Branching scheme class for problem of type "onedimensional".
 */
class BranchingScheme
{

public:

    /*
     * Sub-structures
     */

    struct Insertion
    {
        /**
         * Id of the inserted item type.
         *
         * '-1' if no item is inserted.
         */
        ItemTypeId item_type_id;

        /** 'true' iff the item is inserted in a new bin. */
        bool new_bin;

        bool operator==(const Insertion& insertion) const;
        bool operator!=(const Insertion& insertion) const { return !(*this == insertion); }
    };

    /**
     * Node structure of the branching scheme.
     */
    struct Node
    {
        /** Id of the node. */
        NodeId id = -1;

        /**
         * Pointer to the parent of the node,
         * 'nullptr' if the node is the root.
         */
        std::shared_ptr<Node> parent = nullptr;

        /** Last inserted item type. */
        ItemTypeId item_type_id = -1;

        /** For each item type, number of copies in the node. */
        std::vector<ItemPos> item_number_of_copies;

        /** Number of bins in the node. */
        BinPos number_of_bins = 0;

        /** Number of items in the node. */
        ItemPos number_of_items = 0;

        /** Item length. */
        Volume item_length = 0;

        /** Squared item length. */
        Volume squared_item_length = 0;

        /** Current length. */
        Volume current_length = 0;

        /** Waste. */
        Volume waste = 0;

        /** Maximum xe of all items in the last bin. */
        Length xe_max = 0;

        /** Maximum xs of all items in the last bin. */
        Length xs_max = 0;

        /** Profit. */
        Profit profit = 0;

        /** Length in the last bin. */
        Weight last_bin_length = 0;

        /** Weight of the last bin. */
        Weight last_bin_weight = 0;

        /** Number of items in the last bin. */
        ItemPos last_bin_number_of_items = 0;

        /** Maximum number of items in the last bin. */
        ItemPos last_bin_maximum_number_of_items = -1;

        /**
         * Remaining weight allowed in the last bin to satisfy the maximum
         * weight after constraint.
         */
        Weight last_bin_remaiing_weight = -1;

    };

    struct Parameters
    {
        /** Guide. */
        GuideId guide_id = 0;
    };

    /** Constructor. */
    BranchingScheme(
            const Instance& instance,
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
            const std::shared_ptr<Node>& node_2) const;

    inline bool leaf(
            const std::shared_ptr<Node>& node) const
    {
        return node->number_of_items == instance_.number_of_items();
    }

    bool bound(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    bool better(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    bool equals(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        (void)node_1;
        (void)node_2;
        return false;
    }

    /*
     * Dominances
     */

    inline bool comparable(const std::shared_ptr<Node>&) const { return true; }

    struct NodeHasher
    {
        std::hash<ItemPos> hasher;
        const BranchingScheme& branching_scheme;

        NodeHasher(const BranchingScheme& branching_scheme): branching_scheme(branching_scheme) { }

        inline bool operator()(
                const std::shared_ptr<Node>& node_1,
                const std::shared_ptr<Node>& node_2) const
        {
            return node_1->item_number_of_copies == node_2->item_number_of_copies;
        }

        inline std::size_t operator()(
                const std::shared_ptr<Node>& node) const
        {
            if (branching_scheme.instance().unbounded_knapsack())
                return 0;
            size_t hash = 0;
            for (ItemPos s: node->item_number_of_copies)
                optimizationtools::hash_combine(hash, hasher(s));
            return hash;
        }
    };

    inline NodeHasher node_hasher() const { return NodeHasher(*this); }

    bool dominates(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    /*
     * Outputs
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        ss << node->waste;
        return ss.str();
    }

    Solution to_solution(
            const std::shared_ptr<Node>& node) const;

private:

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    Parameters parameters_;

    mutable Counter node_id_ = 0;

    mutable std::vector<Insertion> insertions_;

    /*
     * Private methods
     */

    /** Get the percentage of item inserted into a node. */
    inline double item_percentage(const Node& node) const { return (double)node.number_of_items / instance_.number_of_items(); }

    /** Get the mean length of a node. */
    inline double mean_length(const Node& node) const { return (double)node.current_length / node.number_of_items; }

    /** Get the mean item length of a node; */
    inline double mean_item_length(const Node& node) const { return (double)node.item_length / node.number_of_items; }

    /** Get the mean squared item length of a node. */
    inline double mean_squared_item_length(const Node& node) const { return (double)node.squared_item_length / node.number_of_items; }

    /** Get the mean remaining item length of a node. */
    inline double mean_remaining_item_length(const Node& node) const { return (double)remaining_item_length(node) / (instance_.number_of_items() - node.number_of_items); }

    /** Get the remaining item length of a node. */
    inline double remaining_item_length(const Node& node) const { return instance_.item_length() - node.item_length; }

    /** Get the waste percentage of a node. */
    inline double waste_percentage(const Node& node) const { return (double)node.waste / node.current_length; }

    /** Get the waste ratio of a node. */
    inline double waste_ratio(const Node& node) const { return (double)node.waste / node.item_length; }

    /** Get the knapsack upper bound of a node. */
    inline Profit ubkp(const Node& node) const;

    /*
     * Insertions
     */

    /** Insertion of one item in the same bin. */
    void insertion_item_same_bin(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id) const;

    /** Insertion of an item in a new bin. */
    void insertion_item_new_bin(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id) const;

};

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node);

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Profit BranchingScheme::ubkp(const Node& node) const
{
    Area remaining_item_length = instance_.item_length() - node.item_length;
    Area remaining_length = instance_.bin_length() - node.current_length;
    if (remaining_length >= remaining_item_length) {
        return instance_.item_profit();
    } else {
        ItemTypeId item_type_id = instance_.max_efficiency_item_type_id();
        return node.profit + remaining_length
            * instance_.item_type(item_type_id).profit
            / instance_.item_type(item_type_id).length;
    }
}

inline bool BranchingScheme::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (parameters_.guide_id) {
    case 0: {
        double guide_1 = (double)node_1->current_length / node_1->item_length;
        double guide_2 = (double)node_2->current_length / node_2->item_length;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 1: {
        double guide_1 = (double)node_1->current_length
            / node_1->item_length
            / mean_item_length(*node_1);
        double guide_2 = (double)node_2->current_length
            / node_2->item_length
            / mean_item_length(*node_2);
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 2: {
        if ((0.1 + waste_percentage(*node_1)) / mean_item_length(*node_1)
                != (0.1 + waste_percentage(*node_2)) / mean_item_length(*node_2))
            return (0.1 + waste_percentage(*node_1)) / mean_item_length(*node_1)
                < (0.1 + waste_percentage(*node_2)) / mean_item_length(*node_2);
        break;
    } case 3: {
        if ((0.1 + waste_percentage(*node_1)) / mean_squared_item_length(*node_1)
                != (0.1 + waste_percentage(*node_2)) / mean_squared_item_length(*node_2))
            return (0.1 + waste_percentage(*node_1)) / mean_squared_item_length(*node_1)
                < (0.1 + waste_percentage(*node_2)) / mean_squared_item_length(*node_2);
        break;
    } case 4: {
        double guide_1 = (double)node_1->current_length / node_1->profit;
        double guide_2 = (double)node_2->current_length / node_2->profit;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 5: {
        double guide_1 = (double)node_1->current_length
            / node_1->profit
            / mean_item_length(*node_1);
        double guide_2 = (double)node_2->current_length
            / node_2->profit
            / mean_item_length(*node_2);
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 6: {
        if (node_1->waste != node_2->waste)
            return node_1->waste < node_2->waste;
        break;
    } case 7: {
        auto ub1 = ubkp(*node_1);
        auto ub2 = ubkp(*node_2);
        if (ub1 != ub2)
            return ub1 > ub2;
        break;
    } case 8: {
        auto ub1 = ubkp(*node_1);
        auto ub2 = ubkp(*node_2);
        if (ub1 != ub2)
            return ub1 > ub2;
        if (node_1->waste != node_2->waste)
            return node_1->waste < node_2->waste;
        break;
    }
    }
    return node_1->id < node_2->id;
}

}
}
