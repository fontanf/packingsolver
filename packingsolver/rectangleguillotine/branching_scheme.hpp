#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangleguillotine
{

/****************************** BranchingScheme *******************************/

class BranchingScheme
{

public:

    /** Subclasses */
    class Node;
    struct Insertion;
    struct SolutionNode;
    struct NodeItem;
    struct SubPlate;
    struct Front;

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
        Depth symmetry_depth = 2;
        bool symmetry_2 = true;

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
            symmetry_depth = 2;
            symmetry_2 = true;
        }
    };

    /** Constructor */
    BranchingScheme(const Instance& instance, const Parameters& parameters);

    /** Destructor */
    virtual ~BranchingScheme() { }

    /**
     * Getters
     */

    const Instance& instance() const { return instance_; }
    CutType1 cut_type_1() const { return parameters_.cut_type_1; }
    CutType2 cut_type_2() const { return parameters_.cut_type_2; }
    CutOrientation first_stage_orientation() const { return parameters_.first_stage_orientation; }
    Length min1cut() const { return parameters_.min1cut; }
    Length max1cut() const { return parameters_.max1cut; }
    Length min2cut() const { return parameters_.min2cut; }
    Length max2cut() const { return parameters_.max2cut; }
    Length min_waste() const { return parameters_.min_waste; }
    bool one2cut() const { return parameters_.one2cut; }
    bool no_item_rotation() const { return parameters_.no_item_rotation; }
    bool cut_through_defects() const { return parameters_.cut_through_defects; }
    Depth symmetry_depth() const { return parameters_.symmetry_depth; }
    bool symmetry_2() const { return parameters_.symmetry_2; }

    bool oriented(ItemTypeId j) const;
    bool no_oriented_items() const { return no_oriented_items_; }
    StackId stack_pred(StackId s) const { return stack_pred_[s]; }

    std::function<bool(const BranchingScheme::Node&, const BranchingScheme::Node&)> compare(GuideId guide_id);

private:

    const Instance& instance_;
    Parameters parameters_;

    bool no_oriented_items_;

    /**
     * If stacks s1 < s2 < s3 contain identical items in the same order, then
     * stack_pred[s1] = -1, stack_pred[s2] = s1 and stack_pred[s3] = s2.
     */
    std::vector<StackId> stack_pred_;

    /**
     * Return true iff s1 and s2 contains identical objects in the same order.
     */
    bool equals(StackId s1, StackId s2);

};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::Parameters& parameters);

/******************************** SolutionNode ********************************/

struct BranchingScheme::SolutionNode
{
    SolutionNodeId f; // father for 2-cuts and 3-cuts, -bin-1 for 1-cuts
    Length p;         // x      for 1-cuts and 3-cuts, y        for 2-cuts

    bool operator==(const BranchingScheme::SolutionNode& node) const;
    bool operator!=(const BranchingScheme::SolutionNode& node) const { return !(*this == node); }
};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::SolutionNode& node);

/********************************** NodeItem **********************************/

struct BranchingScheme::NodeItem
{
    ItemTypeId j;
    SolutionNodeId node;
    // Note that two items may belong to the same node if cut_type_2() is set to
    // "roadef2018". On the contrary, a node may not contain any item if bins
    // contain defects.

    bool operator==(const BranchingScheme::NodeItem& node_item) const;
    bool operator!=(const BranchingScheme::NodeItem& node_item) const { return !(*this == node_item); }
};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::NodeItem& node);

/********************************* Insertion **********************************/

struct BranchingScheme::Insertion
{
    ItemTypeId j1; // item 1
    ItemTypeId j2; // item 2
    Depth  df; // father's depth
    Length x1; // position of the next 1-cut
    Length y2; // position of the next 2-cut
    Length x3; // position of the next 3-cut
    Length x1_max;
    Length y2_max;
    Counter z1;
    Counter z2;

    bool operator==(const Insertion& insertion) const;
    bool operator!=(const Insertion& insertion) const { return !(*this == insertion); }
};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::Insertion& insertion);
std::ostream& operator<<(std::ostream &os, const std::vector<BranchingScheme::Insertion>& insertions);

/********************************** SubPlate **********************************/

struct BranchingScheme::SubPlate
{
    SolutionNodeId node;
    ItemPos n;
    Length l, b, r, t;

    bool operator==(const BranchingScheme::SubPlate& c) const;
    bool operator!=(const BranchingScheme::SubPlate& c) const { return !(*this == c); }
};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::SubPlate& ins);

/*********************************** Front ************************************/

struct BranchingScheme::Front
{
    BinPos i;
    CutOrientation o;
    Length x1_prev, x3_curr, x1_curr;
    Length y2_prev, y2_curr;
    Counter z1, z2;
};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::Front& front);

/************************************ Node ************************************/

