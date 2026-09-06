#include "rectangle/tree_search_maximal_spaces.hpp"

#include "rectangle/sequential_feasibility.hpp"
#include "rectangle/solution_builder.hpp"
#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "algorithms/thread_pool.hpp"
#include "treesearchsolver/iterative_beam_search.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::rectangle;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Internals ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

AnchorInfo compute_anchor_info(const EmptySpace& space, const packingsolver::rectangle::Rectangle& bin_rect)
{
    Length dist_x_start = space.bl_corner.x;
    Length dist_x_end = bin_rect.x - space.xe();
    Length dist_y_start = space.bl_corner.y;
    Length dist_y_end = bin_rect.y - space.ye();
    AnchorInfo info;
    info.dir_x = dist_x_end < dist_x_start;
    info.dir_y = dist_y_end < dist_y_start;
    info.distance = std::min(dist_x_start, dist_x_end)
        + std::min(dist_y_start, dist_y_end);
    info.space_area = space.rect.area();
    return info;
}

Point compute_block_position(
        const AnchorInfo& anchor,
        const EmptySpace& space,
        const packingsolver::rectangle::Rectangle& block_rect)
{
    Point pos;
    pos.x = anchor.dir_x ? space.xe() - block_rect.x : space.xs();
    pos.y = anchor.dir_y ? space.ye() - block_rect.y : space.ys();
    return pos;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
//////////////////////// BranchingSchemeMaximalSpaces //////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingSchemeMaximalSpaces::BranchingSchemeMaximalSpaces(
        const Instance& instance,
        const std::vector<std::vector<Block>>& blocks,
        const MaxReachableLengths& max_reachable_lengths,
        const Parameters& parameters):
    instance_(instance),
    blocks_(blocks),
    parameters_(parameters)
{
    max_reachable_x_ = max_reachable_lengths.x;
    max_reachable_y_ = max_reachable_lengths.y;

    BinPos number_of_bins = instance_.number_of_bins();
    bin_rects_.resize(number_of_bins);
    for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
        const BinType& bin_type = instance_.bin_type(instance_.bin_type_id(bin_pos));
        bin_rects_[bin_pos].x = max_reachable_lengths.x[bin_type.rect.x];
        bin_rects_[bin_pos].y = max_reachable_lengths.y[bin_type.rect.y];
        total_bin_area_ += bin_rects_[bin_pos].area();
    }
}

const std::shared_ptr<BranchingSchemeMaximalSpaces::Node> BranchingSchemeMaximalSpaces::root() const
{
    auto node = std::make_shared<Node>();
    node->id = node_id_++;
    node->item_number_of_copies.assign(instance_.number_of_item_types(), 0);

    BinPos number_of_bins = instance_.number_of_bins();
    node->empty_spaces.resize(number_of_bins);
    node->placed_blocks.resize(number_of_bins);
    node->resource_consumption.resize(number_of_bins);
    node->weight.assign(number_of_bins, 0);
    node->valid_block_ids.resize(number_of_bins);

    for (BinPos bin_pos = 0; bin_pos < number_of_bins; ++bin_pos) {
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        const BinType& bin_type = instance_.bin_type(bin_type_id);

        EmptySpace space;
        space.bl_corner = {0, 0};
        space.rect = bin_rects_[bin_pos];
        node->empty_spaces[bin_pos].push_back(space);
        for (const Defect& defect: bin_type.defects)
            cut_spaces(node->empty_spaces[bin_pos], defect.pos, defect.rect);

        ItemPos number_of_blocks = (ItemPos)blocks_[bin_type_id].size();
        node->valid_block_ids[bin_pos].resize(number_of_blocks);
        std::iota(node->valid_block_ids[bin_pos].begin(), node->valid_block_ids[bin_pos].end(), (ItemPos)0);

        remove_unusable_spaces(*node, bin_pos);
    }
    return node;
}

