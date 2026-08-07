#include "rectangle/onedimentional_contiguity/tree_search.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;
using namespace packingsolver::rectangle::onedimentional_contiguity;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const std::vector<NoGoodCut>& cuts,
        const Parameters& parameters):
    instance_(instance),
    cuts_(cuts),
    parameters_(parameters),
    bin_height_(instance.bin_type(0).rect.y),
    units_and_candidates_(build_units_and_candidates(instance, instance.bin_type(0)))
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies; ++copy)
            units_.push_back({item_type_id, copy});
    }

    candidate_to_cuts_.assign(units_and_candidates_.candidates.size(), {});
    for (size_t cut_id = 0; cut_id < cuts_.size(); ++cut_id)
        for (size_t candidate_id: cuts_[cut_id].candidate_ids)
            candidate_to_cuts_[candidate_id].push_back(cut_id);

    profit_suffix_.assign(units_.size() + 1, 0.0);
    for (size_t unit_id = units_.size(); unit_id-- > 0; ) {
        const ItemType& item_type = instance_.item_type(units_[unit_id].first);
        profit_suffix_[unit_id] = profit_suffix_[unit_id + 1] + item_type.profit;
    }
}

const std::vector<BranchingScheme::Insertion>& BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    insertions_.clear();

    if (parent->number_of_units_decided == (ItemPos)units_.size())
        return insertions_;

    size_t unit_id = (size_t)parent->number_of_units_decided;
    ItemTypeId item_type_id = units_[unit_id].first;
    ItemPos copy = units_[unit_id].second;
    const ItemType& item_type = instance_.item_type(item_type_id);
    const BinType& bin_type = instance_.bin_type(0);

    for (size_t candidate_id: units_and_candidates_.candidates_by_item_type_and_copy[item_type_id][copy]) {
        const Candidate& candidate = units_and_candidates_.candidates[candidate_id];

        // Height check: the relaxation's only geometric constraint - every
        // column's total covering height stays within the bin height.
        bool ok = true;
        for (Length c = candidate.x; ok && c < candidate.x + candidate.width; ++c) {
            if (parent->h_used[c] + candidate.height > bin_height_)
                ok = false;
        }
        if (!ok)
            continue;

        // No-good cuts: reject a candidate that would push any cut it
        // belongs to past its own upper bound.
        for (size_t cut_id: candidate_to_cuts_[candidate_id]) {
            if (parent->cut_counts[cut_id] + 1 > cuts_[cut_id].upper_bound) {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;

        // Hard-capacity resources ('penalize' ones never block an
        // insertion - the corresponding penalty is applied to 'profit' in
        // 'child_tmp' instead).
        for (ResourceId resource_id: item_type.resource_ids[0]) {
            const Resource& resource = bin_type.resource(resource_id);
            if (resource.penalize)
                continue;
            double consumption = resource.item_consumption(item_type_id, copy);
            if (parent->resource_consumption[resource_id] + consumption > resource.capacity * PSTOL) {
                ok = false;
                break;
            }
        }
        if (!ok)
            continue;

        Insertion insertion;
        insertion.unit_id = unit_id;
        insertion.place = true;
        insertion.candidate_id = candidate_id;
        insertions_.push_back(insertion);
    }

    // Skipping a unit entirely is only ever sound for 'Knapsack' (an
    // optional item): 'Feasibility' always sets 'copies_min == copies' (see
    // '../benders_decomposition_contiguity.cpp'), so every unit is mandatory
    // there, and offering "skip" would only ever lead to a dead ('valid ==
    // false') leaf.
    if (instance_.objective() == Objective::Knapsack) {
        Insertion insertion;
        insertion.unit_id = unit_id;
        insertion.place = false;
        insertion.candidate_id = 0;
        insertions_.push_back(insertion);
    }

    return insertions_;
}

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    const Node& parent = *pparent;
    Node node;
    node.parent = pparent;
    node.unit_id = insertion.unit_id;
    node.placed = insertion.place;
    node.number_of_units_decided = parent.number_of_units_decided + 1;
    node.item_type_selected_copies = parent.item_type_selected_copies;
    node.h_used = parent.h_used;
    node.resource_consumption = parent.resource_consumption;
    node.cut_counts = parent.cut_counts;
    node.profit = parent.profit;
    node.valid = parent.valid;

    if (insertion.place) {
        node.candidate_id = insertion.candidate_id;
        const Candidate& candidate = units_and_candidates_.candidates[insertion.candidate_id];
        for (Length c = candidate.x; c < candidate.x + candidate.width; ++c)
            node.h_used[c] += candidate.height;
        node.item_type_selected_copies[candidate.item_type_id]++;
        for (size_t cut_id: candidate_to_cuts_[insertion.candidate_id])
            node.cut_counts[cut_id]++;

        const ItemType& item_type = instance_.item_type(candidate.item_type_id);
        if (instance_.objective() == Objective::Knapsack)
            node.profit += item_type.profit;

        // Resource consumption, mirroring 'rectangle::BranchingScheme::
        // child_tmp': a 'penalize' resource never blocks an insertion (see
        // 'insertions' above), but the first time its consumption crosses
        // 'capacity', 'penalty' is subtracted from 'profit'.
        const BinType& bin_type = instance_.bin_type(0);
        for (ResourceId resource_id: item_type.resource_ids[0]) {
            const Resource& resource = bin_type.resource(resource_id);
            double previous_consumption = node.resource_consumption[resource_id];
            double consumption = resource.item_consumption(candidate.item_type_id, candidate.copy);
            node.resource_consumption[resource_id] += consumption;
            if (resource.penalize
                    && node.resource_consumption[resource_id] > resource.capacity
                    && previous_consumption <= resource.capacity) {
                node.profit -= resource.penalty;
            }
        }
    }

    // Leaf validity: every item type must have reached its own
    // 'copies_min' by the time every unit has been decided - checked here
    // rather than pruned proactively during branching, for simplicity (see
    // the file-level comment in the .hpp).
    if (node.number_of_units_decided == (ItemPos)units_.size()) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            if (node.item_type_selected_copies[item_type_id]
                    < instance_.item_type(item_type_id).copies_min) {
                node.valid = false;
                break;
            }
        }
    }

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

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    Node node;
    node.item_type_selected_copies = std::vector<ItemPos>(instance_.number_of_item_types(), 0);
    node.h_used = std::vector<Length>(instance_.bin_type(0).rect.x, 0);
    node.resource_consumption = std::vector<double>(instance_.bin_type(0).number_of_resources(), 0.0);
    node.cut_counts = std::vector<ItemPos>(cuts_.size(), 0);
    node.id = node_id_++;
    return std::make_shared<Node>(node);
}

