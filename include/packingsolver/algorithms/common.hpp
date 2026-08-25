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
#include <cmath>

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
using ResourceId = int64_t;
using PrecedenceId = int64_t;

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

/**
 * Relative-gap comparisons for profit and cost.
 *
 * strictly_greater/strictly_lesser/equal above use an absolute 1e-6
 * tolerance, which is only meaningful for values whose magnitude is itself
 * close to 1. Profit and cost can be sums over thousands of items (e.g. a
 * knapsack solution's total profit in the tens of millions): summing the
 * same complete set of items in a different order between two otherwise
 * equally good solutions can shift the double-precision result by far more
 * than 1e-6 in absolute terms while being relatively identical, which an
 * absolute-tolerance (or worse, exact) comparison would wrongly treat as
 * one solution being strictly better than the other.
 *
 * The *_profit functions compare a maximized quantity via
 * (v1 - v2) / max(v1, v2); the *_cost functions compare a minimized
 * quantity via (v1 - v2) / min(v1, v2). Falls back to a plain (absolute)
 * comparison when the relevant denominator is 0 or not finite (relative gap
 * is undefined there, and would otherwise silently produce a NaN from an
 * inf/inf division -- since a NaN compares false against +/-1e-6 either
 * way, that NaN would make an unset bound, which defaults to
 * +infinity/-infinity, compare as "equal" to any finite value instead of
 * "not yet meaningfully comparable").
 */
inline bool strictly_greater_profit(double v1, double v2)
{
    if (v1 == v2)
        return false;
    double denominator = std::max(v1, v2);
    return (denominator != 0.0 && std::isfinite(denominator))?
        (v1 - v2) / denominator > 1e-6:
        v1 - v2 > 1e-6;
}

inline bool strictly_lesser_profit(double v1, double v2)
{
    return strictly_greater_profit(v2, v1);
}

inline bool equal_profit(double v1, double v2)
{
    return !strictly_greater_profit(v1, v2) && !strictly_greater_profit(v2, v1);
}

inline bool strictly_greater_cost(double v1, double v2)
{
    if (v1 == v2)
        return false;
    double denominator = std::min(v1, v2);
    return (denominator != 0.0 && std::isfinite(denominator))?
        (v1 - v2) / denominator > 1e-6:
        v1 - v2 > 1e-6;
}

inline bool strictly_lesser_cost(double v1, double v2)
{
    return strictly_greater_cost(v2, v1);
}