BranchingSchemeMaximalSpaces::BestSpaceResult BranchingSchemeMaximalSpaces::find_best_space(
        const Node& parent) const
{
    BestSpaceResult result;
    Length best_distance = std::numeric_limits<Length>::max();
    Area best_area = 0;
    int best_corner = std::numeric_limits<int>::min();
    for (BinPos bin_pos = 0;
            bin_pos < (BinPos)parent.empty_spaces.size();
            ++bin_pos) {
        const std::vector<EmptySpace>& spaces = parent.empty_spaces[bin_pos];
        for (ItemPos space_idx = 0;
                space_idx < (ItemPos)spaces.size();
                ++space_idx) {
            const EmptySpace& space = spaces[space_idx];
            AnchorInfo anchor = compute_anchor_info(space, bin_rects_[bin_pos]);
            Area space_area = space.rect.area();
            int corner = (anchor.dir_x ? 2 : 0) | (anchor.dir_y ? 1 : 0);
            bool is_better = (anchor.distance < best_distance)
                || (anchor.distance == best_distance && space_area > best_area)
                || (anchor.distance == best_distance && space_area == best_area && corner > best_corner);
            if (is_better) {
                result.bin_pos = bin_pos;
                result.space_idx = space_idx;
                result.anchor = anchor;
                best_distance = anchor.distance;
                best_area = space_area;
                best_corner = corner;
            }
        }
    }
    return result;
}

double BranchingSchemeMaximalSpaces::compute_area_loss_factor(
        const SpaceContactInfo& info,
        const Block& block) const
{
    Length rx = info.space_bx - block.rect.x;
    Length ry = info.space_by - block.rect.y;
    Area usable_area = (Area)(block.rect.x + max_reachable_x_[rx])
        * (block.rect.y + max_reachable_y_[ry]);
    Area space_area = (Area)info.space_bx * info.space_by;
    return (double)usable_area / space_area;
}

double BranchingSchemeMaximalSpaces::compute_insertion_guide(
        const Node& parent,
        const Insertion& insertion,
        const SpaceContactInfo& info) const
{
    BinTypeId bin_type_id = instance_.bin_type_id(insertion.bin_pos);
    const Block& block = blocks_[bin_type_id][insertion.block_id];

    double fill_rate = (double)parent.item_area / total_bin_area_;
    double v = insertion_profit(parent, block, insertion.bin_pos);
    double l = compute_area_loss_factor(info, block);
    double n = (double)block.number_of_items;
    if (fill_rate < parameters_.configuration_switch_threshold) {
        double c = compute_relative_contact_area(info, block, insertion.bl_corner, parameters_.delta);
        return v
            * std::pow(c, parameters_.alpha)
            * std::pow(l, parameters_.beta)
            * std::pow(n, -parameters_.gamma);
    } else {
        double c = compute_relative_contact_area(info, block, insertion.bl_corner, parameters_.delta_2);
        return v
            * std::pow(c, parameters_.alpha_2)
            * std::pow(l, parameters_.beta_2)
            * std::pow(n, -parameters_.gamma_2);
    }
}

void BranchingSchemeMaximalSpaces::update_node_max_reachable(const Node& node) const
{
    Length max_x = (Length)max_reachable_x_.size() - 1;
    Length max_y = (Length)max_reachable_y_.size() - 1;

    scratch_reachable_x_.assign(max_x + 1, 0);
    scratch_reachable_y_.assign(max_y + 1, 0);
    scratch_reachable_x_[0] = scratch_reachable_y_[0] = 1;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        ItemPos remaining_copies = item_type.copies - node.item_number_of_copies[item_type_id];
        int number_of_rotations = item_type.oriented ? 1 : 2;
        for (ItemPos copy_idx = 0; copy_idx < remaining_copies; ++copy_idx) {
            scratch_temp_reachable_x_ = scratch_reachable_x_;
            scratch_temp_reachable_y_ = scratch_reachable_y_;
            for (int rot_idx = 0; rot_idx < number_of_rotations; ++rot_idx) {
                bool rotate = (rot_idx == 1);
                Length rx = item_type.x(rotate);
                Length ry = item_type.y(rotate);
                if (rx <= max_x)
                    for (Length v = max_x; v >= rx; --v)
                        if (scratch_reachable_x_[v - rx]) scratch_temp_reachable_x_[v] = 1;
                if (ry <= max_y)
                    for (Length v = max_y; v >= ry; --v)
                        if (scratch_reachable_y_[v - ry]) scratch_temp_reachable_y_[v] = 1;
            }
            scratch_reachable_x_ = scratch_temp_reachable_x_;
            scratch_reachable_y_ = scratch_temp_reachable_y_;
        }
    }

    for (Length v = 1; v <= max_x; ++v)
        max_reachable_x_[v] = scratch_reachable_x_[v] ? v : max_reachable_x_[v - 1];
    for (Length v = 1; v <= max_y; ++v)
        max_reachable_y_[v] = scratch_reachable_y_[v] ? v : max_reachable_y_[v - 1];
}