bool BranchingScheme::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    if (!leaf(node_1))
        return false;
    if (!leaf(node_2))
        return true;
    if (!node_1->valid)
        return false;
    if (!node_2->valid)
        return true;
    if (instance_.objective() == Objective::Knapsack)
        return node_2->profit < node_1->profit;
    // 'Feasibility': any valid leaf settles the question; no leaf is ever
    // "better" than another valid one already found.
    return false;
}

bool BranchingScheme::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    // No usable incumbent yet: nothing to prune against.
    if (!leaf(node_2) || !node_2->valid)
        return false;

    if (instance_.objective() == Objective::Feasibility) {
        // A valid leaf already answers the only question 'Feasibility'
        // asks (does a feasible pattern exist at all?): every other branch
        // can be pruned.
        return true;
    }

    // 'Knapsack': 'node_1' can be pruned if even placing every one of its
    // own remaining units (ignoring height/resource/no-good-cut
    // feasibility - a loose but sound and cheap bound) could not beat
    // 'node_2''s own profit.
    double remaining_upper_bound = profit_suffix_[(size_t)node_1->number_of_units_decided];
    return node_1->profit + remaining_upper_bound <= node_2->profit;
}

OnedimensionalContiguityResult BranchingScheme::to_result(
        const std::shared_ptr<Node>& node) const
{
    OnedimensionalContiguityResult result;
    if (node == nullptr || !leaf(node) || !node->valid)
        return result;

    std::vector<std::shared_ptr<Node>> ancestry;
    for (std::shared_ptr<Node> current = node;
            current->parent != nullptr;
            current = current->parent) {
        ancestry.push_back(current);
    }
    for (auto it = ancestry.rbegin(); it != ancestry.rend(); ++it) {
        if ((*it)->placed)
            result.selected_units.push_back(units_and_candidates_.candidates[(*it)->candidate_id]);
    }
    result.objective_value = node->profit;
    result.status = OnedimensionalContiguityResult::Status::Feasible;
    return result;
}

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return unit_id == insertion.unit_id
        && place == insertion.place
        && candidate_id == insertion.candidate_id;
}

