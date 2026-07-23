#include "onedimensional/tree_search.hpp"

#include "packingsolver/onedimensional/algorithm_formatter.hpp"
#include "onedimensional/solution_builder.hpp"
#include "algorithms/thread_pool.hpp"
#include "treesearchsolver/iterative_beam_search_2.hpp"

#include <algorithm>
#include <numeric>
#include <thread>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters)
{
    // Compute bin_type_ids_ and previous_bins_length_.
    //
    // For the Knapsack objective, bins are considered by increasing space,
    // regardless of the order in which they appear in the instance.
    // Otherwise, bins are considered in the order in which they appear in
    // the instance.
    std::vector<BinTypeId> bin_type_id_order(instance.number_of_bin_types());
    std::iota(bin_type_id_order.begin(), bin_type_id_order.end(), 0);
    if (instance.objective() == Objective::Knapsack) {
        std::stable_sort(
                bin_type_id_order.begin(),
                bin_type_id_order.end(),
                [&instance](BinTypeId bin_type_id_1, BinTypeId bin_type_id_2)
                {
                    return instance.bin_type(bin_type_id_1).space()
                        < instance.bin_type(bin_type_id_2).space();
                });
    }
    Length previous_bin_length = 0;
    for (BinTypeId bin_type_id: bin_type_id_order) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            bin_type_ids_.push_back(bin_type_id);
            previous_bins_length_.push_back(previous_bin_length);
            previous_bin_length += bin_type.length;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    const Node& parent = *pparent;
    Node node;
    const ItemType& item_type = instance().item_type(insertion.item_type_id);

    node.parent = pparent;

    node.item_type_id = insertion.item_type_id;

    // Update number_of_bins and last_bin_direction.
    if (insertion.new_bin) {  // New bin.
        node.number_of_bins = insertion.new_bin_pos + 1;
        node.last_bin_length = item_type.length;
        node.last_bin_weight = item_type.weight;
        node.last_bin_number_of_items = 1;
        node.last_bin_weight = item_type.weight;
        node.last_bin_maximum_number_of_items = item_type.maximum_stackability;
        node.last_bin_remaining_weight = item_type.maximum_weight_after;
    } else {  // Same bin.
        node.number_of_bins = parent.number_of_bins;
        node.last_bin_length = parent.last_bin_length + item_type.length - item_type.nesting_length;
        node.last_bin_weight = parent.last_bin_weight + item_type.weight;
        node.last_bin_number_of_items = parent.last_bin_number_of_items + 1;
        node.last_bin_weight = parent.last_bin_weight + item_type.weight;
        node.last_bin_maximum_number_of_items = std::min(
                parent.last_bin_maximum_number_of_items,
                item_type.maximum_stackability);
        node.last_bin_remaining_weight = std::min(
                parent.last_bin_remaining_weight - item_type.weight,
                item_type.maximum_weight_after);
    }

    BinPos i = node.number_of_bins - 1;

    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_length and profit.
    node.item_number_of_copies = parent.item_number_of_copies;
    node.item_number_of_copies[insertion.item_type_id]++;
    node.number_of_items = parent.number_of_items + 1;
    node.item_length = parent.item_length + item_type.length;
    node.squared_item_length = parent.squared_item_length + item_type.length * item_type.length;
    node.profit = parent.profit + item_type.profit;

    // Compute current_length, guide_length and width using uncovered_items.
    node.current_length = previous_bins_length_[i] + node.last_bin_length;
    node.waste = node.current_length - node.item_length;

    node.id = node_id_++;
    return node;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& parent) const
{
    insertions(parent);
    std::vector<std::shared_ptr<Node>> cs(insertions_.size());
    for (Counter i = 0; i < (Counter)insertions_.size(); ++i)
        cs[i] = std::make_shared<Node>(child_tmp(parent, insertions_[i]));
    return cs;
}

const std::vector<BranchingScheme::Insertion>& BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "insertions" << std::endl;

    insertions_.clear();

    // Insert an item in the same bin.
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = bin_type_ids_[parent->number_of_bins - 1];
        const BinType& bin_type = instance().bin_type(bin_type_id);
        for (ItemTypeId item_type_id: bin_type.item_type_ids) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            insertion_item_same_bin(parent, item_type_id);
        }
    }

    // Insert in a new bin.
    // Bins that can't fit any item are skipped: if a bin position doesn't
    // yield any insertion, the next bin position is tried instead.
    for (BinPos new_bin_pos = parent->number_of_bins;
            insertions_.empty() && new_bin_pos < instance().number_of_bins();
            ++new_bin_pos) {
        BinTypeId bin_type_id = bin_type_ids_[new_bin_pos];
        const BinType& bin_type = instance().bin_type(bin_type_id);
        for (ItemTypeId item_type_id: bin_type.item_type_ids) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            insertion_item_new_bin(parent, item_type_id, new_bin_pos);
        }
    }

    return insertions_;
}

