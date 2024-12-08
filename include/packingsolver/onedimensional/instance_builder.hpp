#pragma once

#include "packingsolver/onedimensional/instance.hpp"

namespace packingsolver
{
namespace onedimensional
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

    /** Read item types from a file. */
    void read_item_types(const std::string& items_path);

    /*
     * Set parameters
     */

    /** Set parameters. */
    void set_parameters(const Parameters& parameters) { instance_.parameters_ = parameters; }

    /*
     * Set bin types
     */

    /** Add a bin type. */
    BinTypeId add_bin_type(
            Length length,
            Profit cost = -1,
            BinPos copies = 1,
            BinPos copies_min = 0);

    /** Set the maximum weight of a bin type. */
    void set_bin_type_maximum_weight(
            BinTypeId bin_type_id,
            Weight maximum_weight);

    /** Add an eligibility id to a bin type. */
    void add_bin_type_eligibility(
            BinTypeId bin_type_id,
            EligibilityId eligibility_id);

    /**
     * Add a bin type from another bin type.
     *
     * This method is used in the column generation procedure.
     */
    void add_bin_type(
            const BinType& bin_type,
            BinPos copies);

    /**
     * Foe each bin type, set an infinite number of copies.
     *
     * This method should be used after reading bin files of Bin Packing
     * Problems where the number of bin types is not given. By default, the
     * number of bins would be set to 1.
     */
    void set_bin_types_infinite_copies();

    /**
     * For each bin type, set its cost to its length.
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
            Length length,
            Profit p = -1,
            ItemPos copies = 1);

    /** Set the weight of an item type. */
    void set_item_type_weight(
            ItemTypeId item_type_id,
            Weight weight);

    /** Set the nesting length of an item type. */
    void set_item_type_nesting_length(
            ItemTypeId item_type_id,
            Length nesting_length);

    /** Set the maximum stackability of an item type. */
    void set_item_type_maximum_stackability(
            ItemTypeId item_type_id,
            ItemPos maximum_stackability);

    /** Set the maximum weight after of an item type. */
    void set_item_type_maximum_weight_after(
            ItemTypeId item_type_id,
            Weight maximum_weight_after);

    /** Set the eligibility id of an item type. */
    void set_item_type_eligibility(
            ItemTypeId item_type_id,
            EligibilityId eligibility_id);

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

    /**
     * For each item type, set its profit to its length.
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

    /** Compute item type max length sum. */
    Length compute_item_type_max_length_sum() const;

    /** Compute bin types length max. */
    Area compute_bin_types_length_max() const;

    /** Compute bin attributes. */
    void compute_bin_attributes();

    /*
     * Private attributes
     */

    /** Instance. */
    Instance instance_;

};

}
}
