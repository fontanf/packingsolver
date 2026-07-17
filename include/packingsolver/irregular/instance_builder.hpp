#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

class InstanceBuilder
{

public:

    /** Constructor. */
    InstanceBuilder() { }

    /** Set the objective. */
    void set_objective(Objective objective) { instance_.objective_ = objective; }

    /*
     * Read from files
     */

    /** Read item types from a file. */
    void read(std::string instance_path);

    /*
     * Set parameters
     */

    /** Set parameters. */
    void set_parameters(const Parameters& parameters) { instance_.parameters_ = parameters; }

    /** Set the feasibility callback. */
    void set_feasibility_callback(const FeasibilityCallback& feasibility_callback) { instance_.feasibility_callback_ = feasibility_callback; }

    /** Add a quality rule. */
    inline void add_quality_rule(
            const std::vector<uint8_t>& quality_rule);

    /** Set item-item minimum spacing. */
    void set_item_item_minimum_spacing(LengthDbl item_item_minimum_spacing) { instance_.parameters_.item_item_minimum_spacing = item_item_minimum_spacing; }

    /** Set the aspect ratio for the open dimension xy objective. */
    void set_open_dimension_xy_aspect_ratio(LengthDbl open_dimension_xy_aspect_ratio) { instance_.parameters_.open_dimension_xy_aspect_ratio = open_dimension_xy_aspect_ratio; }

    /** Set the leftover mode. */
    void set_leftover_mode(LeftoverMode leftover_mode) { instance_.parameters_.leftover_mode = leftover_mode; }

    /*
     * Set bin types
     */

    /** Add a bin type. */
    BinTypeId add_bin_type(
            const Shape& shape);

    /** Set the cost of a bin type. */
    void set_bin_type_cost(
            BinTypeId bin_type_id,
            Profit cost);

    /** Add a defect. */
    DefectId add_defect(
            BinTypeId bin_type_id,
            DefectTypeId type,
            const ShapeWithHoles& shape);

    /** Set item-bin minimum spacing. */
    void set_item_bin_minimum_spacing(
            BinTypeId bin_type_id,
            LengthDbl item_bin_minimum_spacing);

    /** Set item-defect minimum spacing. */
    void set_item_defect_minimum_spacing(
            BinTypeId bin_type_id,
            DefectId defect_id,
            LengthDbl item_defect_minimum_spacing);

    /**
     * Add a bin type from another bin type.
     *
     * This method is used in the column generation procedure.
     */
    BinTypeId add_bin_type(
            const Instance& original_instance,
            BinTypeId original_bin_type_id);

    /** Set the number of copies of a bin type. */
    void set_bin_type_copies(
            BinTypeId bin_type_id,
            BinPos copies);

    /** Set the minimum number of copies of a bin type. */
    void set_bin_type_copies_min(
            BinTypeId bin_type_id,
            BinPos copies_min);

    /**
     * Foe each bin type, set an infinite number of copies.
     *
     * This method should be used after reading bin files of Bin Packing
     * Problems where the number of bin types is not given. By default, the
     * number of bins would be set to 1.
     */
    void set_bin_types_infinite_copies();

    /**
     * For each bin type, set its cost to its area.
     *
     * This method is used to transform a Variable-sized Bin Packing Problem
     * into an Unweighted Variable-sized Bin Packing Problem.
     */
    void set_bin_types_unweighted();

    /*
     * Set item types
     */

    /** Add an item type. */
    ItemTypeId add_item_type(
            const std::vector<ItemShape>& shapes);

    /**
     * Add an allowed rotation to an item type.
     *
     * Appends an entry with a continuous range [start_angle, end_angle] and
     * the given mirror flag to the item type's allowed_rotations list.
     * The first call to this function replaces the default {{0, 0, false}}
     * entry created by add_item_type.
     */
    void add_item_type_allowed_rotation(
            ItemTypeId item_type_id,
            Angle start_angle,
            Angle end_angle,
            bool mirror);

    /**
     * Add an item type from another item type.
     *
     * This method is used in the column generation procedure.
     */
    ItemTypeId add_item_type(
            const Instance& original_instance,
            ItemTypeId original_item_type_id);

    /** Set the profit of an item type. */
    void set_item_type_profit(
            ItemTypeId item_type_id,
            Profit profit);

    /** Set the number of copies of an item type. */
    void set_item_type_copies(
            ItemTypeId item_type_id,
            ItemPos copies);

    /**
     * For each item type, set its profit to its area.
     *
     * This method is used to transform a Knapsack Problem into an Unweighted
     * Knapsack Problem.
     */
    void set_item_types_unweighted();

    /** Set continuous rotations for all item types. */
    void set_item_types_continuous_rotations();

    /** Add a fixed item (a placement that is already determined before the search). */
    void add_fixed_item(
            BinTypeId bin_type_id,
            ItemTypeId item_type_id,
            Point bl_corner,
            Angle angle,
            bool mirror)
    {
        instance_.bin_types_[bin_type_id].fixed_items.push_back({item_type_id, bl_corner, angle, mirror});
    }

    /*
     * Build
     */

    /** Build. */
    Instance build();

private:

    /*
     * Private methods
     */

    /** Compute item types max length sum. */
    LengthDbl compute_item_types_max_length_sum() const;

    /** Compute bin types area max. */
    AreaDbl compute_bin_types_area_max() const;

    /*
     * Private attributes
     */

    /** Instance. */
    Instance instance_;

    /** Mapping from original bin type IDs to sub-instance bin type IDs. */
    std::vector<BinTypeId> orig_to_sub_bin_type_ids_;

    /** Mapping from original item type IDs to sub-instance item type IDs. */
    std::vector<ItemTypeId> orig_to_sub_item_type_ids_;

};

}
}
