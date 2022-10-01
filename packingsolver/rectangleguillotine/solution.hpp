#pragma once

#include "packingsolver/rectangleguillotine/instance.hpp"

#include <sstream>

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

/**
 * Solution class for a problem of type "rectangleguillotine".
 */
class Solution
{

public:

    struct Node
    {
        SolutionNodeId id;
        SolutionNodeId f;
        Depth d;
        BinPos i;
        Length l, r, b, t;
        std::vector<SolutionNodeId> children;
        ItemTypeId j;
        BinTypeId bin_type_id;
    };

    /*
     * Constructors and destructor.
     */

    /** Standard constructor. */
    Solution(const Instance& instance):
        instance_(instance),
        bin_copies_(instance.number_of_bin_types(), 0),
        item_copies_(instance.number_of_item_types(), 0)
    { }
    /** Constructor from a list of nodes. */
    Solution(const Instance& instance, const std::vector<Solution::Node>& nodes);

    /** Copy constructor. */
    Solution(const Solution& solution);
    /** Assignment operator. */
    Solution& operator=(const Solution& solution);

    /** Destructor. */
    virtual ~Solution() { }

    void append(
            const Solution& solution,
            const std::vector<BinTypeId>& bin_type_id,
            const std::vector<ItemTypeId>& item_type_ids,
            BinPos copies);

    /*
     * Getters
     */

    /** Get the instance. */
    inline const Instance& instance() const { return instance_; }
    /** Get the number of items in the solution. */
    inline ItemPos number_of_items() const { return number_of_items_; }
    /** Return 'tree' iff the solution contains all items. */
    inline bool full() const { return number_of_items() == instance().number_of_items(); }
    /** Get the number of bins in the solution. */
    inline BinPos number_of_bins() const { return number_of_bins_; }
    /** Get the height of the solution. */
    inline Length height() const { return height_; }
    /** Get the width of the solution. */
    inline Length width() const { return width_; }
    /** Get the profit of the solution. */
    inline Profit profit() const { return profit_; }
    /** Get the cost of the solution. */
    inline Profit cost() const { return cost_; }
    /** Get the area of the solution. */
    inline Area area() const { return area_; }
    /** Get the total area of the items of the solution. */
    inline Area item_area() const { return item_area_; }
    /** Get the total area of the bins of the solution. */
    inline Area full_area() const { return full_area_; }
    /** Get the waste of the solution. */
    inline Area waste() const { return area_ - item_area_; }
    /** Get the fraction of waste of the solution. */
    inline double waste_percentage() const { return (area() == 0)? 0: (double)waste() / area(); }
    /** Get the waste of the solution including the residual. */
    inline Area full_waste() const { return full_area() - item_area(); }
    /** Get the fraction of waste of the solution including the residual. */
    inline double full_waste_percentage() const { return (full_area() == 0)? 0: (double)full_waste() / full_area(); }
    /** Get the number of copies of item 'j' in the solution. */
    inline ItemPos item_copies(ItemTypeId j) const { return item_copies_[j]; }
    /** Get the nodes of the solution. */
    inline const std::vector<Solution::Node>& nodes() const { return nodes_; }

    bool operator<(const Solution& solution) const;

    /** Write CSV certificate file. */
    void write(Info& info) const;

    void algorithm_start(Info& info, Algorithm algorithm) const;
    void algorithm_end(Info& info) const;

    void display(
            const std::stringstream& algorithm,
            Info& info) const;

private:

    void add_node(const Node& node);

    /** Instance. */
    const Instance& instance_;

    /** Nodes of the solution. */
    std::vector<Solution::Node> nodes_;

    /** Number of items in the solution. */
    ItemPos number_of_items_ = 0;
    /** Number of bins in the solution. */
    BinPos number_of_bins_ = 0;
    /** Total area of the solution. */
    Area area_ = 0;
    /** Total area of the bins of the solution. */
    Area full_area_ = 0;
    /** Total area of the items of the solution. */
    Area item_area_ = 0;
    /** Profit of the solution. */
    Profit profit_ = 0;
    /** Cost of the solution. */
    Profit cost_ = 0;
    /** Width of the solution. */
    Length width_ = 0;
    /** Height of the solution. */
    Length height_ = 0;
    /** Number of copies of each bin type in the solution. */
    std::vector<BinPos> bin_copies_;
    /** Number of copies of each item type in the solution. */
    std::vector<ItemPos> item_copies_;

};

std::ostream& operator<<(std::ostream &os, const Solution::Node& node);
std::ostream& operator<<(std::ostream &os, const Solution& solution);

}
}

