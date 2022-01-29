#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

enum class CutType1 { ThreeStagedGuillotine, TwoStagedGuillotine };
enum class CutType2 { Roadef2018, NonExact, Exact, Homogenous };
enum class CutOrientation { Horinzontal, Vertical, Any };

std::istream& operator>>(std::istream& in, CutType1& cut_type_1);
std::istream& operator>>(std::istream& in, CutType2& cut_type_2);
std::istream& operator>>(std::istream& in, CutOrientation& o);

std::ostream& operator<<(std::ostream &os, CutType1 cut_type_1);
std::ostream& operator<<(std::ostream &os, CutType2 cut_type_2);
std::ostream& operator<<(std::ostream &os, CutOrientation o);

struct Coord
{
    Length x;
    Length y;
};

std::ostream& operator<<(std::ostream &os, Coord xy);

struct Rectangle
{
    Length w;
    Length h;

    Area area() const { return w * h; }
};

bool rect_intersection(Coord c1, Rectangle r1, Coord c2, Rectangle r2);

std::ostream& operator<<(std::ostream &os, Rectangle r);

/******************************************************************************/

struct ItemType
{
    ItemTypeId id;
    Profit profit;
    ItemPos copies;
    Rectangle rect;
    StackId stack;
    bool oriented;
};

std::ostream& operator<<(std::ostream &os, const ItemType& item_type);

struct Defect
{
    DefectId id;
    BinTypeId bin_id;
    Coord pos;
    Rectangle rect;
};

std::ostream& operator<<(std::ostream &os, const Defect& defect);

struct BinType
{
    BinTypeId id;
    Profit cost;
    BinPos copies;
    BinPos copies_min;

    Rectangle rect;
    std::vector<Defect> defects;

    Area previous_bin_area = 0;
    BinPos previous_bin_copies = 0;

    Length  width(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.w: rect.h; }
    Length height(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.h: rect.w; }
};

std::ostream& operator<<(std::ostream &os, const BinType& bin_type);

/********************************** Instance **********************************/

class Instance
{

public:

    /*
     * Constructors and destructor.
     */

    /** Create an instance from a file. */
    Instance(
            Objective objective,
            std::string items_path,
            std::string bins_path,
            std::string defects_path);

    /** Create an instance manually. */
    Instance(Objective objective): objective_(objective) { }

    /** Add an item type. */
    void add_item_type(
            Length w,
            Length h,
            Profit p = -1,
            ItemPos copies = 1,
            bool oriented = false,
            bool new_stack = true);

    /** Add a bin type. */
    void add_bin_type(
            Length w,
            Length h,
            Profit cost = -1,
            BinPos copies = 1,
            BinPos copies_min = 0);

    /** Add a defect. */
    void add_defect(BinTypeId i, Length x, Length y, Length w, Length h);

    /**
     * Add a bin type from another bin type.
     *
     * This method is used in the column generation procedure.
     */
    inline void add_bin_type(
            const BinType& bin_type,
            BinPos copies,
            BinPos copies_min = 0)
    {
        add_bin_type(
                bin_type.rect.w,
                bin_type.rect.h,
                bin_type.cost,
                copies,
                copies_min);
    }

    /**
     * Add an item type from another item type.
     *
     * This method is used in the column generation procedure.
     */
    inline void add_item_type(
            const ItemType& item_type,
            Profit profit,
            ItemPos copies)
    {
        add_item_type(
                item_type.rect.w,
                item_type.rect.h,
                profit,
                copies,
                item_type.oriented);
    }

    /**
     * For each bin type, set an infinite width.
     *
     * This method is used to transform a problem into a Strip Packing problem.
     */
    void set_bin_infinite_width();

    /**
     * For each bin type, set an infinite height.
     *
     * This method is used to transform a problem into a Strip Packing problem.
     */
    void set_bin_infinite_height();

    /**
     * Foe each bin type, set an infinite number of copies.
     *
     * This method should be used after reading bin files of Bin Packing
     * Problems where the number of bin types is not given. By default, the
     * number of bins would be set to 1.
     */
    void set_bin_infinite_copies();

