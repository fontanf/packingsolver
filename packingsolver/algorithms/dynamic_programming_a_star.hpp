#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <set>

namespace packingsolver
{

template <typename Solution, typename BranchingScheme>
class DynamicProgrammingAStar
{

public:

    DynamicProgrammingAStar(
            Solution& sol_best,
            BranchingScheme& branching_scheme,
            Counter thread_id,
            Counter s,
            GuideId guide_id,
            Info info):
        thread_id_(thread_id),
        sol_best_(sol_best),
        branching_scheme_(branching_scheme),
        s_(s),
        guide_id_(guide_id),
        info_(info) { }

    void run();

private:

    Counter thread_id_;
    Solution& sol_best_;
    BranchingScheme& branching_scheme_;
    Counter s_ = -2;
    GuideId guide_id_;
    Info info_ = Info();

    Counter node_number_ = 0;
    Counter q_sizemax_ = 0;

    bool call_history_1(
            std::vector<std::vector<typename BranchingScheme::Front>>& history,
            const typename BranchingScheme::Node& node);
    void run_1();

    bool call_history_2(
            std::vector<std::vector<std::vector<typename BranchingScheme::Front>>>& history,
            const typename BranchingScheme::Node& node);
    void run_2();

    bool call_history_n(
            std::map<std::vector<ItemPos>, std::vector<typename BranchingScheme::Front>>& history,
            const typename BranchingScheme::Node& node);
    void run_n();

};

/************************** Template implementation ***************************/

template <typename Solution, typename BranchingScheme>
bool DynamicProgrammingAStar<Solution, BranchingScheme>::call_history_1(
        std::vector<std::vector<typename BranchingScheme::Front>>& history,
        const typename BranchingScheme::Node& node)
{
    typedef typename BranchingScheme::Front Front;
    std::vector<Front>& list = history[node.pos_stack(0)];

    // Check if front is dominated
    for (Front f: list) {
        if (branching_scheme_.dominates(f, node.front())) {
            LOG(info_, "history " << node.pos_stack(0) << " " << node.pos_stack(1) << " " << node.front() << std::endl);
            LOG(info_, "dominated by " << f << std::endl);
            return false;
        }
    }

    // Remove dominated fronts in list
    for (auto it = list.begin(); it != list.end();) {
        if (branching_scheme_.dominates(node.front(), *it)) {
            *it = list.back();
            list.pop_back();
        } else {
            ++it;
        }
    }

    // Add front
    LOG(info_, "history " << node.pos_stack(0) << " " << node.pos_stack(1) << " " << node.front() << std::endl);
    list.push_back(node.front());

    return true;
}

template <typename Solution, typename BranchingScheme>
void DynamicProgrammingAStar<Solution, BranchingScheme>::run_1()
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;
    typedef typename BranchingScheme::Front Front;

    LOG_FOLD_START(info_, "DPA* 1" << std::endl);
    assert(sol_best_.instance().stack_number() == 1);

    // Initialize queue
    auto comp = branching_scheme_.compare(guide_id_);
    std::multiset<Node, decltype(comp)> q(comp);
    q.insert(Node(branching_scheme_));

    // Create history
    std::vector<std::vector<Front>> history(sol_best_.instance().stack(0).size()+1);

    while (!q.empty()) {
        node_number_++;
        if (q_sizemax_ < (Counter)q.size())
            q_sizemax_ = q.size();
        LOG_FOLD_START(info_, "node_number " << node_number_ << std::endl);

        // Check time
        if (!info_.check_time()) {
            LOG_FOLD_END(info_, "");
            return;
        }

        // Get node
        Node node_cur(*q.begin());
        q.erase(q.begin());
        LOG_FOLD(info_, "node_cur" << std::endl << node_cur);

        // Bound
        if (node_cur.bound(sol_best_)) {
            LOG(info_, " bound ×" << std::endl);
            return;
        }

        for (const Insertion& insertion: node_cur.children(info_)) {
            LOG(info_, insertion << std::endl);
            Node node_tmp(node_cur);
            node_tmp.apply_insertion(insertion, info_);
            LOG_FOLD(info_, "node_tmp" << std::endl << node_tmp);

            // Bound
            if (node_tmp.bound(sol_best_)) {
                LOG(info_, " bound ×" << std::endl);
                continue;
            }

            // Update best solution
            if (sol_best_ < node_tmp) {
                std::stringstream ss;
                ss << "DPA* 1 (thread " << thread_id_ << ")";
                sol_best_.update(node_tmp.convert(sol_best_), ss, info_);
            }

            // Add to history
            if (insertion.j1 != -1 || insertion.j2 != -1) {
                if (!call_history_1(history, node_tmp)) {
                    LOG(info_, " history cut x" << std::endl);
                    continue;
                }
            }

            // Add child to the queue
            if (!node_tmp.full())
                q.insert(node_tmp);
        }

        LOG_FOLD_END(info_, "");
    }

