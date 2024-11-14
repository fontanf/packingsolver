#pragma once

#include "packingsolver/onedimensional/instance.hpp"

namespace packingsolver
{
namespace onedimensional
{

struct SolutionItem
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Initial z-coordinate of the item. */
    Length start;
};

struct SolutionBin
{
    /** Bin type. */
    BinTypeId bin_type_id;

    /** Number of copies. */
    BinPos copies;

    /** End. */
    Length end = 0;

    /** Weight. */
    Weight weight = 0;

    /** Stacks. */
    std::vector<SolutionItem> items;

    /** Maximum number of items allowed in the bin. */
    ItemPos maximum_number_of_items = -1;

    /**
     * Remaining weight allowed in the bin to satisfy the maximum
     * weight after constraint.
     */
    Weight remaiing_weight = -1;
};

/**
 * Solution class for a problem of type "onedimensional".
 */
class Solution
{

public:

    /*
     * Constructors and destructor
     */

    /** Standard constructor. */
    Solution(const Instance& instance):
        instance_(&instance),
        bin_copies_(instance.number_of_bin_types(), 0),
        item_copies_(instance.number_of_item_types(), 0)
    { }

    /** Add a bin at the end of the solution. */
    BinPos add_bin(
            BinTypeId bin_type_id,
            BinPos copies);

    /** Add an item to the solution. */
    void add_item(
            BinPos bin_pos,
            ItemTypeId item_type_id);

    void append(
            const Solution& solution,
            BinPos bin_pos,
            BinPos copies,
            const std::vector<BinTypeId>& bin_type_ids = {},
            const std::vector<ItemTypeId>& item_type_ids = {});

    void append(
            const Solution& solution,
            const std::vector<BinTypeId>& bin_type_ids,
            const std::vector<ItemTypeId>& item_type_ids);

    /** Read a solution from a file. */
    Solution(
            const Instance& instance,
            const std::string& certificate_path);

    /*
     * Getters
     */

    /** Get the instance. */
    inline const Instance& instance() const { return *instance_; }

    /** Get the number of items in the solution. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Return 'true' iff the solution contains all items. */
    inline bool full() const { return number_of_items() == instance().number_of_items(); }

    /** Get the number of bins in the solution. */
    inline BinPos number_of_bins() const { return number_of_bins_; }

    /** Get the number of different bins in the solution. */
    inline BinPos number_of_different_bins() const { return bins_.size(); }

    /** Get the profit of the solution. */
    inline Profit profit() const { return item_profit_; }

    /** Get the cost of the solution. */
    inline Profit cost() const { return bin_cost_; }

    /** Get the length of the solution. */
    inline Volume length() const { return length_; }

    /** Get the total length of the items of the solution. */
    inline Area item_length() const { return item_length_; }

    /** Get the total length of the bins of the solution. */
    inline Area bin_length() const { return bin_length_; }

    /** Get the waste of the solution. */
    inline Area waste() const { return length_ - item_length_; }

    /** Get the fraction of waste of the solution. */
    inline double waste_percentage() const { return (length() == 0)? 0: (double)waste() / length(); }

    /** Get the waste of the solution including the residual. */
    inline Area full_waste() const { return bin_length() - item_length(); }

    /** Get the fraction of waste of the solution including the residual. */
    inline double full_waste_percentage() const { return (bin_length() == 0)? 0.0: (double)full_waste() / bin_length(); }

    /** Return 'true' iff the solution satisfies the packing constraints. */
    inline bool feasible() const { return feasible_; }

    /** Get the number of copies of an item type in the solution. */
    inline ItemPos item_copies(ItemTypeId item_type_id) const { return item_copies_[item_type_id]; }

    /** Get a bin. */
    inline const SolutionBin& bin(BinPos bin_pos) const { return bins_[bin_pos]; }

    /** Get the number of copies of a bin type. */
    inline BinPos bin_copies(BinTypeId bin_type_id) const { return bin_copies_[bin_type_id]; }

    bool operator<(const Solution& solution) const;

    /*
     * Export
     */

    /** Write the solution to a file. */
    void write(const std::string& certificate_path) const;

    /** Export solution characteristics to a JSON structure. */
    nlohmann::json to_json() const;

    /** Write a formatted output of the instance to a stream. */
    void format(
            std::ostream& os,
            int verbosity_level = 1) const;

private:

    /*
     * Private attributes.
     */

    /** Instance. */
    const Instance* instance_;

    /** Bins. */
    std::vector<SolutionBin> bins_;

    /** Number of bins. */
    BinPos number_of_bins_ = 0;

    /** Number of items. */
    BinPos number_of_items_ = 0;

    /** Total length of the solution. */
    Volume length_ = 0;

    /** Total length of the bins of the solution. */
    Volume bin_length_ = 0;

    /** Total length of the items of the solution. */
    Volume item_length_ = 0;

    /** Profit of the solution. */
    Profit item_profit_ = 0;

    /** Cost of the solution. */
    Profit bin_cost_ = 0;

    /** Number of copies of each bin type in the solution. */
    std::vector<BinPos> bin_copies_;

    /** Number of copies of each item type in the solution. */
    std::vector<ItemPos> item_copies_;

    /** 'true' iff the solution is feasible regarding the packing constraints. */
    bool feasible_ = true;

};

std::ostream& operator<<(
        std::ostream& os,
        const SolutionItem& item);

std::ostream& operator<<(
        std::ostream& os,
        const Solution& solution);

}
}
