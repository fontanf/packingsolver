#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include "treesearchsolver/common.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

using SubplateId = int64_t;

/**
 * Branching scheme class for problem of type "rectangleguillotine" generating
 * n-staged and unlimited staged patterns.
 */
class BranchingSchemeN
{

public:

    /*
     * Sub-structures
     */

    struct Subplate
    {
        /** If initial subplate, item type id. */
        ItemTypeId item_type_id = -1;

        /** First subplate. */
        std::shared_ptr<Subplate> subplate_1 = nullptr;

        /** Second subplate. */
        std::shared_ptr<Subplate> subplate_2 = nullptr;

        /** Orientation of the new cut. */
        CutOrientation cut_orientation = CutOrientation::Vertical;

        /** Width. */
        Length width = 0;

        /** Height. */
        Length height = 0;

        /** Number of stages. */
        Counter number_of_stages = 2;
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

        /** Sub-plate. */
        std::shared_ptr<Subplate> subplate = nullptr;

        /** Number of items in the partial solution. */
        ItemPos number_of_items = 0;

        /** Total area of the items of the partial solution. */
        Area item_area = 0;

        /** Item profit. */
        Profit item_profit = 0;

        /** Bound. */
        Profit bound = std::numeric_limits<Profit>::infinity();

        /** For each item type, number of copies in the node. */
        std::vector<ItemPos> item_number_of_copies;
    };

    struct Parameters
    {
        /** Guide. */
        GuideId guide_id = 0;
    };

    /** Constructor. */
    BranchingSchemeN(
            const Instance& instance,
            const Parameters& parameters);

    /** Get instance. */
    inline const Instance& instance() const { return instance_; }

    /** Get parameters. */
    inline const Parameters& parameters() const { return parameters_; }

    /*
     * Branching scheme methods
     */

    const std::shared_ptr<Node> root() const;

    std::vector<std::shared_ptr<Node>> children(
            const std::shared_ptr<Node>& parent) const;

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    inline treesearchsolver::Depth depth(
            const std::shared_ptr<Node>& node) const
    {
        return node->number_of_items;
    }

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

    inline bool comparable(const std::shared_ptr<Node>& node) const { return true; }

    struct NodeHasher
    {
        std::hash<ItemPos> hasher;

        inline bool operator()(
                const std::shared_ptr<Node>& node_1,
                const std::shared_ptr<Node>& node_2) const
        {
            return node_1->item_number_of_copies == node_2->item_number_of_copies;
        }

        inline std::size_t operator()(
                const std::shared_ptr<Node>& node) const
        {
            size_t hash = 0;
            for (ItemPos s: node->item_number_of_copies)
                optimizationtools::hash_combine(hash, hasher(s));
            return hash;
        }
    };

    inline NodeHasher node_hasher() const { return NodeHasher(); }

    struct SubplatePoolHasher
    {
        std::hash<ItemPos> hasher;

        inline bool operator()(
                const std::vector<ItemPos>& item_number_of_copies_1,
                const std::vector<ItemPos>& item_number_of_copies_2) const
        {
            return item_number_of_copies_1 == item_number_of_copies_2;
        }

        inline std::size_t operator()(
                const std::vector<ItemPos>& item_number_of_copies) const
        {
            size_t hash = 0;
            for (ItemPos s: item_number_of_copies)
                optimizationtools::hash_combine(hash, hasher(s));
            return hash;
        }
    };

    inline bool dominates(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        return dominates(node_1->subplate, node_2->subplate);
    }

    inline bool dominates(
            const std::shared_ptr<Subplate>& subplate_1,
            const std::shared_ptr<Subplate>& subplate_2) const
    {
        return subplate_1->width <= subplate_2->width
            && subplate_1->height <= subplate_2->height;
    }

    /*
     * Outputs
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        //ss << node->waste;
        ss << node->item_profit;
        return ss.str();
    }

    Solution to_solution(
            const std::shared_ptr<Node>& node) const;

private:

    /*
     * Private attributes
     */

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    Parameters parameters_;

    using SubpalteMap = std::unordered_map<
        std::vector<ItemPos>,
        std::vector<std::shared_ptr<Subplate>>,
        SubplatePoolHasher,
        SubplatePoolHasher>;

    /** Pool of subplates. */
    mutable std::vector<SubpalteMap> subplate_pool_;

    SubplatePoolHasher subplate_pool_hasher_;

    mutable NodeId node_id_ = 0;

    /*
     * Temporary variables
     */

    /*
     * Private methods
     */

    /**
     * Return true iff s1 and s2 contains identical objects in the same order.
     */
    bool equals(StackId s1, StackId s2);

    inline bool full(const Node& subplate) const { return subplate.number_of_items == instance_.number_of_items(); }

    /** Get the area of a node. */
    inline bool area(const Node& node) const { return node.subplate->width * node.subplate->height; }

    /** Get the mean item area of a node; */
    inline double mean_item_area(const Node& node) const { return (double)node.item_area / node.number_of_items; }

    /** Get the waste of a node. */
    inline bool waste(const Node& node) const { return area(node) - node.item_area; }

    /** Get the waste percentage of a node. */
    inline double waste_percentage(const Node& node) const { return (double)waste(node) / area(node); }

    bool add_subplate_to_pool(
            const std::shared_ptr<Subplate>& subplate,
            ItemPos number_of_items,
            const std::vector<ItemPos>& item_number_of_copies) const;

    std::ostream& format(
            const Subplate& subplate,
            std::ostream& os) const;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BranchingSchemeN::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch(parameters_.guide_id) {
    case 0: {
        if (area(*node_1) == 0) {
            if (area(*node_2) != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (area(*node_2) == 0)
            return true;

        if (waste_percentage(*node_1) != waste_percentage(*node_2))
            return waste_percentage(*node_1) < waste_percentage(*node_2);
        break;
    } case 1: {
        if (area(*node_1) == 0) {
            if (area(*node_2) != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (area(*node_2) == 0)
            return true;

        if (node_1->number_of_items == 0) {
            if (node_2->number_of_items != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->number_of_items == 0)
            return true;

        if (waste_percentage(*node_1) / mean_item_area(*node_1)
                != waste_percentage(*node_2) / mean_item_area(*node_2))
            return waste_percentage(*node_1) / mean_item_area(*node_1)
                < waste_percentage(*node_2) / mean_item_area(*node_2);
        break;
    } case 4: {
        if (node_1->item_profit == 0) {
            if (node_2->item_profit != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->item_profit == 0)
            return true;

        if ((double)area(*node_1) / node_1->item_profit
                != (double)area(*node_2) / node_2->item_profit)
            return (double)area(*node_1) / node_1->item_profit
                < (double)area(*node_2) / node_2->item_profit;
        break;
    } case 5: {
        if (node_1->item_profit == 0) {
            if (node_2->item_profit != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->item_profit == 0)
            return true;

        if (node_1->number_of_items == 0) {
            if (node_2->number_of_items != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->number_of_items == 0)
            return true;

        if ((double)area(*node_1) / node_1->item_profit / mean_item_area(*node_1)
                != (double)area(*node_2) / node_2->item_profit / mean_item_area(*node_2))
            return (double)area(*node_1) / node_1->item_profit / mean_item_area(*node_1)
                < (double)area(*node_2) / node_2->item_profit / mean_item_area(*node_2);
        break;
    }
    }
    return node_1->id < node_2->id;
}

}
}
