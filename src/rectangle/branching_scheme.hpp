#pragma once

#include "packingsolver/rectangle/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangle
{

/**
 * Branching scheme class for problem of type "rectangle".
 */
class BranchingScheme
{

public:

    /*
     * Sub-structures
     */

    /**
     * Structure that stores a point of the skyline.
     */
    struct UncoveredItem
    {
        /** Item type. */
        ItemTypeId item_type_id;

        /** Start x-coordiante. */
        Length xs;

        /** End x-coordinate. */
        Length xe;

        /** End x-coordinate used in dominance check. */
        Length xe_dominance;

        /** Start y-coordinate. */
        Length ys;

        /** End y-coordinate. */
        Length ye;

        bool operator==(const UncoveredItem& uncovered_item) const;
    };

    struct Insertion
    {
        /**
         * Id of the inserted item type.
         *
         * '-1' if no item is inserted.
         */
        ItemTypeId item_type_id;

        /** 'true' iff the inserted item is rotated. */
        bool rotate;

        /**
         * - < 0: the item is inserted in the last bin
         * - 0: the item is inserted in a new bin with horizontal direction
         * - 1: the item is inserted in a new bin with vertical direction
         */
        int8_t new_bin;

        /** x-coordinate of the point of interest. */
        Length x;

        /** y-coordinate of the point of interest. */
        Length y;

        bool operator==(const Insertion& insertion) const;
        bool operator!=(const Insertion& insertion) const { return !(*this == insertion); }
    };

    struct NodeGroup
    {
        /** Number of items from the group packed. */
        ItemPos number_of_items = 0;

        /**
         * Greatest x.
         *
         * This attributes is used when there is an IncreasingX/Y unloading
         * constraint.
         */
        Length x_min = 0;

        /**
         * Smallest x.
         *
         * This attributes is used when there is an IncreasingX/Y unloading
         * constraint.
         */
        Length x_max;

        /** Weight of the items from a smaller or equal group in last bin. */
        Weight last_bin_weight = 0;

        /**
         * Sum of x times weight of all items from a smaller or equal group in
         * the last bin.
         *
         * This attribute is used to compute the center of gravity of the
         * items.
         */
        Weight last_bin_weight_weighted_sum = 0;

        /** Middle axle weight. */
        double last_bin_middle_axle_weight = 0;

        /** Rear axle weight. */
        double last_bin_rear_axle_weight = 0;
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
        ItemTypeId item_type_id;

        /** 'true' iff the last inserted item had been rotated. */
        bool rotate;

        /** x-coordinates of the bottom-left corner of the last inserted item. */
        Length x;

        /** y-coordinates of the bottom-left corner of the last inserted item. */
        Length y;

        /** Last bin direction. */
        Direction last_bin_direction = Direction::X;

        /** Uncovered items. */
        std::vector<UncoveredItem> uncovered_items;

        /** For each item type, number of copies in the node. */
        std::vector<ItemPos> item_number_of_copies;

        /** Number of bins in the node. */
        BinPos number_of_bins = 0;

        /** Number of items in the node. */
        ItemPos number_of_items = 0;

        /** Item area. */
        Area item_area = 0;

        /** Item weight. */
        Weight item_weight = 0.0;

        /** Sum of weight inverses. */
        double weight_profit = 0.0;

        /** Guide item area. */
        Area guide_item_area = 0;

        /** Guide item pseudo-profit. */
        double guide_item_pseudo_profit = 0;

        /** Current area. */
        Area current_area = 0;

        /** Waste. */
        Area waste = 0;

        /** Leftover value. */
        Profit leftover_value = 0;

        /** Area used in the guides. */
        Area guide_area = 0;

        /** Maximum xe of all items in the last bin. */
        Length xe_max = 0;

        /** Maximum ye of all items in the last bin. */
        Length ye_max = 0;

        /** Maximum xs of all items in the last bin. */
        Length xs_max = 0;

        /** Profit. */
        Profit profit = 0;

        /** Guide profit. */
        Profit guide_profit = 0;

        /** Group score. */
        ItemPos group_score = 0;

        /** Groups related attributes. */
        std::vector<NodeGroup> groups;

        /** Overweight for the middle axle weight constraints. */
        double middle_axle_overweight = 0;

        /** Overweight for the rear axle weight constraints. */
        double rear_axle_overweight = 0;
    };

    struct Parameters
    {
        /** Guide. */
        GuideId guide_id = 0;

        /** Direction. */
        Direction direction = Direction::X;

        /**
         * If 'false' follow a "skyline" scheme.
         * If "true", follow a "staircase" scheme.
         */
        bool staircase = false;

        /**
         * How a predecessor is defined:
         * - 0: higher profit
         *      higher or lower weight
         *      everything else identical
         *      => This strategy is designed for the case where the axle weight
         *      constraints are not active.
         * - 1: lower weight
         *      everything else is identical
         *      => This strategy is designed for the case where the middle axle
         *      weight constraint is active. constraints.
         * - 2: higher weight
         *      everything else is identical
         *      => This strategy is designed for the case where the rear axle
         *      weight constraints is active.
         */
        int predecessor_strategy = 0;

        /**
         * How to handle groups in the guide:
         * - 0: the item area and the profit are weighted using the group id of
         *   the item. These group weighted item area and group weighted profit
         *   are used in the guides instead of the item area and the profit.
         * - 1: each node has a group score corresponding to the sum of the
         *   groups ids of the items it contains. The group score is used as
         *   first criterion lexicographically in the guide.
         *
         * Strategy 0 should work better when the bin is large, and strategy
         * 1 when the bin is narrow.
         */
        int group_guiding_strategy = 1;

        /** Part of the solution which is already fixed. */
        Solution* fixed_items = nullptr;
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
            //if (branching_scheme.unbounded_knapsck_)
            //    return true;
            return node_1->item_number_of_copies == node_2->item_number_of_copies;
        }

        inline std::size_t operator()(
                const std::shared_ptr<Node>& node) const
        {
            //if (branching_scheme.unbounded_knapsck_)
            //    return 0;
            size_t hash = 0;
            for (ItemPos s: node->item_number_of_copies)
                optimizationtools::hash_combine(hash, hasher(s));
            return hash;
        }
    };

    inline NodeHasher node_hasher() const { return NodeHasher(*this); }

    inline bool dominates(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        if (instance().objective() == Objective::SequentialOneDimensionalRectangleSubproblem) {
            if (parameters().guide_id == 8) {
                if (strictly_greater(
                            node_1->groups.front().last_bin_middle_axle_weight,
                            node_2->groups.front().last_bin_middle_axle_weight)) {
                    return false;
                }
            } else if (parameters().guide_id == 9) {
                if (strictly_greater(
                            node_1->groups.front().last_bin_rear_axle_weight,
                            node_2->groups.front().last_bin_rear_axle_weight)) {
                    return false;
                }
            }
        }

        if (node_1->number_of_bins < node_2->number_of_bins)
            return true;
        if (node_1->number_of_bins > node_2->number_of_bins)
            return false;

        //if (unbounded_knapsck_ && node_1->profit < node_2->profit)
        //    return false;
        ItemPos pos_1 = node_1->uncovered_items.size() - 1;
        ItemPos pos_2 = node_2->uncovered_items.size() - 1;
        Length x1 = node_1->uncovered_items[pos_1].xe;
        Length x2 = node_2->uncovered_items[pos_2].xe_dominance;
        for (;;) {
            if (x1 > x2)
                return false;
            // TODO handle groups.
            //if (instance().item_type(node_1->uncovered_items[pos_1].item_type_id).group_id
            //        < instance().item_type(node_2->uncovered_items[pos_2].item_type_id).group_id)
            //    return false;
            if (pos_1 == 0 && pos_2 == 0)
                return true;
            if (node_1->uncovered_items[pos_1].ys
                    == node_2->uncovered_items[pos_2].ys) {
                pos_1--;
                pos_2--;
                x1 = (parameters_.staircase)?
                    std::max(x1, node_1->uncovered_items[pos_1].xe_dominance):
                    node_1->uncovered_items[pos_1].xe_dominance;
                x2 = (parameters_.staircase)?
                    std::max(x2, node_2->uncovered_items[pos_2].xe_dominance):
                    node_2->uncovered_items[pos_2].xe_dominance;
            } else if (node_1->uncovered_items[pos_1].ys
                    < node_2->uncovered_items[pos_2].ys) {
                pos_2--;
                x2 = (parameters_.staircase)?
                    std::max(x2, node_2->uncovered_items[pos_2].xe_dominance):
                    node_2->uncovered_items[pos_2].xe_dominance;
            } else {
                pos_1--;
                x1 = (parameters_.staircase)?
                    std::max(x1, node_1->uncovered_items[pos_1].xe_dominance):
                    node_1->uncovered_items[pos_1].xe_dominance;
            }
        }

        return true;
    }

    /*
     * Outputs
     */

    std::string display(const std::shared_ptr<Node>& node) const
    {
        std::stringstream ss;
        //ss << node->waste;
        ss << node->profit;
        return ss.str();
    }

    Solution to_solution(
            const std::shared_ptr<Node>& node) const;

private:

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    Parameters parameters_;

    //bool unbounded_knapsck_ = false;

    std::vector<std::vector<ItemTypeId>> predecessors_;

    std::vector<std::vector<ItemTypeId>> predecessors_1_;

    std::vector<std::vector<ItemTypeId>> predecessors_2_;

    std::shared_ptr<Node> root_ = nullptr;

    mutable Counter node_id_ = 0;

    mutable std::vector<Insertion> insertions_;

    /*
     * Private methods
     */

    /** Insertion of one item. */
    void insertion_item(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id,
            bool rotate,
            int8_t new_bin,
            ItemPos uncovered_item_pos,
            DefectId k) const;

    /**
     * Try inserting an item at a fixed position.
     *
     * This insertion has been designed for the ROADEF/EURO 2022 challenge truck
     * loading problem. In this problem, sparse packings may be required. The
     * support constraint is defined in such a way that if a left corner of an
     * item touches a right corner of another item, then it is considered to be
     * supported.
     */
    void insertion_item_fixed(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id,
            bool rotate,
            Length xs,
            Length ys) const;

    /**
     * Check if an item type dominates another item type depending on the
     * selected predecessor strategy.
     */
    bool dominates(
            ItemTypeId item_type_id_1,
            ItemTypeId item_type_id_2);

};

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredItem& uncovered_items);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node);

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

