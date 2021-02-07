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
    Counter node_number_max = -1;
    Info info = Info();
};

struct IterativeMemoryBoundedAStarOutput
{
    Counter node_number = 0;
    Counter queue_size_max = 1;
};

template <typename Instance, typename Solution, typename BranchingScheme>
inline IterativeMemoryBoundedAStarOutput iterative_memory_bounded_a_star(
        const BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        IterativeMemoryBoundedAStarOptionalParameters parameters = {})
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;

    LOG_FOLD_START(parameters.info, "IMBA*" << std::endl);
    IterativeMemoryBoundedAStarOutput output;

    for (output.queue_size_max = parameters.queue_size_min;
            output.queue_size_max <= (Counter)parameters.queue_size_max;
            output.queue_size_max = output.queue_size_max * parameters.growth_factor) {
        if (output.queue_size_max == (Counter)(output.queue_size_max * parameters.growth_factor))
            output.queue_size_max++;
        LOG_FOLD_START(parameters.info, "queue_size_max " << output.queue_size_max << std::endl);

        // Initialize queue
        std::multiset<std::shared_ptr<const Node>, decltype(branching_scheme)> q(branching_scheme);
        q.insert(branching_scheme.root());

        while (!q.empty()) {
            output.node_number++;
            LOG_FOLD_START(parameters.info, "node_number " << output.node_number << std::endl);

            // Check time.
            if (!parameters.info.check_time()) {
                LOG_FOLD_END(parameters.info, "");
                goto mbastarend;
            }

            if (parameters.node_number_max != -1
                    && output.node_number > parameters.node_number_max) {
                LOG_FOLD_END(parameters.info, "");
                goto mbastarend;
            }

            // Get node from the queue.
            auto node_cur = *q.begin();
            q.erase(q.begin());

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
                    // If the insertion would make the queue go above the
                    // threshold, we only add the node if it is not worse than
                    // the worse node of the queue.
                    if ((Counter)q.size() < output.queue_size_max
                            || branching_scheme(child, *(std::prev(q.end())))) {
                        // Insertion
                        auto it = q.insert(child);
                        // Check if the node dominates some of its neighbors.
                        while (std::next(it) != q.end() && branching_scheme.dominates(**it, **std::next(it)))
                            q.erase(std::next(it));
                        while (it != q.begin() && branching_scheme.dominates(**it, **std::prev(it)))
                            q.erase(std::prev(it));
                        // Check if the node is dominated by some of its
                        // neighbors.
                        if (std::next(it) != q.end() && branching_scheme.dominates(**std::next(it), **it)) {
                            q.erase(it);
                            continue;
                        }
                        if (it != q.begin() && branching_scheme.dominates(**std::prev(it), **it)) {
                            q.erase(it);
                            continue;
                        }
                        // If the size of the queue is above the threshold,
                        // prune worst nodes.
                        if ((Counter)q.size() > output.queue_size_max)
                            q.erase(std::prev(q.end()));
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
    PUT(parameters.info, ss.str(), "NodeNumber", output.node_number);
    LOG_FOLD_END(parameters.info, "");
    return output;
}

}