struct WHX
{
    Length w;
    Length h;
    Length x;
};

class BranchingScheme::Node
{

public:

    /** Standard constructor, branching scheme root node. */
    Node(const BranchingScheme& branching_scheme);

    /** Copy constructor. */
    Node(const BranchingScheme::Node& solution);
    /** Assignment operator. */
    Node& operator=(const Node& solution);

    /** Modifiers. */
    std::vector<Insertion> children(Info& info) const;
    void apply_insertion(const Insertion& insertion, Info& info);

    /** Desctructor. */
    ~Node() { };

    /**
     * Getters
     */

    inline const BranchingScheme& branching_scheme() const { return branching_scheme_; }
    inline const Instance& instance() const { return branching_scheme_.instance(); }

    inline ItemPos item_number()              const { return items_.size(); }
    inline double  item_percentage()          const { return (double)item_number() / instance().item_number(); }
    inline BinPos  bin_number()               const { return first_stage_orientation_.size(); }
    inline Area    area()                     const { return current_area_; }
    inline double  mean_area()                const { return (double)area() / item_number(); }
    inline double  mean_item_area()           const { return (double)item_area() / item_number(); }
    inline double  mean_squared_item_area()   const { return (double)squared_item_area() / item_number(); }
    inline double  mean_remaining_item_area() const { return (double)remaining_item_area() / (instance().item_number() - item_number()); }
    inline double  remaining_item_area()      const { return instance().item_area() - item_area(); }
    inline Area    item_area()                const { return item_area_; }
    inline Area    squared_item_area()        const { return squared_item_area_; }
    inline Profit  profit()                   const { return profit_; }
    inline Profit  ub_profit()                const { return ub_profit_; }
    inline Area    waste()                    const { return waste_; }
    inline double  waste_percentage()         const { return (double)waste() / area(); }
    inline double  waste_ratio()              const { return (double)waste() / item_area(); }
    inline Length  last1cut()                 const { return (branching_scheme().cut_type_1() == CutType1::ThreeStagedGuillotine)?  x1_curr(): y2_curr(); }

    inline CutOrientation first_stage_orientation(BinPos i) const { return first_stage_orientation_[i]; }
    inline bool full() const { return item_number() == instance().item_number(); }

    /**
     * Export
     */
    Solution convert(const Solution&) const;

    /**
     * For unit tests
     */
    const std::vector<SolutionNode>& nodes() const { return nodes_; }
    const SolutionNode& node(SolutionNodeId id) const { return nodes_[id]; }
    SolutionNodeId node_number() const { return nodes_.size(); }
    const NodeItem& item(ItemPos j) const { return items_[j]; }
    const std::vector<NodeItem>& items() const { return items_; }

    const std::array<SubPlate, 4>& subplate_prev() const { return subplates_prev_; };
    const std::array<SubPlate, 4>& subplate_curr() const { return subplates_curr_; };
    inline const SubPlate& subplate_prev(Depth d) const { return subplates_prev_[d]; };
    inline const SubPlate& subplate_curr(Depth d) const { return subplates_curr_[d]; };

    inline Length x1_curr() const { return (subplate_curr(1).node == -1)? 0: node(subplate_curr(1).node).p; }
    inline Length x1_prev() const { return (subplate_prev(1).node == -1)? 0: node(subplate_prev(1).node).p; }
    inline Length y2_curr() const { return (subplate_curr(2).node == -1)? 0: node(subplate_curr(2).node).p; }
    inline Length y2_prev() const { return (subplate_prev(2).node == -1)? 0: node(subplate_prev(2).node).p; }
    inline Length x3_curr() const { return (subplate_curr(3).node == -1)? x1_prev(): node(subplate_curr(3).node).p; }
    inline Length x3_prev() const { return (subplate_prev(3).node == -1)? x1_prev(): node(subplate_prev(3).node).p; }
    inline Front front() const { return {.i = static_cast<BinPos>(bin_number()-1), .o = first_stage_orientation(bin_number() - 1), .x1_prev = x1_prev(), .x3_curr = x3_curr(), .x1_curr = x1_curr(), .y2_prev = y2_prev(), .y2_curr = y2_curr(), .z1 = z1(), .z2 = z2()}; }

    inline Length x1_max() const { return x1_max_; }
    inline Length y2_max() const { return y2_max_; }
    inline Counter z1() const { return z1_; }
    inline Counter z2() const { return z2_; }

    std::vector<ItemPos> pos_stack() const { return pos_stack_; }
    ItemPos pos_stack(StackId s) const { return pos_stack_[s]; }

    static bool dominates(Front f1, Front f2, const BranchingScheme& branching_scheme);

    bool bound(const Solution& sol_best) const;

private:

    /**
     * Attributes
     */

