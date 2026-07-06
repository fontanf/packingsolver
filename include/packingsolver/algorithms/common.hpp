#pragma once

#define _USE_MATH_DEFINES

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "optimizationtools/utils/common.hpp"
#include "optimizationtools/utils/output.hpp"

#include <cstdint>
#include <set>
#include <iomanip>

namespace packingsolver
{

const double PSTOL = 1 + 1e-9;

using Seed = int64_t;
using Counter = int64_t;
using Megabytes = int64_t;

using ItemTypeId = int32_t;
using ItemPos = int32_t;
using GroupId = int32_t;
using StackabilityId = int64_t;
using StackId = int32_t;
using Length = int64_t;
using Area = int64_t;
using Volume = int64_t;
using Weight = double;
using Angle = double;
using AnglePos = int64_t;
using Profit = double;
using CuttingCost = int64_t;
using BinTypeId = int32_t;
using BinPos = int64_t;
using DefectId = int32_t;
using DefectTypeId = int64_t;
using QualityRule = int64_t;
using EligibilityId = int64_t;

using NodeId = int64_t;
using Depth = int16_t;
using GuideId = int16_t;

inline bool strictly_lesser(double v1, double v2)
{
    return v2 - v1 > 1e-6;
}

inline bool strictly_greater(double v1, double v2)
{
    return v1 - v2 > 1e-6;
}

inline bool equal(double v1, double v2)
{
    return std::abs(v1 - v2) <= 1e-6;
}

template<typename T>
double largest_power_of_two_lesser_or_equal(T value)
{
    double res = 1;
    if (res > value) {
        while (res > value)
            res /= 2;
    } else {
        while (res * 2 < value)
            res *= 2;
    }
    return res;
}

template<typename T>
double smallest_power_of_two_greater_or_equal(T value)
{
    double res = 1;
    if (res < value) {
        while (res < value)
            res *= 2;
    } else {
        while (res / 2 > value)
            res /= 2;
    }
    return res;
}


enum class OptimizationMode
{
    Anytime,
    NotAnytime,
    NotAnytimeDeterministic,
    NotAnytimeSequential,
    // AnytimeSequential doesn't exist.
};

std::istream& operator>>(
        std::istream& in,
        OptimizationMode& optimization_mode);

std::ostream& operator<<(
        std::ostream& os,
        OptimizationMode optimization_mode);


enum class ProblemType
{
    RectangleGuillotine,
    Rectangle,
    OneDimensional,
    Box,
    BoxStacks,
    Irregular,
};

std::istream& operator>>(
        std::istream& in,
        ProblemType& problem_type);

std::ostream& operator<<(
        std::ostream& os,
        ProblemType problem_type);


enum class Objective
{
    Default,
    BinPacking,
    BinPackingWithLeftovers,
    OpenDimensionX,
    OpenDimensionY,
    OpenDimensionZ,
    OpenDimensionXY,
    Knapsack,
    VariableSizedBinPacking,
    SequentialOneDimensionalRectangleSubproblem,
    Feasibility,
    BinPackingCuttingCost,
};

std::istream& operator>>(
        std::istream& in,
        Objective& objective);

std::ostream& operator<<(
        std::ostream& os,
        Objective objective);


struct AbstractBinType
{
    BinTypeId id;
    Profit cost;
    BinPos copies;
};

template <typename Instance, typename Solution>
class SolutionPool
{

public:

    struct Entry {
        Solution solution;
        std::string label;
    };

private:

    struct EntryComparator {
        bool operator()(const Entry& e1, const Entry& e2) const {
            if (e1.solution < e2.solution)
                return false;
            if (e2.solution < e1.solution)
                return true;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < e1.solution.instance().number_of_item_types();
                    ++item_type_id)
                if (e1.solution.item_copies(item_type_id)
                        != e2.solution.item_copies(item_type_id))
                    return e1.solution.item_copies(item_type_id)
                        < e2.solution.item_copies(item_type_id);
            return false;
        }
    };

public:

    SolutionPool(const Instance& instance, Counter size_max):
        size_max_(size_max),
        best_{Solution(instance), ""},
        worst_{Solution(instance), ""}
    {
        solutions_.insert({Solution(instance), ""});
    }

    virtual ~SolutionPool() { }

    const std::set<Entry, EntryComparator>& solutions() const { return solutions_; };

