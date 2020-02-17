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
        BinTypeId i;
        Length l, r, b, t;
        std::vector<SolutionNodeId> children;
        ItemTypeId j;
        bool rotate;
    };

    /** Standard constructor. */
    Solution(const Instance& instance): instance_(instance) { }
    /** Constructor from a list of nodes. */
    Solution(const Instance& instance, const std::vector<Solution::Node>& nodes);

    void update(const Solution& solution, const std::stringstream& algorithm, Info& info);

    /** Copy constructor. */
    Solution(const Solution& solution);
    /** Assignment operator. */
    Solution& operator=(const Solution& solution);

    /** Destructor. */
    virtual ~Solution() { }

    /**
     * Getters
     */

    inline const Instance& instance() const { return instance_; }
    inline ItemPos item_number() const { return item_number_; }
    inline bool full() const { return item_number() == instance().item_number(); }
    inline BinPos bin_number() const { return bin_number_; }
    inline Length last1cut() const { return last_1_cut_; }
    inline Profit profit() const { return profit_; }
    inline Area item_area() const { return item_area_; }
    inline Area area() const { return area_; }
    inline Area full_area() const { return full_area_; }
    inline Area waste() const { return area_ - item_area_; }
    inline double waste_percentage() const { return (double)waste() / area(); }
    inline Area full_waste() const { return full_area() - item_area(); }
    inline double full_waste_percentage() const { return (double)full_waste() / full_area(); }

    template <typename S>
    bool operator<(const S& solution) const;

    /** CSV export */
    void write(Info& info) const;

    void algorithm_start(Info& info);
    void algorithm_end(Info& info);

private:

    const Instance& instance_;

    std::vector<Solution::Node> nodes_;

    ItemPos item_number_ = 0;
    BinPos bin_number_ = 0;
    Area area_ = 0;
    Area full_area_ = 0;
    Area item_area_ = 0;
    Profit profit_ = 0;
    Length last_1_cut_ = 0;

};

std::ostream& operator<<(std::ostream &os, const Solution::Node& node);
std::ostream& operator<<(std::ostream &os, const Solution& solution);

/************************** Template implementation ***************************/

template <typename S>
bool Solution::operator<(const S& solution) const
{
    switch (instance().objective()) {
    case Objective::Default: {
        if (solution.profit() < profit())
            return false;
        if (solution.profit() > profit())
            return true;
        return solution.waste() < waste();
    } case Objective::BinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.bin_number() < bin_number();
    } case Objective::StripPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.last1cut() < last1cut();
    } case Objective::BinPackingLeftovers: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.waste() < waste();
    } case Objective::Knapsack: {
        return solution.profit() > profit();
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, branching scheme rectangle::BranchingScheme does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
        return false;
    }
    }
}

}
}

