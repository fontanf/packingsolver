#pragma once

#include "packingsolver/irregular/instance.hpp"

#include <sstream>

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
    Area item_area = 0;
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
            Angle angle);

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

    /** Get the total number of bins in the solution. */
    inline BinPos number_of_bins() const { return number_of_bins_; }

    /** Get the number of different bins in the solution. */
    inline BinPos number_of_different_bins() const { return bins_.size(); }

    /** Get the maximum x of the solution. */
    inline LengthDbl x_max() const { return x_max_; }

    /** Get the maximum y of the solution. */
    inline LengthDbl y_max() const { return y_max_; }

    /** Get the profit of the solution. */
    inline Profit profit() const { return item_profit_; }

    /** Get the cost of the solution. */
    inline Profit cost() const { return bin_cost_; }

    /** Get the area of the solution. */
    inline Area area() const { return area_; }

    /** Get the total area of the items of the solution. */
    inline Area item_area() const { return item_area_; }

    /** Get the total area of the bins of the solution. */
    inline Area bin_area() const { return bin_area_; }

    /** Get the waste of the solution. */
    inline Area waste() const { return area_ - item_area_; }

    /** Get the fraction of waste of the solution. */
    inline double waste_percentage() const { return (double)waste() / area(); }

    /** Get the waste of the solution including the residual. */
    inline Area full_waste() const { return bin_area() - item_area(); }

    /** Get the fraction of waste of the solution including the residual. */
    inline double full_waste_percentage() const { return (double)full_waste() / bin_area(); }

    /** Get the number of copies of item 'j' in the solution. */
    inline ItemPos item_copies(ItemTypeId j) const { return item_copies_[j]; }

    bool operator<(const Solution& solution) const;

    /** CSV export */
    void write(Info& info) const;

    void algorithm_start(Info& info, Algorithm algorithm) const;

    void algorithm_end(Info& info) const;

    void display(
            const std::stringstream& algorithm,
            Info& info) const;

private:

    /** Instance. */
    const Instance* instance_;

    /** bins. */
    std::vector<SolutionBin> bins_;

    /** Number of bins. */
    BinPos number_of_bins_ = 0;

    /** Number of items. */
    ItemPos number_of_items_ = 0;

    /** Total area of the solution. */
    Area area_ = 0;

    /** Total area of the bins of the solution. */
    Area bin_area_ = 0;

    /** Total area of the items of the solution. */
    Area item_area_ = 0;

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

