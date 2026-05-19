#include "box/branching_scheme_maximal_spaces.hpp"

#include "packingsolver/box/solution.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Internals ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

/**
 * For each axis, compute the distance from the space to the nearest bin wall
 * (min of distance to the low wall and distance to the high wall), and record
 * which wall is closer (dir = true means the high wall is closer, so the block
 * should be placed against the high end of the space).  The total anchor
 * distance is the sum of the three per-axis minima; the anchor corner is the
 * corner of the space nearest to a bin corner.
 */
AnchorInfo compute_anchor_info(const EmptySpace& space, const Box& bin_box)
{
    Length dist_x_start = space.bl_corner.x;
    Length dist_x_end = bin_box.x - space.xe();
    Length dist_y_start = space.bl_corner.y;
    Length dist_y_end = bin_box.y - space.ye();
    Length dist_z_start = space.bl_corner.z;
    Length dist_z_end = bin_box.z - space.ze();
    AnchorInfo info;
    info.dir_x = dist_x_end < dist_x_start;
    info.dir_y = dist_y_end < dist_y_start;
    info.dir_z = dist_z_end < dist_z_start;
    info.distance = std::min(dist_x_start, dist_x_end)
        + std::min(dist_y_start, dist_y_end)
        + std::min(dist_z_start, dist_z_end);
    //info.dir_x = false;
    //info.dir_y = false;
    //info.dir_z = false;
    //info.distance = dist_x_start + dist_y_start + dist_z_start;
    info.space_volume = space.box.volume();
    return info;
}

Point compute_block_position(
        const AnchorInfo& anchor,
        const EmptySpace& space,
        const Box& block_box)
{
    Point pos;
    pos.x = anchor.dir_x? space.xe() - block_box.x: space.xs();
    pos.y = anchor.dir_y? space.ye() - block_box.y: space.ys();
    pos.z = anchor.dir_z? space.ze() - block_box.z: space.zs();
    return pos;
}

/**
 * Total face-contact area shared between block A (at bl_corner_1 with box_1)
 * and block B (at bl_corner_2 with box_2).  Returns 0 if the blocks do not
 * touch on any face.
 */
Volume compute_contact_area(
        Point bl_corner_1, const Box& box_1,
        Point bl_corner_2, const Box& box_2)
{
    Length xl1 = bl_corner_1.x, xe1 = bl_corner_1.x + box_1.x;
    Length yl1 = bl_corner_1.y, ye1 = bl_corner_1.y + box_1.y;
    Length zl1 = bl_corner_1.z, ze1 = bl_corner_1.z + box_1.z;
    Length xl2 = bl_corner_2.x, xe2 = bl_corner_2.x + box_2.x;
    Length yl2 = bl_corner_2.y, ye2 = bl_corner_2.y + box_2.y;
    Length zl2 = bl_corner_2.z, ze2 = bl_corner_2.z + box_2.z;
    Volume contact = 0;
    if (xe1 == xl2 || xe2 == xl1) {
        Length ov_y = std::max((Length)0, std::min(ye1, ye2) - std::max(yl1, yl2));
        Length ov_z = std::max((Length)0, std::min(ze1, ze2) - std::max(zl1, zl2));
        contact += ov_y * ov_z;
    }
    if (ye1 == yl2 || ye2 == yl1) {
        Length ov_x = std::max((Length)0, std::min(xe1, xe2) - std::max(xl1, xl2));
        Length ov_z = std::max((Length)0, std::min(ze1, ze2) - std::max(zl1, zl2));
        contact += ov_x * ov_z;
    }
    if (ze1 == zl2 || ze2 == zl1) {
        Length ov_x = std::max((Length)0, std::min(xe1, xe2) - std::max(xl1, xl2));
        Length ov_y = std::max((Length)0, std::min(ye1, ye2) - std::max(yl1, yl2));
        contact += ov_x * ov_y;
    }
    return contact;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// BranchingSchemeMaximalSpaces ////////////////////////////////
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
    max_reachable_z_ = max_reachable_lengths.z;
}

