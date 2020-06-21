#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <set>

namespace packingsolver
{

template <typename Solution, typename BranchingScheme>
class AStar
{

public:

    AStar(
            Solution& sol_best,
            BranchingScheme& branching_scheme,
            Counter thread_id_,
            GuideId guide_id,
            Info info):
        thread_id_(thread_id_),
        sol_best_(sol_best),
        branching_scheme_(branching_scheme),
        guide_id_(guide_id),
        info_(info) { }

    void run();

private:

    Counter thread_id_;
    Solution& sol_best_;
    BranchingScheme& branching_scheme_;
    GuideId guide_id_ = 0;
    Info info_ = Info();

    Counter node_number_ = 0;

};

/************************** Template implementation ***************************/

template <typename Solution, typename BranchingScheme>
void AStar<Solution, BranchingScheme>::run()
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;

    LOG_FOLD_START(info_, "astar" << std::endl);

    // Initialize queue
    auto comp = branching_scheme_.compare(guide_id_);
    std::multiset<std::shared_ptr<const Node>, decltype(comp)> q(comp);
    q.insert(branching_scheme_.root());

    while (!q.empty()) {
        node_number_++;
        LOG_FOLD_START(info_, "node_number " << node_number_ << std::endl);

        // Check time
        if (!info_.check_time()) {
            LOG_FOLD_END(info_, "");
            return;
        }

        // Get node from the queue
        auto node_cur = *q.begin();
        q.erase(q.begin());
        LOG_FOLD(info_, "node_cur" << std::endl << *node_cur);

        // Bound
        if (node_cur->bound(sol_best_)) {
            LOG(info_, " bound ×" << std::endl);
            continue;
        }

        for (const Insertion& insertion: branching_scheme_.children(node_cur, info_)) {
            LOG(info_, insertion << std::endl);
            auto child = branching_scheme_.child(node_cur, insertion);
            //LOG_FOLD(info_, "node_tmp" << std::endl << node_tmp);

            // Bound
            if (child->bound(sol_best_)) {
                LOG(info_, " bound ×" << std::endl);
                continue;
            }

            // Update best solution
            if (sol_best_ < *child) {
                std::stringstream ss;
                ss << "A* (thread " << thread_id_ << ")";
                sol_best_.update(child->convert(sol_best_), ss, info_);
            }

            // Add child to the queue
            if (!child->full())
                q.insert(child);
        }

        LOG_FOLD_END(info_, "");
    }

    std::stringstream ss;
    ss << "A* (thread " << thread_id_ << ")";
    PUT(info_, ss.str(), "NodeNumber", node_number_);
    LOG_FOLD_END(info_, "");
}
}

