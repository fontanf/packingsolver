#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

enum class CutType1 { ThreeStagedGuillotine, TwoStagedGuillotine };
enum class CutType2 { Roadef2018, NonExact, Exact, Homogenous };
enum class CutOrientation { Horinzontal, Vertical, Any };

/**
 * Trims are borders of a bin which are not used to pack item.
 *
 * If a trim is 'Hard', then the trimmed part is effectively cut. This implies
 * that the minium waste constraint considers the new border (after the trim).
 *
 * On the other hand, if a trim is 'Soft', then the trimmed part is not cut
 * even if no item can be packed there. However, the minimum waste constraint
 * considers the real border of the plate and not the trim.
 */
enum class TrimType { Soft, Hard };

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

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Item type structure for a problem of type 'rectangleguillotine'.
 */
struct ItemType
{
    /** Id of the item type. */
    ItemTypeId id;
    /** Profit of the item type. */
    Profit profit;
    /** Number of copies of the item type. */
    ItemPos copies;
    /** Dimensions of the item type. */
    Rectangle rect;
    /** Stack id to which the item type belongs. */
    StackId stack;
    /** Indicates if the item is oriented (i.e. cannot be rotated). */
    bool oriented;
};

std::ostream& operator<<(std::ostream &os, const ItemType& item_type);

/**
 * Defect structure for a problem of type 'rectangleguillotine'.
 */
struct Defect
{
    /** Id of the defect. */
    DefectId id;
    /** Bin type of the defect. */
    BinTypeId bin_id;
    /** Position of the defect. */
    Coord pos;
    /** Dimensions of the defect. */
    Rectangle rect;
};

std::ostream& operator<<(std::ostream &os, const Defect& defect);

/**
 * Bin type structure for a problem of type 'rectangleguillotine'.
 */
struct BinType
{
    /** Id of the bin type. */
    BinTypeId id;
    /** Cost of the bin type. */
    Profit cost;
    /** Maximum number of copies of the bin type. */
    BinPos copies;
    /** Minimum number of copies to use of the bin type. */
    BinPos copies_min;
    /** Dimensions of the bin type. */
    Rectangle rect;
    /** Defects of the bin type. */
    std::vector<Defect> defects;

    /** Bottom trim. */
    Length bottom_trim = 0;
    /** Top trim. */
    Length top_trim = 0;
    /** Left trim. */
    Length left_trim = 0;
    /** Right trim. */
    Length right_trim = 0;

    /** Type of the bottom trim. */
    TrimType bottom_trim_type = TrimType::Hard;
    /** Type of the top trim. */
    TrimType top_trim_type = TrimType::Soft;
    /** Type of the left trim. */
    TrimType left_trim_type = TrimType::Hard;
    /** Type of the right trim. */
    TrimType right_trim_type = TrimType::Soft;

    /*
     * Computed attributes.
     */

    /** Total area of the previous bins. */
    Area previous_bin_area = 0;
    /** Number of previous bins. */
    BinPos previous_bin_copies = 0;

    Length width(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.w: rect.h; }
    Length height(CutOrientation o) const { return (o == CutOrientation::Vertical)? rect.h: rect.w; }
};

