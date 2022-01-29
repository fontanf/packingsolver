#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <set>

namespace packingsolver
{

struct IterativeMemoryBoundedAStarOptionalParameters
{
    Counter thread_id = 0;
    double growth_factor = 1.5;
    Counter queue_size_min = 0;
    Counter queue_size_max = 100000000;
    Counter maximum_number_of_nodes = -1;
    Info info = Info();
};

struct IterativeMemoryBoundedAStarOutput
{
    Counter number_of_nodes = 0;
    Counter queue_size_max = 1;
};

template <typename Instance, typename Solution, typename BranchingScheme>
inline IterativeMemoryBoundedAStarOutput iterative_memory_bounded_a_star(
        const BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        IterativeMemoryBoundedAStarOptionalParameters parameters = {})
{
    typedef typename BranchingScheme::Insertion Insertion;

    LOG_FOLD_START(parameters.info, "IMBA*" << std::endl);
    IterativeMemoryBoundedAStarOutput output;
    auto node_hasher = branching_scheme.node_hasher();

    for (output.queue_size_max = parameters.queue_size_min;
            output.queue_size_max <= (Counter)parameters.queue_size_max;
            output.queue_size_max = output.queue_size_max * parameters.growth_factor) {
        if (output.queue_size_max == (Counter)(output.queue_size_max * parameters.growth_factor))
            output.queue_size_max++;
        LOG_FOLD_START(parameters.info, "queue_size_max " << output.queue_size_max << std::endl);

        // Initialize queue
        NodeMap<BranchingScheme> history{0, node_hasher, node_hasher};
        NodeSet<BranchingScheme> q(branching_scheme);
        auto root = branching_scheme.root();
        add_to_history_and_queue(branching_scheme, history, q, root);

        while (!q.empty()) {
            output.number_of_nodes++;
            LOG_FOLD_START(parameters.info, "number_of_nodes " << output.number_of_nodes << std::endl);

            // Check end.
            if (parameters.info.needs_to_end()) {
                LOG_FOLD_END(parameters.info, "");
                goto mbastarend;
            }

            if (parameters.maximum_number_of_nodes != -1
                    && output.number_of_nodes > parameters.maximum_number_of_nodes) {
                LOG_FOLD_END(parameters.info, "");
                goto mbastarend;
            }

            // Get node from the queue.
            auto node_cur = *q.begin();
            //q.erase(q.begin());
            remove_from_history_and_queue(branching_scheme, history, q, q.begin());

            // Bound.
            if (branching_scheme.bound(*node_cur, solution_pool.worst())) {
                LOG_FOLD_END(parameters.info, "bound ×");
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
                    ss << "IMBA* (thread " << parameters.thread_id << ") q " << output.queue_size_max;
                    solution_pool.add(branching_scheme.to_solution(*child, solution_pool.worst()), ss, parameters.info);
                }

                // Add child to the queue.
                LOG(parameters.info, " add" << std::endl);
                if (!branching_scheme.leaf(*child)) {
                    if ((Counter)q.size() < output.queue_size_max
                            || branching_scheme(child, *(std::prev(q.end())))) {
                        add_to_history_and_queue(branching_scheme, history, q, child);
                        if ((Counter)q.size() > output.queue_size_max) {
                            remove_from_history_and_queue(branching_scheme, history, q, std::prev(q.end()));
                            //q.erase(std::prev(q.end()));
                        }
                    }
                }
            }

            LOG_FOLD_END(parameters.info, "");
        }

        LOG_FOLD_END(parameters.info, "");
        std::stringstream ss;
        ss << "IMBA* (thread " << parameters.thread_id << ")";
        PUT(parameters.info, ss.str(), "QueueMaxSize", output.queue_size_max);
    }
mbastarend:

    std::stringstream ss;
    ss << "IMBA* (thread " << parameters.thread_id << ")";
    PUT(parameters.info, ss.str(), "NumberOfNodes", output.number_of_nodes);
    LOG_FOLD_END(parameters.info, "");
    return output;
}

}

