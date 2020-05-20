#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <set>

namespace packingsolver
{

template <typename Solution, typename BranchingScheme>
class MbaStar
{

public:

    MbaStar(
            Solution& sol_best,
            BranchingScheme& branching_scheme,
            Counter thread_id_,
            double growth_factor,
            GuideId guide_id,
            Info info):
        thread_id_(thread_id_),
        sol_best_(sol_best),
        branching_scheme_(branching_scheme),
        growth_factor_(growth_factor),
        guide_id_(guide_id),
        info_(info) { }

    void run();

private:

    Counter thread_id_;
    Solution& sol_best_;
    BranchingScheme& branching_scheme_;
    double growth_factor_ = 1.5;
    GuideId guide_id_ = 0;
    Info info_ = Info();

    Counter node_number_ = 0;
    Counter q_sizemax_ = 1;

};

/************************** Template implementation ***************************/

template <typename Solution, typename BranchingScheme>
void MbaStar<Solution, BranchingScheme>::run()
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;

    LOG_FOLD_START(info_, "MBA*" << std::endl);

    for (q_sizemax_ = 0; q_sizemax_ < (Counter)100000000; q_sizemax_ = q_sizemax_ * growth_factor_) {
        if (q_sizemax_ == (Counter)(q_sizemax_*growth_factor_))
            q_sizemax_++;
        LOG_FOLD_START(info_, "q_sizemax_ " << q_sizemax_ << std::endl);

        // Initialize queue
        auto comp = branching_scheme_.compare(guide_id_);
        std::multiset<Node, decltype(comp)> q(comp);
        q.insert(Node(branching_scheme_));

        while (!q.empty()) {
            node_number_++;
            LOG_FOLD_START(info_, "node_number_ " << node_number_ << std::endl);

            // Check time
            if (!info_.check_time()) {
                LOG_FOLD_END(info_, "");
                goto mbastarend;
            }

            // Get node from the queue
            Node node_cur(*q.begin());
            q.erase(q.begin());
            LOG_FOLD(info_, "node_cur" << std::endl << node_cur);

            // Bound
            if (node_cur.bound(sol_best_)) {
                LOG_FOLD_END(info_, "bound ×");
                continue;
            }

            for (const Insertion& insertion: node_cur.children(info_)) {
                LOG(info_, insertion << std::endl);
                Node node_tmp(node_cur);
                node_tmp.apply_insertion(insertion, info_);

                // Bound
                if (node_tmp.bound(sol_best_)) {
                    LOG(info_, " bound ×" << std::endl);
                    continue;
                }

                // Update best solution
                if (sol_best_ < node_tmp) {
                    std::stringstream ss;
                    ss << "MBA* (thread " << thread_id_ << ") q " << q_sizemax_;
                    sol_best_.update(node_tmp.convert(sol_best_), ss, info_);
                }

                // Add child to the queue
                LOG(info_, " add" << std::endl);
                if (!node_tmp.full()) {
                    // If the insertion would make the queue go above the
                    // threshold, we only add the node if it is not worse than
                    // the worse node of the queue.
                    if ((Counter)q.size() < q_sizemax_ || comp(node_tmp, *(std::prev(q.end())))) {
                        // Insertion
                        auto it = q.insert(node_tmp);
                        // Check if the node dominates some of its neighbors.
                        while (std::next(it) != q.end() && branching_scheme_.dominates(*it, *std::next(it)))
                            q.erase(std::next(it));
                        while (it != q.begin() && branching_scheme_.dominates(*it, *std::prev(it)))
                            q.erase(std::prev(it));
                        // Check if the node is dominated by some of its
                        // neighbors.
                        if (std::next(it) != q.end() && branching_scheme_.dominates(*std::next(it), *it)) {
                            q.erase(it);
                            continue;
                        }
                        if (it != q.begin() && branching_scheme_.dominates(*std::prev(it), *it)) {
                            q.erase(it);
                            continue;
                        }
                        // If the size of the queue is above the threshold,
                        // prune worst nodes.
                        if ((Counter)q.size() > q_sizemax_)
                            q.erase(std::prev(q.end()));
                    }
                }
            }

            LOG_FOLD_END(info_, "");
        }

        LOG_FOLD_END(info_, "");
        std::stringstream ss;
        ss << "MBA* (thread " << thread_id_ << ")";
        PUT(info_, ss.str(), "QueueMaxSize", q_sizemax_);
    }
mbastarend:

    std::stringstream ss;
    ss << "MBA* (thread " << thread_id_ << ")";
    PUT(info_, ss.str(), "NodeNumber", node_number_);
    LOG_FOLD_END(info_, "");
}

}

