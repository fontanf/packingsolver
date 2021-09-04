#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangleguillotine
{

class BranchingScheme
{

public:

    /**
     * Sub-structures.
     */

    struct Parameters
    {
        CutType1 cut_type_1 = CutType1::ThreeStagedGuillotine;
        CutType2 cut_type_2 = CutType2::NonExact;
        CutOrientation first_stage_orientation = CutOrientation::Vertical;
        Length min1cut = 0;
        Length max1cut = -1;
        Length min2cut = 0;
        Length max2cut = -1;
        Length min_waste = 1;
        bool one2cut = false;
        bool no_item_rotation = false;
        bool cut_through_defects = false;
        GuideId guide_id = 0;

        void set_predefined(std::string str);
        void set_roadef2018()
        {
            cut_type_1 = rectangleguillotine::CutType1::ThreeStagedGuillotine;
            cut_type_2 = rectangleguillotine::CutType2::Roadef2018;
            first_stage_orientation = rectangleguillotine::CutOrientation::Vertical;
            min1cut = 100;
            max1cut = 3500;
            min2cut = 100;
            min_waste = 20;
            no_item_rotation = false;
            cut_through_defects = false;
        }
    };

    struct SolutionNode
    {
        SolutionNodeId f; // father for 2-cuts and 3-cuts, -bin-1 for 1-cuts
        Length p;         // x      for 1-cuts and 3-cuts, y        for 2-cuts

        bool operator==(const BranchingScheme::SolutionNode& node) const;
        bool operator!=(const BranchingScheme::SolutionNode& node) const { return !(*this == node); }
    };

    struct NodeItem
    {
        ItemTypeId j;
        SolutionNodeId node;
        // Note that two items may belong to the same node if cut_type_2() is set to
        // "roadef2018". On the contrary, a node may not contain any item if bins
        // contain defects.

        bool operator==(const BranchingScheme::NodeItem& node_item) const;
        bool operator!=(const BranchingScheme::NodeItem& node_item) const { return !(*this == node_item); }
    };

    struct Insertion
    {
        /** Id of the item at the bottom of the third-level sub-plate, -1 if none. */
        ItemTypeId j1;
        /** Id of the item at the top of the third-level sub-plate, -1 if none. */
        ItemTypeId j2;

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
         * x1_max_ is the maximum position of the current 1-cut.
         * It is used when otherwise, a 2-cut of the current 1-level sub-plate
         * would intersect a defect.
         */
        Length x1_max;

        /**
         * y2_max_ is the maximum position of the current 2-cut.
         * It is used when otherwise, a 3-cut of the current 2-level sub-plate
         * would intersect a defect.
         */
        Length y2_max;

        /**
         * z1_
         * * 0: to increase the width of the last 1-cut, it is necessary to add at
         * least the minimum waste.
         * * 1: the width of the last 1-cut can be increased by any value.
         */
        Counter z1;

        /**
         * z2_
         * * 0: to increase the height of the last 2-cut, it is necessary to add at
         * least the minimum waste.
         * * 1: the height of the last 2-cut can be increased by any value.
         * * 2: the height of the last 2-cut cannot be increased (case where it
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
        ItemTypeId j;
        bool rotate;
        Length x;
    };

    struct Node
    {
        NodeId id = -1;
        std::shared_ptr<Node> father = nullptr;
        ItemTypeId j1 = -1;
        ItemTypeId j2 = -1;
        Depth df = -1;
        Length x1_curr = 0;
        Length x1_prev = 0;
        Length y2_curr = 0;
        Length y2_prev = 0;
        Length x3_curr = 0;
        Length x1_max = -1;
        Length y2_max = -1;
        Counter z1 = 0;
        Counter z2 = 0;

        /**
         * pos_stack[s] == k iff the solution contains items 0 to k - 1 in the
         * sequence of stack s.
         */
        std::vector<ItemPos> pos_stack = {};

        BinPos bin_number = 0;
        CutOrientation first_stage_orientation;

        ItemPos item_number    = 0;
        Area item_area         = 0;
        Area squared_item_area = 0;
        Area current_area      = 0;
        Area waste             = 0;
        Profit profit          = 0;

        /**
         * Contains the list of items (id, rotate, left cut position) inserted
         * above a defect in the current 2-level sub-plate.
         */
        std::vector<JRX> subplate2curr_items_above_defect = {};
    };

    /** Constructor */
    BranchingScheme(const Instance& instance, const Parameters& parameters);

    /** Destructor */
    virtual ~BranchingScheme() { }

    /**
     * Branching scheme methods
     */

    std::vector<Insertion> insertions(
            const std::shared_ptr<Node>& father,
            Info& info) const;

    std::shared_ptr<Node> child(
            const std::shared_ptr<Node>& father,
            const Insertion& insertion) const;

    const Instance& instance() const { return instance_; }

    const std::shared_ptr<Node> root() const;

    inline bool operator()(
            const std::shared_ptr<Node>& node_1,
            const std::shared_ptr<Node>& node_2) const;

    inline bool leaf(
            const Node& node) const
    {
        return node.item_number == instance_.item_number();
    }

    bool bound(
            const Node&,
            const Solution& solution_best) const;

    bool better(
            const Node& node,
            const Solution& solution_best) const;

    /**
     * Dominances.
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

    const Instance& instance_;
    Parameters parameters_;

    bool no_oriented_items_;

    /**
     * If stacks s1 < s2 < s3 contain identical items in the same order, then
     * stack_pred[s1] = -1, stack_pred[s2] = s1 and stack_pred[s3] = s2.
     */
    std::vector<StackId> stack_pred_;

    mutable NodeId node_id_ = 0;

    /**
     * Private methods
     */

    inline bool oriented(ItemTypeId j) const { return (instance_.item_type(j).oriented)? true: parameters_.no_item_rotation; }

    /**
     * Return true iff s1 and s2 contains identical objects in the same order.
     */
    bool equals(StackId s1, StackId s2);

    Front front(const Node&) const;
    bool dominates(const Front& f1, const Front& f2) const;

    inline bool                        full(const Node& node) const { return node.item_number == instance_.item_number(); }
    inline double           item_percentage(const Node& node) const { return (double)node.item_number / instance_.item_number(); }
    inline double                 mean_area(const Node& node) const { return (double)node.current_area / node.item_number; }
    inline double            mean_item_area(const Node& node) const { return (double)node.item_area / node.item_number; }
    inline double    mean_squared_item_area(const Node& node) const { return (double)node.squared_item_area / node.item_number; }
    inline double  mean_remaining_item_area(const Node& node) const { return (double)remaining_item_area(node) / (instance_.item_number() - node.item_number); }
    inline double       remaining_item_area(const Node& node) const { return instance_.item_area() - node.item_area; }
    inline double          waste_percentage(const Node& node) const { return (double)node.waste / node.current_area; }
    inline double               waste_ratio(const Node& node) const { return (double)node.waste / node.item_area; }
    inline Length                     width(const Node& node) const { return (parameters_.cut_type_1 == CutType1::ThreeStagedGuillotine)? node.x1_curr: node.y2_curr; }
    inline Length                    height(const Node& node) const { return (parameters_.cut_type_1 == CutType1::ThreeStagedGuillotine)? node.x1_curr: node.y2_curr; }
    inline Profit                      ubkp(const Node& node) const;
    inline bool       last_insertion_defect(const Node& node) const { return node.bin_number > 0 && node.j1 == -1 && node.j2 == -1; }

    inline Front front(const Node& node, const Insertion& insertion) const;
    inline Area  waste(const Node& node, const Insertion& insertion) const;

    /** Attributes of the new node if an insertion is performed at depth df.  */
    inline CutOrientation last_bin_orientation(const Node& node, Depth df) const;
    inline BinPos last_bin(const Node& node, Depth df) const;
    inline Length  x1_prev(const Node& node, Depth df) const;
    inline Length  y2_prev(const Node& node, Depth df) const;
    inline Length  x3_prev(const Node& node, Depth df) const;
    inline Length   x1_max(const Node& node, Depth df) const;
    inline Length   y2_max(const Node& node, Depth df, Length x3) const;

    /** Insertion of one item. */
    void insertion_1_item(const Node& father, std::vector<Insertion>& insertions,
            ItemTypeId j, bool rotate, Depth df, Info& info) const;
    /** Insertion of two items. */
    void insertion_2_items(const Node& father, std::vector<Insertion>& insertions,
            ItemTypeId j1, bool rotate1, ItemTypeId j2, bool rotate2, Depth df, Info& info) const;
    /** Insertion of a defect. */
    void insertion_defect(const Node& father, std::vector<Insertion>& insertions,
            const Defect& k, Depth df, Info& info) const;
    /** Update insertion (x1, z1, y2, z2) and add insertion to insertions. */
    void update(const Node& father, std::vector<Insertion>& insertions,
            Insertion& insertion, Info& info) const;

    bool check(const std::vector<Solution::Node>& nodes) const;
};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::Parameters& parameters);
std::ostream& operator<<(std::ostream &os, const BranchingScheme::SolutionNode& node);
std::ostream& operator<<(std::ostream &os, const BranchingScheme::NodeItem& node);
std::ostream& operator<<(std::ostream &os, const BranchingScheme::Insertion& insertion);
std::ostream& operator<<(std::ostream &os, const std::vector<BranchingScheme::Insertion>& insertions);
std::ostream& operator<<(std::ostream &os, const BranchingScheme::Front& front);
std::ostream& operator<<(std::ostream &os, const BranchingScheme::Node& node);

/****************************** Inlined methods *******************************/

inline Profit BranchingScheme::ubkp(const Node& node) const
{
    Area remaining_item_area     = instance_.item_area() - node.item_area;
    Area remaining_packabla_area = instance_.packable_area() - node.current_area;
    if (remaining_packabla_area >= remaining_item_area) {
        return instance_.item_profit();
    } else {
        ItemTypeId j = instance_.max_efficiency_item();
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

        if (node_1->item_number == 0) {
            if (node_2->item_number != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->item_number == 0)
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

        if (node_1->item_number == 0) {
            if (node_2->item_number != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->item_number == 0)
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

        if (node_1->item_number == 0) {
            if (node_2->item_number != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->item_number == 0)
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

        if (node_1->item_number == 0) {
            if (node_2->item_number != 0) {
                return false;
            } else {
                return node_1->id < node_2->id;
            }
        }
        if (node_2->item_number == 0)
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

