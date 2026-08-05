#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <functional>

namespace packingsolver
{
namespace onedimensional
{

/** File format for 'Instance::write'. */
enum class InstanceFormat
{
    /**
     * Three CSV files (items, bins, parameters), matching
     * 'InstanceBuilder::read_item_types'/'read_bin_types'/'read_parameters'.
     * Cannot represent resources, eligibility, or item type precedences;
     * 'write' throws if the instance has any of these.
     */
    Csv,
    /**
     * A single JSON file, matching 'InstanceBuilder::read' - the only
     * format that can represent every feature of the instance.
     */
    Json,
};

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Item type precedence: no unit of 'dominated_item_type_id' may be used
 * unless 'dominating_item_type_id' uses all of its own copies (used by
 * 'milp_assignment'; see its "Constraints: item type precedence" for the
 * exact semantics and soundness scope). Added via
 * 'InstanceBuilder::add_item_type_precedence'.
 */
struct Precedence
{
    /** Dominated item type of this precedence. */
    ItemTypeId dominated_item_type_id = -1;

    /** Dominating item type of this precedence. */
    ItemTypeId dominating_item_type_id = -1;
};

/**
 * Item type structure for a problem of type 'onedimensional'.
 */
struct ItemType
{
    /** Dimension of the item type. */
    Length length;

    /** Profit of the item type. */
    Profit profit;

    /** Number of copies of the item type. */
    ItemPos copies;

    /**
     * Minimum number of copies of the item type to pack.
     *
     * Set to '-1' before 'InstanceBuilder::build()' resolves it: '0' for
     * objective 'Knapsack' (packing an item type is optional), 'copies' for
     * every other objective (every copy of every item type must be packed).
     */
    ItemPos copies_min = -1;

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

    /**
     * Ids of the precedences (see 'Instance::precedences') in which this
     * item type is the dominated party.
     */
    std::vector<PrecedenceId> dominated_precedence_ids;

    /**
     * Ids of the precedences (see 'Instance::precedences') in which this
     * item type is the dominating party.
     */
    std::vector<PrecedenceId> dominating_precedence_ids;

    Length space() const { return length; }

    /*
     * Computed attributes.
     */

    /** Number of fixed copies of the item type (pre-placed in bin types). */
    ItemPos copies_fixed = 0;

};

std::ostream& operator<<(
        std::ostream& os,
        const ItemType& item_type);

struct FixedItem
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Initial position of the item. */
    Length start;
};

/**
 * Bin type structure for a problem of type 'onedimensional'.
 */
struct BinType
{
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

    /** Fixed items pre-placed in every bin of this type. */
    std::vector<FixedItem> fixed_items;

    /**
     * Resource capacities, indexed by resource_id (a resource_id is local to
     * this bin type).
     */
    std::vector<double> resource_capacities;

    /**
     * Resource consumption schedule of each item type for each resource of
     * this bin type, indexed by [resource_id][item_type_id][copy]: the
     * consumption charged for the 'copy'-th (0-indexed) unit of the item
     * type packed in a bin of this type. A 'copy' past the end of the
     * schedule repeats its last entry (so a length-1 schedule means
     * "the same consumption regardless of how many copies are already
     * packed" - the common case); a resource_id or item_type_id past the
     * end of these vectors implicitly consumes 0 regardless of 'copy'.
     *
     * A non-uniform schedule lets a resource express "at least N copies of
     * this item type" as a plain capacity/consumption row: an all-ones
     * schedule of length N followed by a single trailing 0 makes the total
     * consumption equal to 'min(count, N)', which only keeps growing while
     * count < N. Combined across item types in one resource, this is what
     * lets combinatorial cuts (e.g. the rectangle Benders decomposition's
     * no-good cuts and pairwise-incompatibility cuts) be expressed exactly
     * as resources instead of needing a dedicated mechanism.
     */
    std::vector<std::vector<std::vector<double>>> item_resource_consumptions;

    /*
     * Computed attributes.
     */

    /** Item type ids. */
    std::vector<ItemTypeId> item_type_ids;

    inline Volume space() const { return length; }

    /** Get the number of resources of this bin type. */
    inline ResourceId number_of_resources() const { return resource_capacities.size(); }