void BranchingSchemeMaximalSpaces::remove_spaces_and_update_waste(
        Node& node,
        const std::vector<ItemPos>& space_ids) const
{
    // Build the list of surviving space indices for the coverage computation.
    scratch_is_removed_.assign(node.empty_spaces.size(), false);
    for (ItemPos space_id: space_ids)
        scratch_is_removed_[space_id] = true;
    scratch_surviving_ids_.clear();
    for (ItemPos other_id = 0;
            other_id < (ItemPos)node.empty_spaces.size();
            ++other_id) {
        if (!scratch_is_removed_[other_id])
            scratch_surviving_ids_.push_back(other_id);
    }

    // For each removed space R, compute vol(R \ union of surviving spaces)
    // exactly using a stack-based region-subtraction:
    //   - Each stack item is (remaining_box, next_surviving_idx).
    //   - When remaining_box intersects the next surviving space, split it
    //     into the (up to 6) disjoint sub-boxes that cover remaining_box minus
    //     the intersection, and push each with next_surviving_idx + 1.
    //   - When all surviving spaces have been consumed, the box is fully
    //     uncovered: add its volume to waste.
    // This avoids the overcounting that arises from simply summing pairwise
    // intersection volumes when multiple surviving spaces overlap each other
    // within R.
    for (ItemPos remove_id: space_ids) {
        scratch_remove_stack_.clear();
        scratch_remove_stack_.push_back({node.empty_spaces[remove_id], 0});
        while (!scratch_remove_stack_.empty()) {
            RemoveWasteStackItem item = scratch_remove_stack_.back();
            scratch_remove_stack_.pop_back();
            const EmptySpace& rem = item.remaining;
            if (item.next_surviving_idx >= (ItemPos)scratch_surviving_ids_.size()) {
                node.waste += rem.box.volume();
                continue;
            }
            const EmptySpace& other =
                node.empty_spaces[scratch_surviving_ids_[item.next_surviving_idx]];
            ItemPos next = item.next_surviving_idx + 1;
            Length ix_start = std::max(rem.xs(), other.xs());
            Length ix_end   = std::min(rem.xe(), other.xe());
            Length iy_start = std::max(rem.ys(), other.ys());
            Length iy_end   = std::min(rem.ye(), other.ye());
            Length iz_start = std::max(rem.zs(), other.zs());
            Length iz_end   = std::min(rem.ze(), other.ze());
            if (ix_end <= ix_start || iy_end <= iy_start || iz_end <= iz_start) {
                // No intersection: pass unchanged to the next surviving space.
                scratch_remove_stack_.push_back({rem, next});
                continue;
            }
            // Split rem into up to 6 sub-boxes covering rem \ intersection.
            // Each sub-box is disjoint and the union equals rem \ intersection.
            if (ix_start > rem.xs()) {
                EmptySpace sub;
                sub.bl_corner = rem.bl_corner;
                sub.box = {ix_start - rem.xs(), rem.box.y, rem.box.z};
                scratch_remove_stack_.push_back({sub, next});
            }
            if (ix_end < rem.xe()) {
                EmptySpace sub;
                sub.bl_corner = {ix_end, rem.bl_corner.y, rem.bl_corner.z};
                sub.box = {rem.xe() - ix_end, rem.box.y, rem.box.z};
                scratch_remove_stack_.push_back({sub, next});
            }
            if (iy_start > rem.ys()) {
                EmptySpace sub;
                sub.bl_corner = {ix_start, rem.bl_corner.y, rem.bl_corner.z};
                sub.box = {ix_end - ix_start, iy_start - rem.ys(), rem.box.z};
                scratch_remove_stack_.push_back({sub, next});
            }
            if (iy_end < rem.ye()) {
                EmptySpace sub;
                sub.bl_corner = {ix_start, iy_end, rem.bl_corner.z};
                sub.box = {ix_end - ix_start, rem.ye() - iy_end, rem.box.z};
                scratch_remove_stack_.push_back({sub, next});
            }
            if (iz_start > rem.zs()) {
                EmptySpace sub;
                sub.bl_corner = {ix_start, iy_start, rem.bl_corner.z};
                sub.box = {ix_end - ix_start, iy_end - iy_start, iz_start - rem.zs()};
                scratch_remove_stack_.push_back({sub, next});
            }
            if (iz_end < rem.ze()) {
                EmptySpace sub;
                sub.bl_corner = {ix_start, iy_start, iz_end};
                sub.box = {ix_end - ix_start, iy_end - iy_start, rem.ze() - iz_end};
                scratch_remove_stack_.push_back({sub, next});
            }
        }
    }

    // Erase all listed spaces in descending index order so that earlier
    // indices remain valid after each erase and sorted order is preserved.
    scratch_sorted_ids_.assign(space_ids.begin(), space_ids.end());
    std::sort(scratch_sorted_ids_.begin(), scratch_sorted_ids_.end(),
            [](ItemPos id_1, ItemPos id_2) { return id_1 > id_2; });
    for (ItemPos remove_id: scratch_sorted_ids_)
        node.empty_spaces.erase(node.empty_spaces.begin() + remove_id);
}

const std::shared_ptr<BranchingSchemeMaximalSpaces::Node> BranchingSchemeMaximalSpaces::root() const
{
    auto node = std::make_shared<Node>();
    node->id = node_id_++;
    node->item_number_of_copies.assign(instance_.number_of_item_types(), 0);
    node->number_of_bins = 0;
    return node;
}

