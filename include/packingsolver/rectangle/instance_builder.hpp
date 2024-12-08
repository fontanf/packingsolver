#pragma once

#include "packingsolver/rectangle/instance.hpp"

namespace packingsolver
{
namespace rectangle
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

    /** Read parameters from a file. */
    void read_parameters(const std::string& parameters_path);

    /** Read bin types from a file. */
    void read_bin_types(const std::string& bins_path);

    /** Read defects from a file. */
    void read_defects(const std::string& defects_path);

    /** Read item types from a file. */
    void read_item_types(const std::string& items_path);

    /*
     * Set parameters
     */

    /** Set parameters. */
    void set_parameters(const Parameters& parameters) { instance_.parameters_ = parameters; }

    /** Set whether or not to consider weight constraints for a group. */
    void set_group_weight_constraints(
            GroupId group_id,
            bool check_weight_constraints);

    void set_unloading_constraint(UnloadingConstraint unloading_constraint) { instance_.parameters_.unloading_constraint = unloading_constraint; }

    /*
     * Set bin types
     */

    /** Add a bin type. */
    BinTypeId add_bin_type(
            Length x,
            Length y,
            Profit cost = -1,
            BinPos copies = 1,
            BinPos copies_min = 0);

    /** Set the maximum weight of a bin type. */
    void set_bin_type_maximum_weight(
            BinTypeId bin_type_id,
            Weight maximum_weight);

    /** Set the semi-trailer truck parameters of a bin type. */
    void set_bin_type_semi_trailer_truck_parameters(
            BinTypeId bin_type_id,
            const SemiTrailerTruckData& semi_trailer_truck_data);

    /** Add a defect. */
    void add_defect(
            BinTypeId bin_type_id,
            Length pos_x,
            Length pos_y,
            Length rect_x,
            Length rect_y);

    /**
     * Add a bin type from another bin type.
     *
     * This method is used in the column generation procedure.
     */
    void add_bin_type(
            const BinType& bin_type,
            BinPos copies);

    /**
     * For each bin type, set an infinite x.
     *
     * This method is used to transform a problem into a Strip Packing problem.
     */
    void set_bin_types_infinite_x();

    /**
     * For each bin type, set an infinite y.
     *
     * This method is used to transform a problem into a Strip Packing problem.
     */
    void set_bin_types_infinite_y();

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
            Length x,
            Length y,
            Profit p = -1,
            ItemPos copies = 1,
            bool oriented = false,
            GroupId group = 0);

    /** Set the weight of an item type. */
    void set_item_type_weight(
            ItemTypeId item_type_id,
            Weight weight);

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
     * For each item type, set an infinite number of copies.
     *
     * This method is used to transform a Knapsack Problem into an Unbounded
     * Knapsack Problem.
     */
    void set_item_types_infinite_copies();

    void multiply_item_types_copies(ItemPos factor);

    /**
     * For each item type, set its profit to its area.
     *
     * This method is used to transform a Knapsack Problem into an Unweighted
     * Knapsack Problem.
     */
    void set_item_types_unweighted();

    /** For each item type, set 'oriented' to 'true'. */
    void set_item_types_oriented();

    /*
     * Build
     */

    /** Build. */
    Instance build();

private:

    /*
     * Private methods
     */

    /** Compute item type max length sum. */
    Length compute_item_types_max_length_sum() const;

    /** Compute bin attributes. */
    void compute_bin_attributes();

    /** Compute bin types area max. */
    Area compute_bin_types_area_max() const;

    /*
     * Private attributes
     */

    /** Instance. */
    Instance instance_;

};

}
}