std::ostream& operator<<(std::ostream &os, const BinType& bin_type);

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Instance class for a problem of type "rectangleguillotine".
 */
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
    ItemTypeId add_item_type(
            Length w,
            Length h,
            Profit p = -1,
            ItemPos copies = 1,
            bool oriented = false,
            bool new_stack = true);

    /** Add a bin type. */
    BinTypeId add_bin_type(
            Length w,
            Length h,
            Profit cost = -1,
            BinPos copies = 1,
            BinPos copies_min = 0);

    /** Add trims to bin type 'i'. */
    void add_trims(
            BinTypeId i,
            Length left_trim,
            TrimType left_trim_type,
            Length right_trim,
            TrimType right_trim_type,
            Length bottom_trim,
            TrimType bottom_trim_type,
            Length top_trim,
            TrimType top_trim_type);

    /** Add a defect. */
    void add_defect(
            BinTypeId i,
            Length x,
            Length y,
            Length w,
            Length h);

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
    inline ItemPos stack_size(StackId s) const { return items_pos2type_[s].size(); }
    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return defects_.size(); }
    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }
    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bins_pos2type_.size(); }

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
    inline const ItemType& item(StackId s, ItemPos j_pos) const { return item_types_[items_pos2type_[s][j_pos]]; }
    /** Get the i_pos's bin. */
    inline const BinType& bin(BinPos i_pos) const { return bin_types_[bins_pos2type_[i_pos]]; }
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
    inline Length width(
            const ItemType& item,
            bool rotate,
            CutOrientation o) const;

    /**
     * Get the height of an item depending on whether it has been rotated and
     * the orientation of the bin.
     */
    inline Length height(
            const ItemType& item,
            bool rotate,
            CutOrientation o) const;

    /**
     * Bin trims.
     */

    /** Get the bottom trim of a bin depending on its orientation. */
    inline Length bottom_trim(
            const BinType& bin_type,
            CutOrientation o) const;

    /** Get the top trim of a bin depending on its orientation. */
    inline Length top_trim(
            const BinType& bin_type,
            CutOrientation o) const;

    /** Get the left trim of a bin depending on its orientation. */
    inline Length left_trim(
            const BinType& bin_type,
            CutOrientation o) const;

    /** Get the right trim of a bin depending on its orientation. */
    inline Length right_trim(
            const BinType& bin_type,
            CutOrientation o) const;

    /**
     * Get the type of the bottom trim of a bin depending on its orientation.
     */
    inline TrimType bottom_trim_type(
            const BinType& bin_type,
            CutOrientation o) const;

    /**
     * Get the type of the top trim of a bin depending on its orientation.
     */
    inline TrimType top_trim_type(
            const BinType& bin_type,
            CutOrientation o) const;

    /**
     * Get the type of the left trim of a bin depending on its orientation.
     */
    inline TrimType left_trim_type(
            const BinType& bin_type,
            CutOrientation o) const;

    /**
     * Get the type of the right trim of a bin depending on its orientation.
     */
    inline TrimType right_trim_type(
            const BinType& bin_type,
            CutOrientation o) const;

    /*
     * Defect coordinates.
     */

    /**
     * Get the left coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length left(
            const Defect& defect,
            CutOrientation o) const;

    /**
     * Get the right coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length right(
            const Defect& defect,
            CutOrientation o) const;

    /**
     * Get the top coordinate of a defect depending on the orientatino of the
     * bin.
     */
    inline Length top(
            const Defect& defect,
            CutOrientation o) const;

    /**
     * Get the bottom coordinate of a defect depending on the orientatino of
     * the bin.
     */
    inline Length bottom(
            const Defect& defect,
            CutOrientation o) const;

    /*
     * Intersections.
     */

    /**
     * Return the id of a defect intersecting rectangle (l,r,b,t) in bin type i
     * with orientation o.
     *
     * Return -1 if there is none.
     */
    inline DefectId rect_intersects_defect(
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
    inline DefectId item_intersects_defect(
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
    inline DefectId x_intersects_defect(
            Length x,
            BinTypeId i,
            CutOrientation o) const;

    /**
     * Return the id of a defect intersecting an y-coordinate in bin type i
     * with orientation o.
     *
     * Return -1 if there is none.
     */
    inline DefectId y_intersects_defect(
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
    /** Convert item position to item type. */
    std::vector<std::vector<BinTypeId>> items_pos2type_;
    /** Convert bin position to bin type. */
    std::vector<BinTypeId> bins_pos2type_;
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

    /** True iff all item types have an infinite number of copies. */
    bool all_item_type_infinite_copies_ = false;

};

/** Stream insertion operator. */
std::ostream& operator<<(std::ostream &os, const Instance& ins);

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Length Instance::bottom_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? bin_type.bottom_trim: bin_type.left_trim;
}

Length Instance::top_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? bin_type.top_trim: bin_type.right_trim;
}

Length Instance::left_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? bin_type.left_trim: bin_type.top_trim;
}

Length Instance::right_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? bin_type.right_trim: bin_type.bottom_trim;
}

TrimType Instance::bottom_trim_type(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.bottom_trim_type:
        bin_type.left_trim_type;
}

TrimType Instance::top_trim_type(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.top_trim_type:
        bin_type.right_trim_type;
}

TrimType Instance::left_trim_type(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.left_trim_type:
        bin_type.top_trim_type;
}

TrimType Instance::right_trim_type(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.right_trim_type:
        bin_type.bottom_trim_type;
}

Length Instance::left(
        const Defect& defect,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? defect.pos.x: defect.pos.y;
}

Length Instance::right(
        const Defect& defect,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        defect.pos.x + defect.rect.w:
        defect.pos.y + defect.rect.h;
}

Length Instance::top(
        const Defect& defect,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        defect.pos.y + defect.rect.h:
        defect.pos.x + defect.rect.w;
}

Length Instance::bottom(
        const Defect& defect,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? defect.pos.y: defect.pos.x;
}

Length Instance::width(
        const ItemType& item,
        bool rotate,
        CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.w: item.rect.h;
    } else {
        return (!rotate)? item.rect.h: item.rect.w;
    }
}

Length Instance::height(
        const ItemType& item,
        bool rotate,
        CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item.rect.h: item.rect.w;
    } else {
        return (!rotate)? item.rect.w: item.rect.h;
    }
}

DefectId Instance::rect_intersects_defect(
        Length l,
        Length r,
        Length b,
        Length t,
        BinTypeId i,
        CutOrientation o) const
{
    assert(l <= r);
    assert(b <= t);
    for (const Defect& defect: bin(i).defects) {
        if (left(defect, o) >= r)
            continue;
        if (l >= right(defect, o))
            continue;
        if (bottom(defect, o) >= t)
            continue;
        if (b >= top(defect, o))
            continue;
        return defect.id;
    }
    return -1;
}

DefectId Instance::item_intersects_defect(
        Length l,
        Length b,
        const ItemType& item_type,
        bool rotate,
        BinTypeId i,
        CutOrientation o) const
{
    return rect_intersects_defect(
            l, l + width(item_type, rotate, o),
            b, b + height(item_type, rotate, o),
            i, o);
}

DefectId Instance::y_intersects_defect(
        Length l,
        Length r,
        Length y,
        BinTypeId i,
        CutOrientation o) const
{
    DefectId k_min = -1;
    for (const Defect& k: bin(i).defects) {
        if (right(k, o) <= l || left(k, o) >= r)
            continue;
        if (bottom(k, o) >= y || top(k, o) <= y)
            continue;
        if (k_min == -1 || left(k, o) < left(defect(k_min), o))
            k_min = k.id;
    }
    return k_min;
}

DefectId Instance::x_intersects_defect(
        Length x,
        BinTypeId i,
        CutOrientation o) const
{
    for (const Defect& k: bin(i).defects)
        if (left(k, o) < x && right(k, o) > x)
            return k.id;
    return -1;
}

}
}