    const Solution& best() const { return best_.solution; }
    const std::string& best_label() const { return best_.label; }
    const Solution& worst() const { return worst_.solution; }

    /**
     * Add a solution to the solution pool.
     *
     * Return -1 if the solution is not added
     *        +1 if the solution is the new best solution
     *         0 if the solution is added but is not the new best solution.
     */
    int add(
            const Solution& solution,
            const std::string& label = "")
    {
        // If the solution is worse than the worst solution of the pool, stop.
        if ((Counter)solutions_.size() >= size_max_)
            if (!(worst_.solution < solution))
                return -1;

        // Check again after mutex lock.
        if ((Counter)solutions_.size() >= size_max_) {
            if (!(worst_.solution < solution)) {
                return -1;
            }
        }

        // Add new solution to solution pool.
        auto res = solutions_.insert({solution, label});
        if (!res.second)
            return -1;

        bool new_best = res.first == solutions_.begin();

        // If the pool size is now above its maximum allowed size, remove worst
        // solutions from it.
        if ((Counter)solutions_.size() > size_max_)
            solutions_.erase(std::prev(solutions_.end()));

        // Update best_ and worst_.
        best_ = *solutions_.begin();
        worst_ = *std::prev(solutions_.end());

        return (new_best)? 1: 0;
    }

private:

    Counter size_max_;
    std::set<Entry, EntryComparator> solutions_;
    Entry best_;
    Entry worst_;

};

/**
 * Output stucture for packing optimization algorithms.
 */
template <typename Instance, typename Solution>
struct Output: optimizationtools::Output
{
    Output(const Instance& instance):
        solution_pool(instance, 1) { }


    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;

    /** Knapsack bound. */
    Profit knapsack_bound = std::numeric_limits<Profit>::infinity();

    /** Bin packing bound. */
    BinPos bin_packing_bound = 0;

    /** Bin packing bound. */
    Profit variable_sized_bin_packing_bound = 0;

    /** Open dimension X bound. */
    Length open_dimension_x_bound = 0;

    /** Open dimension Y bound. */
    Length open_dimension_y_bound = 0;

    /** Elapsed time. */
    double time = 0.0;


    virtual nlohmann::json to_json() const
    {
        return nlohmann::json {
            {"Solution", solution_pool.best().to_json()},
            {"Time", time}
        };
    }

    virtual int format_width() const { return 11; }

    virtual void format(std::ostream& os) const
    {
        int width = format_width();
        os
            << std::setw(width) << std::left << "Time (s): " << time << std::endl
            ;
    }
};

template <typename Instance, typename Solution>
using NewSolutionCallback = std::function<void(const Output<Instance, Solution>&)>;

template <typename Instance, typename Solution>
struct Parameters: optimizationtools::Parameters
{
    /** Maximum size of the solution pool. */
    Counter maximum_size_of_the_solution_pool = 1;

    /** New solution callback. */
    NewSolutionCallback<Instance, Solution> new_solution_callback = [](const Output<Instance, Solution>&) { };

    /** JSON search tree path. */
    std::string json_search_tree_path;
};

template <typename Instance>
double largest_bin_space(const Instance& instance)
{
    double space_max = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id)
        if (space_max < instance.bin_type(bin_type_id).space())
            space_max = instance.bin_type(bin_type_id).space();
    return space_max;
}

/**
 * Return a copy of the solution with identical bins grouped together.
 *
 * Bins with the same bin type and the same content (items / stacks / nodes at
 * the same positions) are merged into a single SolutionBin with the combined
 * copy count.  The domain-specific Solution must provide operator== on
 * SolutionBin.
 */
template <typename Solution>
Solution group_identical_bins(const Solution& solution)
{
    struct BinGroup
    {
        BinPos original_bin_pos;
        BinPos total_copies;
    };
    std::vector<BinGroup> groups;

    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const auto& bin = solution.bin(bin_pos);
        bool found = false;
        for (BinGroup& group: groups) {
            if (solution.bin(group.original_bin_pos) == bin) {
                group.total_copies += bin.copies;
                found = true;
                break;
            }
        }
        if (!found)
            groups.push_back({bin_pos, bin.copies});
    }

    Solution grouped_solution(solution.instance());
    for (const BinGroup& group: groups)
        grouped_solution.append(solution, group.original_bin_pos, group.total_copies);
    return grouped_solution;
}

}