void BranchingScheme::insertion_item_same_bin(
        const std::shared_ptr<Node>& parent,
        ItemTypeId item_type_id) const
{
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = bin_type_ids_[parent->number_of_bins - 1];
    const BinType& bin_type = instance().bin_type(bin_type_id);

    // Check bin length.
    if (parent->last_bin_length + item_type.length - item_type.nesting_length > bin_type.length)
        return;
    // Check maximum weight.
    if (parent->last_bin_weight + item_type.weight > bin_type.maximum_weight * PSTOL)
        return;
    // Check maximum stackability.
    ItemPos last_bin_maximum_number_of_items = std::min(
            parent->last_bin_maximum_number_of_items,
            item_type.maximum_stackability);
    if (parent->last_bin_number_of_items + 1 > last_bin_maximum_number_of_items)
        return;
    // Check maximum weight above.
    if (item_type.weight > parent->last_bin_remaining_weight * PSTOL)
        return;

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.new_bin = false;
    insertions_.push_back(insertion);
}

void BranchingScheme::insertion_item_new_bin(
        const std::shared_ptr<Node>& parent,
        ItemTypeId item_type_id,
        BinPos new_bin_pos) const
{
    //std::cout << "insertion_item " << item_type_id << std::endl;
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = bin_type_ids_[new_bin_pos];
    const BinType& bin_type = instance().bin_type(bin_type_id);
    // Check bin length.
    if (item_type.length > bin_type.length)
        return;
    // Check maximum weight.
    if (item_type.weight > bin_type.maximum_weight * PSTOL)
        return;

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.new_bin = true;
    insertion.new_bin_pos = new_bin_pos;
    insertions_.push_back(insertion);
}

bool BranchingScheme::dominates(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (node_1->item_type_id != node_2->item_type_id)
        return false;
    return node_1->current_length <= node_2->current_length;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    BranchingScheme::Node node;
    node.item_number_of_copies = std::vector<ItemPos>(instance_.number_of_item_types(), 0);
    node.id = node_id_++;
    return std::make_shared<Node>(node);
}