inline bool equal_cost(double v1, double v2)
{
    return !strictly_greater_cost(v1, v2) && !strictly_greater_cost(v2, v1);
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


/**
 * File format for an 'Instance::write'/'InstanceBuilder::read' pair.
 *
 * 'Csv' is a set of domain-specific CSV files (items, bins, [defects],
 * parameters); it cannot represent every instance feature (e.g. resources,
 * eligibility, item type precedences depending on the domain), so 'write'
 * throws if the instance has any feature the CSV format can't represent.
 *
 * 'Json' is a single JSON file that can represent every feature of the
 * instance.
 */
enum class InstanceFormat
{
    Csv,
    Json,
};


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

/**
 * A resource of a bin type: a capacity, together with each item type's
 * consumption of it. Shared across every problem type so that a resource
 * (and, in particular, a combinatorial cut expressed as one - see below)
 * means the same thing everywhere; see e.g. 'onedimensional::BinType::resources'
 * / 'InstanceBuilder::add_bin_type_resource' /
 * 'InstanceBuilder::add_resource_consumption' for one domain's wiring of it.
 * Support for actually consuming a resource (let alone a 'penalize' one)
 * varies by domain and by algorithm within a domain - check the domain's own
 * 'BinType'/'Solution'/algorithms before assuming it is handled everywhere.
 *
 * By default ('penalize == false'), the capacity is a hard constraint: a bin
 * whose consumption exceeds it is infeasible. This is what lets
 * combinatorial cuts (e.g. the rectangle Benders decomposition's no-good
 * cuts and pairwise-incompatibility cuts) be expressed exactly as
 * resources, via 'item_consumptions''s per-copy schedule - see its own doc
 * comment.
 *
 * When 'penalize' is 'true' instead, exceeding the capacity does not make
 * the bin infeasible: 'penalty' is subtracted from the solution's profit the
 * first time (per bin) consumption crosses 'capacity', matching a
 * subset-row-cut-style soft penalty (e.g. a triplet cut: packing at least
 * two of three given item types together in the same bin costs 'penalty').
 */
struct Resource
{
    /** Capacity of the resource. */
    double capacity = 0.0;

    /**
     * Resource consumption schedule of each item type, indexed by
     * [item_type_id][copy]: the consumption charged for the 'copy'-th
     * (0-indexed) unit of the item type packed in a bin with this resource.
     * A 'copy' past the end of the schedule repeats its last entry (so a
     * length-1 schedule means "the same consumption regardless of how many
     * copies are already packed" - the common case); an 'item_type_id' past
     * the end of this vector implicitly consumes 0 regardless of 'copy'.
     *
     * A non-uniform schedule lets a resource express "at least N copies of
     * this item type" as a plain capacity/consumption row: an all-ones
     * schedule of length N followed by a single trailing 0 makes the total
     * consumption equal to 'min(count, N)', which only keeps growing while
     * count < N.
     */
    std::vector<std::vector<double>> item_consumptions;

    /**
     * If 'true', exceeding 'capacity' does not make a bin infeasible;
     * instead, 'penalty' is subtracted from the solution's profit. See this
     * struct's own doc comment.
     */
    bool penalize = false;

    /** Penalty subtracted from the solution's profit; only used when 'penalize' is 'true'. */
    double penalty = 0.0;

    /** Get the consumption of the 'copy'-th (0-indexed) unit of an item type. */
    inline double item_consumption(
            ItemTypeId item_type_id,
            ItemPos copy) const
    {
        if (item_type_id >= (ItemTypeId)item_consumptions.size())
            return 0.0;
        const std::vector<double>& schedule = item_consumptions[item_type_id];
        if (schedule.empty())
            return 0.0;
        return (copy < (ItemPos)schedule.size())?
            schedule[copy]:
            schedule.back();
    }
};

/**
 * Prefix sums of 'resource''s per-copy consumption schedule for
 * 'item_type_id': result[k] = sum of
 * 'resource.item_consumption(item_type_id, copy)' for 'copy' in [0, k), for
 * k in [0, schedule.size()] - always has at least one entry (result[0] ==
 * 0), even for an item type absent from 'resource.item_consumptions' or
 * with an empty schedule.
 *
 * Not stored on 'Resource' itself: only a consumer that bundles several
 * copies of an item at once (e.g. rectangle::block.cpp's guillotine blocks)
 * ever needs a *range* sum - every other domain's tree search looks up one
 * copy at a time via 'Resource::item_consumption' directly, so precomputing
 * this unconditionally in every domain's 'InstanceBuilder::build' would pay
 * for something most of them never read. Call this once per (bin type,
 * item type, resource) a block-bundling consumer actually cares about, and
 * pass the result to 'sum_item_consumption' for an O(1) range sum instead
 * of an O(schedule.size()) one - worthwhile whenever the same triple gets
 * queried many times (e.g. once per generated block).
 */
inline std::vector<double> compute_resource_consumption_prefix_sums(
        const Resource& resource,
        ItemTypeId item_type_id)
{
    if (item_type_id >= (ItemTypeId)resource.item_consumptions.size())
        return {0.0};
    const std::vector<double>& schedule = resource.item_consumptions[item_type_id];
    std::vector<double> prefix_sums(schedule.size() + 1, 0.0);
    for (std::size_t k = 0; k < schedule.size(); ++k)
        prefix_sums[k + 1] = prefix_sums[k] + schedule[k];
    return prefix_sums;
}

/**
 * Sum of 'resource.item_consumption(item_type_id, copy)' for 'copy' in
 * '[0, count)', in O(1) given 'prefix_sums' (see
 * 'compute_resource_consumption_prefix_sums', called against the same
 * 'resource'/'item_type_id').
 */
inline double sum_item_consumption(
        const Resource& resource,
        ItemTypeId item_type_id,
        const std::vector<double>& prefix_sums,
        ItemPos count)
{
    ItemPos schedule_length = (ItemPos)prefix_sums.size() - 1;
    if (schedule_length <= 0 || count <= 0)
        return 0.0;
    ItemPos head_count = (std::min)(count, schedule_length);
    double sum = prefix_sums[head_count];
    ItemPos tail_count = count - head_count;
    if (tail_count > 0)
        sum += (double)tail_count * resource.item_consumption(item_type_id, schedule_length - 1);
    return sum;
}

/**
 * Sum, over all 'penalize' resources with a negative 'penalty', of
 * '-resource.penalty' (i.e. the magnitude of the profit gain each one could
 * contribute if triggered).
 *
 * A negative penalty *increases* the reported profit when its resource's
 * capacity is exceeded (see 'Resource'). A Knapsack bound computed without
 * modeling resources at all (e.g. a closed-form area/weight relaxation) is
 * only a valid upper bound on the profit actually reported once resources
 * are taken into account if this worst-case gain - every negative-penalty
 * resource triggering at once - is added back to it.
 */
inline Profit negative_penalty_sum(const std::vector<Resource>& resources)
{
    Profit sum = 0.0;
    for (const Resource& resource: resources)
        if (resource.penalize && resource.penalty < 0.0)
            sum -= resource.penalty;
    return sum;
}

/**
 * Same as 'negative_penalty_sum(const std::vector<Resource>&)', summed over
 * every bin type of 'instance', each weighted by its number of copies: since
 * a 'penalize' resource is tracked per bin (see 'Resource'), each copy of a
 * bin type can trigger it independently, so the worst case scales with the
 * number of available copies.
 */
template <typename Instance>
inline Profit negative_penalty_sum(const Instance& instance)
{
    Profit sum = 0.0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const auto& bin_type = instance.bin_type(bin_type_id);
        sum += bin_type.copies * negative_penalty_sum(bin_type.resources);
    }
    return sum;
}

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
 * Print a "Primal bound" / "Dual bound" / "Gap" block for a pair of bound
 * values on the same scale (which one is the primal and which is the dual
 * doesn't matter here: only their difference relative to the primal value is
 * used).
 */
