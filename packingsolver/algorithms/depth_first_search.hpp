#pragma once

#include "packingsolver/algorithms/common.hpp"

namespace packingsolver
{

struct DepthFirstSearchOptionalParameters
{
    Counter thread_id = 0;
    Info info = Info();
};

struct DepthFirstSearchOutput
{
    Counter node_number = 0;
};

template <typename Instance, typename Solution, typename BranchingScheme>
inline void rec(
        const BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        DepthFirstSearchOptionalParameters parameters,
        const std::shared_ptr<typename BranchingScheme::Node>& node_cur,
        DepthFirstSearchOutput& output)
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;

    LOG_FOLD_START(parameters.info, "rec" << std::endl);
    output.node_number++;

    // Check end
    if (parameters.info.needs_to_end()) {
        LOG_FOLD_END(parameters.info, "");
        return;
    }

    // Bound
    if (branching_scheme.bound(*node_cur, solution_pool.worst())) {
        LOG(parameters.info, " bound ×" << std::endl);
        return;
    }

    std::vector<std::shared_ptr<Node>> children;
    for (const Insertion& insertion: branching_scheme.insertions(node_cur, parameters.info)) {
        auto child = branching_scheme.child(node_cur, insertion);

        // Bound
        if (branching_scheme.bound(*child, solution_pool.worst())) {
            LOG(parameters.info, " bound ×" << std::endl);
            continue;
        }

        // Update best solution
        if (branching_scheme.better(*child, solution_pool.worst())) {
            std::stringstream ss;
            ss << "DFS (thread " << parameters.thread_id << ")";
            solution_pool.add(branching_scheme.to_solution(*child, solution_pool.worst()), ss, parameters.info);
        }

        // Add child to the queue
        if (!branching_scheme.leaf(*child))
            children.push_back(child);
    }

    //sort(children.begin(), children.end(), branching_scheme);

    for (const auto& child: children)
        rec(branching_scheme, solution_pool, parameters, child, output);

    LOG_FOLD_END(parameters.info, "");
}

template <typename Instance, typename Solution, typename BranchingScheme>
inline DepthFirstSearchOutput depth_first_search(
        const BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        DepthFirstSearchOptionalParameters parameters = {})
{
    DepthFirstSearchOutput output;
    auto root = branching_scheme.root();
    rec(branching_scheme, solution_pool, parameters, root, output);
    return output;
}

}

