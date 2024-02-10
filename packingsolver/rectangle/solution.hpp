#pragma once

#include "packingsolver/rectangle/instance.hpp"

namespace packingsolver
{
namespace rectangle
{

struct SolutionItem
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Position of the bottom-left corner of the item. */
    Point bl_corner;

    /** Whether the item is rotated or not. */
    bool rotate;
};

struct SolutionBin
{
    /** Bin type. */
    BinTypeId bin_type_id;

    /** Number of copies. */
    BinPos copies;

    /** Items. */
    std::vector<SolutionItem> items;

    /** For each group, weight. */
    std::vector<Weight> weight;

    /**
     * For each group, sum of x times weight of all items.
     *
     * This attribute is used to compute the center of gravity of the
     * items.
     */
    std::vector<Weight> weight_weighted_sum;
};

/**
 * Solution class for a problem of type "rectangle".
 */
class Solution
{

public:

    /*
     * Constructors and destructor.
     */

    /** Standard constructor. */
    Solution(const Instance& instance):
        instance_(&instance),
        bin_copies_(instance.number_of_bin_types(), 0),
        item_copies_(instance.number_of_item_types(), 0)
    { }

    /** Add a bin at the end of the solution. */
    BinPos add_bin(
            BinTypeId i,
            BinPos copies);

    /** Add an item to the solution. */
    void add_item(
            BinPos i,
            ItemTypeId j,
            Point bl_corner,
            bool rotate);

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

    /*
     * Getters
     */

    /** Get the instance. */
    inline const Instance& instance() const { return *instance_; }

    /** Get the number of items in the solution. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Return 'tree' iff the solution contains all items. */
    inline bool full() const { return number_of_items() == instance().number_of_items(); }

    /** Get the number of bins in the solution. */
    inline BinPos number_of_bins() const { return number_of_bins_; }

    /** Get the number of different bins in the solution. */
    inline BinPos number_of_different_bins() const { return bins_.size(); }

    /** Get the maximum x of the solution. */
    inline Length x_max() const { return x_max_; }

    /** Get the maximum y of the solution. */
    inline Length y_max() const { return y_max_; }

    /** Get the profit of the solution. */
    inline Profit profit() const { return item_profit_; }

    /** Get the cost of the solution. */
    inline Profit cost() const { return bin_cost_; }

    /** Get the area of the solution. */
    inline Area area() const { return area_; }

    /** Get the total area of the items of the solution. */
    inline Area item_area() const { return item_area_; }

    /** Get the total weight of the items of the solution. */
    inline Weight item_weight() const { return item_weight_; }

    /** Get the total area of the bins of the solution. */
    inline Area bin_area() const { return bin_area_; }

    /** Get the total weight of the bins of the solution. */
    inline Weight bin_weight() const { return bin_weight_; }

    /** Get the area load. */
    inline double area_load() const { return (double)item_area() / instance().bin_area(); }

    /** Get the weight load. */
    inline double weight_load() const { return (double)item_weight() / instance().bin_weight(); }

    /** Get the waste of the solution. */
    inline Area waste() const { return area_ - item_area_; }

    /** Get the fraction of waste of the solution. */
    inline double waste_percentage() const { return(area() == 0.0)? 0: (double)waste() / area(); }

    /** Get the waste of the solution including the residual. */
    inline Area full_waste() const { return bin_area() - item_area(); }

    /** Get the fraction of waste of the solution including the residual. */
    inline double full_waste_percentage() const { return (bin_area() == 0)? 0: (double)full_waste() / bin_area(); }

    /** Get the number of copies of item 'j' in the solution. */
    inline ItemPos item_copies(ItemTypeId j) const { return item_copies_[j]; }

    /** Get a bin. */
    const SolutionBin& bin(BinPos i) const { return bins_[i]; }

    /** Get the number of copies of bin 'i' in the solution. */
    inline BinPos bin_copies(BinTypeId i) const { return bin_copies_[i]; }

    /** Get the middle axle overweight. */
    inline ItemPos middle_axle_overweight() const { return middle_axle_overweight_; }

    /** Get the rear axle overweight. */
    inline ItemPos rear_axle_overweight() const { return rear_axle_overweight_; }

    /** Get the load of the least loaded bin (in terms of volume). */
    double least_load() const;

    /**
     * Get the "adjusted" number of bins.
     *
     * This corresponds to the number of bins - 1 + the load of the least
     * loaded bin.
     */
    inline double adjusted_number_of_bins() const { return (number_of_bins() == 0)? 0: number_of_bins() - 1 + least_load(); }

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
    ItemPos number_of_items_ = 0;

    /** Total area of the solution. */
    Area area_ = 0;

    /** Total area of the bins of the solution. */
    Area bin_area_ = 0;

    /** Total weight of the bins of the solution. */
    Volume bin_weight_ = 0;

    /** Total area of the items of the solution. */
    Area item_area_ = 0;

    /** Total weight of the items of the solution. */
    Weight item_weight_ = 0;

    /** Profit of the solution. */
    Profit item_profit_ = 0;

    /** Cost of the solution. */
    Profit bin_cost_ = 0;

    /** Maximum x of the solution. */
    Length x_max_ = 0;

    /** Maximum y of the solution. */
    Length y_max_ = 0;

    /** Number of copies of each bin type in the solution. */
    std::vector<BinPos> bin_copies_;

    /** Number of copies of each item type in the solution. */
    std::vector<ItemPos> item_copies_;

    /** Overweight for the middle axle weight constraints. */
    double middle_axle_overweight_ = 0;

    /** Overweight for the rear axle weight constraints. */
    double rear_axle_overweight_ = 0;

};

std::ostream& operator<<(
        std::ostream& os,
        const SolutionItem& item);

std::ostream& operator<<(
        std::ostream& os,
        const Solution& solution);

}
}