BranchingSchemeMaximalSpaces::BestSpaceResult BranchingSchemeMaximalSpaces::find_best_space(
        const Node& parent,
        BinTypeId bin_type_id) const
{
    const BinType& bin_type = instance_.bin_type(bin_type_id);
    const std::vector<Block>& bin_blocks = blocks_[bin_type_id];
    BestSpaceResult result;
    Length best_distance = std::numeric_limits<Length>::max();
    Volume best_volume = 0;
    int best_corner = std::numeric_limits<int>::min();
    for (ItemPos space_idx = 0;
            space_idx < (ItemPos)parent.empty_spaces.size();
            ++space_idx) {
        const EmptySpace& space = parent.empty_spaces[space_idx];
        bool has_fitting_block = false;
        for (ItemPos block_id: parent.valid_block_ids) {
            const Block& block = bin_blocks[block_id];
            if (block.box.x <= space.box.x
                    && block.box.y <= space.box.y
                    && block.box.z <= space.box.z) {
                has_fitting_block = true;
                break;
            }
        }
        if (!has_fitting_block)
            continue;
        AnchorInfo anchor = compute_anchor_info(space, bin_type.box);
        Volume space_volume = space.box.volume();
        int corner = (anchor.dir_x ? 4 : 0) | (anchor.dir_y ? 2 : 0) | (anchor.dir_z ? 1 : 0);
        bool is_better = (anchor.distance < best_distance)
            || (anchor.distance == best_distance && space_volume > best_volume)
            || (anchor.distance == best_distance && space_volume == best_volume && corner > best_corner);
        if (is_better) {
            result.space_idx = space_idx;
            result.anchor = anchor;
            best_distance = anchor.distance;
            best_volume = space_volume;
            best_corner = corner;
        }
    }
    return result;
}

double BranchingSchemeMaximalSpaces::compute_volume_loss_factor(
        const SpaceContactInfo& info,
        const Block& block) const
{
    Length rx = info.space_bx - block.box.x;
    Length ry = info.space_by - block.box.y;
    Length rz = info.space_bz - block.box.z;
    Volume usable_volume = (Volume)(block.box.x + max_reachable_x_[rx])
        * (block.box.y + max_reachable_y_[ry])
        * (block.box.z + max_reachable_z_[rz]);
    Volume space_volume = (Volume)info.space_bx * info.space_by * info.space_bz;
    return (double)usable_volume / space_volume;
}

double BranchingSchemeMaximalSpaces::compute_insertion_guide(
        const Node& parent,
        const Insertion& insertion,
        const SpaceContactInfo& info) const
{
    BinPos bin_pos = insertion.new_bin?
        parent.number_of_bins:
        parent.number_of_bins - 1;
    BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
    const Block& block = blocks_[bin_type_id][insertion.block_id];

    double fill_rate = instance_.bin_volume() > 0?
        (double)parent.item_volume / instance_.bin_volume(): 0.0;
    double v = (double)block.item_volume;
    double l = compute_volume_loss_factor(info, block);
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
    Length max_z = (Length)max_reachable_z_.size() - 1;

    scratch_reachable_x_.assign(max_x + 1, 0);
    scratch_reachable_y_.assign(max_y + 1, 0);
    scratch_reachable_z_.assign(max_z + 1, 0);
    scratch_reachable_x_[0] = scratch_reachable_y_[0] = scratch_reachable_z_[0] = 1;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        ItemPos remaining_copies = item_type.copies - node.item_number_of_copies[item_type_id];
        for (ItemPos copy_idx = 0; copy_idx < remaining_copies; ++copy_idx) {
            scratch_temp_reachable_x_ = scratch_reachable_x_;
            scratch_temp_reachable_y_ = scratch_reachable_y_;
            scratch_temp_reachable_z_ = scratch_reachable_z_;
            for (Rotation rotation: item_type.rotations) {
                Box box = item_type.box.rotate(rotation);
                if (box.x <= max_x)
                    for (Length v = max_x; v >= box.x; --v)
                        if (scratch_reachable_x_[v - box.x]) scratch_temp_reachable_x_[v] = 1;
                if (box.y <= max_y)
                    for (Length v = max_y; v >= box.y; --v)
                        if (scratch_reachable_y_[v - box.y]) scratch_temp_reachable_y_[v] = 1;
                if (box.z <= max_z)
                    for (Length v = max_z; v >= box.z; --v)
                        if (scratch_reachable_z_[v - box.z]) scratch_temp_reachable_z_[v] = 1;
            }
            scratch_reachable_x_ = scratch_temp_reachable_x_;
            scratch_reachable_y_ = scratch_temp_reachable_y_;
            scratch_reachable_z_ = scratch_temp_reachable_z_;
        }
    }

    for (Length v = 1; v <= max_x; ++v)
        max_reachable_x_[v] = scratch_reachable_x_[v] ? v : max_reachable_x_[v - 1];
    for (Length v = 1; v <= max_y; ++v)
        max_reachable_y_[v] = scratch_reachable_y_[v] ? v : max_reachable_y_[v - 1];
    for (Length v = 1; v <= max_z; ++v)
        max_reachable_z_[v] = scratch_reachable_z_[v] ? v : max_reachable_z_[v - 1];
}

double BranchingSchemeMaximalSpaces::active_delta(const Node& node) const
{
    double fill_rate = instance_.bin_volume() > 0?
        (double)node.item_volume / instance_.bin_volume(): 0.0;
    if (fill_rate < parameters_.configuration_switch_threshold) {
        return parameters_.delta;
    } else {
        return parameters_.delta_2;
    }
}

