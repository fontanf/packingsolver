#pragma once

#include "optimizationtools/utils/info.hpp"
#include "optimizationtools/utils/utils.hpp"

#include <cstdint>
#include <set>

namespace packingsolver
{

using Seed = int64_t;
using Counter = int64_t;

using ItemTypeId = int16_t;
using ItemPos = int16_t;
using GroupId = int16_t;
using StackabilityId = int64_t;
using StackId = int16_t;
using Length = int64_t;
using Area = int64_t;
using Volume = int64_t;
using Weight = double;
using Angle = double;
using AnglePos = int64_t;
using Profit = double;
using BinTypeId = int16_t;
using BinPos = int16_t;
using DefectId = int16_t;
using DefectTypeId = int64_t;
using QualityRule = int64_t;
using EligibilityId = int64_t;

using NodeId = int64_t;
using Depth = int16_t;
using GuideId = int16_t;

using optimizationtools::Info;

enum class ProblemType
{
    RectangleGuillotine,
    Rectangle,
    Irregular,
};

enum class Algorithm
{
    Auto,
    TreeSearch,
    ColumnGeneration,
    DichotomicSearch,
    Minlp,
};

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
};

enum class Direction { X, Y, Any };

std::istream& operator>>(std::istream& in, ProblemType& problem_type);
std::istream& operator>>(std::istream& in, Algorithm& algorithm);
std::istream& operator>>(std::istream& in, Objective& objective);
std::istream& operator>>(std::istream& in, Direction& o);
std::ostream& operator<<(std::ostream &os, ProblemType problem_type);
std::ostream& operator<<(std::ostream &os, Algorithm algorithm);
std::ostream& operator<<(std::ostream &os, Objective objective);
std::ostream& operator<<(std::ostream &os, Direction o);

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
        for (ItemTypeId j = 0; j < solution_1.instance().number_of_item_types(); ++j)
            if (solution_1.item_copies(j) != solution_2.item_copies(j))
                return solution_1.item_copies(j) < solution_2.item_copies(j);
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

    bool add(
            const Solution& solution,
            const std::stringstream& ss,
            Info& info)
    {
        // If the solution is worse than the worst solution of the pool, stop.
        if ((Counter)solutions_.size() >= size_max_)
            if (!(worst_ < solution))
                return false;
        // Lock mutex.
        info.lock();
        // Check again after mutex lock.
        if ((Counter)solutions_.size() >= size_max_) {
            if (!(worst_ < solution)) {
                info.unlock();
                return false;
            }
        }
        // If new best solution, display.
        if (*solutions_.begin() < solution)
            solution.display(ss, info);
        // Add new solution to solution pool.
        auto res = solutions_.insert(solution);
        // If the pool size is now above its maximum allowed size, remove worst
        // solutions from it.
        if ((Counter)solutions_.size() > size_max_)
            solutions_.erase(std::prev(solutions_.end()));
        // Update best_ and worst_.
        best_ = *solutions_.begin();
        worst_ = *std::prev(solutions_.end());
        // Unlock mutex.
        info.unlock();
        return res.second;
    }

private:

    Counter size_max_;
    SolutionPoolComparator<Solution> solution_pool_comparator_;
    std::set<Solution, SolutionPoolComparator<Solution>> solutions_;
    Solution best_;
    Solution worst_;

};

template <typename BranchingScheme>
using NodeMap = std::unordered_map<
        std::shared_ptr<typename BranchingScheme::Node>,
        std::vector<std::shared_ptr<typename BranchingScheme::Node>>,
        const typename BranchingScheme::NodeHasher&,
        const typename BranchingScheme::NodeHasher&>;

template <typename BranchingScheme>
using NodeSet = std::set<
        std::shared_ptr<typename BranchingScheme::Node>,
        const BranchingScheme&>;

template <typename BranchingScheme>
inline bool add_to_history_and_queue(
        const BranchingScheme& branching_scheme,
        NodeMap<BranchingScheme>& history,
        NodeSet<BranchingScheme>& q,
        const std::shared_ptr<typename BranchingScheme::Node>& node)
{
    using Node = typename BranchingScheme::Node;
    assert(node != nullptr);

    // If node is not comparable, stop.
    if (branching_scheme.comparable(node)) {
        auto& list = history[node];

        // Check if node is dominated.
        for (const std::shared_ptr<Node>& n: list)
            if (branching_scheme.dominates(n, node))
                return false;

        // Remove dominated nodes from history.
        for (auto it = list.begin(); it != list.end();) {
            if (branching_scheme.dominates(node, *it)) {
                q.erase(*it);
                *it = list.back();
                list.pop_back();
            } else {
                ++it;
            }
        }

        // Add node to history.
        list.push_back(node);
    }

    // Add to queue.
    q.insert(node);
    assert(history.find(node) != history.end());
    return true;
}

template <typename BranchingScheme>
inline void remove_from_history(
        const BranchingScheme& branching_scheme,
        NodeMap<BranchingScheme>& history,
        const std::shared_ptr<typename BranchingScheme::Node>& node)
{
    // Remove from history.
    if (branching_scheme.comparable(node)) {
        assert(history.find(node) != history.end());
        auto& list = history[node];
        for (auto it = list.begin(); it != list.end();) {
            if (*it == node) {
                *it = list.back();
                list.pop_back();
                if (list.empty())
                    history.erase(node);
                return;
            } else {
                ++it;
            }
        }
        assert(false);
    }
}

template <typename BranchingScheme>
inline void remove_from_history_and_queue(
        const BranchingScheme& branching_scheme,
        NodeMap<BranchingScheme>& history,
        std::set<std::shared_ptr<typename BranchingScheme::Node>, const BranchingScheme&>& q,
        typename NodeSet<BranchingScheme>::const_iterator node)
{
    // Remove from history.
    remove_from_history(branching_scheme, history, *node);
    // Remove from queue.
    q.erase(node);
}

}

