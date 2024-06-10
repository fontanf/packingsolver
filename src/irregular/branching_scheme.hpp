#pragma once

#include "packingsolver/irregular/solution.hpp"
#include "irregular/covering_with_rectangles.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <sstream>

namespace packingsolver
{
namespace irregular
{

using ItemShapePos = int64_t;
using ItemShapeRectanglePos = int64_t;
using RectangleSetId = int64_t;

struct ItemShapeRectangles
{
    std::vector<ShapeRectangle> rectangles;
};

struct ItemTypeRectangles
{
    std::vector<ItemShapeRectangles> shapes;
};

struct RectangleSetItem
{
    ItemTypeId item_type_id;

    Point bottom_left;

    Angle angle;
};

struct RectangleSet
{
    std::vector<RectangleSetItem> items;

    std::vector<std::pair<ItemTypeId, ItemPos>> item_types;

    LengthDbl x_min;

    LengthDbl x_max;

    LengthDbl y_min;

    LengthDbl y_max;
};

/**
 * Branching scheme class for problem of type "irregular".
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
    struct UncoveredRectangle
    {
        /** Item type. */
        ItemTypeId item_type_id;

        /** Item shape. */
        ItemShapePos item_shape_pos;

        /** Item shape rectangle. */
        ItemShapeRectanglePos item_shape_rectangle_pos;

        /** Start x-coordiante. */
        LengthDbl xs;

        /** End x-coordinate. */
        LengthDbl xe;

        /** Start y-coordinate. */
        LengthDbl ys;

        /** End y-coordinate. */
        LengthDbl ye;

        bool operator==(const UncoveredRectangle& uncovered_rectangle) const;
    };

    struct Insertion
    {
        /** Id of the inserted rectangle set. */
        RectangleSetId rectangle_set_id;

        ItemPos item_pos;

        ItemShapePos item_shape_pos;

        RectanglePos item_shape_rectangle_pos;

        /**
         * - < 0: the item is inserted in the last bin
         * - 0: the item is inserted in a new bin with horizontal direction
         * - 1: the item is inserted in a new bin with vertical direction
         */
        int8_t new_bin;

        /** x-coordinate of the point of interest. */
        LengthDbl x;

        /** y-coordinate of the point of interest. */
        LengthDbl y;

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

        /** Last inserted rectangle set. */
        RectangleSetId rectangle_set_id;

        /** x-coordinates of the bottom-left corner of the last inserted item. */
        LengthDbl x;

        /** y-coordinates of the bottom-left corner of the last inserted item. */
        LengthDbl y;

        /** Last bin direction. */
        Direction last_bin_direction = Direction::X;

        /** Uncovered rectangles. */
        std::vector<UncoveredRectangle> uncovered_rectangles;

        /** Extra rectangles. */
        std::vector<UncoveredRectangle> extra_rectangles;

        /** For each item type, number of copies in the node. */
        std::vector<ItemPos> item_number_of_copies;

        /** Number of bins in the node. */
        BinPos number_of_bins = 0;

        /** Number of items in the node. */
        ItemPos number_of_items = 0;

        /** Item area. */
        AreaDbl item_area = 0;

        /** Current area. */
        AreaDbl current_area = 0;

        /** Waste. */
        AreaDbl waste = 0;

        /** Area used in the guides. */
        AreaDbl guide_area = 0;

        /** Maximum xe of all items in the last bin. */
        LengthDbl xe_max = 0;

        /** Maximum xs of all items in the last bin. */
        LengthDbl xs_max = 0;

        /** Profit. */
        Profit profit = 0;

