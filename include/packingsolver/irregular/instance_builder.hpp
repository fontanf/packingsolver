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

    /** Add a quality rule. */
    inline void add_quality_rule(
            const std::vector<uint8_t>& quality_rule);

    /** Set item-item minimum spacing. */
    void set_item_item_minimum_spacing(LengthDbl item_item_minimum_spacing) { instance_.parameters_.item_item_minimum_spacing = item_item_minimum_spacing; }

    /** Set item-bin minimum spacing. */
    void set_item_bin_minimum_spacing(LengthDbl item_bin_minimum_spacing) { instance_.parameters_.item_bin_minimum_spacing = item_bin_minimum_spacing; }

    /*
     * Set bin types
     */

    /** Add a bin type. */
    BinTypeId add_bin_type(
            const Shape& shape,
            Profit cost = -1,
            BinPos copies = 1,
            BinPos copies_min = 0);

    /** Add a defect. */
    void add_defect(
            BinTypeId bin_type_id,
            DefectTypeId type,
            const Shape& shape,
            const std::vector<Shape>& holes = {});

    /**
     * Add a bin type from another bin type.
     *
     * This method is used in the column generation procedure.
     */
    void add_bin_type(
            const BinType& bin_type,
            BinPos copies,
            BinPos copies_min = 0);

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
            const std::vector<ItemShape>& shapes,
            Profit profit = -1,
            ItemPos copies = 1,
            const std::vector<std::pair<Angle, Angle>>& allowed_rotations = {{0, 0}});

    /** Set allow mirroring of an item type. */
    void set_item_type_allow_mirroring(
            ItemTypeId item_type_id,
            bool allow_mirroring);

    /**
     * Add an item type from another item type.
     *
     * This method is used in the column generation procedure.
     */
    void add_item_type(
            const ItemType& item_type,
            Profit profit,
            ItemPos copies);

    /**
     * For each item type, set its profit to its area.
     *
     * This method is used to transform a Knapsack Problem into an Unweighted
     * Knapsack Problem.
     */
    void set_item_types_unweighted();

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

};

}
}
