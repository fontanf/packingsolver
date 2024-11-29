#pragma once

#include "packingsolver/boxstacks/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace boxstacks
{

/**
 * Branching scheme class for problem of type "boxstacks".
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
        /** Items. */
        std::vector<ItemTypeId> item_type_ids;

        /** Weight of the stack. */
        Weight weight = 0;

        /** Number of items in the stack. */
        ItemPos number_of_items = 0;

        /** Maximum number of items in the stack. */
        ItemPos maximum_number_of_items = -1;

        /**
         * Remaining weight allowed in the stack to satisfy the maximum weight
         * above constraint.
         */
        Weight remaiing_weight = -1;

        /** Start x-coordiante. */
        Length xs;

        /** End x-coordinate. */
        Length xe;

        /** Start y-coordinate. */
        Length ys;

        /** End y-coordinate. */
        Length ye;

        /** End z-coordinate. */
        Length ze;

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

        /** Rotation of the inserted item. */
        int rotation;

        /**
         * When added above a previous item, the position of the uncovered
         * item.
         */
        int uncovered_item_pos;

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

        /** z-coordinate of the point of interest. */
        Length z;

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

        /** Rotation of the last inserted item. */
        int rotation;

        /** x-coordinates of the bottom-left corner of the last inserted item. */
        Length x;

        /** y-coordinates of the bottom-left corner of the last inserted item. */
        Length y;

        bool new_stack;

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

        /** Number of stacked items. */
        ItemPos number_of_stacked_items = 0;

        /** Item volume. */
        Volume item_volume = 0;

        /** Item weight. */
        Weight item_weight = 0.0;

        /** Guide item volume. */
        Volume guide_item_volume = 0;

        /** Squared item volume. */
        Volume squared_item_volume = 0;

        /** Current volume. */
        Volume current_volume = 0;

        /** Waste. */
        Volume waste = 0;

        /** Volume used in the guides. */
        Volume guide_volume = 0;

        /** Maximum xe of all items in the last bin. */
        Length xe_max = 0;

        /** Maximum xs of all items in the last bin. */
        Length xs_max = 0;

        /** Profit. */
        Profit profit = 0;

        /** Guide profit. */
        Profit guide_profit = 0;

        /** Groups related attributes. */
        std::vector<NodeGroup> groups;

        /** 'true' iff weight constraints are satisfied. */
        bool weight_constraints_satisfied = true;

        /** Overweight for the middle axle weight constraints. */
        double middle_axle_overweight = 0;

        /** Overweight for the rear axle weight constraints. */
        double rear_axle_overweight = 0;

        double last_bin_middle_axle_weight = 0;

        double last_bin_rear_axle_weight = 0;

        /** Width or height. */
        Length length = 0;
    };

    struct Parameters
    {
        /** Guide. */
        GuideId guide_id = 0;

        /** Direction. */
        Direction direction = Direction::X;

        /** Maximum number of selected items. */
        ItemPos maximum_number_of_selected_items = -1;
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

    inline bool dominates(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        if (node_1->number_of_bins < node_2->number_of_bins)
            return true;
        if (node_1->number_of_bins > node_2->number_of_bins)
            return false;

        if (node_1->last_bin_middle_axle_weight > node_2->last_bin_middle_axle_weight)
            return false;
        if (node_1->uncovered_items.size() != node_2->uncovered_items.size())
            return false;
        for (ItemPos pos = 0; pos < (ItemPos)node_1->uncovered_items.size(); ++pos) {
            UncoveredItem& uncovered_item_1 = node_1->uncovered_items[pos];
            UncoveredItem& uncovered_item_2 = node_2->uncovered_items[pos];
            if (uncovered_item_1.xs != uncovered_item_2.xs
                    || uncovered_item_1.ys != uncovered_item_2.ys
                    || uncovered_item_1.xe != uncovered_item_2.xe
                    || uncovered_item_1.ye != uncovered_item_2.ye) {
                return false;
            }
            if (uncovered_item_1.ze > uncovered_item_2.ze)
                return false;
            if (uncovered_item_1.remaiing_weight < instance().item_weight()
                    && uncovered_item_1.remaiing_weight < uncovered_item_2.remaiing_weight)
                return false;
            if (uncovered_item_1.maximum_number_of_items < instance().number_of_items()
                    && uncovered_item_1.maximum_number_of_items < uncovered_item_2.maximum_number_of_items)
                return false;
            if (!uncovered_item_1.item_type_ids.empty()
                    && !uncovered_item_2.item_type_ids.empty()
                    && instance().item_type(uncovered_item_1.item_type_ids.back()).stackability_id
                    != instance().item_type(uncovered_item_2.item_type_ids.back()).stackability_id) {
                return false;
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

    std::vector<std::vector<ItemTypeId>> predecessors_;

    ItemPos number_of_selected_items_ = 0;

    std::vector<ItemPos> number_of_selected_copies_;

    double item_distribution_parameter_ = 1.0;

    std::vector<std::pair<double, double>> expected_axle_weights_;

    std::vector<double> expected_lengths_;

    mutable Counter node_id_ = 0;

    mutable std::vector<Insertion> insertions_;

    /*
     * Private methods
     */

    /** Get the percentage of item inserted into a node. */
    inline double item_percentage(const Node& node) const { return (double)node.number_of_items / instance_.number_of_items(); }

    /** Get the mean volume of a node. */
    inline double mean_volume(const Node& node) const { return (double)node.current_volume / node.number_of_items; }

    /** Get the mean item volume of a node; */
    inline double mean_item_volume(const Node& node) const { return (double)node.item_volume / node.number_of_items; }

    /** Get the mean squared item volume of a node. */
    inline double mean_squared_item_volume(const Node& node) const { return (double)node.squared_item_volume / node.number_of_items; }

    /** Get the mean remaining item volume of a node. */
    inline double mean_remaining_item_volume(const Node& node) const { return (double)remaining_item_volume(node) / (instance_.number_of_items() - node.number_of_items); }

    /** Get the remaining item volume of a node. */
    inline double remaining_item_volume(const Node& node) const { return instance_.item_volume() - node.item_volume; }

    /** Get the waste percentage of a node. */
    inline double waste_percentage(const Node& node) const { return (double)node.waste / node.current_volume; }

    /** Get the volume load of a node. */
    inline double volume_load(const Node& node) const { return (double)node.item_volume / instance().bin_volume(); }

    /** Get the weight load of a node. */
    inline double weight_load(const Node& node) const { return (double)node.item_weight / instance().bin_weight(); }

    /** Get the waste ratio of a node. */
    inline double waste_ratio(const Node& node) const { return (double)node.waste / node.item_volume; }

    /** Get the knapsack upper bound of a node. */
    inline Profit ubkp(const Node& node) const;

    /*
     * Insertions
     */

    /** Insertion of one item above the previous inserted one. */
    void insertion_item_above(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id,
            int rotation,
            ItemPos uncovered_item_pos) const;

    /** Insertion of one item in a new stack. */
    void insertion_item(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id,
            int rotation,
            int8_t new_bin,
            ItemPos uncovered_item_pos,
            DefectId defect_id) const;

    /** Insertion of one item in a new stack. */
    void insertion_item_left(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id,
            int rotation,
            ItemPos uncovered_item_pos) const;

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

Profit BranchingScheme::ubkp(const Node& node) const
{
    Area remaining_item_volume = instance_.item_volume() - node.item_volume;
    Area remaining_packabla_volume = instance_.bin_volume() - node.current_volume;
    if (remaining_packabla_volume >= remaining_item_volume) {
        return instance_.item_profit();
    } else {
        ItemTypeId item_type_id = instance_.max_efficiency_item_type_id();
        return node.profit + remaining_packabla_volume
            * instance_.item_type(item_type_id).profit
            / instance_.item_type(item_type_id).volume();
    }
}

inline bool BranchingScheme::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch(parameters_.guide_id) {
    case 0: {
        if (node_1->guide_volume == 0)
            return node_2->guide_volume != 0;
        if (node_2->guide_volume == 0)
            return false;
        double guide_1 = (double)node_1->guide_volume / node_1->guide_item_volume;
        double guide_2 = (double)node_2->guide_volume / node_2->guide_item_volume;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 1: {
        if (node_1->guide_volume == 0)
            return node_2->guide_volume != 0;
        if (node_2->guide_volume == 0)
            return false;
        if (node_1->number_of_items == 0)
            return node_2->number_of_items != 0;
        if (node_2->number_of_items == 0)
            return true;
        double guide_1 = (double)node_1->guide_volume
            / node_1->guide_item_volume
            / mean_item_volume(*node_1);
        double guide_2 = (double)node_2->guide_volume
            / node_2->guide_item_volume
            / mean_item_volume(*node_2);
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 2: {
        if (node_1->current_volume == 0)
            return node_2->current_volume != 0;
        if (node_2->current_volume == 0)
            return false;
        if (node_1->number_of_items == 0)
            return node_2->number_of_items != 0;
        if (node_2->number_of_items == 0)
            return true;
        if ((0.1 + waste_percentage(*node_1)) / mean_item_volume(*node_1)
                != (0.1 + waste_percentage(*node_2)) / mean_item_volume(*node_2))
            return (0.1 + waste_percentage(*node_1)) / mean_item_volume(*node_1)
                < (0.1 + waste_percentage(*node_2)) / mean_item_volume(*node_2);
        break;
    } case 3: {
        if (node_1->current_volume == 0)
            return node_2->current_volume != 0;
        if (node_2->current_volume == 0)
            return false;
        if (node_1->number_of_items == 0)
            return node_2->number_of_items != 0;
        if (node_2->number_of_items == 0)
            return true;
        if ((0.1 + waste_percentage(*node_1)) / mean_squared_item_volume(*node_1)
                != (0.1 + waste_percentage(*node_2)) / mean_squared_item_volume(*node_2))
            return (0.1 + waste_percentage(*node_1)) / mean_squared_item_volume(*node_1)
                < (0.1 + waste_percentage(*node_2)) / mean_squared_item_volume(*node_2);
        break;
    } case 4: {
        if (node_1->guide_profit == 0)
            return node_2->guide_profit != 0;
        if (node_2->guide_profit == 0)
            return true;
        //if (node_1->middle_axle_weight_violation != node_2->middle_axle_weight_violation)
        //    return node_1->middle_axle_weight_violation < node_2->middle_axle_weight_violation;
        //double guide_1 = (double)node_1->guide_volume / node_1->guide_profit;
        //double guide_2 = (double)node_2->guide_volume / node_2->guide_profit;
        double guide_1 = -node_1->guide_profit;
        if (node_1->xs_max > expected_lengths_[node_1->number_of_items]) {
            guide_1 += (double)(node_1->xs_max - expected_lengths_[node_1->number_of_items])
                / expected_lengths_[node_1->number_of_items];
        }
        if (node_1->last_bin_middle_axle_weight > expected_axle_weights_[node_1->number_of_items].first) {
            guide_1 += (double)(node_1->last_bin_middle_axle_weight - expected_axle_weights_[node_1->number_of_items].first)
                / expected_axle_weights_[node_1->number_of_items].first;
        }
        //guide_1 += (double)node_1->number_of_stacked_items / instance().number_of_items() / 10;
        double guide_2 = -node_2->guide_profit;
        if (node_2->xs_max > expected_lengths_[node_2->number_of_items]) {
            guide_2 += (double)(node_2->xs_max - expected_lengths_[node_2->number_of_items])
                / expected_lengths_[node_2->number_of_items];
        }
        if (node_2->last_bin_middle_axle_weight > expected_axle_weights_[node_2->number_of_items].first) {
            guide_2 += (double)(node_2->last_bin_middle_axle_weight - expected_axle_weights_[node_2->number_of_items].first)
                / expected_axle_weights_[node_2->number_of_items].first;
        }
        //guide_2 += (double)node_2->number_of_stacked_items / instance().number_of_items() / 10;
        //std::cout << "guide_1 " << guide_1 << " guide_2 " << guide_2 << std::endl;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        double g1 = node_1->item_weight / node_1->item_volume;
        double g2 = node_2->item_weight / node_2->item_volume;
        if (g1 != g2)
            return g1 < g2;
        break;
    } case 5: {
        if (node_1->guide_profit == 0)
            return node_2->guide_profit != 0;
        if (node_2->guide_profit == 0)
            return true;
        if (node_1->number_of_items == 0)
            return node_2->number_of_items != 0;
        if (node_2->number_of_items == 0)
            return true;
        if (node_1->middle_axle_overweight != node_2->middle_axle_overweight)
            return node_1->middle_axle_overweight < node_2->middle_axle_overweight;
        double guide_1 = (double)node_1->guide_volume
            / node_1->guide_profit
            / mean_item_volume(*node_1);
        double guide_2 = (double)node_2->guide_volume
            / node_2->guide_profit
            / mean_item_volume(*node_2);
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