double BranchingSchemeMaximalSpaces::active_delta(const Node& node) const
{
    double fill_rate = (double)node.item_area / total_bin_area_;
    if (fill_rate < parameters_.configuration_switch_threshold) {
        return parameters_.delta;
    } else {
        return parameters_.delta_2;
    }
}

bool BranchingSchemeMaximalSpaces::block_resource_capacity_ok(
        const Node& parent,
        const Block& block,
        BinPos bin_pos) const
{
    const BinType& bin_type = instance_.bin_type(instance_.bin_type_id(bin_pos));
    const std::vector<double>& resource_consumption = parent.resource_consumption[bin_pos];
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (resource.penalize)
            continue;
        double previous_consumption = resource_consumption.empty()?
            0.0: resource_consumption[resource_id];
        if (previous_consumption + block.resource_consumption[resource_id] > resource.capacity * PSTOL)
            return false;
    }
    return true;
}

Profit BranchingSchemeMaximalSpaces::insertion_profit(
        const Node& parent,
        const Block& block,
        BinPos bin_pos) const
{
    const BinType& bin_type = instance_.bin_type(instance_.bin_type_id(bin_pos));
    const std::vector<double>& resource_consumption = parent.resource_consumption[bin_pos];
    Profit profit = block.item_profit;
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (!resource.penalize)
            continue;
        double previous_consumption = resource_consumption.empty()?
            0.0: resource_consumption[resource_id];
        double new_consumption = previous_consumption + block.resource_consumption[resource_id];
        if (new_consumption > resource.capacity && previous_consumption <= resource.capacity)
            profit -= resource.penalty;
    }
    return profit;
}

const std::vector<BranchingSchemeMaximalSpaces::Insertion>& BranchingSchemeMaximalSpaces::insertions(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "insertions"
    //    << " id " << parent->id
    //    << " p " << parent->profit
    //    << " n " << parent->number_of_items
    //    << " a " << parent->block_area
    //    << " rb " << parent->valid_block_ids.size()
    //    << " es " << parent->empty_spaces.size()
    //    << std::endl;
    update_node_max_reachable(*parent);
    insertions_.clear();

    double delta = active_delta(*parent);

    BestSpaceResult best = find_best_space(*parent);
    if (best.space_idx != -1) {
        BinPos bin_pos = best.bin_pos;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        const EmptySpace& space = parent->empty_spaces[bin_pos][best.space_idx];
        SpaceContactInfo contact_info = compute_space_contact_info(
                parent->placed_blocks[bin_pos], bin_pos, space, delta);
        for (ItemPos block_id: parent->valid_block_ids[bin_pos]) {
            const Block& block = blocks_[bin_type_id][block_id];
            if (block.rect.x > space.rect.x
                    || block.rect.y > space.rect.y)
                continue;
            // Weight and resource capacity are already guaranteed by
            // 'valid_block_ids' (pruned in 'apply_insertion' - both only
            // ever grow monotonically, so a block ruled out there stays
            // ruled out for the rest of this subtree).
            Insertion insertion;
            insertion.bin_pos = bin_pos;
            insertion.space_id = best.space_idx;
            insertion.block_id = block_id;
            insertion.bl_corner = compute_block_position(best.anchor, space, block.rect);
            insertion.guide = compute_insertion_guide(*parent, insertion, contact_info);
            insertions_.push_back(insertion);
        }
    }

    std::sort(insertions_.begin(), insertions_.end(),
            [](const Insertion& insertion_1, const Insertion& insertion_2) {
                return insertion_1.guide > insertion_2.guide;
            });
    return insertions_;
}

BranchingSchemeMaximalSpaces::Insertion BranchingSchemeMaximalSpaces::best_insertion(
        Node& parent) const
{
    Insertion best;
    double best_score = 0;

    double delta = active_delta(parent);

    BestSpaceResult best_space = find_best_space(parent);
    if (best_space.space_idx != -1) {
        BinPos bin_pos = best_space.bin_pos;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        const EmptySpace& space = parent.empty_spaces[bin_pos][best_space.space_idx];
        SpaceContactInfo contact_info = compute_space_contact_info(
                parent.placed_blocks[bin_pos], bin_pos, space, delta);
        for (ItemPos block_id: parent.valid_block_ids[bin_pos]) {
            const Block& block = blocks_[bin_type_id][block_id];
            if (block.rect.x > space.rect.x
                    || block.rect.y > space.rect.y)
                continue;
            // Weight and resource capacity are already guaranteed by
            // 'valid_block_ids' - see the matching comment in
            // 'insertions'.
            Insertion insertion;
            insertion.bin_pos = bin_pos;
            insertion.space_id = best_space.space_idx;
            insertion.block_id = block_id;
            insertion.bl_corner = compute_block_position(best_space.anchor, space, block.rect);
            double score = compute_insertion_guide(parent, insertion, contact_info);
            if (score > best_score) {
                best_score = score;
                best = insertion;
            }
        }
    }

    return best;
}

