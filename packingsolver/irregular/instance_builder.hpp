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
    inline void add_bin_type(
            const BinType& bin_type,
            BinPos copies,
            BinPos copies_min = 0)
    {
        add_bin_type(
                bin_type.shape,
                bin_type.cost,
                copies,
                copies_min);
    }

    /*
     * Set item types
     */

    /** Add an item type. */
    ItemTypeId add_item_type(
            const std::vector<ItemShape>& shapes,
            Profit profit = -1,
            ItemPos copies = 1,
            const std::vector<std::pair<Angle, Angle>>& allowed_rotations = {{0, 0}});

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
                item_type.shapes,
                profit,
                copies,
                item_type.allowed_rotations);
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

    /** Compute number of items. */
    ItemPos compute_number_of_items() const;

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
