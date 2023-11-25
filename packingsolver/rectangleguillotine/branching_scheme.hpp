#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangleguillotine
{

/**
 * Branching scheme class for problem of type "rectangleguillotine" generating
 * 2-stage and 3-stage patterns.
 *
 * At each level of the tree, a new third-level sub-plate is filled:
 * - either in the current second-level sub-plate
 * - or in the current first level sub-plate, in a new second-level sub-plate
 * - or in a the current bin, in a new first-level sub-plate
 * - or in a new bin
 *
 * This third-level sub-plate can contain:
 * - A single item at the bottom with possibly some waste above
 * - A single item at the top with waste below (in case it would intersect a
 *   defect if placed at the bottom)
 * - Two items (with the same width)
 * - No item, when packing a defect
 */
class BranchingScheme
{

public:

    /*
     * Sub-structures
     */

    struct Insertion
    {
        /** Id of the item at the bottom of the third-level sub-plate, -1 if none. */
        ItemTypeId item_type_id_1;

        /** Id of the item at the top of the third-level sub-plate, -1 if none. */
        ItemTypeId item_type_id_2;

        /**
         * Depth of the father in the tree representation of the solution:
         * * 2: same second-level sub-plate
         * * 1: same first-level sub-plate, new second-level sub-plate
         * * 0: same bin, new first-level sub-plate
         * * -1: new bin, first stage veritical
         * * -2: new bin, first stage horizontal
         */
        Depth df;

        /** Position of the current 1-cut. */
        Length x1;

        /** Position of the current 2-cut. */
        Length y2;

        /** Position of the current 3-cut. */
        Length x3;

        /**
         * Maximum position of the current 1-cut can be shifted.
         * It is used when otherwise, a 2-cut of the current 1-level sub-plate
         * would intersect a defect.
         */
        Length x1_max;

        /**
         * Maximum position at which of the current 2-cut can be shifted.
         * It is used when otherwise, a 3-cut of the current 2-level sub-plate
         * would intersect a defect.
         */
        Length y2_max;

        /**
         * z1:
         * - 0: to increase the width of the last 1-cut, it is necessary to add at
         * least the minimum waste.
         * - 1: the width of the last 1-cut can be increased by any value.
         */
        Counter z1;

        /**
         * z2:
         * - 0: to increase the height of the last 2-cut, it is necessary to add at
         * least the minimum waste.
         * - 1: the height of the last 2-cut can be increased by any value.
         * - 2: the height of the last 2-cut cannot be increased (case where it
         * contains of 4-cut with 2 items).
         */
        Counter z2;

        bool operator==(const Insertion& insertion) const;
        bool operator!=(const Insertion& insertion) const { return !(*this == insertion); }
    };

    struct Front
    {
        BinPos i;
        CutOrientation o;
        Length x1_prev, x3_curr, x1_curr;
        Length y2_prev, y2_curr;

        bool operator==(const Front& front) const;
        bool operator!=(const Front& front) const { return !(*this == front); }
    };

    struct JRX
    {
        ItemTypeId item_type_id;
        bool rotate;
        Length x;
    };

    /**
     * Node structure of the branching scheme.
     */
    struct Node
    {
        /** Id of the node. */
        NodeId id = -1;

        /**
         * Pointer to the father of the node,
         * 'nullptr' if the node is the root.
         */
        std::shared_ptr<Node> father = nullptr;

        /**
         * Type of the last item added to the partial solution at the bottom of
         * the third level sub-plate.
         * -1 if no such item have been added or if the node is the root.
         */
        ItemTypeId item_type_id_1 = -1;

        /**
         * Type of the last item added to the partial solution at the top of
         * the third level sub-plate.
         * -1 if no such item have been added or if the node is the root.
         */
        ItemTypeId item_type_id_2 = -1;

        /** Depth of the last insertion, see Insertion. */
        Depth df = -1;

        /** Position of the current 1-cut. */
        Length x1_curr = 0;

        /** Position of the previous 1-cut. */
        Length x1_prev = 0;

        /** Position of the current 2-cut. */
        Length y2_curr = 0;

        /** Position of the previous 2-cut. */
        Length y2_prev = 0;

        /** Position of the current 3-cut. */
        Length x3_curr = 0;

        /** Maximum position at which the current 1-cut can be shifted. */
        Length x1_max = -1;

        /** Maximum position at which the current 2-cut can be shifted. */
        Length y2_max = -1;

        /** 'z1' value of the last insertion, see Insertion. */
        Counter z1 = 0;

        /** 'z2' value of the last insertion, see Insertion. */
        Counter z2 = 0;

        /**
         * pos_stack[s] == k iff the solution contains items 0 to k - 1 in the
         * sequence of stack s.
         */
        std::vector<ItemPos> pos_stack = {};

        /** Number of bins used in the partial solution. */
        BinPos number_of_bins = 0;

        /**
         * Orientation of the first stage of the last bin of the partial
         * solution.
         * */
        CutOrientation first_stage_orientation = CutOrientation::Vertical;

        /** Number of items in the partial solution. */
        ItemPos number_of_items = 0;

        /** Total area of the items of the partial solution. */
        Area item_area = 0;

        /** Total squared area of the items of the partial solution. */
        Area squared_item_area = 0;

        /** Area of the partial solution. */
        Area current_area = 0;

        /** Waste of the partial solution. */
        Area waste = 0;

        /** Profit of the partial solution. */
        Profit profit = 0;

        /**
         * Contains the list of items (id, rotate, left cut position) inserted
         * above a defect in the current 2-level sub-plate.
         */
        std::vector<JRX> subplate2curr_items_above_defect = {};
    };

    struct Parameters
    {
        /** Guide. */
        GuideId guide_id = 0;

        /** First stage orientation. */
        CutOrientation first_stage_orientation = CutOrientation::Any;
    };

    /** Constructor. */
    BranchingScheme(
            const Instance& instance,
            const Parameters& parameters);

    BranchingScheme(
            const Instance& instance):
        BranchingScheme(instance, Parameters()) {  }

    /** Get instance. */
    inline const Instance& instance() const { return instance_; }

    /*
     * Branching scheme methods
     */

    std::vector<Insertion> insertions(
            const std::shared_ptr<Node>& father,
            Info& info) const;

    std::shared_ptr<Node> child(
            const std::shared_ptr<Node>& father,
            const Insertion& insertion) const;

    const std::shared_ptr<Node> root() const;

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    inline bool leaf(
            const Node& node) const
    {
        return node.number_of_items == instance_.number_of_items();
    }

    bool bound(
            const Node&,
            const Solution& solution_best) const;

    bool better(
            const Node& node,
            const Solution& solution_best) const;

    /*
     * Dominances
     */

    inline bool comparable(
            const std::shared_ptr<Node>& node) const
    {
        return (!last_insertion_defect(*node));
    }

    struct NodeHasher
    {
        std::hash<ItemPos> hasher;

        inline bool operator()(
                const std::shared_ptr<Node>& node_1,
                const std::shared_ptr<Node>& node_2) const
        {
            return node_1->pos_stack == node_2->pos_stack;
        }

        inline std::size_t operator()(
                const std::shared_ptr<Node>& node) const
        {
            size_t hash = 0;
            for (ItemPos s: node->pos_stack)
                optimizationtools::hash_combine(hash, hasher(s));
            return hash;
        }
    };

    inline NodeHasher node_hasher() const { return NodeHasher(); }

    bool dominates(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const
    {
        return dominates(front(*node_1), front(*node_2));
    }

    Solution to_solution(
            const Node& node,
            const Solution& solution_dummy) const;

private:

    /*
     * Private attributes
     */

    /** Instance. */
    const Instance& instance_;

    /** Parameters. */
    Parameters parameters_;

    /** First stage orientation. */
    CutOrientation first_stage_orientation_ = CutOrientation::Any;

    bool no_oriented_items_;

    /**
     * If stacks s1 < s2 < s3 contain identical items in the same order, then
     * stack_pred[s1] = -1, stack_pred[s2] = s1 and stack_pred[s3] = s2.
     */
    std::vector<StackId> stack_pred_;

    mutable NodeId node_id_ = 0;

    /*
     * Temporary variables
     */

    /** Position of the last bin. */
    mutable BinPos i = -1;

    /** Orientation of the last bin. */
    mutable CutOrientation o = CutOrientation::Vertical;

    /*
     * Private methods
     */

    /**
     * Return true iff s1 and s2 contains identical objects in the same order.
     */
    bool equals(StackId s1, StackId s2);

    Front front(const Node&) const;

    bool dominates(const Front& f1, const Front& f2) const;

    /** Get the percentage of item inserted into a node. */
    inline double item_percentage(const Node& node) const { return (double)node.number_of_items / instance_.number_of_items(); }

    /** Get the mean area of a node. */
    inline double mean_area(const Node& node) const { return (double)node.current_area / node.number_of_items; }

    /** Get the mean item area of a node; */
    inline double mean_item_area(const Node& node) const { return (double)node.item_area / node.number_of_items; }

    /** Get the mean squared item area of a node. */
    inline double mean_squared_item_area(const Node& node) const { return (double)node.squared_item_area / node.number_of_items; }

    /** Get the mean remaining item area of a node. */
    inline double mean_remaining_item_area(const Node& node) const { return (double)remaining_item_area(node) / (instance_.number_of_items() - node.number_of_items); }

    /** Get the remaining item area of a node. */
    inline double remaining_item_area(const Node& node) const { return instance_.item_area() - node.item_area; }

    /** Get the waste percentage of a node. */
    inline double waste_percentage(const Node& node) const { return (double)node.waste / node.current_area; }

    /** Get the waste ratio of a node. */
    inline double waste_ratio(const Node& node) const { return (double)node.waste / node.item_area; }

    /** Get the width of a node. */
    inline Length width(const Node& node) const { return (instance_.cut_type_1() == CutType1::ThreeStagedGuillotine)? node.x1_curr: node.y2_curr; }

    /** Get the height of a node. */
    inline Length height(const Node& node) const { return (instance_.cut_type_1() == CutType1::ThreeStagedGuillotine)? node.x1_curr: node.y2_curr; }

    /** Get the knapsack upper bound of a node. */
    inline Profit ubkp(const Node& node) const;

    inline bool last_insertion_defect(const Node& node) const { return node.number_of_bins > 0 && node.item_type_id_1 == -1 && node.item_type_id_2 == -1; }

    inline Front front(const Node& node, const Insertion& insertion) const;

    inline Area waste(const Node& node, const Insertion& insertion) const;

    /*
     * Attributes of the new node if an insertion is performed at depth 'df'.
     */

    /**
     * Orientation of the last bin if an insertion is performed at depth 'df'.
     */
    inline CutOrientation last_bin_orientation(
            const Node& node,
            Depth df) const;

    /**
     * Position of the last bin if an insertion is performed at depth 'df'.
     * */
    inline BinPos last_bin(
            const Node& node,
            Depth df) const;

    /**
     * Position of the previous 1-cut if an insertion is performed at depth
     * 'df'.
     * */
    inline Length x1_prev(
            const Node& node,
            Depth df) const;

    /**
     * Position of the previous 2-cut if an insertion is performed at depth
     * 'df'.
     */
    inline Length y2_prev(
            const Node& node,
            Depth df) const;

    /**
     * Position of the previous 2-cut if an insertion is performed at depth
     * 'df'.
     */
    inline Length x3_prev(
            const Node& node,
            Depth df) const;

    /**
     * Maximum possible osition of the current 1-cut if an insertion is
     * performed at depth 'df'.
     */
    inline Length x1_max(
            const Node& node,
            Depth df) const;

    /**
     * Maximum possible osition of the current 2-cut if an insertion is
     * performed at depth 'df'.
     */
    inline Length y2_max(
            const Node& node,
            Depth df,
            Length x3) const;

    /*
     * Insertions
     */

    /** Insertion of one item. */
    void insertion_1_item(
            const Node& father,
            std::vector<Insertion>& insertions,
            ItemTypeId item_type_id,
            bool rotate,
            Depth df,
            Info& info) const;

    /** Insertion of two items. */
    void insertion_2_items(
            const Node& father,
            std::vector<Insertion>& insertions,
            ItemTypeId item_type_id_1,
            bool rotate1,
            ItemTypeId item_type_id_2,
            bool rotate2,
            Depth df,
            Info& info) const;

    /** Insertion of a defect. */
    void insertion_defect(
            const Node& father,
            std::vector<Insertion>& insertions,
            DefectId defect_id,
            Depth df,
            Info& info) const;

    /** Update insertion (x1, z1, y2, z2) and add insertion to insertions. */
    void update(
            const Node& father,
            std::vector<Insertion>& insertions,
            Insertion& insertion,
            Info& info) const;

    bool check(const std::vector<SolutionNode>& nodes) const;
};

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion);

std::ostream& operator<<(
        std::ostream& os,
        const std::vector<BranchingScheme::Insertion>& insertions);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Front& front);