BranchingSchemeMaximalSpaces::SpaceContactInfo BranchingSchemeMaximalSpaces::compute_space_contact_info(
        const std::vector<Node::PlacedBlock>& placed_blocks,
        BinPos bin_pos,
        const EmptySpace& space,
        double delta) const
{
    BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
    const Rectangle& bin_rect = bin_rects_[bin_pos];

    SpaceContactInfo info;
    info.space_xs = space.bl_corner.x;
    info.space_ys = space.bl_corner.y;
    info.space_bx = space.rect.x;
    info.space_by = space.rect.y;
    info.tol_x = (Length)(delta * space.rect.x);
    info.tol_y = (Length)(delta * space.rect.y);

    Length xl = space.bl_corner.x, xh = xl + space.rect.x;
    Length yl = space.bl_corner.y, yh = yl + space.rect.y;
    info.xl_wall = (xl <= info.tol_x);
    info.yl_wall = (yl <= info.tol_y);
    info.xh_wall = (bin_rect.x - xh <= info.tol_x);
    info.yh_wall = (bin_rect.y - yh <= info.tol_y);

    for (const Node::PlacedBlock& pb: placed_blocks) {
        const Block& pb_block = blocks_[bin_type_id][pb.block_id];
        Length pb_xl = pb.bl_corner.x, pb_xh = pb_xl + pb_block.rect.x;
        Length pb_yl = pb.bl_corner.y, pb_yh = pb_yl + pb_block.rect.y;

        if (std::abs(pb_xh - xl) <= info.tol_x) {
            Length lo1 = std::max((Length)0, pb_yl - yl);
            Length hi1 = std::min(space.rect.y, pb_yh - yl);
            if (lo1 < hi1)
                info.xl_neighbors.push_back({lo1, hi1, pb_xh});
        }
        if (std::abs(pb_xl - xh) <= info.tol_x) {
            Length lo1 = std::max((Length)0, pb_yl - yl);
            Length hi1 = std::min(space.rect.y, pb_yh - yl);
            if (lo1 < hi1)
                info.xh_neighbors.push_back({lo1, hi1, pb_xl});
        }
        if (std::abs(pb_yh - yl) <= info.tol_y) {
            Length lo1 = std::max((Length)0, pb_xl - xl);
            Length hi1 = std::min(space.rect.x, pb_xh - xl);
            if (lo1 < hi1)
                info.yl_neighbors.push_back({lo1, hi1, pb_yh});
        }
        if (std::abs(pb_yl - yh) <= info.tol_y) {
            Length lo1 = std::max((Length)0, pb_xl - xl);
            Length hi1 = std::min(space.rect.x, pb_xh - xl);
            if (lo1 < hi1)
                info.yh_neighbors.push_back({lo1, hi1, pb_yl});
        }
    }
    return info;
}

