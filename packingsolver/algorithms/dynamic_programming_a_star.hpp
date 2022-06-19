#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <set>

namespace packingsolver
{

struct DynamicProgrammingAStarOptionalParameters
{
    Counter thread_id = 0;
    Info info = Info();
};

struct DynamicProgrammingAStarOutput
{
    Counter number_of_nodes = 0;
    Counter queue_size_max = 1;
};

template <typename Instance, typename Solution, typename BranchingScheme>
inline DynamicProgrammingAStarOutput dynamic_programming_a_star(
        const BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        DynamicProgrammingAStarOptionalParameters parameters = {})
{
    using Insertion = typename BranchingScheme::Insertion;

    FFOT_LOG_FOLD_START(parameters.info, "DPA* 1" << std::endl);
    DynamicProgrammingAStarOutput output;

    auto node_hasher = branching_scheme.node_hasher();
    NodeMap<BranchingScheme> history{0, node_hasher, node_hasher};
    NodeSet<BranchingScheme> q(branching_scheme);

    // Initialize queue
    auto root = branching_scheme.root();
    add_to_history_and_queue(branching_scheme, history, q, root);

    while (!q.empty()) {
        output.number_of_nodes++;
        if (output.queue_size_max < (Counter)q.size())
            output.queue_size_max = q.size();
        FFOT_LOG_FOLD_START(parameters.info, "number_of_nodes " << output.number_of_nodes << std::endl);

        // Check end.
        if (parameters.info.needs_to_end()) {
            FFOT_LOG_FOLD_END(parameters.info, "");
            break;;
        }

        // Get node
        auto node_cur = *q.begin();
        q.erase(q.begin());

        // Bound.
        if (branching_scheme.bound(*node_cur, solution_pool.worst())) {
            FFOT_LOG(parameters.info, " bound ×" << std::endl);
            continue;
        }

        for (const Insertion& insertion: branching_scheme.insertions(node_cur, parameters.info)) {
            auto child = branching_scheme.child(node_cur, insertion);

            // Bound.
            if (branching_scheme.bound(*child, solution_pool.worst())) {
                FFOT_LOG(parameters.info, " bound ×" << std::endl);
                continue;
            }

            // Update best solution.
            if (branching_scheme.better(*child, solution_pool.worst())) {
                std::stringstream ss;
                ss << "DPA* (thread " << parameters.thread_id << ")";
                solution_pool.add(branching_scheme.to_solution(*child, solution_pool.worst()), ss, parameters.info);
            }

            // Add child to the queue.
            if (!branching_scheme.leaf(*child))
                add_to_history_and_queue(branching_scheme, history, q, child);
        }

        FFOT_LOG_FOLD_END(parameters.info, "");
    }

    std::stringstream ss;
    ss << "DPA* (thread " << parameters.thread_id << ")";
    parameters.info.add_to_json(ss.str(), "NumberOfNodes", output.number_of_nodes);
    FFOT_LOG_FOLD_END(parameters.info, "");
    return output;
}

}