    const BranchingScheme& branching_scheme_;

    std::vector<SolutionNode> nodes_ = {};

    /**
     * pos_stack_[s] == k iff the solution contains items 0 to k-1 in the
     * sequence of stack s.
     */
    std::vector<ItemPos> pos_stack_ = {};

    /**
     * items_[j].j is the jth item of the the solution.
     * items_[j].node is the index of its associated node of nodes_.
     */
    std::vector<NodeItem> items_ = {};

    std::vector<CutOrientation> first_stage_orientation_;

    Area item_area_         = 0;
    Area squared_item_area_ = 0;
    Area current_area_      = 0;
    Area waste_             = 0;
    Profit profit_          = 0;
    Profit ub_profit_       = -1;

    std::array<SubPlate, 4> subplates_curr_ {{{.node = -1}, {.node = -1}, {.node = -1}, {.node = -1}}};
    std::array<SubPlate, 4> subplates_prev_ {{{.node = -1}, {.node = -1}, {.node = -1}, {.node = -1}}};

    /**
     * x1_max_ is the maximum position of the current 1-cut.
     * Used when otherwise, one of its 2-cut would intersect a defect.
     */
    Length x1_max_ = -1;

    /**
     * y2_max_ is the maximum position of the current 2-cut.
     * Used when otherwise, one of its 3-cut would intersect a defect.
     */
    Length y2_max_ = -1;

    /**
     * z1_
     * * 0: to increase the width of the last 1-cut, it is necessary to add at
     * least the minimum waste.
     * * 1: the width of the last 1-cut can be increased by any value.
     */
    Counter z1_ = 0;

    /**
     * z2_
     * * 0: to increase the height of the last 2-cut, it is necessary to add at
     * least the minimum waste.
     * * 1: the height of the last 2-cut can be increased by any value.
     * * 2: the height of the last 2-cut cannot be increased (case where it
     * contains of 4-cut with 2 items).
     */
    Counter z2_ = 0;

    Counter df_min_ = -4;

    /**
     * Contains the list of items (id, rotation, right cut position) inserted
     * above a defect between the previous and the current 2-cut.
     */
    std::vector<WHX> whx_ = {};


    /**
     * Private methods
     */

    bool check_symmetries(Depth df, Info& info) const;

    /**
     * children
     */
    void compute_ub_profit();
    void update_subplates_prev_and_curr(Depth df, ItemPos n);

    /**
     * Insertion of one item.
     */
    void insertion_1_item(std::vector<Insertion>& insertions,
            ItemTypeId j, bool rotate, Depth df, Info& info) const;
    /**
     * Insertion of one item above a defect.
     */
    void insertion_1_item_4cut(std::vector<Insertion>& insertions,
            DefectId k, ItemTypeId j, bool rotate, Depth df, Info& info) const;
    /**
     * Insertion of two items.
     */
    void insertion_2_items(std::vector<Insertion>& insertions,
            ItemTypeId j1, bool rotate1, ItemTypeId j2, bool rotate2, Depth df, Info& info) const;
    /**
     * Insertion of a defect.
     */
    void insertion_defect(std::vector<Insertion>& insertions,
            const Defect& k, Depth df, Info& info) const;

    /**
     * Coordinates of the bottom left side of a new insertion at depth df.
     */
    Coord coord(Depth df) const;
    BinPos last_bin(Depth df) const;
    CutOrientation last_bin_orientation(Depth df) const;
    Length x1_prev(Depth df) const;
    Length y2_prev(Depth df) const;
    Length x1_max(Depth df) const;
    Length y2_max(Depth df, Length x3) const;
    Front front(const Insertion& insertion) const;
    Area waste(const Insertion& insertion) const;

    /**
     * Compute i.x1 and i.z1 depending on x3 and x1_curr().
     */
    void insertion_item_update_x1_z1(Info& info, Insertion& insertion) const;
    void insertion_defect_update_x1_z1(Info& info, Insertion& insertion) const;

    /**
     * Update i.x1 depending on defect intersections.
     * Return false if infeasible.
     */
    bool compute_width(Info& info, Insertion& insertion) const;

    /**
     * Compute i.y2 and i.z2 depending on y4 and y2_curr().
     */
    bool insertion_item_update_y2_z2(Info& info, Insertion& insertion) const;
    bool insertion_2_items_update_y2_z2(Info& info, Insertion& insertion) const;
    bool insertion_defect_update_y2_z2(Info& info, Insertion& insertion) const;

    /**
     * Update i.y2 depending on defect intersections.
     * Return false if infeasible.
     */
    bool compute_height(Info& info, Insertion& insertion) const;

    bool check(const std::vector<Solution::Node>& nodes) const;

};

std::ostream& operator<<(std::ostream &os, const BranchingScheme::Node& node);

}
}

