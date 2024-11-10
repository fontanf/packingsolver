#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

enum class CutType { Roadef2018, NonExact, Exact, Homogenous };
enum class CutOrientation { Horizontal, Vertical, Any };

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

std::istream& operator>>(
        std::istream& in,
        CutType& cut_type_2);

std::istream& operator>>(
        std::istream& in,
        CutOrientation& o);

std::istream& operator>>(
        std::istream& in,
        TrimType& trim_type);

std::ostream& operator<<(
        std::ostream& os,
        CutType cut_type_2);

std::ostream& operator<<(
        std::ostream& os,
        CutOrientation o);

std::ostream& operator<<(
        std::ostream& os,
        TrimType trim_type);

struct Coord
{
    /** x-coordinate. */
    Length x;

    /** y-coordinate. */
    Length y;
};

std::ostream& operator<<(
        std::ostream& os,
        Coord xy);

struct Rectangle
{
    /** Width. */
    Length w;

    /** Height. */
    Length h;

    /** Get the area of the rectangle. */
    Area area() const { return w * h; }

    /** Get the length of the largest side of the rectangle. */
    Length max() const { return std::max(w, h); }
};

bool rect_intersection(
        Coord c1,
        Rectangle r1,
        Coord c2,
        Rectangle r2);

std::ostream& operator<<(
        std::ostream& os,
        Rectangle r);

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct Parameters
{
    /** Number of stages. */
    Counter number_of_stages = 3;

    /** Cut type. */
    CutType cut_type = CutType::NonExact;

    /** Orientation of the first stage. */
    CutOrientation first_stage_orientation = CutOrientation::Vertical;

    /** Minimum distance between two consecutive 1-cuts. */
    Length minimum_distance_1_cuts = 0;

    /** Maximum distance between two consecutive 1-cuts. */
    Length maximum_distance_1_cuts = -1;

    /** Minimum distance between two consecutive 2-cuts. */
    Length minimum_distance_2_cuts = 0;

    /** Minimum distance between two cuts. */
    Length minimum_waste_length = 1;

    /**
     * Maximum number of 2-cuts in a first-level sub-plate.
     */
    Counter maximum_number_2_cuts = -1;

    /** Boolean indicating whether it is allowed to cut through defects. */
    bool cut_through_defects = false;

    /** Cut thickness. */
    Length cut_thickness = 0;
};

/**
 * Defect structure for a problem of type 'rectangleguillotine'.
 */
struct Defect
{
    /** Id of the defect. */
    DefectId id;

    /** Bin type of the defect. */
    BinTypeId bin_type_id;

    /** Position of the defect. */
    Coord pos;

    /** Dimensions of the defect. */
    Rectangle rect;
};

std::ostream& operator<<(
        std::ostream& os,
        const Defect& defect);

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
     * Computed attributes
     */

    /** Get the area of the bin type. */
    inline Area area() const { return (rect.h - top_trim - bottom_trim) * (rect.w - right_trim - left_trim); }

    inline Area space() const { return area(); }
};

std::ostream& operator<<(
        std::ostream& os,
        const BinType& bin_type);

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
    StackId stack_id;

    /** Indicates if the item is oriented (i.e. cannot be rotated). */
    bool oriented;

    /*
     * Computed attributes
     */

    /** Get the area of the item type. */
    inline Area area() const { return rect.area(); }

    inline Area space() const { return area(); }
};

std::ostream& operator<<(
        std::ostream& os,
        const ItemType& item_type);

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
     * Getters
     */

    /** Get the problem type. */
    static inline ProblemType type() { return ProblemType::RectangleGuillotine; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /*
     * Getters: parameters
     */

    inline const Parameters& parameters() const { return parameters_; }

    /*
     * Getters: bin types
     */

    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }

    /** Get bin type i. */
    inline const BinType& bin_type(BinTypeId bin_type_id) const { return bin_types_[bin_type_id]; }

    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bin_type_ids_.size(); }

    /** Get the bin area. */
    inline Area bin_area() const { return bins_area_sum_; }

    /** Get the id of a bin at a given position. */
    inline BinTypeId bin_type_id(BinPos bin_pos) const { return bin_type_ids_[bin_pos]; }

    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return number_of_defects_; }

    /** Get the total area of the bins before a given position. */
    inline Area previous_bin_area(BinPos bin_pos) const { return previous_bins_area_[bin_pos]; }

    /*
     * Getters: bin type dimensions
     */

    /** Get the width of a bin type depending on its orientation. */
    inline Length width(
            const BinType& bin_type,
            CutOrientation o) const;

    /** Get the height of a bin type depending on its orientation. */
    inline Length height(
            const BinType& bin_type,
            CutOrientation o) const;

    /**
     * Getters: bin type trims
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
     * Getters: defect coordinates
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
     * Getters: item types
     */

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }

    /** Get item type j. */
    inline const ItemType& item_type(ItemTypeId item_type_id) const { return item_types_[item_type_id]; }

    /** Get the number of items. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Get the number of stacks. */
    inline StackId number_of_stacks() const { return item_type_ids_.size(); }

    /** Get the size of stack s. */
    inline ItemPos stack_size(StackId s) const { return item_type_ids_[s].size(); }

    /** Get the item_pos's item of stack s. */
    inline ItemTypeId item(StackId s, ItemPos item_pos) const { return item_type_ids_[s][item_pos]; }

    /** Get the total area of the items. */
    inline Area item_area() const { return item_area_; }

    /** Get the mean area of the items. */
    inline Area mean_area() const { return item_area_ / number_of_items(); }

    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }

    /** Get the id of the item type with maximum efficiency. */
    inline ItemTypeId max_efficiency_item_type_id() const { return max_efficiency_item_type_id_; }

    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsck() const { return all_item_types_infinite_copies_; }

    /** Get the item types. */
    inline const std::vector<ItemType>& item_types() const { return item_types_; }

    /*
     * Getters: item type dimensions
     */

    /**
     * Get the width of an item depending on whether it has been rotated and
     * the orientation of the bin.
     */
    inline Length width(
            const ItemType& item_type,
            bool rotate,
            CutOrientation o) const;

    /**
     * Get the height of an item depending on whether it has been rotated and
     * the orientation of the bin.
     */
    inline Length height(
            const ItemType& item_type,
            bool rotate,
            CutOrientation o) const;

    /*
     * Intersections
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
            const BinType& bin_type,
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
            const BinType& bin_type,
            CutOrientation o) const;

    /**
     * Return the id of a defect intersecting an x-coordinate in bin type i
     * with orientation o.
     *
     * Return -1 if there is none.
     */
    inline DefectId x_intersects_defect(
            Length x,
            const BinType& bin_type,
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
            const BinType& bin_type,
            CutOrientation o) const;

    /*
     * Export
     */

    /** Print the instance into a stream. */
    std::ostream& format(
            std::ostream& os,
            int verbosity_level = 1) const;

    /** Write the instance to a file. */
    void write(const std::string& instance_path) const;

