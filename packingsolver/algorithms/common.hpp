#pragma once

#include "optimizationtools/info.hpp"
#include "optimizationtools/utils.hpp"

#include <cstdint>
#include <set>

namespace packingsolver
{

typedef int16_t ItemTypeId;
typedef int16_t ItemPos;
typedef int64_t Length;
typedef int64_t Area;
typedef int16_t StackId;
typedef int64_t Profit;
typedef int16_t DefectId;
typedef int16_t BinTypeId;
typedef int16_t BinPos;
typedef int16_t Depth;
typedef int64_t Seed;
typedef int16_t GuideId;
typedef int64_t Counter;
typedef int64_t NodeId;

using optimizationtools::Info;

enum class ProblemType { RectangleGuillotine, Rectangle };
enum class Objective {
    Default,
    BinPacking,
    BinPackingWithLeftovers,
    StripPackingWidth,
    StripPackingHeight,
    Knapsack,
    VariableSizedBinPacking,
};

std::istream& operator>>(std::istream& in, ProblemType& problem_type);
std::istream& operator>>(std::istream& in, Objective& objective);
std::ostream& operator<<(std::ostream &os, ProblemType problem_type);
std::ostream& operator<<(std::ostream &os, Objective objective);

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
        for (ItemTypeId j = 0; j < solution_1.instance().item_type_number(); ++j)
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
        info.output->mutex_sol.lock();
        // Check again after mutex lock.
        if ((Counter)solutions_.size() >= size_max_) {
            if (!(worst_ < solution)) {
                info.output->mutex_sol.unlock();
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
        info.output->mutex_sol.unlock();
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
    typedef typename BranchingScheme::Node Node;
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

