#pragma once

#include "packingsolver/algorithms/common.hpp"
#include "packingsolver/algorithms/truck.hpp"

namespace packingsolver
{
namespace rectangle
{

enum class Direction { X, Y, Any };

std::istream& operator>>(
        std::istream& in,
        Direction& o);

std::ostream& operator<<(
        std::ostream& os,
        Direction o);


enum class UnloadingConstraint
{
    None,
    OnlyXMovements,
    OnlyYMovements,
    IncreasingX,
    IncreasingY,
};

std::istream& operator>>(
        std::istream& in,
        UnloadingConstraint& unloading_constraint);

std::ostream& operator<<(
        std::ostream& os,
        UnloadingConstraint unloading_constraint);

struct Point
{
    /** x-coordinate. */
    Length x;

    /** y-coordinate. */
    Length y;
};

std::ostream& operator<<(
        std::ostream& os,
        Point xy);

struct Rectangle
{
    /** Width. */
    Length x;

    /** Height. */
    Length y;

    /** Get the area of the rectangle. */
    Area area() const { return x * y; }

    /** Get the legnth of the largest side of the rectangle. */
    Length max() const { return std::max(x, y); }

    /** Get the legnth of the largest side of the rectangle. */
    Length min() const { return std::min(x, y); }
};

bool rect_intersection(Point c1, Rectangle r1, Point c2, Rectangle r2);
bool rect_intersection(
        Length x1, Length x2, Length y1, Length y2,
        Length x3, Length x4, Length y3, Length y4);

std::ostream& operator<<(
        std::ostream& os,
        Rectangle r);

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Group structure.
 *
 * Item types from the same group are delivered together.
 *
 * The way groups are packed in the bins depends on the unloading constraint
 * type of the instance.
 */
struct Group
{
    /** Item types belonging to this group. */
    std::vector<ItemTypeId> item_types;

    /** Number of items. */
    ItemPos number_of_items = 0;
};

/**
 * Item type structure for a problem of type 'rectangle'.
 */
struct ItemType
{
    /** Dimensions of the item type. */
    Rectangle rect;

    /** Profit of the item type. */
    Profit profit;

    /** Number of copies of the item type. */
    ItemPos copies;

    /** Group of the item type for the unloading constraint. */
    GroupId group_id;

    /** Indicates if the item is oriented (i.e. cannot be rotated). */
    bool oriented;

    /** Weight of the item type. */
    Weight weight = 0;

    /*
     * Computed attributes
     */

    /** Get the width of the item depending on its orientation. */
    inline Length x(bool rotate) const { return (rotate)? rect.y: rect.x; }

    /** Get the height of the item depending on its orientation. */
    inline Length y(bool rotate) const { return (rotate)? rect.x: rect.y; }

    /** Get the area of the item type. */
    inline Area area() const { return rect.area(); }

    inline Area space() const { return area(); }
};

std::ostream& operator<<(
        std::ostream& os,
        const ItemType& item_type);

/**
 * Defect structure for a problem of type 'rectangle'.
 */
struct Defect
{
    /** Bin type of the defect. */
    BinTypeId bin_type_id;

    /** Position of the defect. */
    Point pos;

    /** Dimensions of the defect. */
    Rectangle rect;

    /*
     * Computed attributes
     */

    /** Get the left coordinate of the defect. */
    inline Length x_start() const { return pos.x; }

    /** Get the right coordinate of the defect. */
    inline Length x_end() const { return pos.x + rect.x; }

    /** Get the bottom coordinate of the defect. */
    inline Length y_start() const { return pos.y; }

    /** Get the top coordinate of the defect. */
    inline Length y_end() const { return pos.y + rect.y; }
};

std::ostream& operator<<(
        std::ostream& os,
        const Defect& defect);

/**
 * Bin type structure for a problem of type 'rectangle'.
 */
struct BinType
{
    /** Dimensions of the bin type. */
    Rectangle rect;

    /** Cost of the bin type. */
    Profit cost;

    /** Maximum number of copies of the bin type. */
    BinPos copies;

    /** Minimum number of copies to use of the bin type. */
    BinPos copies_min;

    /** Maximum weight. */
    Weight maximum_weight = 0;

    /** Semi-trailer truck data. */
    SemiTrailerTruckData semi_trailer_truck_data;

    /** Defects of the bin type. */
    std::vector<Defect> defects;

    /*
     * Computed attributes
     */

    /** Get the area of the bin type. */
    inline Area area() const { return rect.x * rect.y; }

    inline Area space() const { return area(); }
};

std::ostream& operator<<(
        std::ostream& os,
        const BinType& bin_type);

struct Parameters
{
    /** Unloading constraint. */
    UnloadingConstraint unloading_constraint = UnloadingConstraint::None;