    /**
     * For each bin type, set its cost to its area.
     *
     * This method is used to transform a Variable-sized Bin Packing Problem
     * into an Unweighted Variable-sized Bin Packing Problem.
     */
    void set_bin_unweighted();

    /**
     * For each item type, set an infinite number of copies.
     *
     * This method is used to transform a Knapsack Problem into an Unbounded
     * Knapsack Problem.
     */
    void set_item_infinite_copies();

    /**
     * For each item type, set its profit to its area.
     *
     * This method is used to transform a Knapsack Problem into an Unweighted
     * Knapsack Problem.
     */
    void set_unweighted();

    /*
     * Getters
     */

    /** Get the problem type. */
    inline ProblemType type() const { return ProblemType::RectangleGuillotine; };
    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }
    /** Get the number of items. */
    inline ItemTypeId number_of_items() const { return number_of_items_; }
    /** Get the number of stacks. */
    inline StackId number_of_stacks() const { return stacks_.size(); }
    /** Get the size of stack s. */
    inline ItemPos stack_size(StackId s) const { return stack_sizes_[s]; }
    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return defects_.size(); }
    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }
    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return number_of_bins_; }

    /** Get the total area of the items. */
    inline Area item_area() const { return item_area_; }
    /** Get the mean area of the items. */
    inline Area mean_area() const { return item_area_ / number_of_items(); }
    /** Get the total area of the defects. */
    inline Area defect_area() const { return defect_area_; }
    /** Get the total packable area. */
    inline Area packable_area() const { return packable_area_; }
    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }
    /** Get the id of the item type with maximum efficiency. */
    inline ItemTypeId max_efficiency_item() const { return max_efficiency_item_; }
    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsck() const { return all_item_type_infinite_copies_; }

    /** Get item type j. */
    inline const ItemType& item_type(ItemTypeId j) const { return item_types_[j]; }
    /** Get defect k. */
    inline const Defect& defect(DefectId k) const { return defects_[k]; }
    /** Get bin type i. */
    inline const BinType& bin_type(BinTypeId i) const { return bin_types_[i]; }

    /** Get the j_pos's item of stack s. */
    inline const ItemType& item(StackId s, ItemPos j_pos) const;
    /** Get the i_pos's bin. */
    inline const BinType& bin(BinPos i_pos) const;
    /** Get the total area of the bins before bin i_pos. */
    Area previous_bin_area(BinPos i_pos) const;

    /** Get the item types. */
    inline const std::vector<ItemType>& item_types() const { return item_types_; }
    /** Get stack s. */
    inline const std::vector<ItemType>& stack(StackId s) const { return stacks_[s]; }
    /** Get the stacks. */
    inline const std::vector<std::vector<ItemType>>& stacks() const { return stacks_; }
    /** Get the defects. */
    inline const std::vector<Defect>& defects() const { return defects_; }

    /*
     * Item type dimensions.
     */

    /**
     * Get the width of an item depending on whether it has been rotated and
     * the orientation of the bin.
     */
    inline Length width(const ItemType& item, bool rotate, CutOrientation o) const;

    /**
     * Get the height of an item depending on whether it has been rotated and
     * the orientation of the bin.
     */
    inline Length height(const ItemType& item, bool rotate, CutOrientation o) const;

    /*
     * Defect coordinates.
     */

    /**
     * Get the left coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length left(const Defect& defect, CutOrientation o) const;

    /**
     * Get the right coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length right(const Defect& defect, CutOrientation o) const;

    /**
     * Get the top coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length top(const Defect& defect, CutOrientation o) const;

    /**
     * Get the bottom coordinate of a defect depending on the orientatino of
     * the bin.
     */
    inline Length bottom(const Defect& defect, CutOrientation o) const;

    /*
     * Intersections.
     */

    /**
     * Return the id of a defect intersecting rectangle (l,r,b,t) in bin type i
     * with orientation o.
     *
     * Return -1 if there is none.
     */
    DefectId rect_intersects_defect(
            Length l,
            Length r,
            Length b,
            Length t,
            BinTypeId i,
            CutOrientation o) const;

    /**
     * Return the id of a defect intersecting an item type located at
     * coordinates (l,b) depending on its orientation in bin type i with
     * orientation o.
     *
     * Return -1 if there is none.
     */
    DefectId item_intersects_defect(
            Length l,
            Length b,
            const ItemType& item,
            bool rotate,
            BinTypeId i,
            CutOrientation o) const;

    /**
     * Return the id of a defect intersecting an x-coordinate in bin type i
     * with orientation o.
     *
     * Return -1 if there is none.
     */
    DefectId x_intersects_defect(
            Length x,
            BinTypeId i,
            CutOrientation o) const;

    /**
     * Return the id of a defect intersecting an y-coordinate in bin type i
     * with orientation o.
     *
     * Return -1 if there is none.
     */
    DefectId y_intersects_defect(
            Length l,
            Length r,
            Length y,
            BinTypeId i,
            CutOrientation o) const;

    /*
     * Export.
     */

    /** Write the instance to a file. */
    void write(std::string instance_path) const;

