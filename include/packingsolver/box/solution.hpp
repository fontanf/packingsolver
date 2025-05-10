#pragma once

#include "packingsolver/box/instance.hpp"

namespace packingsolver
{
namespace box
{

struct SolutionItem
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Bottom-left corner of the item. */
    Point bl_corner;

    /** Rotation of the item. */
    int rotation;
};

struct SolutionBin
{
    /** Bin type. */
    BinTypeId bin_type_id;

    /** Number of copies. */
    BinPos copies;

    /** Items. */
    std::vector<SolutionItem> items;

    /** Item weight. */
    Weight weight = 0.0;

    /** Item profit. */
    Profit profit = 0.0;
};

/**
 * Solution class for a problem of type "box".
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
            BinTypeId bin_type_id,
            BinPos copies);

    /** Add an item to the solution. */
    void add_item(
            BinPos bin_pos,
            ItemTypeId item_type_id,
            const Point& bl_corner,
            int rotation);

    /** Destructor. */
    virtual ~Solution() { }

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

    /*
     * Getters: bins
     */

    /** Get the number of bins in the solution. */
    inline BinPos number_of_bins() const { return number_of_bins_; }

    /** Get the number of different bins in the solution. */
    inline BinPos number_of_different_bins() const { return bins_.size(); }

    /** Get a bin. */
    inline const SolutionBin& bin(BinPos bin_pos) const { return bins_[bin_pos]; }

    /** Get the number of copies of a bin type in the solution. */
    inline BinPos bin_copies(BinTypeId bin_type_id) const { return bin_copies_[bin_type_id]; }

    /** Get the cost of the solution. */
    inline Profit cost() const { return bin_cost_; }

    /** Get the total volume of the bins of the solution. */
    inline Volume bin_volume() const { return bin_volume_; }

    /** Get the total floor area of the bins of the solution. */
    inline Area bin_area() const { return bin_area_; }

    /** Get the total weight of the bins of the solution. */
    inline Weight bin_weight() const { return bin_weight_; }

    /*
     * Getters: stacks
     */

    /** Get the number of stacks in the solution. */
    inline ItemPos number_of_stacks() const { return number_of_stacks_; }

    /** Get the total area of the stacks of the solution. */
    inline Area stack_area() const { return stack_area_; }

    /*
     * Getters: items
     */

    /** Get the number of items in the solution. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Return 'true' iff the solution contains all items. */
    inline bool full() const { return number_of_items() == instance().number_of_items(); }

    /** Get the total volume of the items of the solution. */
    inline Volume item_volume() const { return item_volume_; }

    /** Get the total weight of the items of the solution. */
    inline Weight item_weight() const { return item_weight_; }

    /** Get the profit of the solution. */
    inline Profit profit() const { return item_profit_; }

    /** Get the number of copies of item an item type in the solution. */
    inline ItemPos item_copies(ItemTypeId item_type_id) const { return item_copies_[item_type_id]; }

    /*
     * Getters: others
     */

    /** Get the maximum x of the solution. */
    inline Length x_max() const { return x_max_; }

    /** Get the maximum y of the solution. */
    inline Length y_max() const { return y_max_; }

    /** Get the maximum z of the solution. */
    inline Length z_max() const { return z_max_; }

    /** Get the area of the solution. */
    inline Profit leftover_value() const { return leftover_value_; }

    /** Get the volume of the solution. */
    inline Volume volume() const { return volume_; }

    /*
     * Getters: computed values
     */

    /** Get the volume load of the solution. */
    inline double volume_load() const { return (double)item_volume() / instance().bin_volume(); }

    /** Get the area load of the solution. */
    inline double area_load() const { return (double)stack_area() / instance().bin_area(); }

    /** Get the weight load of the solution. */
    inline double weight_load() const { return (double)item_weight() / instance().bin_weight(); }

    /** Get the item fraction of the solution. */
    inline double item_fraction() const { return (double)number_of_items() / instance().number_of_items(); }

    /** Get the volume fraction of the solution. */
    inline double volume_fraction() const { return (double)item_volume() / instance().item_volume(); }

    /** Get the weight fraction of the solution. */
    inline double weight_fraction() const { return (double)item_weight() / instance().item_weight(); }

    /** Get the waste of the solution. */
    inline Area waste() const { return volume_ - item_volume_; }

    /** Get the fraction of waste of the solution. */
    inline double waste_percentage() const { return (volume() == 0)? 0: (double)waste() / volume(); }

    /** Get the waste of the solution including the residual. */
    inline Area full_waste() const { return bin_volume() - item_volume(); }

    /** Get the fraction of waste of the solution including the residual. */
    inline double full_waste_percentage() const { return (bin_volume() == 0)? 0.0: (double)full_waste() / bin_volume(); }

    bool feasible() const;

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

    /** Number of stacks. */
    BinPos number_of_stacks_ = 0;

    /** Number of items. */
    BinPos number_of_items_ = 0;

    /** Total volume of the solution. */
    Volume volume_ = 0;

    /** Value of the leftover. */
    Profit leftover_value_ = 0.0;

    /** Total volume of the bins of the solution. */
    Volume bin_volume_ = 0;

    /** Total floor area of the bins of the solution. */
    Area bin_area_ = 0;

    /** Total area of the stacks of the solution. */
    Area stack_area_ = 0;

    /** Total weight of the bins of the solution. */
    Weight bin_weight_ = 0;

    /** Cost of the solution. */
    Profit bin_cost_ = 0;

    /** Total volume of the items of the solution. */
    Volume item_volume_ = 0;

    /** Total weight of the items of the solution. */
    Weight item_weight_ = 0;

    /** Profit of the solution. */
    Profit item_profit_ = 0;

    /** Maximum x of the solution. */
    Length x_max_ = 0;

    /** Maximum y of the solution. */
    Length y_max_ = 0;

    /** Maximum z of the solution. */
    Length z_max_ = 0;

    /** Number of copies of each bin type in the solution. */
    std::vector<BinPos> bin_copies_;

    /** Number of copies of each item type in the solution. */
    std::vector<ItemPos> item_copies_;

    /*
     * Private methods.
     */

};

std::ostream& operator<<(
        std::ostream& os,
        const SolutionItem& item);

std::ostream& operator<<(
        std::ostream& os,
        const Solution& solution);

}
}