inline bool BranchingScheme::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (parameters_.group_guiding_strategy == 1) {
        if (node_1->group_score != node_2->group_score)
            return node_1->group_score > node_2->group_score;
    }

    switch (parameters_.guide_id) {
    case 0: {
        double guide_1 = (double)node_1->guide_area / node_1->guide_item_area;
        double guide_2 = (double)node_2->guide_area / node_2->guide_item_area;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 1: {
        double guide_1 = (double)node_1->guide_area
            / node_1->guide_item_area
            / node_1->guide_item_pseudo_profit
            * node_1->number_of_items;
        double guide_2 = (double)node_2->guide_area
            / node_2->guide_item_area
            / node_2->guide_item_pseudo_profit
            * node_2->number_of_items;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 2: {
        double ye_max_1 = node_1->uncovered_items[node_1->uncovered_items.size() - 2].ye;
        double ye_max_2 = node_2->uncovered_items[node_2->uncovered_items.size() - 2].ye;
        double guide_1 = (double)(node_1->xe_max * ye_max_1) / node_1->item_area;
        double guide_2 = (double)(node_2->xe_max * ye_max_2) / node_2->item_area;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 3: {
        double ye_max_1 = node_1->uncovered_items[node_1->uncovered_items.size() - 2].ye;
        double ye_max_2 = node_2->uncovered_items[node_2->uncovered_items.size() - 2].ye;
        double guide_1 = (double)(node_1->xe_max * ye_max_1)
            / node_1->item_area
            / node_1->item_area
            * node_1->number_of_items;
        double guide_2 = (double)(node_2->xe_max * ye_max_2)
            / node_2->item_area
            / node_2->item_area
            * node_2->number_of_items;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 4: {
        double guide_1 = (double)node_1->guide_area / node_1->guide_profit;
        double guide_2 = (double)node_2->guide_area / node_2->guide_profit;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 5: {
        double guide_1 = (double)node_1->guide_area
            / node_1->guide_profit
            / node_1->item_area
            * node_1->number_of_items;
        double guide_2 = (double)node_2->guide_area
            / node_2->guide_profit
            / node_2->item_area
            * node_2->number_of_items;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 6: {
        if (node_1->waste != node_2->waste)
            return node_1->waste < node_2->waste;
        break;
    } case 7: {
        break;
    } case 8: {
        // Guide for problems where the middle axle weight constraint is critical.

        Area area_to_pack = instance().item_area();
        if (parameters().fixed_items != nullptr)
            area_to_pack += parameters().fixed_items->area() - parameters().fixed_items->item_area();
        BinPos number_of_bins = (area_to_pack - 1) / instance().bin_type(0).area() + 1;
        Area bin_area = number_of_bins * instance().bin_type(0).area();
        double load_ref = (double)area_to_pack / bin_area;
        double load_1 = (double)node_1->item_area / node_1->guide_area;
        double load_2 = (double)node_2->item_area / node_2->guide_area;
        //std::cout << "load_1 " << load_1 << " load_2 " << load_2 << " load_ref " << load_ref << std::endl;
        if (load_1 != load_2) {
            if (load_1 < load_ref && load_2 < load_ref) {
                return load_1 > load_2;
            } else if (load_1 < load_ref) {
                return false;
            } else if (load_2 < load_ref) {
                return true;
            }
        }

        double guide_1 = (double)node_1->groups.front().last_bin_middle_axle_weight;
        double guide_2 = (double)node_2->groups.front().last_bin_middle_axle_weight;
        //std::cout << "guide " << guide_1 << " " << guide_2 << std::endl;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 9: {
        // Guide for problems where the rear axle weight constraint is critical.

        Area area_to_pack = instance().item_area();
        if (parameters().fixed_items != nullptr)
            area_to_pack += parameters().fixed_items->area() - parameters().fixed_items->item_area();
        BinPos number_of_bins = (area_to_pack - 1) / instance().bin_type(0).area() + 1;
        Area bin_area = number_of_bins * instance().bin_type(0).area();
        double load_ref = (double)area_to_pack / bin_area;
        double load_1 = (double)node_1->item_area / node_1->guide_area;
        double load_2 = (double)node_2->item_area / node_2->guide_area;
        //std::cout << "load_1 " << load_1 << " load_2 " << load_2 << " load_ref " << load_ref << std::endl;
        if (load_1 != load_2) {
            if (load_1 < load_ref && load_2 < load_ref) {
                return load_1 > load_2;
            } else if (load_1 < load_ref) {
                return false;
            } else if (load_2 < load_ref) {
                return true;
            }
        }

        double guide_1 = (double)node_1->groups.front().last_bin_weight;
        double guide_2 = (double)node_2->groups.front().last_bin_weight;
        if (guide_1 != guide_2)
            return guide_1 > guide_2;
        break;
    }
    }
    return node_1->id < node_2->id;
}

}
}