const std::vector<BranchingSchemeMaximalSpaces::Insertion>& BranchingSchemeMaximalSpaces::insertions(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "insertions " << parent->id
    //    << " # blocks " << parent->number_of_blocks
    //    << " # items " << parent->number_of_items
    //    << " item_volume " << parent->item_volume
    //    << " profit " << parent->profit
    //    << " guide_volume " << parent->greedy_value
    //    << std::endl;
    update_node_max_reachable(*parent);
    insertions_.clear();

    double delta = active_delta(*parent);

    if (!parent->empty_spaces.empty()) {
        BinPos bin_pos = parent->number_of_bins - 1;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);

        BestSpaceResult best = find_best_space(*parent, bin_type_id);

        if (best.space_idx != -1) {
            const EmptySpace& space = parent->empty_spaces[best.space_idx];
            SpaceContactInfo contact_info = compute_space_contact_info(
                    parent->placed_blocks.back(), bin_type_id, space, delta);
            for (ItemPos block_id: parent->valid_block_ids) {
                const Block& block = blocks_[bin_type_id][block_id];
                if (block.box.x > space.box.x
                        || block.box.y > space.box.y
                        || block.box.z > space.box.z)
                    continue;
                Insertion insertion;
                insertion.new_bin = false;
                insertion.space_id = best.space_idx;
                insertion.block_id = block_id;
                insertion.bl_corner = compute_block_position(best.anchor, space, block.box);
                insertion.guide = compute_insertion_guide(*parent, insertion, contact_info);
                insertions_.push_back(insertion);
            }
        }
    }

    if (insertions_.empty()
            && parent->number_of_bins < instance_.number_of_bins()) {
        BinPos bin_pos = parent->number_of_bins;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        EmptySpace space;
        space.bl_corner = {0, 0, 0};
        space.box = bin_type.box;
        AnchorInfo anchor = compute_anchor_info(space, bin_type.box);
        SpaceContactInfo contact_info = compute_space_contact_info(
                {}, bin_type_id, space, delta);
        for (ItemPos block_id = 0;
                block_id < (ItemPos)blocks_[bin_type_id].size();
                ++block_id) {
            const Block& block = blocks_[bin_type_id][block_id];
            if (block.box.x > bin_type.box.x
                    || block.box.y > bin_type.box.y
                    || block.box.z > bin_type.box.z)
                continue;
            bool feasible = true;
            for (const auto& item_copy: block.item_copies) {
                if (parent->item_number_of_copies[item_copy.first] + item_copy.second
                        > instance_.item_type(item_copy.first).copies) {
                    feasible = false;
                    break;
                }
            }
            if (!feasible)
                continue;
            Insertion insertion;
            insertion.new_bin = true;
            insertion.space_id = 0;
            insertion.block_id = block_id;
            insertion.bl_corner = compute_block_position(anchor, space, block.box);
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
    update_node_max_reachable(parent);
    Insertion best;
    double best_score = 0;

    double delta = active_delta(parent);

    if (!parent.empty_spaces.empty()) {
        BinPos bin_pos = parent.number_of_bins - 1;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);

        BestSpaceResult best_space = find_best_space(parent, bin_type_id);

        if (best_space.space_idx != -1) {
            const EmptySpace& space = parent.empty_spaces[best_space.space_idx];
            SpaceContactInfo contact_info = compute_space_contact_info(
                    parent.placed_blocks.back(), bin_type_id, space, delta);
            for (ItemPos block_id: parent.valid_block_ids) {
                const Block& block = blocks_[bin_type_id][block_id];
                if (block.box.x > space.box.x
                        || block.box.y > space.box.y
                        || block.box.z > space.box.z)
                    continue;
                Insertion insertion;
                insertion.new_bin = false;
                insertion.space_id = best_space.space_idx;
                insertion.block_id = block_id;
                insertion.bl_corner = compute_block_position(best_space.anchor, space, block.box);
                double score = compute_insertion_guide(parent, insertion, contact_info);
                if (score > best_score) {
                    best_score = score;
                    best = insertion;
                }
            }
        }
    }

    if (best.block_id == -1
            && parent.number_of_bins < instance_.number_of_bins()) {
        BinPos bin_pos = parent.number_of_bins;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        EmptySpace space;
        space.bl_corner = {0, 0, 0};
        space.box = bin_type.box;
        AnchorInfo anchor = compute_anchor_info(space, bin_type.box);
        SpaceContactInfo contact_info = compute_space_contact_info(
                {}, bin_type_id, space, delta);
        for (ItemPos block_id = 0;
                block_id < (ItemPos)blocks_[bin_type_id].size();
                ++block_id) {
            const Block& block = blocks_[bin_type_id][block_id];
            if (block.box.x > bin_type.box.x
                    || block.box.y > bin_type.box.y
                    || block.box.z > bin_type.box.z)
                continue;
            bool feasible = true;
            for (const auto& item_copy: block.item_copies) {
                if (parent.item_number_of_copies[item_copy.first] + item_copy.second
                        > instance_.item_type(item_copy.first).copies) {
                    feasible = false;
                    break;
                }
            }
            if (!feasible)
                continue;
            Insertion insertion;
            insertion.new_bin = true;
            insertion.space_id = 0;
            insertion.block_id = block_id;
            insertion.bl_corner = compute_block_position(anchor, space, block.box);
            double score = compute_insertion_guide(parent, insertion, contact_info);
            if (score > best_score) {
                best_score = score;
                best = insertion;
            }
        }
    }

    return best;
}