        /** Width or height. */
        LengthDbl length = 0;
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
        //if (unbounded_knapsck_ && node_1->profit < node_2->profit)
        //    return false;
        ItemPos pos_1 = node_1->uncovered_rectangles.size() - 1;
        ItemPos pos_2 = node_2->uncovered_rectangles.size() - 1;
        LengthDbl x1 = node_1->uncovered_rectangles[pos_1].xe;
        LengthDbl x2 = node_2->uncovered_rectangles[pos_2].xe;
        for (;;) {
            if (x1 > x2)
                return false;
            if (pos_1 == 0 && pos_2 == 0)
                return true;
            if (node_1->uncovered_rectangles[pos_1].ys
                    == node_2->uncovered_rectangles[pos_2].ys) {
                pos_1--;
                pos_2--;
                x1 = (parameters_.staircase)?
                    std::max(x1, node_1->uncovered_rectangles[pos_1].xe):
                    node_1->uncovered_rectangles[pos_1].xe;
                x2 = (parameters_.staircase)?
                    std::max(x2, node_2->uncovered_rectangles[pos_2].xe):
                    node_2->uncovered_rectangles[pos_2].xe;
            } else if (node_1->uncovered_rectangles[pos_1].ys
                    < node_2->uncovered_rectangles[pos_2].ys) {
                pos_2--;
                x2 = (parameters_.staircase)?
                    std::max(x2, node_2->uncovered_rectangles[pos_2].xe):
                    node_2->uncovered_rectangles[pos_2].xe;
            } else {
                pos_1--;
                x1 = (parameters_.staircase)?
                    std::max(x1, node_1->uncovered_rectangles[pos_1].xe):
                    node_1->uncovered_rectangles[pos_1].xe;
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

    /** Item types rectangles in X direction. */
    std::vector<ItemTypeRectangles> item_types_rectangles_x_;

    /** Item types rectangles in y direction. */
    std::vector<ItemTypeRectangles> item_types_rectangles_y_;

    /** Rectangle sets in x direction. */
    std::vector<RectangleSet> rectangle_sets_x_;

    /** Rectangle sets in y direction. */
    std::vector<RectangleSet> rectangle_sets_y_;

    mutable Counter node_id_ = 0;

    mutable std::vector<Insertion> insertions_;

    /*
     * Private methods
     */

    /** Get the percentage of item inserted into a node. */
    inline double item_percentage(const Node& node) const { return (double)node.number_of_items / instance_.number_of_items(); }

    /** Get the mean area of a node. */
    inline double mean_area(const Node& node) const { return (double)node.current_area / node.number_of_items; }

    /** Get the mean item area of a node; */
    inline double mean_item_area(const Node& node) const { return (double)node.item_area / node.number_of_items; }

    /** Get the mean remaining item area of a node. */
    inline double mean_remaining_item_area(const Node& node) const { return (double)remaining_item_area(node) / (instance_.number_of_items() - node.number_of_items); }

    /** Get the remaining item area of a node. */
    inline double remaining_item_area(const Node& node) const { return instance_.item_area() - node.item_area; }

    /** Get the waste percentage of a node. */
    inline double waste_percentage(const Node& node) const { return (double)node.waste / node.current_area; }

    /** Get the waste ratio of a node. */
    inline double waste_ratio(const Node& node) const { return (double)node.waste / node.item_area; }

    /** Get the area load of a node. */
    inline double area_load(const Node& node) const { return (double)node.item_area / instance().bin_area(); }

    /** Insertion of one item. */
    void insertion_rectangle_set(
            const std::shared_ptr<Node>& parent,
            RectangleSetId rectangle_set_id,
            ItemPos item_pos,
            ItemShapePos item_shape_pos,
            RectanglePos item_shape_rectangle_pos,
            int8_t new_bin,
            ItemPos uncovered_rectangle_pos,
            ItemPos extra_rectangle_pos,
            DefectId k) const;

};

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredRectangle& uncovered_rectangles);

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
    switch(parameters_.guide_id) {
    case 0: {
        if (node_1->guide_area == 0)
            return node_2->guide_area != 0;
        if (node_2->guide_area == 0)
            return false;
        double guide_1 = (double)node_1->guide_area / node_1->item_area;
        double guide_2 = (double)node_2->guide_area / node_2->item_area;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 1: {
        if (node_1->guide_area == 0)
            return node_2->guide_area != 0;
        if (node_2->guide_area == 0)
            return false;
        if (node_1->number_of_items == 0)
            return node_2->number_of_items != 0;
        if (node_2->number_of_items == 0)
            return true;
        double guide_1 = (double)node_1->guide_area
            / node_1->item_area
            / mean_item_area(*node_1);
        double guide_2 = (double)node_2->guide_area
            / node_2->item_area
            / mean_item_area(*node_2);
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 2: {
        if (node_1->guide_area == 0)
            return node_2->guide_area != 0;
        if (node_2->guide_area == 0)
            return false;
        if (node_1->number_of_items == 0)
            return node_2->number_of_items != 0;
        if (node_2->number_of_items == 0)
            return true;
        double guide_1 = (double)node_1->guide_area
            / node_1->item_area
            / mean_item_area(*node_1)
            * (0.1 + waste_percentage(*node_1));
        double guide_2 = (double)node_2->guide_area
            / node_2->item_area
            / mean_item_area(*node_2)
            * (0.1 + waste_percentage(*node_2));
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 4: {
        if (node_1->profit == 0)
            return node_2->profit != 0;
        if (node_2->profit == 0)
            return true;
        double guide_1 = (double)node_1->guide_area / node_1->profit;
        double guide_2 = (double)node_2->guide_area / node_2->profit;
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 5: {
        if (node_1->profit == 0)
            return node_2->profit != 0;
        if (node_2->profit == 0)
            return true;
        if (node_1->number_of_items == 0)
            return node_2->number_of_items != 0;
        if (node_2->number_of_items == 0)
            return true;
        double guide_1 = (double)node_1->guide_area
            / node_1->profit
            / mean_item_area(*node_1);
        double guide_2 = (double)node_2->guide_area
            / node_2->profit
            / mean_item_area(*node_2);
        if (guide_1 != guide_2)
            return guide_1 < guide_2;
        break;
    } case 6: {
        if (node_1->waste != node_2->waste)
            return node_1->waste < node_2->waste;
        break;
    }
    }
    return node_1->id < node_2->id;
}

}
}
