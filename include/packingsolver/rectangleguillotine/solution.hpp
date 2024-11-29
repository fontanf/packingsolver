#pragma once

#include "packingsolver/rectangleguillotine/instance.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

using SolutionNodeId = int64_t;

/**
 * Depth :
 * * -1: root
 * *  0: bin
 * *  1: 1-cut
 * *  2: 2-cut
 * *  3: 3-cut
 * *  4: 4-cut
 */

struct SolutionNode
{
    SolutionNodeId f;

    Depth d;

    Length l, r, b, t;

    std::vector<SolutionNodeId> children;

    ItemTypeId item_type_id;
};

struct SolutionBin
{
    /** Bin type. */
    BinTypeId bin_type_id;

    /** Number of copies. */
    BinPos copies;

    /** First cut orientation. */
    CutOrientation first_cut_orientation;

    /** Nodes. */
    std::vector<SolutionNode> nodes;
};

/**
 * Solution class for a problem of type "rectangleguillotine".
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
    inline Profit cost() const { return cost_; }

    /*
     * Getters: items
     */

    /** Get the number of items in the solution. */
    inline ItemPos number_of_items() const { return number_of_items_; }

    /** Return 'true' iff the solution contains all items. */
    inline bool full() const { return number_of_items() == instance().number_of_items(); }

    /** Get the total area of the items of the solution. */
    inline Area item_area() const { return item_area_; }

    /** Get the profit of the solution. */
    inline Profit profit() const { return profit_; }

    /** Get the number of copies of an item type in the solution. */
    inline ItemPos item_copies(ItemTypeId item_type_id) const { return item_copies_[item_type_id]; }

    /*
     * Getters: others
     */

    /** Get the height of the solution. */
    inline Length height() const { return height_; }

    /** Get the width of the solution. */
    inline Length width() const { return width_; }

    /** Get the area of the solution. */
    inline Area area() const { return area_; }

    /** Get the total area of the bins of the solution. */
    inline Area full_area() const { return full_area_; }

    /*
     * Getters: computed values
     */

    /** Get the waste of the solution. */
    inline Area waste() const { return area_ - item_area_; }

    /** Get the fraction of waste of the solution. */
    inline double waste_percentage() const { return (area() == 0)? 0: (double)waste() / area(); }

    /** Get the waste of the solution including the residual. */
    inline Area full_waste() const { return full_area() - item_area(); }

    /** Get the fraction of waste of the solution including the residual. */
    inline double full_waste_percentage() const { return (full_area() == 0)? 0: (double)full_waste() / full_area(); }

    /*
     * Others
     */

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
     * Private methods
     */

    void update_indicators(
            BinPos bin_pos);

    /*
     * Private attributes
     */

    /** Instance. */
    const Instance* instance_;

    /*
     * Private attributes: bins
     */

    /** Bins. */
    std::vector<SolutionBin> bins_;

    /** Number of bins in the solution. */
    BinPos number_of_bins_ = 0;

    /** Number of copies of each bin type in the solution. */
    std::vector<BinPos> bin_copies_;

    /** Cost of the solution. */
    Profit cost_ = 0;

    /*
     * Private attributes: items
     */

    /** Number of items in the solution. */
    ItemPos number_of_items_ = 0;

    /** Number of copies of each item type in the solution. */
    std::vector<ItemPos> item_copies_;

    /** Total area of the items of the solution. */
    Area item_area_ = 0;

    /** Profit of the solution. */
    Profit profit_ = 0;

    /*
     * Private attributes: others
     */

    /** Total area of the solution. */
    Area area_ = 0;

    /** Total area of the bins of the solution. */
    Area full_area_ = 0;

    /** Width of the solution. */
    Length width_ = 0;

    /** Height of the solution. */
    Length height_ = 0;

    friend class SolutionBuilder;

};

std::ostream& operator<<(
        std::ostream& os,
        const SolutionNode& node);

std::ostream& operator<<(
        std::ostream& os,
        const Solution& solution);

}
}
