#pragma once

#include "packingsolver/algorithms/common.hpp"

#include "packingsolver/irregular/shape.hpp"

namespace packingsolver
{
namespace irregular
{

struct ItemShape
{
    /** Main shape. */
    Shape shape;

    /**
     * Holes.
     *
     * Holes are shapes contained inside the main shape.
     */
    std::vector<Shape> holes;

    /** Quality rule. */
    QualityRule quality_rule = 0;

    bool check() const;

    std::string to_string(Counter indentation) const;
};

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct Parameters
{
    /**
     * Quality rules.
     *
     * if 'quality_rules_[quality_rule][k] = 0' (resp. '1'), then defect 'k' is
     * not allowed (resp. allowed) for qulity rule rule 'quality_rule'.
     */
    std::vector<std::vector<uint8_t>> quality_rules;

    /** Minimum distance between two items. */
    LengthDbl item_item_minimum_spacing = 0.0;

    /** Minimum distance between and item and a bin. */
    LengthDbl item_bin_minimum_spacing = 0.0;
};

/**
 * Defect structure for a problem of type 'irregular'.
 */
struct Defect
{
    /** Shape. */
    Shape shape;

    /** Holes. */
    std::vector<Shape> holes;

    /** Type of the defect. */
    DefectTypeId type = -1;

    std::string to_string(Counter indentation) const;
};

/**
 * Bin type structure for a problem of type 'irregular'.
 */
struct BinType
{
    /** Cost of the bin type. */
    Profit cost;

    /** Maximum number of copies of the bin type. */
    BinPos copies;

    /** Minimum number of copies to use of the bin type. */
    BinPos copies_min;

    /** Shape of the bin type. */
    Shape shape;

    /** Defects of the bin type. */
    std::vector<Defect> defects;

    /*
     * Computed attributes.
     */

    /** Area of the bin type. */
    AreaDbl area = 0.0;

    /** Minimum x of the item type. */
    LengthDbl x_min;

    /** Maximum x of the item type. */
    LengthDbl x_max;

    /** Minimum y of the item type. */
    LengthDbl y_min;

    /** Maximum y of the item type. */
    LengthDbl y_max;

    AreaDbl space() const { return area; }

    AreaDbl packable_area(QualityRule quality_rule) const { (void)quality_rule; return 0; } // TODO

    std::string to_string(Counter indentation) const;

    void write_svg(
            const std::string& file_path) const;
};

/**
 * Item type structure for a problem of type 'irregular'.
 */
struct ItemType
{
    /** Profit of the item type. */
    Profit profit;

    /** Number of copies of the item type. */
    ItemPos copies;

    /**
     * Shape of the item type.
     *
     * The shape is composed of multiple non-overlaping sub-shapes which may
     * follow different quality rules.
     */
    std::vector<ItemShape> shapes;

    /** Allowed rotations of the item type. */
    std::vector<std::pair<Angle, Angle>> allowed_rotations = {{0, 0}};

    /** Allow mirroring the item type. */
    bool allow_mirroring = false;

    /*
     * Computed attributes.
     */

    /** Area of the item type. */
    AreaDbl area = 0;

    AreaDbl space() const { return area; }

    /** Return type of shape of the item type. */
    ShapeType shape_type() const;

    std::pair<Point, Point> compute_min_max(
            Angle angle = 0.0,
            bool mirror = false) const;

    bool has_full_continuous_rotations() const;

    bool has_only_discrete_rotations() const;

    std::string to_string(Counter indentation) const;

    void write_svg(
            const std::string& file_path) const;
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Instance class for a problem of type "irregular".
 */
class Instance
{

public:

    /*
     * Getters
     */

    /** Get the problem type. */
    static inline ProblemType type() { return ProblemType::Irregular; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /*
     * Getters: parameters
     */

    /** Get parameters. */
    const Parameters& parameters() const { return parameters_; }

    /**
     * Return 'true' iff quality_rule 'quality_rule' can contain a defect of
     * type 'type'.
     */
    bool can_contain(QualityRule quality_rule, DefectTypeId type) const;

    /*
     * Getters: bin types
     */

    /** Get the number of bin types. */
    inline BinTypeId number_of_bin_types() const { return bin_types_.size(); }

    /** Get bin type i. */
    inline const BinType& bin_type(BinTypeId bin_type_id) const { return bin_types_[bin_type_id]; }

    /** Get the number of bins. */
    inline BinPos number_of_bins() const { return bin_type_ids_.size(); }

    /** Get the i_pos's bin. */
    inline BinTypeId bin_type_id(BinPos bin_pos) const { return bin_type_ids_[bin_pos]; }

    /** Get the total area of the bins before bin i_pos. */
    inline AreaDbl previous_bin_area(BinPos bin_pos) const { return previous_bins_area_[bin_pos]; }

    /** Get the total area of the bins. */
    inline AreaDbl bin_area() const { return bin_area_; }

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

    /** Get the number rectangular of items. */
    inline ItemPos number_of_rectangular_items() const { return number_of_rectangular_items_; }

    /** Get the number circular of items. */
    inline ItemPos number_of_circular_items() const { return number_of_circular_items_; }

    /** Get the total area of the items. */
    inline AreaDbl item_area() const { return item_area_; }

    /** Get the mean area of the items. */
    inline AreaDbl mean_area() const { return item_area_ / number_of_items(); }

    /** Get the smallest area of the items. */
    inline AreaDbl smallest_item_area() const { return smallest_item_area_; }

    /** Get the largest area of the items. */
    inline AreaDbl largest_item_area() const { return largest_item_area_; }

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

    /** Convert bin position to bin type. */
    std::vector<BinTypeId> bin_type_ids_;

    /** For each bin position, the area of the previous bins. */
    std::vector<AreaDbl> previous_bins_area_;

    /** Total bin area. */
    AreaDbl bin_area_ = 0;

    /** Largest bin cost. */
    Profit largest_bin_cost_ = 0.0;

    /** Number of defects. */
    DefectId number_of_defects_ = 0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Number of rectangular items. */
    ItemPos number_of_rectangular_items_ = 0;

    /** Number of circular items. */
    ItemPos number_of_circular_items_ = 0;

    /** Total item area. */
    AreaDbl item_area_ = 0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Largest item profit. */
    Profit largest_item_profit_ = 0.0;

    /** Smallest item area. */
    AreaDbl smallest_item_area_ = std::numeric_limits<AreaDbl>::infinity();

    /** Largest item area. */
    AreaDbl largest_item_area_ = 0.0;

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