bool BranchingScheme::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        if (node_2->profit > node_1->profit)
            return false;
        if (node_2->profit < node_1->profit)
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->number_of_bins > node_1->number_of_bins;
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::Knapsack: {
        return node_2->profit < node_1->profit;
    } case Objective::Feasibility: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "branching scheme 'onedimensional::BranchingScheme' "
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingScheme::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        if (!leaf(node_2)) {
            return (ubkp(*node_1) <= node_2->profit);
        } else {
            if (ubkp(*node_1) != node_2->profit)
                return (ubkp(*node_1) <= node_2->profit);
            return node_1->waste >= node_2->waste;
        }
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_2))
            return false;
        BinPos bin_pos = -1;
        Area a = instance_.item_length() + node_1->waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance_.number_of_bins())
                return true;
            BinTypeId bin_type_id = bin_type_ids_[bin_pos];
            a -= instance().bin_type(bin_type_id).length;
        }
        return (bin_pos + 1 >= node_2->number_of_bins);
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_2))
            return false;
        return node_1->waste >= node_2->waste;
    } case Objective::Knapsack: {
        return false;
        return (ubkp(*node_1) <= node_2->profit);
    } case Objective::Feasibility: {
        if (leaf(node_2))
            return true;
        return false;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "branching scheme 'onedimensional::BranchingScheme' "
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// export ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Solution BranchingScheme::to_solution(
        const std::shared_ptr<Node>& node) const
{
    std::vector<std::shared_ptr<Node>> descendents;
    for (std::shared_ptr<Node> current_node = node;
            current_node->parent != nullptr;
            current_node = current_node->parent) {
        descendents.push_back(current_node);
    }
    std::reverse(descendents.begin(), descendents.end());

    SolutionBuilder solution_builder(instance());
    BinPos bin_pos = -1;
    BinPos number_of_bins = 0;
    for (auto current_node: descendents) {
        // Bins that were skipped because no item fit in them are still
        // added to the solution, as empty bins.
        while (number_of_bins < current_node->number_of_bins) {
            bin_pos = solution_builder.add_bin(bin_type_ids_[number_of_bins], 1);
            number_of_bins++;
        }
        solution_builder.add_item(bin_pos, current_node->item_type_id);
    }
    return solution_builder.build();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return ((item_type_id == insertion.item_type_id)
            && (new_bin == insertion.new_bin)
            );
}

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "item_type_id " << insertion.item_type_id
        << " new_bin " << insertion.new_bin
        ;
    return os;
}

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node)
{
    os << "number_of_items " << node.number_of_items
        << " number_of_bins " << node.number_of_bins
        << std::endl;
    os << "item_length " << node.item_length
        << " current_length " << node.current_length
        << std::endl;
    os << "waste " << node.waste
        << " profit " << node.profit
        << std::endl;

    // item_number_of_copies
    os << "item_number_of_copies" << std::flush;
    for (ItemPos j_pos: node.item_number_of_copies)
        os << " " << j_pos;
    os << std::endl;

    return os;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// tree_search //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const packingsolver::onedimensional::TreeSearchOutput packingsolver::onedimensional::tree_search(
        const Instance& instance,
        const TreeSearchParameters& parameters)
{
    TreeSearchOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    std::vector<GuideId> guides;
    if (!parameters.guides.empty()) {
        guides = parameters.guides;
    } else if (instance.objective() == Objective::Knapsack) {
        guides = {4, 5};
    } else if (instance.objective() == Objective::BinPackingWithLeftovers) {
        guides = {0, 1};
    } else {
        guides = {0, 2};
    }

    std::vector<double> growth_factors = {1.5};
    if (guides.size() * 2 <= 4)
        growth_factors = {1.33, 1.5};
    if (parameters.optimization_mode != OptimizationMode::Anytime)
        growth_factors = {1.5};

    std::vector<BranchingScheme> branching_schemes;
    std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme>> ibs_parameters_list;
    std::vector<Output> local_outputs;
    for (double growth_factor: growth_factors) {
        for (GuideId guide_id: guides) {
            BranchingScheme::Parameters branching_scheme_parameters;
            branching_scheme_parameters.guide_id = guide_id;
            branching_schemes.push_back(BranchingScheme(instance, branching_scheme_parameters));
            treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
            ibs_parameters.verbosity_level = 0;
            ibs_parameters.timer = parameters.timer;
            ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            ibs_parameters.growth_factor = growth_factor;
            if (parameters.optimization_mode != OptimizationMode::Anytime) {
                ibs_parameters.minimum_size_of_the_queue = 1;
                ibs_parameters.growth_factor = parameters.not_anytime_tree_search_queue_size;
                ibs_parameters.maximum_size_of_the_queue = parameters.not_anytime_tree_search_queue_size;
            }
            if (!parameters.json_search_tree_path.empty()) {
                ibs_parameters.json_search_tree_path = parameters.json_search_tree_path
                    + "_guide_" + std::to_string(guide_id);
            }
            ibs_parameters_list.push_back(ibs_parameters);
            local_outputs.push_back(Output(instance));
        }
    }

    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    std::vector<std::thread> threads;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Counter scheme_idx = 0; scheme_idx < (Counter)branching_schemes.size(); ++scheme_idx) {
        // Always record into 'local_outputs[scheme_idx]' first (this is
        // also what the deterministic replay below reads from); in
        // non-deterministic mode, additionally forward immediately to the
        // shared 'algorithm_formatter', since there ordering across
        // schemes doesn't need to be deferred for reproducibility.
        ibs_parameters_list[scheme_idx].new_solution_callback
            = [&algorithm_formatter, &local_outputs, &branching_schemes, scheme_idx, deterministic](
                    const treesearchsolver::Output<BranchingScheme>& tss_output)
            {
                const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                    = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                Solution solution = branching_schemes[scheme_idx].to_solution(
                        tssibs_output.solution_pool.best());
                std::stringstream ss;
                ss << "g " << branching_schemes[scheme_idx].parameters().guide_id
                    << " q " << tssibs_output.maximum_size_of_the_queue;
                local_outputs[(size_t)scheme_idx].solution_pool.add(solution, ss.str());

                if (tssibs_output.optimal) {
                    if (solution.instance().objective() == packingsolver::Objective::BinPacking) {
                        local_outputs[(size_t)scheme_idx].bin_packing_bound
                            = solution.number_of_bins();
                    } else if (solution.instance().objective() == packingsolver::Objective::Feasibility) {
                        if (!solution.full())
                            local_outputs[(size_t)scheme_idx].is_proven_infeasible = true;
                    } else if (solution.instance().objective() == packingsolver::Objective::Knapsack) {
                        local_outputs[(size_t)scheme_idx].knapsack_bound
                            = solution.profit();
                    }
                }

                if (!deterministic) {
                    algorithm_formatter.update_solution(
                            local_outputs[(size_t)scheme_idx].solution_pool.best(),
                            local_outputs[(size_t)scheme_idx].solution_pool.best_label());
                    algorithm_formatter.update_bounds(local_outputs[(size_t)scheme_idx]);
                }
            };
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
            threads.push_back(std::thread(
                        wrapper<decltype(&treesearchsolver::iterative_beam_search_2<BranchingScheme>), treesearchsolver::iterative_beam_search_2<BranchingScheme>>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(branching_schemes[scheme_idx]),
                        ibs_parameters_list[scheme_idx]));
        } else {
            try {
                treesearchsolver::iterative_beam_search_2<BranchingScheme>(
                        branching_schemes[scheme_idx],
                        ibs_parameters_list[scheme_idx]);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    for (Counter thread_idx = 0; thread_idx < (Counter)threads.size(); ++thread_idx)
        threads[thread_idx].join();
    for (const std::exception_ptr& exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
    if (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic) {
        for (Counter scheme_idx = 0; scheme_idx < (Counter)branching_schemes.size(); ++scheme_idx) {
            algorithm_formatter.update_solution(
                    local_outputs[(size_t)scheme_idx].solution_pool.best(),
                    local_outputs[(size_t)scheme_idx].solution_pool.best_label());
            algorithm_formatter.update_bounds(local_outputs[(size_t)scheme_idx]);
        }
    }

    algorithm_formatter.end();
    return output;
}

