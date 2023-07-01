#pragma once

#include "packingsolver/rectangleguillotine/instance.hpp"

namespace packingsolver
{
namespace rectangleguillotine
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
    void read_parameters(std::string parameters_path);

    /** Read bin types from a file. */
    void read_bin_types(std::string bins_path);

    /** Read defects from a file. */
    void read_defects(std::string defects_path);

    /** Read item types from a file. */
    void read_item_types(std::string items_path);

    /*
     * Set parameters
     */

    /** Set parameters. */
    void set_parameters(const Parameters& parameters) { instance_.parameters_ = parameters; }

    void set_cut_type_1(CutType1 cut_type_1) { instance_.parameters_.cut_type_1 = cut_type_1; }

    void set_cut_type_2(CutType2 cut_type_2) { instance_.parameters_.cut_type_2 = cut_type_2; }

    void set_first_stage_orientation(CutOrientation first_stage_orientation) { instance_.parameters_.first_stage_orientation = first_stage_orientation; }

    void set_min1cut(Length min1cut) { instance_.parameters_.min1cut = min1cut; }

    void set_max1cut(Length max1cut) { instance_.parameters_.max1cut = max1cut; }

    void set_min2cut(Length min2cut) { instance_.parameters_.min2cut = min2cut; }

    void set_max2cut(Length max2cut) { instance_.parameters_.max2cut = max2cut; }

    void set_min_waste(Length min_waste) { instance_.parameters_.min_waste = min_waste; }

    void set_one2cut(bool one2cut) { instance_.parameters_.one2cut = one2cut; }

    void set_cut_through_defects(bool cut_through_defects) { instance_.parameters_.cut_through_defects = cut_through_defects; }

    /** Set cut thickness. */
    void set_cut_thickness(Length cut_thickness) { instance_.parameters_.cut_thickness = cut_thickness; }

    void set_predefined(std::string str);

    void set_roadef2018();

    /*
     * Set bin types
     */

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
    void add_bin_type(
            const BinType& bin_type,
            BinPos copies,
            BinPos copies_min = 0);

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
            Length w,
            Length h,
            Profit p = -1,
            ItemPos copies = 1,
            bool oriented = false,
            StackId stack_id = -1);

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

    /** Compute number of items. */
    ItemPos compute_number_of_items() const;

    /** Compute item types max length sum. */
    Length compute_item_types_max_length_sum() const;

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