private:

    /*
     * Private attributes.
     */

    /** Objective. */
    Objective objective_;

    /** Item types. */
    std::vector<ItemType> item_types_;
    /** Defects. */
    std::vector<Defect> defects_;
    /** Bin types. */
    std::vector<BinType> bin_types_;

    /** Stacks. */
    std::vector<std::vector<ItemType>> stacks_;

    /** Number of items. */
    ItemPos number_of_items_ = 0;
    /** Number of bins. */
    BinPos number_of_bins_ = 0;
    /** Number of items in each stack. */
    std::vector<ItemPos> stack_sizes_;
    /** Total length (max of width and height) of the items. */
    Length length_sum_ = 0;
    /** Total item area. */
    Area item_area_ = 0;
    /** Total defect area. */
    Area defect_area_ = 0;
    /** Total packable area. */
    Area packable_area_ = 0;
    /** Total item profit. */
    Profit item_profit_ = 0;
    /** Id of the item with maximum efficiency. */
    ItemTypeId max_efficiency_item_ = -1;

    /** True iff all bin types have a single copy. */
    bool all_bin_type_one_copy_ = true;
    /** True iff all item types have a single copy. */
    bool all_item_type_one_copy_ = true;
    /** True iff all item types have an infinite number of copies. */
    bool all_item_type_infinite_copies_ = false;

};

/** Stream insertion operator. */
std::ostream& operator<<(std::ostream &os, const Instance& ins);

/****************************** inlined methods *******************************/

const ItemType& Instance::item(StackId s, ItemPos j_pos) const
{
    assert(j_pos < stack_sizes_[s]);

    if (all_item_type_one_copy_)
        return stacks_[s][j_pos];

    ItemPos j_tmp = 0;
    ItemTypeId j = 0;
    for (;;) {
        if (j_tmp <= j_pos && j_pos < j_tmp + stacks_[s][j].copies) {
            return stacks_[s][j];
        } else {
            j_tmp += stacks_[s][j].copies;
            j++;
        }
    }
}

const BinType& Instance::bin(BinPos i_pos) const
{
    assert(i_pos < number_of_bins_);

    if (all_bin_type_one_copy_)
        return bin_types_[i_pos];

    BinPos i_tmp = 0;
    BinTypeId i = 0;
    for (;;) {
        if (i_tmp <= i_pos && i_pos < i_tmp + bin_types_[i].copies) {
            return bin_types_[i];
        } else {
            i_tmp += bin_types_[i].copies;
            i++;
        }
    }
}

Length Instance::left(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?  defect.pos.x: defect.pos.y;
}

Length Instance::right(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        defect.pos.x + defect.rect.w:
        defect.pos.y + defect.rect.h;
}

Length Instance::top(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        defect.pos.y + defect.rect.h:
        defect.pos.x + defect.rect.w;
}

Length Instance::bottom(const Defect& defect, CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? defect.pos.y: defect.pos.x;
}

Length Instance::width(const ItemType& item, bool rotate, CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.w: item.rect.h;
    } else {
        return (!rotate)? item.rect.h: item.rect.w;
    }
}

Length Instance::height(const ItemType& item, bool rotate, CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.h: item.rect.w;
    } else {
        return (!rotate)? item.rect.w: item.rect.h;
    }
}

}
}