std::ostream& packingsolver::rectangle::onedimentional_contiguity::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "unit_id " << insertion.unit_id
        << " place " << insertion.place
        << " candidate_id " << insertion.candidate_id;
    return os;
}

std::ostream& packingsolver::rectangle::onedimentional_contiguity::operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node)
{
    os << "number_of_units_decided " << node.number_of_units_decided
        << " profit " << node.profit
        << " valid " << node.valid
        << std::endl;
    return os;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// tree_search ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

OnedimensionalContiguityResult packingsolver::rectangle::onedimentional_contiguity::tree_search(
        const Instance& instance,
        const std::vector<NoGoodCut>& cuts,
        const TreeSearchParameters& parameters)
{
    // Same structural pre-check as 'milp' (see its own doc comment): an
    // item type that must be packed but has no candidate placement at all
    // makes the relaxation trivially infeasible - checked here rather than
    // left to the search, which would otherwise have no way to ever reach
    // a leaf at all for that unit.
    UnitsAndCandidates units_and_candidates = build_units_and_candidates(instance, instance.bin_type(0));
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (instance.item_type(item_type_id).copies_min > 0
                && units_and_candidates.candidates_by_item_type[item_type_id].empty()) {
            OnedimensionalContiguityResult result;
            result.status = OnedimensionalContiguityResult::Status::Infeasible;
            return result;
        }
    }

    BranchingScheme::Parameters branching_scheme_parameters;
    branching_scheme_parameters.guide_id = parameters.guide_id;
    BranchingScheme branching_scheme(instance, cuts, branching_scheme_parameters);

    treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
    ibs_parameters.verbosity_level = 0;
    ibs_parameters.timer = parameters.timer;
    if (parameters.optimization_mode == OptimizationMode::Anytime) {
        ibs_parameters.growth_factor = 1.5;
    } else {
        ibs_parameters.minimum_size_of_the_queue = parameters.not_anytime_queue_size;
        ibs_parameters.maximum_size_of_the_queue = parameters.not_anytime_queue_size;
        ibs_parameters.growth_factor = 1.0;
    }

    // 'treesearchsolver::SolutionPool' seeds itself with the branching
    // scheme's own root node (see its constructor), so 'solution_pool.
    // best()' can return that (non-leaf) root - not a real result - as
    // long as no genuine leaf has ever been found; only trust it once it's
    // actually a leaf (see 'to_result' below for the corresponding 'valid'
    // check).
    std::shared_ptr<BranchingScheme::Node> best_node = nullptr;
    ibs_parameters.new_solution_callback
        = [&best_node, &branching_scheme](const treesearchsolver::Output<BranchingScheme>& tss_output)
        {
            const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
            const std::shared_ptr<BranchingScheme::Node>& node = tssibs_output.solution_pool.best();
            if (node != nullptr && branching_scheme.leaf(node))
                best_node = node;
        };

    treesearchsolver::IterativeBeamSearch2Output<BranchingScheme> tssibs_output
        = treesearchsolver::iterative_beam_search_2<BranchingScheme>(
                branching_scheme, ibs_parameters);

    if (best_node != nullptr)
        return branching_scheme.to_result(best_node);

    // No valid leaf found. Only sound to report 'Infeasible' if the search
    // was exhaustive (proven optimal - i.e. it explored the whole tree,
    // not just timed out or hit the queue-size limit); otherwise leave
    // 'result.status' at its default 'Inconclusive', matching 'milp''s own
    // inconclusive-timeout contract.
    OnedimensionalContiguityResult result;
    if (tssibs_output.optimal)
        result.status = OnedimensionalContiguityResult::Status::Infeasible;
    return result;
}