template <typename T>
void write_bound(
        std::ostream& os,
        int width,
        T primal_bound,
        T dual_bound)
{
    os << std::setw(width) << std::left << "Primal bound: " << primal_bound << std::endl;
    os << std::setw(width) << std::left << "Dual bound: " << dual_bound << std::endl;
    double gap = (primal_bound != 0)?
        100.0 * std::abs((double)primal_bound - (double)dual_bound) / std::abs((double)primal_bound):
        ((dual_bound != 0)? 100.0: 0.0);
    os << std::setw(width) << std::left << "Gap: " << gap << "%" << std::endl;
}

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

    /** Elapsed time. */
    double time = 0.0;


    virtual nlohmann::json to_json() const
    {
        return nlohmann::json {
            {"Solution", solution_pool.best().to_json()},
            {"Time", time}
        };
    }

    virtual int format_width() const
    {
        // Fits "Is proven infeasible: ", now printed for every objective
        // (not just 'Feasibility'), and "Primal bound: ", the longest of
        // the "Primal bound" / "Dual bound" / "Gap" lines printed for every
        // other objective.
        return 23;
    }

    virtual void format(std::ostream& os) const
    {
        int width = format_width();
        os
            << std::setw(width) << std::left << "Time (s): " << time << std::endl
            ;
    }
};

template <typename Instance, typename Solution, typename Output = packingsolver::Output<Instance, Solution>>
using NewSolutionCallback = std::function<void(const Output&)>;

template <typename Instance, typename Solution, typename Output = packingsolver::Output<Instance, Solution>>
struct Parameters: optimizationtools::Parameters
{
    /** Maximum size of the solution pool. */
    Counter maximum_size_of_the_solution_pool = 1;

    /** New solution callback. */
    NewSolutionCallback<Instance, Solution, Output> new_solution_callback = [](const Output&) { };

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
        grouped_solution.append_bin(solution, group.original_bin_pos, group.total_copies);
    return grouped_solution;
}

/**
 * Given a solution that may violate the "bin usage order" constraint (every
 * bin type used before the last one actually used must be used up entirely -
 * see e.g. 'onedimensional::Solution::bin_type_order_feasible'), return an
 * equivalent solution that satisfies it: every bin type used before the last
 * one actually used in 'solution' gets all of its 'copies' included (even
 * the empty ones).
 *
 * Bin instances of the same type are interchangeable (identical capacity),
 * so 'solution's own non-empty bins of a given type are simply carried
 * over, in order, into that many of the type's slots; any remaining slots
 * (for types before the last used one) are left empty. The domain-specific
 * Solution must provide 'append_bin' (see 'group_identical_bins') and
 * 'append_empty_bin(bin_type_id, copies)'.
 *
 * Useful when assembling a solution from a source that has no reason to
 * respect this order on its own - e.g. 'onedimensional::milp_assignment's
 * sequential feasibility scheme, whose recursive 'Feasibility' sub-MILP
 * calls create no 'y' variable and so silently drop any bin instance that
 * ends up empty, which could otherwise make a candidate look satisfiable by
 * skipping mandatory earlier bin types.
 */
template <typename Solution>
Solution enforce_bin_type_order(const Solution& solution)
{
    const auto& instance = solution.instance();

    BinTypeId last_used_bin_type_id = -1;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        last_used_bin_type_id = std::max(
                last_used_bin_type_id,
                solution.bin(bin_pos).bin_type_id);
    }
    if (last_used_bin_type_id == -1)
        return solution;

    // Flatten 'solution's non-empty bins into one entry per physical bin
    // instance (a bin with 'copies' > 1 stands for that many identical
    // physical bins), grouped by bin type.
    std::vector<std::vector<BinPos>> solution_bin_positions(instance.number_of_bin_types());
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const auto& bin = solution.bin(bin_pos);
        for (BinPos copy = 0; copy < bin.copies; ++copy)
            solution_bin_positions[bin.bin_type_id].push_back(bin_pos);
    }

    Solution new_solution(instance);
    for (BinTypeId bin_type_id = 0;
            bin_type_id <= last_used_bin_type_id;
            ++bin_type_id) {
        const std::vector<BinPos>& positions = solution_bin_positions[bin_type_id];
        // Every bin type strictly before the last used one must have all of
        // its copies present (even empty ones); the last used type itself
        // only needs however many 'solution' actually placed.
        BinPos target_copies = (bin_type_id < last_used_bin_type_id)?
            instance.bin_type(bin_type_id).copies:
            (BinPos)positions.size();
        for (BinPos copy = 0; copy < (BinPos)positions.size() && copy < target_copies; ++copy)
            new_solution.append_bin(solution, positions[copy], 1);
        if (target_copies > (BinPos)positions.size())
            new_solution.append_empty_bin(bin_type_id, target_copies - (BinPos)positions.size());
    }
    return new_solution;
}

}