BranchingSchemeMaximalSpaces::SpaceContactInfo
BranchingSchemeMaximalSpaces::compute_space_contact_info(
        const std::vector<Node::PlacedBlock>& placed_blocks,
        BinTypeId bin_type_id,
        const EmptySpace& space,
        double delta) const
{
    const BinType& bin_type = instance_.bin_type(bin_type_id);
    SpaceContactInfo info;
    info.space_xs = space.bl_corner.x;
    info.space_ys = space.bl_corner.y;
    info.space_zs = space.bl_corner.z;
    info.space_bx = space.box.x;
    info.space_by = space.box.y;
    info.space_bz = space.box.z;
    info.tol_x = (Length)(delta * space.box.x);
    info.tol_y = (Length)(delta * space.box.y);
    info.tol_z = (Length)(delta * space.box.z);

    Length xl = space.bl_corner.x, xh = xl + space.box.x;
    Length yl = space.bl_corner.y, yh = yl + space.box.y;
    Length zl = space.bl_corner.z, zh = zl + space.box.z;
    info.xl_wall = (xl <= info.tol_x);
    info.yl_wall = (yl <= info.tol_y);
    info.zl_wall = (zl <= info.tol_z);
    info.xh_wall = (bin_type.box.x - xh <= info.tol_x);
    info.yh_wall = (bin_type.box.y - yh <= info.tol_y);
    info.zh_wall = (bin_type.box.z - zh <= info.tol_z);

    for (const Node::PlacedBlock& pb: placed_blocks) {
        const Block& pb_block = blocks_[bin_type_id][pb.block_id];
        Length pb_xl = pb.bl_corner.x, pb_xh = pb_xl + pb_block.box.x;
        Length pb_yl = pb.bl_corner.y, pb_yh = pb_yl + pb_block.box.y;
        Length pb_zl = pb.bl_corner.z, pb_zh = pb_zl + pb_block.box.z;

        if (std::abs(pb_xh - xl) <= info.tol_x) {
            Length lo1 = std::max((Length)0, pb_yl - yl);
            Length hi1 = std::min(space.box.y, pb_yh - yl);
            Length lo2 = std::max((Length)0, pb_zl - zl);
            Length hi2 = std::min(space.box.z, pb_zh - zl);
            if (lo1 < hi1 && lo2 < hi2)
                info.xl_neighbors.push_back({lo1, hi1, lo2, hi2, pb_xh});
        }
        if (std::abs(pb_xl - xh) <= info.tol_x) {
            Length lo1 = std::max((Length)0, pb_yl - yl);
            Length hi1 = std::min(space.box.y, pb_yh - yl);
            Length lo2 = std::max((Length)0, pb_zl - zl);
            Length hi2 = std::min(space.box.z, pb_zh - zl);
            if (lo1 < hi1 && lo2 < hi2)
                info.xh_neighbors.push_back({lo1, hi1, lo2, hi2, pb_xl});
        }
        if (std::abs(pb_yh - yl) <= info.tol_y) {
            Length lo1 = std::max((Length)0, pb_xl - xl);
            Length hi1 = std::min(space.box.x, pb_xh - xl);
            Length lo2 = std::max((Length)0, pb_zl - zl);
            Length hi2 = std::min(space.box.z, pb_zh - zl);
            if (lo1 < hi1 && lo2 < hi2)
                info.yl_neighbors.push_back({lo1, hi1, lo2, hi2, pb_yh});
        }
        if (std::abs(pb_yl - yh) <= info.tol_y) {
            Length lo1 = std::max((Length)0, pb_xl - xl);
            Length hi1 = std::min(space.box.x, pb_xh - xl);
            Length lo2 = std::max((Length)0, pb_zl - zl);
            Length hi2 = std::min(space.box.z, pb_zh - zl);
            if (lo1 < hi1 && lo2 < hi2)
                info.yh_neighbors.push_back({lo1, hi1, lo2, hi2, pb_yl});
        }
        if (std::abs(pb_zh - zl) <= info.tol_z) {
            Length lo1 = std::max((Length)0, pb_xl - xl);
            Length hi1 = std::min(space.box.x, pb_xh - xl);
            Length lo2 = std::max((Length)0, pb_yl - yl);
            Length hi2 = std::min(space.box.y, pb_yh - yl);
            if (lo1 < hi1 && lo2 < hi2)
                info.zl_neighbors.push_back({lo1, hi1, lo2, hi2, pb_zh});
        }
        if (std::abs(pb_zl - zh) <= info.tol_z) {
            Length lo1 = std::max((Length)0, pb_xl - xl);
            Length hi1 = std::min(space.box.x, pb_xh - xl);
            Length lo2 = std::max((Length)0, pb_yl - yl);
            Length hi2 = std::min(space.box.y, pb_yh - yl);
            if (lo1 < hi1 && lo2 < hi2)
                info.zh_neighbors.push_back({lo1, hi1, lo2, hi2, pb_zl});
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
    // Block position relative to the space origin.
    Length rel_x = bl_corner.x - info.space_xs;
    Length rel_y = bl_corner.y - info.space_ys;
    Length rel_z = bl_corner.z - info.space_zs;
    // Second-filter tolerances based on block dimensions.
    Length btol_x = (Length)(delta * block.box.x);
    Length btol_y = (Length)(delta * block.box.y);
    Length btol_z = (Length)(delta * block.box.z);
    // Absolute face positions of the block.
    Length block_xl = bl_corner.x;
    Length block_xh = bl_corner.x + block.box.x;
    Length block_yl = bl_corner.y;
    Length block_yh = bl_corner.y + block.box.y;
    Length block_zl = bl_corner.z;
    Length block_zh = bl_corner.z + block.box.z;

    Volume contact = 0;

    // XL face (block's -x face touching space's -x side)
    if (info.xl_wall && std::abs(block_xl - info.space_xs) <= btol_x)
        contact += block.box.y * block.box.z;
    for (const SpaceContactInfo::Neighbor& nb: info.xl_neighbors) {
        if (std::abs(block_xl - nb.orthogonal_pos) > btol_x)
            continue;
        Length ov1 = std::max((Length)0, std::min(rel_y + block.box.y, nb.hi1) - std::max(rel_y, nb.lo1));
        Length ov2 = std::max((Length)0, std::min(rel_z + block.box.z, nb.hi2) - std::max(rel_z, nb.lo2));
        contact += ov1 * ov2;
    }
    // XH face (block's +x face touching space's +x side)
    if (info.xh_wall && std::abs(block_xh - (info.space_xs + info.space_bx)) <= btol_x)
        contact += block.box.y * block.box.z;
    for (const SpaceContactInfo::Neighbor& nb: info.xh_neighbors) {
        if (std::abs(block_xh - nb.orthogonal_pos) > btol_x)
            continue;
        Length ov1 = std::max((Length)0, std::min(rel_y + block.box.y, nb.hi1) - std::max(rel_y, nb.lo1));
        Length ov2 = std::max((Length)0, std::min(rel_z + block.box.z, nb.hi2) - std::max(rel_z, nb.lo2));
        contact += ov1 * ov2;
    }
    // YL face
    if (info.yl_wall && std::abs(block_yl - info.space_ys) <= btol_y)
        contact += block.box.x * block.box.z;
    for (const SpaceContactInfo::Neighbor& nb: info.yl_neighbors) {
        if (std::abs(block_yl - nb.orthogonal_pos) > btol_y)
            continue;
        Length ov1 = std::max((Length)0, std::min(rel_x + block.box.x, nb.hi1) - std::max(rel_x, nb.lo1));
        Length ov2 = std::max((Length)0, std::min(rel_z + block.box.z, nb.hi2) - std::max(rel_z, nb.lo2));
        contact += ov1 * ov2;
    }
    // YH face
    if (info.yh_wall && std::abs(block_yh - (info.space_ys + info.space_by)) <= btol_y)
        contact += block.box.x * block.box.z;
    for (const SpaceContactInfo::Neighbor& nb: info.yh_neighbors) {
        if (std::abs(block_yh - nb.orthogonal_pos) > btol_y)
            continue;
        Length ov1 = std::max((Length)0, std::min(rel_x + block.box.x, nb.hi1) - std::max(rel_x, nb.lo1));
        Length ov2 = std::max((Length)0, std::min(rel_z + block.box.z, nb.hi2) - std::max(rel_z, nb.lo2));
        contact += ov1 * ov2;
    }
    // ZL face
    if (info.zl_wall && std::abs(block_zl - info.space_zs) <= btol_z)
        contact += block.box.x * block.box.y;
    for (const SpaceContactInfo::Neighbor& nb: info.zl_neighbors) {
        if (std::abs(block_zl - nb.orthogonal_pos) > btol_z)
            continue;
        Length ov1 = std::max((Length)0, std::min(rel_x + block.box.x, nb.hi1) - std::max(rel_x, nb.lo1));
        Length ov2 = std::max((Length)0, std::min(rel_y + block.box.y, nb.hi2) - std::max(rel_y, nb.lo2));
        contact += ov1 * ov2;
    }
    // ZH face
    if (info.zh_wall && std::abs(block_zh - (info.space_zs + info.space_bz)) <= btol_z)
        contact += block.box.x * block.box.y;
    for (const SpaceContactInfo::Neighbor& nb: info.zh_neighbors) {
        if (std::abs(block_zh - nb.orthogonal_pos) > btol_z)
            continue;
        Length ov1 = std::max((Length)0, std::min(rel_x + block.box.x, nb.hi1) - std::max(rel_x, nb.lo1));
        Length ov2 = std::max((Length)0, std::min(rel_y + block.box.y, nb.hi2) - std::max(rel_y, nb.lo2));
        contact += ov1 * ov2;
    }
    double block_surface_area = 2.0 * (
        (double)block.box.x * block.box.y
        + (double)block.box.x * block.box.z
        + (double)block.box.y * block.box.z);
    return (double)contact / block_surface_area;
}

void BranchingSchemeMaximalSpaces::apply_insertion(
        Node& node,
        const Insertion& insertion) const
{
    node.id = node_id_++;
    node.new_bin = insertion.new_bin;

    if (insertion.new_bin) {
        node.number_of_bins++;
        node.empty_spaces.clear();
        BinTypeId new_bin_type_id = instance_.bin_type_id(node.number_of_bins - 1);
        EmptySpace space;
        space.bl_corner.x = 0;
        space.bl_corner.y = 0;
        space.bl_corner.z = 0;
        space.box = instance().bin_type(new_bin_type_id).box;
        node.empty_spaces.push_back(space);
        node.block_volume = instance().previous_bin_volume(node.number_of_bins - 1);
        node.placed_blocks.push_back({});
        // Initialize valid_block_ids to all block indices for this bin type.
        // Blocks are generated to fit the bin by construction; quantity and
        // post-placement size filtering happens at the end.
        ItemPos number_of_blocks = (ItemPos)blocks_[new_bin_type_id].size();
        node.valid_block_ids.resize(number_of_blocks);
        std::iota(node.valid_block_ids.begin(), node.valid_block_ids.end(), (ItemPos)0);
    }

    BinPos bin_pos = node.number_of_bins - 1;
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const Block& block = blocks_[bin_type_id][insertion.block_id];
    for (const auto& item_copy: block.item_copies) {
        node.item_number_of_copies[item_copy.first] += item_copy.second;
        node.profit += instance_.item_type(item_copy.first).profit * item_copy.second;
    }
    node.item_volume += block.item_volume;
    node.block_volume += block.box.volume();
    node.waste += block.box.volume() - block.item_volume;
    node.number_of_items += (ItemPos)block.items.size();
    node.number_of_blocks++;

    Node::PlacedBlock current_pb;
    current_pb.block_id = insertion.block_id;
    current_pb.bl_corner = insertion.bl_corner;
    node.placed_blocks.back().push_back(std::move(current_pb));

    // Filter valid_block_ids: remove blocks that no longer have sufficient
    // item copies available after this placement.
    {
        const std::vector<Block>& bin_blocks = blocks_[bin_type_id];
        ItemPos block_idx = 0;
        while (block_idx < (ItemPos)node.valid_block_ids.size()) {
            const Block& candidate = bin_blocks[node.valid_block_ids[block_idx]];
            bool feasible = true;
            for (const auto& item_copy: candidate.item_copies) {
                if (node.item_number_of_copies[item_copy.first] + item_copy.second
                        > instance_.item_type(item_copy.first).copies) {
                    feasible = false;
                    break;
                }
            }
            if (!feasible) {
                node.valid_block_ids[block_idx] = node.valid_block_ids.back();
                node.valid_block_ids.pop_back();
            } else {
                ++block_idx;
            }
        }
    }

    cut_spaces(node.empty_spaces, insertion.bl_corner, block.box);
    // Remove spaces that have no fitting block in the updated valid_block_ids.
    {
        const std::vector<Block>& bin_blocks = blocks_[bin_type_id];
        ItemPos space_idx = 0;
        while (space_idx < (ItemPos)node.empty_spaces.size()) {
            bool has_fitting_block = false;
            for (ItemPos block_id: node.valid_block_ids) {
                const Block& candidate = bin_blocks[block_id];
                if (candidate.box.x <= node.empty_spaces[space_idx].box.x
                        && candidate.box.y <= node.empty_spaces[space_idx].box.y
                        && candidate.box.z <= node.empty_spaces[space_idx].box.z) {
                    has_fitting_block = true;
                    break;
                }
            }
            if (!has_fitting_block) {
                node.empty_spaces[space_idx] = node.empty_spaces.back();
                node.empty_spaces.pop_back();
            } else {
                ++space_idx;
            }
        }
    }
}

bool BranchingSchemeMaximalSpaces::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->number_of_bins > node_1->number_of_bins;
    } case Objective::Knapsack: {
        return node_2->greedy_value < node_1->greedy_value;
    } case Objective::Feasibility: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "Branching scheme 'box::BranchingScheme' "
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
    return false;
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

    Solution solution(instance_);
    if (greedy_node.number_of_bins == 0)
        return solution;

    for (BinPos bin_idx = 0;
            bin_idx < (BinPos)greedy_node.placed_blocks.size();
            ++bin_idx) {
        BinTypeId bin_type_id = instance_.bin_type_id(bin_idx);
        BinPos bin_pos = solution.add_bin(bin_type_id, 1);
        for (const Node::PlacedBlock& pb: greedy_node.placed_blocks[bin_idx]) {
            const Block& block = blocks_[bin_type_id][pb.block_id];
            for (const SolutionItem& solution_item: block.items) {
                Point item_bl_corner;
                item_bl_corner.x = pb.bl_corner.x + solution_item.bl_corner.x;
                item_bl_corner.y = pb.bl_corner.y + solution_item.bl_corner.y;
                item_bl_corner.z = pb.bl_corner.z + solution_item.bl_corner.z;
                solution.add_item(
                    bin_pos,
                    solution_item.item_type_id,
                    item_bl_corner,
                    solution_item.rotation);
            }
        }
    }

    return solution;
}

