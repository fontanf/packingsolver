#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <set>

namespace packingsolver
{

struct IterativeBeamSearchOptionalParameters
{
    Counter thread_id = 0;
    double growth_factor = 1.5;
    Counter queue_size_min = 0;
    Counter queue_size_max = 100000000;
    Counter maximum_number_of_nodes = -1;

    optimizationtools::Info info = optimizationtools::Info();
};

struct IterativeBeamSearchOutput
{
    Counter number_of_nodes = 0;
    Counter queue_size_max = 1;
};

template <typename Instance, typename Solution, typename BranchingScheme>
inline IterativeBeamSearchOutput iterative_beam_search(
        const BranchingScheme& branching_scheme,
        SolutionPool<Instance, Solution>& solution_pool,
        std::mutex& mutex,
        IterativeBeamSearchOptionalParameters parameters = {})
{
    using Insertion = typename BranchingScheme::Insertion;

    FFOT_LOG_FOLD_START(parameters.info, "IBS" << std::endl);
    IterativeBeamSearchOutput output;
    auto node_hasher = branching_scheme.node_hasher();
    NodeSet<BranchingScheme> q1(branching_scheme);
    NodeSet<BranchingScheme> q2(branching_scheme);
    NodeSet<BranchingScheme> q3(branching_scheme);
    NodeMap<BranchingScheme> history_1{0, node_hasher, node_hasher};
    NodeMap<BranchingScheme> history_2{0, node_hasher, node_hasher};

    for (output.queue_size_max = parameters.queue_size_min;;
            output.queue_size_max = output.queue_size_max * parameters.growth_factor) {
        if (output.queue_size_max == (Counter)(output.queue_size_max * parameters.growth_factor))
            output.queue_size_max++;
        if (output.queue_size_max > parameters.queue_size_max)
            break;

        // Initialize queue.
        bool stop = true;
        NodeSet<BranchingScheme>* q = &q1;
        NodeSet<BranchingScheme>* q_next = &q2;
        NodeSet<BranchingScheme>* q_next_2 = &q3;
        NodeSet<BranchingScheme>* q_tmp = nullptr;
        NodeMap<BranchingScheme>* history_next = &history_1;
        NodeMap<BranchingScheme>* history_next_2 = &history_2;
        NodeMap<BranchingScheme>* history_tmp = nullptr;
        q->clear();
        q_next->clear();
        q_next_2->clear();
        history_next->clear();
        history_next_2->clear();
        auto node_cur = branching_scheme.root();
        q->insert(node_cur);

        for (Counter depth = 0; !q->empty() || !q_next->empty(); ++depth) {

            while (!q->empty()) {
                output.number_of_nodes++;
                FFOT_LOG_FOLD_START(parameters.info, "number_of_nodes " << output.number_of_nodes << std::endl);

                // Check end.
                if (parameters.info.needs_to_end()) {
                    FFOT_LOG_FOLD_END(parameters.info, "");
                    goto ibsend;
                }

                // Check node limit.
                if (parameters.maximum_number_of_nodes != -1
                        && output.number_of_nodes > parameters.maximum_number_of_nodes) {
                    FFOT_LOG_FOLD_END(parameters.info, "");
                    goto ibsend;
                }

                // Get node from the queue.
                auto node_cur = *q->begin();
                q->erase(q->begin());

                // Bound.
                if (branching_scheme.bound(*node_cur, solution_pool.worst())) {
                    FFOT_LOG_FOLD_END(parameters.info, "bound ×");
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
                        mutex.lock();
                        std::stringstream ss;
                        ss << "IBS (thread " << parameters.thread_id << ") q " << output.queue_size_max;
                        solution_pool.add(branching_scheme.to_solution(*child, solution_pool.worst()), ss, parameters.info);
                        mutex.unlock();
                    }

                    // Add child to the queue.
                    if (!branching_scheme.leaf(*child)) {
                        if (child->number_of_items == node_cur->number_of_items + 1) {
                            if ((Counter)q_next->size() >= output.queue_size_max)
                                stop = false;
                            if ((Counter)q_next->size() < output.queue_size_max
                                    || branching_scheme(child, *(std::prev(q_next->end())))) {
                                add_to_history_and_queue(branching_scheme, *history_next, *q_next, child);
                                if ((Counter)q_next->size() > output.queue_size_max)
                                    remove_from_history_and_queue(branching_scheme, *history_next, *q_next, std::prev(q_next->end()));
                            }
                        } else if (child->number_of_items == node_cur->number_of_items + 2) {
                            if ((Counter)q_next_2->size() >= output.queue_size_max)
                                stop = false;
                            if ((Counter)q_next_2->size() < output.queue_size_max
                                    || branching_scheme(child, *(std::prev(q_next_2->end())))) {
                                add_to_history_and_queue(branching_scheme, *history_next_2, *q_next_2, child);
                                if ((Counter)q_next_2->size() > output.queue_size_max)
                                    remove_from_history_and_queue(branching_scheme, *history_next_2, *q_next_2, std::prev(q_next_2->end()));
                            }
                        } else if (child->number_of_items == node_cur->number_of_items) {
                            q->insert(child);
                        } else {
                        }
                    }
                }

            }
            q_tmp = q;
            q = q_next;
            q_next = q_next_2;
            q_next_2 = q_tmp;
            q_next_2->clear();
            history_tmp = history_next;
            history_next = history_next_2;
            history_next_2 = history_tmp;
            history_next_2->clear();
        }

        if (stop)
            break;
    }
ibsend:

    std::stringstream ss;
    ss << "IBS (thread " << parameters.thread_id << ")";
    parameters.info.lock();
    parameters.info.add_to_json(ss.str(), "NumberOfNodes", output.number_of_nodes);
    parameters.info.unlock();
    FFOT_LOG_FOLD_END(parameters.info, "");
    return output;
}

}