    LOG_FOLD_END(info_, "");
}

/******************************************************************************/

template <typename Solution, typename BranchingScheme>
bool DynamicProgrammingAStar<Solution, BranchingScheme>::call_history_2(
        std::vector<std::vector<std::vector<typename BranchingScheme::Front>>>& history,
        const typename BranchingScheme::Node& node)
{
    typedef typename BranchingScheme::Front Front;
    std::vector<Front>& list = history[node.pos_stack(0)][node.pos_stack(1)];

    // Check if front is dominated
    for (Front f: list) {
        if (branching_scheme_.dominates(f, node.front())) {
            LOG(info_, "history " << node.pos_stack(0) << " " << node.pos_stack(1) << " " << node.front() << std::endl);
            LOG(info_, "dominated by " << f << std::endl);
            return false;
        }
    }

    // Remove dominated fronts in list
    for (auto it = list.begin(); it != list.end();) {
        if (branching_scheme_.dominates(node.front(), *it)) {
            *it = list.back();
            list.pop_back();
        } else {
            ++it;
        }
    }

    // Add front
    LOG(info_, "history " << node.pos_stack(0) << " " << node.pos_stack(1) << " " << node.front() << std::endl);
    list.push_back(node.front());

    return true;
}

template <typename Solution, typename BranchingScheme>
void DynamicProgrammingAStar<Solution, BranchingScheme>::run_2()
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;
    typedef typename BranchingScheme::Front Front;

    LOG_FOLD_START(info_, "DPA* 2" << std::endl);
    assert(sol_best_.instance().stack_number() == 2);

    // Initialize queue
    auto comp = branching_scheme_.compare(guide_id_);
    std::multiset<Node, decltype(comp)> q(comp);
    q.insert(Node(branching_scheme_));

    // Create history
    std::vector<std::vector<std::vector<Front>>> history;
    for (StackId i = 0; i <= (StackId)sol_best_.instance().stack(0).size(); ++i)
        history.push_back(std::vector<std::vector<Front>>(sol_best_.instance().stack(1).size()+1));

    while (!q.empty()) {
        node_number_++;
        if (q_sizemax_ < (Counter)q.size())
            q_sizemax_ = q.size();
        LOG_FOLD_START(info_, "node_number " << node_number_ << std::endl);

        // Check time
        if (!info_.check_time()) {
            LOG_FOLD_END(info_, "");
            return;
        }

        // Get node
        Node node_cur(*q.begin());
        q.erase(q.begin());
        LOG_FOLD(info_, "node_cur" << std::endl << node_cur);

        // Bound
        if (node_cur.bound(sol_best_)) {
            LOG(info_, " bound ×" << std::endl);
            return;
        }

        for (const Insertion& insertion: node_cur.children(info_)) {
            LOG(info_, insertion << std::endl);
            Node node_tmp(node_cur);
            node_tmp.apply_insertion(insertion, info_);
            LOG_FOLD(info_, "node_tmp" << std::endl << node_tmp);

            // Bound
            if (node_tmp.bound(sol_best_)) {
                LOG(info_, " bound ×" << std::endl);
                continue;
            }

            // Update best solution
            if (sol_best_ < node_tmp) {
                std::stringstream ss;
                ss << "DPA* 2 (thread " << thread_id_ << ")";
                sol_best_.update(node_tmp.convert(sol_best_), ss, info_);
            }

            // Add to history
            if (insertion.j1 != -1 || insertion.j2 != -1) {
                if (!call_history_2(history, node_tmp)) {
                    LOG(info_, " history cut x" << std::endl);
                    continue;
                }
            }

            // Add child to the queue
            if (!node_tmp.full())
                q.insert(node_tmp);
        }

        LOG_FOLD_END(info_, "");
    }

    LOG_FOLD_END(info_, "");
}

