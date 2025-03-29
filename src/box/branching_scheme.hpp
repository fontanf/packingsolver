#pragma once

#include "packingsolver/box/solution.hpp"
#include "box/instance_flipper.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace box
{

/**
 * Branching scheme class for problem of type "box".
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
        /** x-coordinate. */
        Length x;

        /** Start y-coordinate. */
        Length ys;

        /** End y-coordinate. */
        Length ye;

        /** Start z-coordinate. */
        Length zs;

        /** End z-coordinate. */
        Length ze;

        bool operator==(const UncoveredItem& uncovered_item) const;
    };

    struct YUncoveredItem
    {
        /** Start x-coordinate. */
        Length xs;

        /** End x-coordinate. */
        Length xe;

        /** y-coordinate. */
        Length y;

        /** Start z-coordinate. */
        Length zs;

        /** End z-coordinate. */
        Length ze;
    };

    struct ZUncoveredItem
    {
        /** Start x-coordinate. */
        Length xs;

        /** End x-coordinate. */
        Length xe;

        /** z-coordinate. */
        Length z;

        /** Start z-coordinate. */
        Length ys;

        /** End z-coordinate. */
        Length ye;
    };

    struct Insertion
    {
        /** Id of the inserted item type. */
        ItemTypeId item_type_id;

        /** Rotation of the inserted item. */
        int rotation;

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

        /** Rotation of the last inserted item. */
        int rotation = -1;

        /** x-coordinates of the bottom-left corner of the last inserted item. */
        Length x = -1;

        /** y-coordinates of the bottom-left corner of the last inserted item. */
        Length y = -1;

        /** z-coordinates of the bottom-left corner of the last inserted item. */
        Length z = -1;

        /** Last bin direction. */
        Direction last_bin_direction = Direction::X;

        /** Uncovered items. */
        std::vector<UncoveredItem> uncovered_items;

        /** Rectangles which high y side is not covered. */
        std::vector<YUncoveredItem> y_uncovered_items;

        /** Rectangles which high z side is not covered. */
        std::vector<ZUncoveredItem> z_uncovered_items;

        /** Last bin weight. */
        Weight last_bin_weight = 0;

        /** For each item type, number of copies in the node. */
        std::vector<ItemPos> item_number_of_copies;

        /** Number of bins in the node. */
        BinPos number_of_bins = 0;

        /** Number of items in the node. */
        ItemPos number_of_items = 0;

        /** Item volume. */
        Volume item_volume = 0;

        /** Item weight. */
        Weight item_weight = 0.0;

        /** Current volume. */
        Volume current_volume = 0;

        /** Waste. */
        Volume waste = 0;

        /** Volume used in the guides. */
        Volume guide_volume = 0;

        /** Maximum xe of all items in the last bin. */
        Length xe_max = 0;

        /** Maximum ye of all items in the last bin. */
        Length ye_max = 0;

        /** Maximum xs of all items in the last bin. */
        Length xs_max = 0;

        /** Profit. */
        Profit profit = 0;

        /** Width or height. */
        Length length = 0;
    };

    struct Parameters
    {
        /** Guide. */
        GuideId guide_id = 0;

        /** Direction. */
        Direction direction = Direction::X;
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

        for (const UncoveredItem& uncovered_item_1: node_1->uncovered_items) {
            for (const UncoveredItem& uncovered_item_2: node_2->uncovered_items) {
                if (uncovered_item_1.ys >= uncovered_item_2.ye)
                    continue;
                if (uncovered_item_1.ye <= uncovered_item_2.ys)
                    continue;
                if (uncovered_item_1.zs >= uncovered_item_2.ze)
                    continue;
                if (uncovered_item_1.ze <= uncovered_item_2.zs)
                    continue;
                if (uncovered_item_1.x > uncovered_item_2.x)
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

    /** Instance flipper. */
    InstanceFlipper instance_flipper_;

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

    const Instance& instance(Direction direction) const { return (direction == Direction::X)? instance_: instance_flipper_.flipped_instance(); }

    const Instance& instance(int new_bin) const { return (new_bin % 2 == 1)? instance_: instance_flipper_.flipped_instance(); }

    /** Get the percentage of item inserted into a node. */
    inline double item_percentage(const Node& node) const { return (double)node.number_of_items / instance_.number_of_items(); }

    /** Get the mean volume of a node. */
    inline double mean_volume(const Node& node) const { return (double)node.current_volume / node.number_of_items; }

    /** Get the mean item volume of a node; */
    inline double mean_item_volume(const Node& node) const { return (double)node.item_volume / node.number_of_items; }

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

    void update_uncovered_item(
            std::vector<UncoveredItem>& uncovered_items,
            const UncoveredItem& uncovered_item,
            Length ys,
            Length ye,
            Length zs,
            Length ze) const;

    void update_y_uncovered_item(
            std::vector<YUncoveredItem>& y_uncovered_items,
            const YUncoveredItem& y_uncovered_item,
            Length ys,
            Length ye,
            Length zs,
            Length ze) const;

    void update_z_uncovered_item(
            std::vector<ZUncoveredItem>& z_uncovered_items,
            const ZUncoveredItem& z_uncovered_item,
            Length ys,
            Length ye,
            Length zs,
            Length ze) const;

    /** Insertion of one item in a new stack. */
    void insertion_item(
            const std::shared_ptr<Node>& parent,
            ItemTypeId item_type_id,
            int rotation,
            int8_t new_bin,
            ItemPos y_uncovered_item_pos,
            ItemPos z_uncovered_item_pos) const;

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
        const BranchingScheme::UncoveredItem& uncovered_item);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::YUncoveredItem& y_uncovered_item);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::ZUncoveredItem& uncovered_item);

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
        ItemTypeId item_type_id = instance_.largest_efficiency_item_type_id();
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
        double guide_1 = (double)node_1->guide_volume / node_1->item_volume;
        double guide_2 = (double)node_2->guide_volume / node_2->item_volume;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 1: {
        double guide_1 = (double)node_1->guide_volume
            / node_1->item_volume
            / mean_item_volume(*node_1);
        double guide_2 = (double)node_2->guide_volume
            / node_2->item_volume
            / mean_item_volume(*node_2);
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 4: {
        double guide_1 = (double)node_1->guide_volume / node_1->profit;
        double guide_2 = (double)node_2->guide_volume / node_2->profit;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 5: {
        double guide_1 = (double)node_1->guide_volume
            / node_1->profit
            / node_1->item_volume
            * node_1->number_of_items;
        double guide_2 = (double)node_2->guide_volume
            / node_2->profit
            / node_2->item_volume
            * node_2->number_of_items;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    }
    }
    return node_1->id < node_2->id;
}

}
}
