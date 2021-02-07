#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <set>

namespace packingsolver
{

struct DynamicProgrammingAStarOptionalParameters
{
    Counter thread_id = 0;
    Counter bucket_size_max = 2;
    Info info = Info();
};

struct DynamicProgrammingAStarOutput
{
    Counter node_number = 0;
    Counter queue_size_max = 1;
};

template <typename BranchingScheme>
bool call_history(
        const BranchingScheme& branching_scheme,
        std::map<std::vector<ItemPos>, std::vector<std::shared_ptr<const typename BranchingScheme::Node>>>& history,
        const std::shared_ptr<const typename BranchingScheme::Node>& node)
{
    typedef typename BranchingScheme::Node Node;

    const auto& bucket = branching_scheme.bucket(*node);

    // If is not comparable. For example, last insertion was a defect insertion.
    if (bucket.empty())
        return true;

    auto& list = history[bucket];

    // Check if node is dominated.
    for (const std::shared_ptr<const Node>& n: list) {
        if (branching_scheme.dominates(*n, *node)) {
            //LOG(info_, "dominated by " << f << std::endl);
            return false;
        }
    }

    // Remove dominated nodes from history.
    for (auto it = list.begin(); it != list.end();) {
        if (branching_scheme.dominates(*node, **it)) {
            *it = list.back();
            list.pop_back();
        } else {
            ++it;
        }
    }

    // Add node to history.
    //LOG(info_, "history " << node->pos_stack(0) << " " << node->pos_stack(1) << " " << node->front() << std::endl);
    list.push_back(node);
    return true;
}

template <typename Instance, typename Solution, typename BranchingScheme>
inline DynamicProgrammingAStarOutput dynamic_programming_a_star(
        const BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        DynamicProgrammingAStarOptionalParameters parameters = {})
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;

    LOG_FOLD_START(parameters.info, "DPA* 1" << std::endl);
    DynamicProgrammingAStarOutput output;

    // Initialize queue
    std::multiset<std::shared_ptr<const Node>, decltype(branching_scheme)> q(branching_scheme);
    auto root = branching_scheme.root();
    if ((Counter)branching_scheme.bucket(*root).size() > parameters.bucket_size_max)
        return output;
    q.insert(root);

    // Create history.
    std::map<std::vector<ItemPos>, std::vector<std::shared_ptr<const Node>>> history;

    while (!q.empty()) {
        output.node_number++;
        if (output.queue_size_max < (Counter)q.size())
            output.queue_size_max = q.size();
        LOG_FOLD_START(parameters.info, "node_number " << output.node_number << std::endl);

        // Check time.
        if (!parameters.info.check_time()) {
            LOG_FOLD_END(parameters.info, "");
            break;;
        }

        // Get node
        auto node_cur = *q.begin();
        q.erase(q.begin());

        // Bound.
        if (branching_scheme.bound(*node_cur, solution_pool.worst())) {
            LOG(parameters.info, " bound ×" << std::endl);
            continue;
        }

        for (const Insertion& insertion: branching_scheme.insertions(node_cur, parameters.info)) {
            auto child = branching_scheme.child(node_cur, insertion);

            // Bound.
            if (branching_scheme.bound(*child, solution_pool.worst())) {
                LOG(parameters.info, " bound ×" << std::endl);
                continue;
            }

            // Update best solution.
            if (branching_scheme.better(*child, solution_pool.worst())) {
                std::stringstream ss;
                ss << "DPA* (thread " << parameters.thread_id << ")";
                solution_pool.add(branching_scheme.to_solution(*child, solution_pool.worst()), ss, parameters.info);
            }

            // Add to history
            if (!call_history(branching_scheme, history, child)) {
                LOG(parameters.info, " history cut x" << std::endl);
                continue;
            }

            // Add child to the queue
            if (!branching_scheme.leaf(*child))
                q.insert(child);
        }

        LOG_FOLD_END(parameters.info, "");
    }

    std::stringstream ss;
    ss << "DPA* (thread " << parameters.thread_id << ")";
    PUT(parameters.info, ss.str(), "NodeNumber", output.node_number);
    LOG_FOLD_END(parameters.info, "");
    return output;
}

}

