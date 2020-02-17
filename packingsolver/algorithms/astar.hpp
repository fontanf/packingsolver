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
    std::multiset<Node, decltype(comp)> q(comp);
    q.insert(Node(branching_scheme_));

    while (!q.empty()) {
        node_number_++;
        LOG_FOLD_START(info_, "node_number " << node_number_ << std::endl);

        // Check time
        if (!info_.check_time()) {
            LOG_FOLD_END(info_, "");
            return;
        }

        // Get node from the queue
        Node node_cur(*q.begin());
        q.erase(q.begin());
        LOG_FOLD(info_, "node_cur" << std::endl << node_cur);

        // Bound
        if (node_cur.bound(sol_best_)) {
            LOG(info_, " bound ×" << std::endl);
            continue;
        }

        for (const Insertion& insertion: node_cur.children(info_)) {
            LOG(info_, insertion << std::endl);
            Node node_tmp(node_cur);
            node_tmp.apply_insertion(insertion, info_);
            //LOG_FOLD(info_, "node_tmp" << std::endl << node_tmp);

            // Bound
            if (node_tmp.bound(sol_best_)) {
                LOG(info_, " bound ×" << std::endl);
                continue;
            }

            // Update best solution
            if (sol_best_ < node_tmp) {
                std::stringstream ss;
                ss << "A* (thread " << thread_id_ << ")";
                sol_best_.update(node_tmp.convert(sol_best_), ss, info_);
            }

            // Add child to the queue
            if (!node_tmp.full())
                q.insert(node_tmp);
        }

        LOG_FOLD_END(info_, "");
    }

    std::stringstream ss;
    ss << "A* (thread " << thread_id_ << ")";
    PUT(info_, ss.str(), "NodeNumber", node_number_);
    LOG_FOLD_END(info_, "");
}
}