private:

    /*
     * Private methods
     */

    /** Create an instance manually. */
    Instance() { }

    /*
     * Private attributes
     */

    /** Objective. */
    Objective objective_;

    /** Parameters. */
    Parameters parameters_;

    /** Bin types. */
    std::vector<BinType> bin_types_;

    /** Item types. */
    std::vector<ItemType> item_types_;

    /*
     * Private attributes computed by the 'build' method
     */

    /** For each bin position, the corresponding bin type. */
    std::vector<BinTypeId> bin_type_ids_;

    /** For each bin position, the area of the previous bins. */
    std::vector<Area> previous_bins_area_;

    /** Bins area sum. */
    Area bins_area_sum_ = 0;

    /** Number of defects. */
    DefectId number_of_defects_ = 0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Convert item position to item type. */
    std::vector<std::vector<ItemTypeId>> item_type_ids_;

    /** Total item area. */
    Area item_area_ = 0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Id of the item with maximum efficiency. */
    ItemTypeId max_efficiency_item_type_id_ = -1;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_types_infinite_copies_ = false;

    friend class InstanceBuilder;

};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Inlined methods ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

inline Length Instance::width(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? bin_type.rect.w: bin_type.rect.h;
}

inline Length Instance::height(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)? bin_type.rect.h: bin_type.rect.w;
}

Length Instance::bottom_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.bottom_trim:
        bin_type.left_trim;
}

Length Instance::top_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.top_trim:
        bin_type.right_trim;
}

Length Instance::left_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.left_trim:
        bin_type.bottom_trim;
}

Length Instance::right_trim(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.right_trim:
        bin_type.top_trim;
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
        bin_type.bottom_trim_type;
}

TrimType Instance::right_trim_type(
        const BinType& bin_type,
        CutOrientation o) const
{
    return (o == CutOrientation::Vertical)?
        bin_type.right_trim_type:
        bin_type.top_trim_type;
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
        const ItemType& item_type,
        bool rotate,
        CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item_type.rect.w: item_type.rect.h;
    } else {
        return (!rotate)? item_type.rect.h: item_type.rect.w;
    }
}

Length Instance::height(
        const ItemType& item_type,
        bool rotate,
        CutOrientation o) const
{
    if (o == CutOrientation::Vertical) {
        return (!rotate)? item_type.rect.h: item_type.rect.w;
    } else {
        return (!rotate)? item_type.rect.w: item_type.rect.h;
    }
}

DefectId Instance::rect_intersects_defect(
        Length l,
        Length r,
        Length b,
        Length t,
        const BinType& bin_type,
        CutOrientation o) const
{
    assert(l <= r);
    assert(b <= t);
    for (const Defect& defect: bin_type.defects) {
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
        const BinType& bin_type,
        CutOrientation o) const
{
    return rect_intersects_defect(
            l, l + width(item_type, rotate, o),
            b, b + height(item_type, rotate, o),
            bin_type, o);
}

DefectId Instance::y_intersects_defect(
        Length l,
        Length r,
        Length y,
        const BinType& bin_type,
        CutOrientation o) const
{
    DefectId k_min = -1;
    for (const Defect& k: bin_type.defects) {
        if (right(k, o) <= l || left(k, o) >= r)
            continue;
        if (bottom(k, o) >= y || top(k, o) <= y)
            continue;
        if (k_min == -1 || left(k, o) < left(bin_type.defects[k_min], o))
            k_min = k.id;
    }
    return k_min;
}

DefectId Instance::x_intersects_defect(
        Length x,
        const BinType& bin_type,
        CutOrientation o) const
{
    for (const Defect& k: bin_type.defects)
        if (left(k, o) < x && right(k, o) > x)
            return k.id;
    return -1;
}

}
}
