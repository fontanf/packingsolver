#pragma once

#include "packingsolver/rectangleguillotine/instance.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangleguillotine
{

typedef int16_t SolutionNodeId;

/**
 * Depth :
 * * -1: root
 * *  0: bin
 * *  1: 1-cut
 * *  2: 2-cut
 * *  3: 3-cut
 * *  4: 4-cut
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
        bool rotate;
        BinTypeId bin_type_id;
    };

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
            BinTypeId bin_type_id,
            const std::vector<ItemTypeId>& item_type_ids,
            BinPos copies);

    /**
     * Getters
     */

    inline const Instance& instance() const { return instance_; }
    inline ItemPos number_of_items() const { return number_of_items_; }
    inline bool full() const { return number_of_items() == instance().number_of_items(); }
    inline BinPos number_of_bins() const { return number_of_bins_; }
    inline Length height() const { return height_; }
    inline Length width() const { return width_; }
    inline Profit profit() const { return profit_; }
    inline Profit cost() const { return cost_; }
    inline Area item_area() const { return item_area_; }
    inline Area area() const { return area_; }
    inline Area full_area() const { return full_area_; }
    inline Area waste() const { return area_ - item_area_; }
    inline double waste_percentage() const { return (double)waste() / area(); }
    inline Area full_waste() const { return full_area() - item_area(); }
    inline double full_waste_percentage() const { return (double)full_waste() / full_area(); }
    inline const std::vector<Solution::Node>& nodes() const { return nodes_; }
    inline ItemPos item_copies(ItemTypeId j) const { return item_copies_[j]; }

    bool operator<(const Solution& solution) const;

    /** CSV export */
    void write(Info& info) const;

    void algorithm_start(Info& info) const;
    void algorithm_end(Info& info) const;

    void display(
            const std::stringstream& algorithm,
            Info& info) const;

private:

    void add_node(const Node& node);

    const Instance& instance_;

    std::vector<Solution::Node> nodes_;

    ItemPos number_of_items_ = 0;
    BinPos number_of_bins_ = 0;
    Area area_ = 0;
    Area full_area_ = 0;
    Area item_area_ = 0;
    Profit profit_ = 0;
    Profit cost_ = 0;
    Length width_ = 0;
    Length height_ = 0;
    std::vector<BinPos> bin_copies_;
    std::vector<ItemPos> item_copies_;

};

std::ostream& operator<<(std::ostream &os, const Solution::Node& node);
std::ostream& operator<<(std::ostream &os, const Solution& solution);

}
}