/******************************************************************************/

template <typename Solution, typename BranchingScheme>
bool DynamicProgrammingAStar<Solution, BranchingScheme>::call_history_n(
        std::map<std::vector<ItemPos>, std::vector<typename BranchingScheme::Front>>& history,
        const typename BranchingScheme::Node& node)
{
    typedef typename BranchingScheme::Front Front;
    auto list = history.insert({node.pos_stack(), {}}).first;

    // Check if front is dominated
    for (Front f: list->second) {
        if (branching_scheme_.dominates(f, node.front())) {
            LOG(info_, "history " << node.pos_stack(0) << " " << node.pos_stack(1) << " " << node.front() << std::endl);
            LOG(info_, "dominated by " << f << std::endl);
            return false;
        }
    }

    // Remove dominated fronts in list
    for (auto it = list->second.begin(); it != list->second.end();) {
        if (branching_scheme_.dominates(node.front(), *it)) {
            *it = list->second.back();
            list->second.pop_back();
        } else {
            ++it;
        }
    }

    // Add front
    LOG(info_, "history " << node.pos_stack(0) << " " << node.pos_stack(1) << " " << node.front() << std::endl);
    list->second.push_back(node.front());

    return true;
}

template <typename Solution, typename BranchingScheme>
void DynamicProgrammingAStar<Solution, BranchingScheme>::run_n()
{
    typedef typename BranchingScheme::Node Node;
    typedef typename BranchingScheme::Insertion Insertion;
    typedef typename BranchingScheme::Front Front;

    LOG_FOLD_START(info_, "DPA* n" << std::endl);

    auto comp = branching_scheme_.compare(guide_id_);
    std::multiset<Node, decltype(comp)> q(comp);
    q.insert(Node(branching_scheme_));

    // Create history cut object
    std::map<std::vector<ItemPos>, std::vector<Front>> history;

    while (!q.empty()) {
        node_number_++;
        if (q_sizemax_ < (Counter)q.size())
            q_sizemax_ = q.size();
        LOG_FOLD_START(info_, "node " << node_number_ << std::endl);

        // Check time
        if (!info_.check_time()) {
            LOG_FOLD_END(info_, "");
            return;
        }

        // Get node
        Node node_cur(*q.begin());
        q.erase(q.begin());
        LOG_FOLD(info_, "node_cur" << std::endl << node_cur);

        // Bound
        if (node_cur.bound(sol_best_)) {
            LOG(info_, " bound ×" << std::endl);
            return;
        }

        for (const Insertion& insertion: node_cur.children(info_)) {
            LOG(info_, insertion << std::endl);
            Node node_tmp(node_cur);
            node_tmp.apply_insertion(insertion, info_);
            LOG_FOLD(info_, "node_tmp" << std::endl << node_tmp);

            // Bound
            if (node_tmp.bound(sol_best_)) {
                LOG(info_, " bound ×" << std::endl);
                continue;
            }

            // Update best solution
            if (sol_best_ < node_tmp) {
                std::stringstream ss;
                ss << "DPA* n (thread " << thread_id_ << ")";
                sol_best_.update(node_tmp.convert(sol_best_), ss, info_);
            }

            // Add to history
            if (insertion.j1 != -1 || insertion.j2 != -1) {
                if (!call_history_n(history, node_tmp)) {
                    LOG(info_, " history cut x" << std::endl);
                    continue;
                }
            }

            // Add child to the queue
            if (!node_tmp.full())
                q.insert(node_tmp);
        }

        LOG_FOLD_END(info_, "");
    }

    LOG_FOLD_END(info_, "");
}

/******************************************************************************/

template <typename Solution, typename BranchingScheme>
void DynamicProgrammingAStar<Solution, BranchingScheme>::run()
{
    switch (s_) {
    case -2: {
        switch (sol_best_.instance().stack_number()) {
        case 1: {
            run_1();
            break;
        } case 2: {
            run_2();
            break;
        } default: {
        }
        }
        break;
    } case -1: {
        if (sol_best_.instance().stack_number() == 1)
            run_1();
        break;
    } default: {
        if (sol_best_.instance().state_number() <= s_)
            run_n();
    }
    }

    PUT(info_, "DPA*", "Nodes", node_number_);
    PUT(info_, "DPA*", "QueueMaxSize", q_sizemax_);
}

}