Volume BranchingSchemeMaximalSpaces::compute_guide_greedy(const Node& node) const
{
    Node greedy_node = node;
    while (true) {
        Insertion insertion = best_insertion(greedy_node);
        if (insertion.block_id == -1)
            break;
        apply_insertion(greedy_node, insertion);
    }
    //std::cout << greedy_node.item_volume << std::endl;
    return greedy_node.item_volume;
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
        const Box& block_box)
{
    return bl_corner.x < space.xe() && bl_corner.x + block_box.x > space.xs()
        && bl_corner.y < space.ye() && bl_corner.y + block_box.y > space.ys()
        && bl_corner.z < space.ze() && bl_corner.z + block_box.z > space.zs();
}

void BranchingSchemeMaximalSpaces::add_empty_space(
        std::vector<EmptySpace>& spaces,
        const EmptySpace& new_space)
{
    if (new_space.box.x == 0 || new_space.box.y == 0 || new_space.box.z == 0)
        return;
    for (const EmptySpace& existing: spaces)
        if (existing.contains(new_space))
            return;
    spaces.push_back(new_space);
}

void BranchingSchemeMaximalSpaces::cut_spaces(
        std::vector<EmptySpace>& spaces,
        Point bl_corner,
        const Box& block_box)
{
    for (ItemPos i = 0; i < (ItemPos)spaces.size(); ) {
        if (!overlaps(spaces[i], bl_corner, block_box)) {
            ++i;
            continue;
        }
        // Copy the overlapping space, then remove it with O(1) swap.
        EmptySpace space = spaces[i];
        spaces[i] = spaces.back();
        spaces.pop_back();

        // Generate up to 6 maximal sub-spaces (one per face of the block).
        // Each sub-space spans the full extent of the parent space in the two
        // dimensions perpendicular to the cut direction.

        // Back (x-)
        if (bl_corner.x > space.xs()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bl_corner.z = space.bl_corner.z;
            sub.box.x = bl_corner.x - space.bl_corner.x;
            sub.box.y = space.box.y;
            sub.box.z = space.box.z;
            add_empty_space(spaces, sub);
        }
        // Front (x+)
        if (bl_corner.x + block_box.x < space.xe()) {
            EmptySpace sub;
            sub.bl_corner.x = bl_corner.x + block_box.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bl_corner.z = space.bl_corner.z;
            sub.box.x = space.xe() - (bl_corner.x + block_box.x);
            sub.box.y = space.box.y;
            sub.box.z = space.box.z;
            add_empty_space(spaces, sub);
        }
        // Right (y-)
        if (bl_corner.y > space.ys()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bl_corner.z = space.bl_corner.z;
            sub.box.x = space.box.x;
            sub.box.y = bl_corner.y - space.bl_corner.y;
            sub.box.z = space.box.z;
            add_empty_space(spaces, sub);
        }
        // Left (y+)
        if (bl_corner.y + block_box.y < space.ye()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = bl_corner.y + block_box.y;
            sub.bl_corner.z = space.bl_corner.z;
            sub.box.x = space.box.x;
            sub.box.y = space.ye() - (bl_corner.y + block_box.y);
            sub.box.z = space.box.z;
            add_empty_space(spaces, sub);
        }
        // Bottom (z-)
        if (bl_corner.z > space.zs()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bl_corner.z = space.bl_corner.z;
            sub.box.x = space.box.x;
            sub.box.y = space.box.y;
            sub.box.z = bl_corner.z - space.bl_corner.z;
            add_empty_space(spaces, sub);
        }
        // Top (z+)
        if (bl_corner.z + block_box.z < space.ze()) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bl_corner.z = bl_corner.z + block_box.z;
            sub.box.x = space.box.x;
            sub.box.y = space.box.y;
            sub.box.z = space.ze() - (bl_corner.z + block_box.z);
            add_empty_space(spaces, sub);
        }
        // Do not increment i: the element swapped into position i must be checked.
    }
}
