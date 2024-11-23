#pragma once

#define _USE_MATH_DEFINES

#include "optimizationtools/utils/output.hpp"

#include <cstdint>
#include <set>
#include <iomanip>

namespace packingsolver
{

const double PSTOL = 1 + 1e-9;

using Seed = int64_t;
using Counter = int64_t;

using ItemTypeId = int16_t;
using ItemPos = int32_t;
using GroupId = int16_t;
using StackabilityId = int64_t;
using StackId = int32_t;
using Length = int64_t;
using Area = int64_t;
using Volume = int64_t;
using Weight = double;
using Angle = double;
using AnglePos = int64_t;
using Profit = double;
using BinTypeId = int16_t;
using BinPos = int64_t;
using DefectId = int16_t;
using DefectTypeId = int64_t;
using QualityRule = int64_t;
using EligibilityId = int64_t;

using NodeId = int64_t;
using Depth = int16_t;
using GuideId = int16_t;

inline bool strictly_lesser(double v1, double v2)
{
    if (v1 >= v2)
        return false;
    return v2 - v1 > 1e-9 * (std::max)((std::max)(1e-5, std::abs(v1)), std::abs(v2));
}

inline bool strictly_greater(double v1, double v2)
{
    if (v1 <= v2)
        return false;
    return v1 - v2 > 1e-9 * (std::max)((std::max)(1e-5, std::abs(v1)), std::abs(v2));
}

inline bool equal(double v1, double v2)
{
    if (v1 == v2)
        return true;
    return std::abs(v1 - v2) <= 1e-9 * (std::max)((std::max)(1e-5, std::abs(v1)), std::abs(v2));
}


enum class OptimizationMode
{
    Anytime,
    NotAnytime,
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
    OpenDimensionXY,
    Knapsack,
    VariableSizedBinPacking,
    SequentialOneDimensionalRectangleSubproblem,
};

std::istream& operator>>(
        std::istream& in,
        Objective& objective);

std::ostream& operator<<(
        std::ostream& os,
        Objective objective);


enum class Direction { X, Y, Any };

std::istream& operator>>(
        std::istream& in,
        Direction& o);

std::ostream& operator<<(
        std::ostream& os,
        Direction o);


struct AbstractBinType
{
    BinTypeId id;
    Profit cost;
    BinPos copies;
};

template <typename Solution>
struct SolutionPoolComparator
{
    bool operator()(
            const Solution& solution_1,
            const Solution& solution_2) const {
        if (solution_1 < solution_2)
            return false;
        if (solution_2 < solution_1)
            return true;
        for (ItemTypeId item_type_id = 0;
                item_type_id < solution_1.instance().number_of_item_types();
                ++item_type_id)
            if (solution_1.item_copies(item_type_id)
                    != solution_2.item_copies(item_type_id))
                return solution_1.item_copies(item_type_id)
                    < solution_2.item_copies(item_type_id);
        return false;
    }
};

template <typename Instance, typename Solution>
class SolutionPool
{

public:

    SolutionPool(const Instance& instance, Counter size_max):
        size_max_(size_max),
        best_(instance),
        worst_(instance)
    {
        solutions_.insert(Solution(instance));
    }

    virtual ~SolutionPool() { }

    const std::set<Solution, SolutionPoolComparator<Solution>>& solutions() const { return solutions_; };

    const Solution& best() const { return best_; }
    const Solution& worst() const { return worst_; }

    /**
     * Add a solution to the solution pool.
     *
     * Return -1 if the solution is not added
     *        +1 if the solution is the new best solution
     *         0 if the solution is added but is not the new best solution.
     */
    int add(
            const Solution& solution)
    {
        // If the solution is worse than the worst solution of the pool, stop.
        if ((Counter)solutions_.size() >= size_max_)
            if (!(worst_ < solution))
                return -1;

        // Check again after mutex lock.
        if ((Counter)solutions_.size() >= size_max_) {
            if (!(worst_ < solution)) {
                return -1;
            }
        }

        // Add new solution to solution pool.
        auto res = solutions_.insert(solution);
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
    SolutionPoolComparator<Solution> solution_pool_comparator_;
    std::set<Solution, SolutionPoolComparator<Solution>> solutions_;
    Solution best_;
    Solution worst_;

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

template<class F, F f> struct wrapper_impl;
template<class R, class... Args, R(*f)(Args...)>
struct wrapper_impl<R(*)(Args...), f>
{
    static void wrap(
            std::exception_ptr& exception_ptr,
            Args... args)
    {
        try {
            f(args...);
        } catch (...) {
            exception_ptr = std::current_exception();
        }
    }
};

template<class F, F f>
constexpr auto wrapper = wrapper_impl<F, f>::wrap;

}