    /**
     * Get the consumption of the 'copy'-th (0-indexed) unit of an item type
     * for a resource.
     */
    inline double item_resource_consumption(
            ItemTypeId item_type_id,
            ResourceId resource_id,
            ItemPos copy) const
    {
        if (resource_id >= (ResourceId)item_resource_consumptions.size())
            return 0.0;
        const std::vector<std::vector<double>>& consumptions = item_resource_consumptions[resource_id];
        if (item_type_id >= (ItemTypeId)consumptions.size())
            return 0.0;
        const std::vector<double>& schedule = consumptions[item_type_id];
        if (schedule.empty())
            return 0.0;
        return (copy < (ItemPos)schedule.size())?
            schedule[copy]:
            schedule.back();
    }

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

class Solution;

/**
 * User-provided feasibility callback.
 *
 * Called on a fully-built Solution in addition to the built-in feasibility
 * checks; returning 'false' marks the solution as infeasible.
 */
using FeasibilityCallback = std::function<bool(const Solution&)>;

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

    /** Get the feasibility callback. */
    inline const FeasibilityCallback& feasibility_callback() const { return feasibility_callback_; }

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

    /** Get the largest cost of the bins. */
    inline Profit largest_bin_cost() const { return largest_bin_cost_; }

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

    /** Get the largest profit of the items. */
    inline Profit largest_item_profit() const { return largest_item_profit_; }

    /** Get the id of the item type with largest efficiency. */
    inline ItemTypeId largest_efficiency_item_type_id() const { return largest_efficiency_item_type_id_; }

    /** Get the largest number of copies of the items. */
    inline ItemPos largest_item_copies() const { return largest_item_copies_; }

    /** Return true iff all items have infinite copies. */
    inline bool unbounded_knapsack() const { return all_item_types_infinite_copies_; }

    /**
     * Return 'true' iff item type 'item_type_id' fits in at least one bin
     * type.
     */
    bool fits_some_bin(ItemTypeId item_type_id) const;

    /** Return 'true' iff item type 'item_type_id' fits in bin type 'bin_type_id'. */
    bool item_type_fits_bin_type(
            ItemTypeId item_type_id,
            BinTypeId bin_type_id) const;

    /** Get the item type precedences; see 'Precedence'. */
    inline const std::vector<Precedence>& precedences() const { return precedences_; }

    /*
     * Export
     */

    /** Print the instance into a stream. */
    std::ostream& format(
            std::ostream& os,
            int verbosity_level = 1) const;

    /**
     * Write the instance to a file, in the given format (default 'Csv',
     * matching the previous behavior). See 'InstanceFormat'.
     */
    void write(
            const std::string& instance_path,
            InstanceFormat format = InstanceFormat::Csv) const;

    /** Write the instance as three CSV files; see 'InstanceFormat::Csv'. */
    void write_csv(const std::string& instance_path) const;

    /** Write the instance as a single JSON file; see 'InstanceFormat::Json'. */
    void write_json(const std::string& instance_path) const;

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

    /** User-provided feasibility callback. */
    FeasibilityCallback feasibility_callback_ = [](const Solution&) { return true; };

    /** Bin types. */
    std::vector<BinType> bin_types_;

    /** Item types. */
    std::vector<ItemType> item_types_;

    /** Item type precedences; see 'precedences'. */
    std::vector<Precedence> precedences_;

    /*
     * Private attributes computed by the 'build' method
     */

    /** For each bin position, the corresponding bin type. */
    std::vector<BinTypeId> bin_type_ids_;

    /** For each bin position, the length of the previous bins. */
    std::vector<Area> previous_bins_length_;

    /** Total packable length. */
    Length bin_length_ = 0;

    /** Largest bin cost. */
    Profit largest_bin_cost_ = 0.0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Total item length. */
    Length item_length_ = 0;

    /** Total item profit. */
    Profit item_profit_ = 0;

    /** Largest item profit. */
    Profit largest_item_profit_ = 0.0;

    /** Id of the item with maximum efficiency. */
    ItemTypeId largest_efficiency_item_type_id_ = -1;

    /** Largest item copies. */
    ItemPos largest_item_copies_ = 0;

    /** True iff all item types have an infinite number of copies. */
    bool all_item_types_infinite_copies_ = false;

    friend class InstanceBuilder;

};

}
}
