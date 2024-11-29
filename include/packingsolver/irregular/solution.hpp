#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

struct SolutionItem
{
    /** Item type. */
    ItemTypeId item_type_id;

    /** Position of the bottom-left corner of the item. */
    Point bl_corner;

    /** Rotation angle of the item. */
    Angle angle;

    /** Mirror the item. */
    bool mirror;
};

struct SolutionBin
{
    /** Bin type. */
    BinTypeId bin_type_id;

    /** Number of copies. */
    BinPos copies;

    /** Items. */
    std::vector<SolutionItem> items;

    /** Item area. */
    AreaDbl item_area = 0;
};

/**
 * Solution class for a problem of type "irregular".
 */
class Solution
{

public:

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
            Point bl_corner,
            Angle angle,
            bool mirror);

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

    /** Get the total area of the bins of the solution. */
    inline AreaDbl bin_area() const { return bin_area_; }

    /*
     * Getters: items
     */

    /** Get the number of items in the solution. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Return 'true' iff the solution contains all items. */
    inline bool full() const { return number_of_items() == instance().number_of_items(); }

    /** Get the total area of the items of the solution. */
    inline AreaDbl item_area() const { return item_area_; }

    /** Get the profit of the solution. */
    inline Profit profit() const { return item_profit_; }

    /** Get the number of copies of an item type in the solution. */
    inline ItemPos item_copies(ItemTypeId item_type_id) const { return item_copies_[item_type_id]; }

    /*
     * Getters: others
     */

    /** Get the maximum x of the solution. */
    inline LengthDbl x_max() const { return x_max_; }

    /** Get the maximum y of the solution. */
    inline LengthDbl y_max() const { return y_max_; }

    /** Get the area of the solution. */
    inline Profit leftover_value() const { return leftover_value_; }

    /** Get the waste of the solution including the residual. */
    inline AreaDbl full_waste() const { return bin_area() - item_area(); }

    /** Get the fraction of waste of the solution including the residual. */
    inline double full_waste_percentage() const { return (double)full_waste() / bin_area(); }

    bool operator<(const Solution& solution) const;

    /*
     * Export
     */

    /** Write the solution to a file. */
    void write(const std::string& certificate_path) const;

    /** Write the solution to an SVG file. */
    void write_svg(
            const std::string& file_path,
            BinPos bin_pos) const;

    /** Export solution characteristics to a JSON structure. */
    nlohmann::json to_json() const;

    /** Write a formatted output of the instance to a stream. */
    void format(
            std::ostream& os,
            int verbosity_level = 1) const;

private:

    /** Instance. */
    const Instance* instance_;

    /** bins. */
    std::vector<SolutionBin> bins_;

    /** Number of bins. */
    BinPos number_of_bins_ = 0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Value of the leftover. */
    Profit leftover_value_ = 0.0;

    /** Total area of the bins of the solution. */
    AreaDbl bin_area_ = 0;

    /** Total area of the items of the solution. */
    AreaDbl item_area_ = 0;

    /** Profit of the solution. */
    Profit item_profit_ = 0;

    /** Cost of the solution. */
    Profit bin_cost_ = 0;

    /** Maximum x of the solution. */
    LengthDbl x_max_ = 0;

    /** Maximum y of the solution. */
    LengthDbl y_max_ = 0;

    /** Number of copies of each bin type in the solution. */
    std::vector<BinPos> bin_copies_;

    /** Number of copies of each item type in the solution. */
    std::vector<ItemPos> item_copies_;

};

std::ostream& operator<<(std::ostream &os, const SolutionItem& item);
std::ostream& operator<<(std::ostream &os, const Solution& solution);

}
}