    /**
     * 'true' iff weight constraints must be satisfied for all items belonging
     * to a larger or equal group.
     */
    std::vector<bool> check_weight_constraints;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Instance class for a problem of type "rectangle".
 */
class Instance
{

public:

    /*
     * Getters
     */

    /** Get the problem type. */
    static inline ProblemType type() { return ProblemType::Rectangle; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /*
     * Getters: parameters
     */

    /** Get parameters. */
    const Parameters& parameters() const { return parameters_; }

    /** Get unloading constraint type. */
    inline UnloadingConstraint unloading_constraint() const { return parameters_.unloading_constraint; }

    inline bool check_weight_constraints(GroupId group_id) const { if (group_id >= (GroupId)parameters_.check_weight_constraints.size()) return true; return parameters_.check_weight_constraints[group_id]; }

    /*
     * Getters: bin types
     */

    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }

    /** Get a bin type. */
    inline const BinType& bin_type(BinTypeId bin_type_id) const { return bin_types_[bin_type_id]; }

    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bin_type_ids_.size(); }

    /** Get the id of a bin at a given position. */
    inline BinTypeId bin_type_id(BinPos bin_pos) const { return bin_type_ids_[bin_pos]; }

    /** Get the total area of the bins. */
    inline Area bin_area() const { return bin_area_; }

    /** Get the total weight of the bins. */
    inline Weight bin_weight() const { return bin_weight_; }

    /** Get the total area of the bins before bin i_pos. */
    inline Area previous_bin_area(BinPos bin_pos) const { return previous_bins_area_[bin_pos]; }

    /** Get the largest cost of the bins. */
    inline Profit largest_bin_cost() const { return largest_bin_cost_; }

    /** Get the number of defects. */
    inline DefectId number_of_defects() const { return number_of_defects_; }

    /*
     * Getters: item types
     */

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }

    /** Get an item type. */
    inline const ItemType& item_type(ItemTypeId item_type_id) const { return item_types_[item_type_id]; }

    /** Get the number of items. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Get the number of groups. */
    inline StackId number_of_groups() const { return groups_.size(); }

    /** Get the size of group 'group_id'. */
    inline ItemPos group_size(GroupId group_id) const { return groups_[group_id].number_of_items; }

    /** Get a group. */
    inline const Group& group(GroupId group_id) const { return groups_[group_id]; }

    /** Get the total area of the items. */
    inline Area item_area() const { return item_area_; }

    /** Get the total width of the items. */
    inline Length total_item_width() const { return total_item_width_; }

    /** Get the total height of the items. */
    inline Length total_item_height() const { return total_item_height_; }

    /** Get the smallest width of the items. */
    inline Length smallest_item_width() const { return smallest_item_width_; }

    /** Get the smallest height of the items. */
    inline Length smallest_item_height() const { return smallest_item_height_; }

    /** Get the mean area of the items. */
    inline Area mean_area() const { return item_area_ / number_of_items(); }

    /** Get the total weight of the items. */
    inline Weight item_weight() const { return item_weight_; }

    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }

    /** Get the largest profit of the items. */
    inline Profit largest_item_profit() const { return largest_item_profit_; }

    /** Get the id of the item type with largest efficiency. */
    inline ItemTypeId largest_efficiency_item_type_id() const { return largest_efficiency_item_type_id_; }

    /** Get the largest number of copies of the items. */
    inline ItemPos largest_item_copies() const { return largest_item_copies_; }

    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsack() const { return all_item_types_infinite_copies_; }

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

    /** Total bin area. */
    Volume bin_area_ = 0;

    /** Total weight of the bins. */
    Weight bin_weight_ = 0.0;

    /** Largest bin cost. */
    Profit largest_bin_cost_ = 0.0;

    /** Number of defects. */
    DefectId number_of_defects_ = 0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Groups. */
    std::vector<Group> groups_;

    /** Total item area. */
    Area item_area_ = 0;

    /** Total item width. */
    Length total_item_width_ = 0;

    /** Total item height. */
    Length total_item_height_ = 0;

    /** Smallest item height. */
    Length smallest_item_height_ = 0;

    /** Smallest item width. */
    Length smallest_item_width_ = 0;

    /** Total weight of the items. */
    Weight item_weight_ = 0.0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Largest item profit. */
    Profit largest_item_profit_ = 0.0;

    /** Id of the item with largest efficiency. */
    ItemTypeId largest_efficiency_item_type_id_ = -1;

    /** Largest item copies. */
    ItemPos largest_item_copies_ = 0;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_types_infinite_copies_ = false;

    friend class InstanceBuilder;

};

}
}