double BranchingSchemeMaximalSpaces::compute_relative_contact_area(
        const SpaceContactInfo& info,
        const Block& block,
        Point bl_corner,
        double delta) const
{
    Length rel_x = bl_corner.x - info.space_xs;
    Length rel_y = bl_corner.y - info.space_ys;
    Length btol_x = (Length)(delta * block.rect.x);
    Length btol_y = (Length)(delta * block.rect.y);
    Length block_xl = bl_corner.x;
    Length block_xh = bl_corner.x + block.rect.x;
    Length block_yl = bl_corner.y;
    Length block_yh = bl_corner.y + block.rect.y;

    Length contact = 0;

    // XL edge (block's -x edge)
    if (info.xl_wall && std::abs(block_xl - info.space_xs) <= btol_x)
        contact += block.rect.y;
    for (const SpaceContactInfo::Neighbor& nb: info.xl_neighbors) {
        if (std::abs(block_xl - nb.orthogonal_pos) > btol_x)
            continue;
        Length ov = std::max((Length)0, std::min(rel_y + block.rect.y, nb.hi1) - std::max(rel_y, nb.lo1));
        contact += ov;
    }
    // XH edge (block's +x edge)
    if (info.xh_wall && std::abs(block_xh - (info.space_xs + info.space_bx)) <= btol_x)
        contact += block.rect.y;
    for (const SpaceContactInfo::Neighbor& nb: info.xh_neighbors) {
        if (std::abs(block_xh - nb.orthogonal_pos) > btol_x)
            continue;
        Length ov = std::max((Length)0, std::min(rel_y + block.rect.y, nb.hi1) - std::max(rel_y, nb.lo1));
        contact += ov;
    }
    // YL edge (block's -y edge)
    if (info.yl_wall && std::abs(block_yl - info.space_ys) <= btol_y)
        contact += block.rect.x;
    for (const SpaceContactInfo::Neighbor& nb: info.yl_neighbors) {
        if (std::abs(block_yl - nb.orthogonal_pos) > btol_y)
            continue;
        Length ov = std::max((Length)0, std::min(rel_x + block.rect.x, nb.hi1) - std::max(rel_x, nb.lo1));
        contact += ov;
    }
    // YH edge (block's +y edge)
    if (info.yh_wall && std::abs(block_yh - (info.space_ys + info.space_by)) <= btol_y)
        contact += block.rect.x;
    for (const SpaceContactInfo::Neighbor& nb: info.yh_neighbors) {
        if (std::abs(block_yh - nb.orthogonal_pos) > btol_y)
            continue;
        Length ov = std::max((Length)0, std::min(rel_x + block.rect.x, nb.hi1) - std::max(rel_x, nb.lo1));
        contact += ov;
    }

    double block_perimeter = 2.0 * (block.rect.x + block.rect.y);
    return (double)contact / block_perimeter;
}

void BranchingSchemeMaximalSpaces::apply_insertion(
        Node& node,
        const Insertion& insertion) const
{
    node.id = node_id_++;

    BinPos bin_pos = insertion.bin_pos;
    BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
    const Block& block = blocks_[bin_type_id][insertion.block_id];
    const BinType& bin_type = instance_.bin_type(bin_type_id);

    // 'insertion_profit' must run against 'node''s resource_consumption
    // right before this block's own is added below (it detects a
    // 'penalize' resource's *first* crossing of its capacity).
    node.profit += insertion_profit(node, block, bin_pos);
    if (bin_type.number_of_resources() > 0) {
        if (node.resource_consumption[bin_pos].empty())
            node.resource_consumption[bin_pos].assign(bin_type.number_of_resources(), 0.0);
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            node.resource_consumption[bin_pos][resource_id] += block.resource_consumption[resource_id];
        }
    }

    for (const auto& item_copy: block.item_copies)
        node.item_number_of_copies[item_copy.first] += item_copy.second;
    node.item_area += block.item_area;
    node.block_area += block.rect.area();
    node.weight[bin_pos] += block.weight;
    node.number_of_items += (ItemPos)block.items.size();
    node.number_of_blocks++;

    Node::PlacedBlock current_pb;
    current_pb.block_id = insertion.block_id;
    current_pb.bl_corner = insertion.bl_corner;
    node.placed_blocks[bin_pos].push_back(std::move(current_pb));

    // Item-copy usage is global (shared across bins), so this insertion can
    // rule out blocks in every bin's valid_block_ids, not just bin_pos's -
    // but weight/resource capacity are per-bin, so only bin_pos itself needs
    // those two re-checked.
    for (BinPos other_bin_pos = 0;
            other_bin_pos < (BinPos)node.valid_block_ids.size();
            ++other_bin_pos) {
        prune_valid_block_ids(node, other_bin_pos, other_bin_pos == bin_pos);
    }

    cut_spaces(node.empty_spaces[bin_pos], insertion.bl_corner, block.rect);
    remove_unusable_spaces(node, bin_pos);
}