std::ostream& operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node);

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

inline Profit BranchingScheme::ubkp(const Node& node) const
{
    Area remaining_item_area = instance_.item_area() - node.item_area;
    Area remaining_packabla_area = instance_.bin_area() - node.current_area;
    if (remaining_packabla_area >= remaining_item_area) {
        return instance_.item_profit();
    } else {
        ItemTypeId j = instance_.max_efficiency_item_type_id();
        double e = (double)instance_.item_type(j).profit / instance_.item_type(j).rect.area();
        Profit p = node.profit + remaining_packabla_area * e;
        //std::cout << "j " << j << " " << instance_.item(j) << std::endl;
        //std::cout << "instance_.item_area() " << instance_.item_area() << std::endl;
        //std::cout << "instance_.packable_area() " << instance_.packable_area() << std::endl;
        //std::cout << "node.item_area " << node.item_area << std::endl;
        //std::cout << "remaining_packabla_area " << remaining_packabla_area << std::endl;
        //std::cout << "e " << e << std::endl;
        //std::cout << "p " << p << std::endl;
        assert(p >= 0);
        return p;
    }
}

bool BranchingScheme::operator()(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch(parameters_.guide_id) {
    case 0: {
        if (node_1->current_area == 0) {
            if (node_2->current_area != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->current_area == 0)
            return true;

        if (waste_percentage(*node_1) != waste_percentage(*node_2))
            return waste_percentage(*node_1) < waste_percentage(*node_2);
        break;
    } case 1: {
        if (node_1->current_area == 0) {
            if (node_2->current_area != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->current_area == 0)
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
    } case 2: {
        if (node_1->current_area == 0) {
            if (node_2->current_area != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->current_area == 0)
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

        if ((0.1 + waste_percentage(*node_1)) / mean_item_area(*node_1)
                != (0.1 + waste_percentage(*node_2)) / mean_item_area(*node_2))
            return (0.1 + waste_percentage(*node_1)) / mean_item_area(*node_1)
                < (0.1 + waste_percentage(*node_2)) / mean_item_area(*node_2);
        break;
    } case 3: {
        if (node_1->current_area == 0) {
            if (node_2->current_area != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->current_area == 0)
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

        if ((0.1 + waste_percentage(*node_1)) / mean_squared_item_area(*node_1)
                != (0.1 + waste_percentage(*node_2)) / mean_squared_item_area(*node_2))
            return (0.1 + waste_percentage(*node_1)) / mean_squared_item_area(*node_1)
                < (0.1 + waste_percentage(*node_2)) / mean_squared_item_area(*node_2);
        break;
    } case 4: {
        if (node_1->profit == 0) {
            if (node_2->profit != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->profit == 0)
            return true;

        if ((double)node_1->current_area / node_1->profit
                != (double)node_2->current_area / node_2->profit)
            return (double)node_1->current_area / node_1->profit
                < (double)node_2->current_area / node_2->profit;
        break;
    } case 5: {
        if (node_1->profit == 0) {
            if (node_2->profit != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->profit == 0)
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

        if ((double)node_1->current_area / node_1->profit / mean_item_area(*node_1)
                != (double)node_2->current_area / node_2->profit / mean_item_area(*node_2))
            return (double)node_1->current_area / node_1->profit / mean_item_area(*node_1)
                < (double)node_2->current_area / node_2->profit / mean_item_area(*node_2);
        break;
    } case 6: {
        if (node_1->waste != node_2->waste)
            return node_1->waste < node_2->waste;
        break;
    } case 7: {
        if (ubkp(*node_1) != ubkp(*node_2))
            return ubkp(*node_1) < ubkp(*node_2);
        break;
    } case 8: {
        if (ubkp(*node_1) != ubkp(*node_2))
            return ubkp(*node_1) < ubkp(*node_2);
        if (node_1->waste != node_2->waste)
            return node_1->waste < node_2->waste;
        break;
    }
    }
    return node_1->id < node_2->id;
}

}
}

