#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{
namespace onedimensional
{

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Item type structure for a problem of type 'onedimensional'.
 */
struct ItemType
{
    /** Id of the item type. */
    ItemTypeId id;

    /** Dimension of the item type. */
    Length length;

    /** Profit of the item type. */
    Profit profit;

    /** Number of copies of the item type. */
    ItemPos copies;

    /** Weight of the item type. */
    Weight weight = 0;

    /**
     * Nesting length.
     *
     * Length to remove when the item is packed after another item in the same
     * bin.
     */
    Length nesting_length = 0;

    /**
     * Maximum stackability.
     *
     * Maximum number of items in a bin containing this item type.
     */
    ItemPos maximum_stackability = std::numeric_limits<ItemPos>::max();

    /** Maximum weight of the items packed after items of this type. */
    Weight maximum_weight_after = std::numeric_limits<Weight>::infinity();

    /**
     * Eligibility.
     *
     * - 'eligibility_id == -1' means that the item type can be packed in any
     *   bin type.
     * - 'eligibility_id >= 0' means that the item type can only be packed in
     *   bin type supporting eligibility id 'eligibility_id'.
     */
    EligibilityId eligibility_id = -1;

    Length space() const { return length; }

};

std::ostream& operator<<(
        std::ostream& os,
        const ItemType& item_type);

/**
 * Bin type structure for a problem of type 'onedimensional'.
 */
struct BinType
{
    /** Id of the bin type. */
    BinTypeId id;

    /** Dimension of the bin type. */
    Length length;

    /** Cost of the bin type. */
    Profit cost;

    /** Maximum number of copies of the bin type. */
    BinPos copies;

    /** Minimum number of copies to use of the bin type. */
    BinPos copies_min;

    /** Maximum weight allowed in the bin type.  */
    Weight maximum_weight = std::numeric_limits<Weight>::infinity();

    /** Eligibility ids. */
    std::vector<EligibilityId> eligibility_ids;

    /*
     * Computed attributes.
     */

    /** Item type ids. */
    std::vector<ItemTypeId> item_type_ids;

    inline Volume space() const { return length; }

};

std::ostream& operator<<(
        std::ostream& os,
        const BinType& bin_type);

struct Parameters
{
};

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Instance class for a problem of type "onedimensional".
 */
class Instance
{

public:

    /*
     * Getters
     */

    /** Get the problem type. */
    static inline ProblemType type() { return ProblemType::OneDimensional; };

    /** Get the objective of the problem. */
    inline Objective objective() const { return objective_; }

    /*
     * Getters: parameters
     */

    /** Get parameters. */
    const Parameters& parameters() const { return parameters_; }

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

    /** Get the total length of the bins. */
    inline Area bin_length() const { return bin_length_; }

    /** Get the total length of the bins before bin i_pos. */
    inline Area previous_bin_length(BinPos bin_pos) const { return previous_bins_length_[bin_pos]; }

    /** Get the maximum cost of the bins. */
    inline Profit maximum_bin_cost() const { return maximum_bin_cost_; }

    /*
     * Getters: item types
     */

    /* Get the number of item types. */
    inline ItemTypeId number_of_item_types() const { return item_types_.size(); }

    /** Get an item type. */
    inline const ItemType& item_type(ItemTypeId item_type_id) const { return item_types_[item_type_id]; }

    /** Get the number of items. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Get the total length of the items. */
    inline Area item_length() const { return item_length_; }

    /** Get the mean length of the items. */
    inline Area mean_item_length() const { return item_length_ / number_of_items(); }

    /** Get the total profit of the items. */
    inline Profit item_profit() const { return item_profit_; }

    /** Get the id of the item type with maximum efficiency. */
    inline ItemTypeId max_efficiency_item_type_id() const { return max_efficiency_item_type_id_; }

    /** Get the maximum number of copies of the items. */
    inline ItemPos maximum_item_copies() const { return maximum_item_copies_; }

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

    /** For each bin position, the length of the previous bins. */
    std::vector<Area> previous_bins_length_;

    /** Total packable length. */
    Length bin_length_ = 0;

    /** Maximum bin cost. */
    Profit maximum_bin_cost_ = 0.0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Total item length. */
    Length item_length_ = 0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Id of the item with maximum efficiency. */
    ItemTypeId max_efficiency_item_type_id_ = -1;

    /** Maximum item copies. */
    ItemPos maximum_item_copies_ = 0;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_types_infinite_copies_ = false;

    friend class InstanceBuilder;

};

}
}