void BranchingSchemeMaximalSpaces::prune_valid_block_ids(
        Node& node,
        BinPos bin_pos,
        bool check_weight_and_resources) const
{
    BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
    const std::vector<Block>& bin_blocks = blocks_[bin_type_id];
    Weight maximum_weight = instance_.bin_type(bin_type_id).maximum_weight;
    std::vector<ItemPos>& valid_block_ids = node.valid_block_ids[bin_pos];
    ItemPos block_idx = 0;
    while (block_idx < (ItemPos)valid_block_ids.size()) {
        const Block& candidate = bin_blocks[valid_block_ids[block_idx]];
        bool feasible = true;
        for (const auto& item_copy: candidate.item_copies) {
            if (node.item_number_of_copies[item_copy.first] + item_copy.second
                    > instance_.item_type(item_copy.first).copies) {
                feasible = false;
                break;
            }
        }
        if (feasible && check_weight_and_resources) {
            if (node.weight[bin_pos] + candidate.weight > maximum_weight)
                feasible = false;
            if (feasible && !block_resource_capacity_ok(node, candidate, bin_pos))
                feasible = false;
        }
        if (!feasible) {
            valid_block_ids[block_idx] = valid_block_ids.back();
            valid_block_ids.pop_back();
        } else {
            ++block_idx;
        }
    }
}

void BranchingSchemeMaximalSpaces::remove_unusable_spaces(
        Node& node,
        BinPos bin_pos) const
{
    BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
    const std::vector<Block>& bin_blocks = blocks_[bin_type_id];
    std::vector<EmptySpace>& spaces = node.empty_spaces[bin_pos];
    const std::vector<ItemPos>& valid_block_ids = node.valid_block_ids[bin_pos];
    ItemPos space_idx = 0;
    while (space_idx < (ItemPos)spaces.size()) {
        bool has_fitting_block = false;
        for (ItemPos block_id: valid_block_ids) {
            const Block& candidate = bin_blocks[block_id];
            if (candidate.rect.x <= spaces[space_idx].rect.x
                    && candidate.rect.y <= spaces[space_idx].rect.y) {
                has_fitting_block = true;
                break;
            }
        }
        if (!has_fitting_block) {
            spaces[space_idx] = spaces.back();
            spaces.pop_back();
        } else {
            ++space_idx;
        }
    }
}

bool BranchingSchemeMaximalSpaces::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::Knapsack: {
        return node_2->greedy_value < node_1->greedy_value;
    } case Objective::Feasibility: {
        return node_2->greedy_value < node_1->greedy_value;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "Branching scheme 'rectangle::BranchingSchemeMaximalSpaces' "
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingSchemeMaximalSpaces::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::Knapsack: {
        if (leaf(node_2))
            return true;
        return false;
    } case Objective::Feasibility: {
        if (leaf(node_2))
            return true;
        return false;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "Branching scheme 'rectangle::BranchingSchemeMaximalSpaces' "
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

Solution BranchingSchemeMaximalSpaces::to_solution(const std::shared_ptr<Node>& node_orig) const
{
    Node greedy_node = *node_orig;
    while (true) {
        Insertion insertion = best_insertion(greedy_node);
        if (insertion.block_id == -1)
            break;
        apply_insertion(greedy_node, insertion);
    }

    SolutionBuilder solution_builder(instance_);
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        BinPos solution_bin_pos = solution_builder.add_bin(bin_type_id, 1);
        for (const Node::PlacedBlock& pb: greedy_node.placed_blocks[bin_pos]) {
            const Block& block = blocks_[bin_type_id][pb.block_id];
            for (const SolutionItem& solution_item: block.items) {
                Point item_bl_corner;
                item_bl_corner.x = pb.bl_corner.x + solution_item.bl_corner.x;
                item_bl_corner.y = pb.bl_corner.y + solution_item.bl_corner.y;
                solution_builder.add_item(
                    solution_bin_pos,
                    solution_item.item_type_id,
                    item_bl_corner,
                    solution_item.rotate);
            }
        }
    }

    return solution_builder.build();
}

Profit BranchingSchemeMaximalSpaces::compute_guide_greedy(const Node& node) const
{
    Node greedy_node = node;
    while (true) {
        Insertion insertion = best_insertion(greedy_node);
        if (insertion.block_id == -1)
            break;
        apply_insertion(greedy_node, insertion);
    }
    return greedy_node.profit;
}

std::shared_ptr<BranchingSchemeMaximalSpaces::Node> BranchingSchemeMaximalSpaces::next_child(
        const std::shared_ptr<Node>& parent) const
{
    if (parent->next_child_pos == 0)
        parent->cached_insertions = insertions(parent);
    NodeId pos = parent->next_child_pos;
    parent->next_child_pos++;
    if (pos >= (NodeId)parent->cached_insertions.size())
        return nullptr;
    const Insertion& insertion = parent->cached_insertions[pos];

    std::vector<Insertion> saved_insertions;
    std::swap(parent->cached_insertions, saved_insertions);
    auto node = std::make_shared<Node>(*parent);
    std::swap(parent->cached_insertions, saved_insertions);

    node->cached_insertions.clear();
    node->next_child_pos = 0;
    apply_insertion(*node, insertion);
    node->greedy_value = compute_guide_greedy(*node);

    return node;
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////// Static helpers /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BranchingSchemeMaximalSpaces::overlaps(
        const EmptySpace& space,
        Point bl_corner,
        const packingsolver::rectangle::Rectangle& block_rect)
{
    return bl_corner.x < space.xe() && bl_corner.x + block_rect.x > space.xs()
        && bl_corner.y < space.ye() && bl_corner.y + block_rect.y > space.ys();
}

void BranchingSchemeMaximalSpaces::add_empty_space(
        std::vector<EmptySpace>& spaces,
        const EmptySpace& new_space)
{
    if (new_space.rect.x == 0 || new_space.rect.y == 0)
        return;
    for (const EmptySpace& existing: spaces)
        if (existing.contains(new_space))
            return;
    spaces.push_back(new_space);
}

void BranchingSchemeMaximalSpaces::cut_spaces(
        std::vector<EmptySpace>& spaces,
        Point bl_corner,
        const packingsolver::rectangle::Rectangle& block_rect)
{
    for (ItemPos i = 0; i < (ItemPos)spaces.size(); ) {
        if (!overlaps(spaces[i], bl_corner, block_rect)) {
            ++i;
            continue;
        }
        EmptySpace space = spaces[i];
        spaces[i] = spaces.back();
        spaces.pop_back();

        // Left (x-)
        if (bl_corner.x > space.xs()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.rect.x = bl_corner.x - space.bl_corner.x;
            sub.rect.y = space.rect.y;
            add_empty_space(spaces, sub);
        }
        // Right (x+)
        if (bl_corner.x + block_rect.x < space.xe()) {
            EmptySpace sub;
            sub.bl_corner.x = bl_corner.x + block_rect.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.rect.x = space.xe() - (bl_corner.x + block_rect.x);
            sub.rect.y = space.rect.y;
            add_empty_space(spaces, sub);
        }
        // Bottom (y-)
        if (bl_corner.y > space.ys()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.rect.x = space.rect.x;
            sub.rect.y = bl_corner.y - space.bl_corner.y;
            add_empty_space(spaces, sub);
        }
        // Top (y+)
        if (bl_corner.y + block_rect.y < space.ye()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = bl_corner.y + block_rect.y;
            sub.rect.x = space.rect.x;
            sub.rect.y = space.ye() - (bl_corner.y + block_rect.y);
            add_empty_space(spaces, sub);
        }
        // Do not increment i: the element swapped into position i must be checked.
    }
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////// tree_search_maximal_spaces ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

const packingsolver::rectangle::TreeSearchMaximalSpacesOutput packingsolver::rectangle::tree_search_maximal_spaces(
        const Instance& instance,
        const TreeSearchMaximalSpacesParameters& parameters)
{
    TreeSearchMaximalSpacesOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers) {
        SequentialFeasibilitySolver solver = [&parameters, &algorithm_formatter](
                const Instance& sub_instance)
        {
            TreeSearchMaximalSpacesParameters inner_parameters;
            inner_parameters.verbosity_level = 0;
            inner_parameters.timer = parameters.timer;
            inner_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            inner_parameters.optimization_mode = parameters.optimization_mode;
            inner_parameters.not_anytime_tree_search_queue_size
                = parameters.not_anytime_tree_search_queue_size;
            return tree_search_maximal_spaces(sub_instance, inner_parameters).solution_pool;
        };

        SequentialFeasibilityParameters sf_parameters;
        sf_parameters.verbosity_level = 0;
        sf_parameters.timer = parameters.timer;
        sf_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sf_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& sf_output)
        {
            algorithm_formatter.update_solution(
                    sf_output.solution_pool.best(),
                    sf_output.solution_pool.best_label());
        };

        sequential_feasibility(instance, solver, sf_parameters);
        algorithm_formatter.end();
        return output;
    }

    MaxReachableLengths max_reachable_lengths = compute_max_reachable_lengths(instance);
    std::vector<std::vector<Block>> all_blocks = compute_blocks(instance);

    std::vector<BranchingSchemeMaximalSpaces> branching_schemes;
    std::vector<treesearchsolver::IterativeBeamSearchParameters<BranchingSchemeMaximalSpaces>> ibs_parameters_list;
    std::vector<Output> local_outputs;
    {
        BranchingSchemeMaximalSpaces::Parameters branching_scheme_parameters;
        branching_schemes.push_back(BranchingSchemeMaximalSpaces(instance, all_blocks, max_reachable_lengths, branching_scheme_parameters));
        treesearchsolver::IterativeBeamSearchParameters<BranchingSchemeMaximalSpaces> ibs_parameters;
        ibs_parameters.verbosity_level = 0;
        ibs_parameters.timer = parameters.timer;
        ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        ibs_parameters.global_history = true;
        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            ibs_parameters.minimum_size_of_the_queue = 1;
            ibs_parameters.growth_factor = parameters.not_anytime_tree_search_queue_size;
            ibs_parameters.maximum_size_of_the_queue = parameters.not_anytime_tree_search_queue_size;
        }
        ibs_parameters_list.push_back(ibs_parameters);
        local_outputs.push_back(Output(instance));
    }

    std::vector<std::thread> threads;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Counter scheme_idx = 0; scheme_idx < (Counter)branching_schemes.size(); ++scheme_idx) {
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeDeterministic) {
            ibs_parameters_list[scheme_idx].new_solution_callback
                = [&algorithm_formatter, &branching_schemes, scheme_idx](
                        const treesearchsolver::Output<BranchingSchemeMaximalSpaces>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemeMaximalSpaces>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemeMaximalSpaces>&>(tss_output);
                    Solution solution = branching_schemes[scheme_idx].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "n " << tssibs_output.maximum_size_of_the_queue;
                    algorithm_formatter.update_solution(solution, ss.str());
                    if (tssibs_output.optimal) {
                        // 'Solution::operator<' always ranks a feasible
                        // solution above an infeasible one, regardless of
                        // objective, so 'solution_pool.best()' can only be
                        // infeasible here if no feasible solution was found
                        // anywhere in this now-exhaustive search: a genuine
                        // infeasibility proof, independent of objective.
                        if (!solution.feasible()) {
                            // For every non-Knapsack objective (Feasibility
                            // included), 'copies_min' defaults to 'copies',
                            // so this already subsumes "not everything got
                            // packed" - no separate 'full()' check needed.
                            algorithm_formatter.update_is_proven_infeasible();
                        } else if (solution.instance().objective() == packingsolver::Objective::Knapsack) {
                            algorithm_formatter.update_knapsack_bound(solution.profit());
                        }
                    }
                };
        } else {
            ibs_parameters_list[scheme_idx].new_solution_callback
                = [&local_outputs, &branching_schemes, scheme_idx](
                        const treesearchsolver::Output<BranchingSchemeMaximalSpaces>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemeMaximalSpaces>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemeMaximalSpaces>&>(tss_output);
                    Solution solution = branching_schemes[scheme_idx].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "n " << tssibs_output.maximum_size_of_the_queue;
                    local_outputs[(size_t)scheme_idx].solution_pool.add(solution, ss.str());

                    if (tssibs_output.optimal) {
                        // 'Solution::operator<' always ranks a feasible
                        // solution above an infeasible one, regardless of
                        // objective, so 'solution_pool.best()' can only be
                        // infeasible here if no feasible solution was found
                        // anywhere in this now-exhaustive search: a genuine
                        // infeasibility proof, independent of objective.
                        if (!solution.feasible()) {
                            // For every non-Knapsack objective (Feasibility
                            // included), 'copies_min' defaults to 'copies',
                            // so this already subsumes "not everything got
                            // packed" - no separate 'full()' check needed.
                            local_outputs[(size_t)scheme_idx].is_proven_infeasible = true;
                        } else if (solution.instance().objective() == packingsolver::Objective::Knapsack) {
                            local_outputs[(size_t)scheme_idx].knapsack_bound
                                = solution.profit();
                        }
                    }
                };
        }
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
            threads.push_back(std::thread(
                        wrapper<decltype(&treesearchsolver::iterative_beam_search<BranchingSchemeMaximalSpaces>), treesearchsolver::iterative_beam_search<BranchingSchemeMaximalSpaces>>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(branching_schemes[scheme_idx]),
                        ibs_parameters_list[scheme_idx]));
        } else {
            try {
                treesearchsolver::iterative_beam_search<BranchingSchemeMaximalSpaces>(
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
