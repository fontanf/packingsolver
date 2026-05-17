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
    //info.dir_x = dist_x_end < dist_x_start;
    //info.dir_y = dist_y_end < dist_y_start;
    //info.dir_z = dist_z_end < dist_z_start;
    //info.distance = std::min(dist_x_start, dist_x_end)
    //    + std::min(dist_y_start, dist_y_end)
    //    + std::min(dist_z_start, dist_z_end);
    info.dir_x = false;
    info.dir_y = false;
    info.dir_z = false;
    info.distance = dist_x_start + dist_y_start + dist_z_start;
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
    compute_pareto_fronts();
}

void BranchingSchemeMaximalSpaces::compute_pareto_fronts()
{
    pareto_front_block_ids_.resize(instance_.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const std::vector<Block>& blocks = blocks_[bin_type_id];
        std::vector<ItemPos>& front = pareto_front_block_ids_[bin_type_id];
        for (ItemPos block_id = 0;
                block_id < (ItemPos)blocks.size();
                ++block_id) {
            const Box& box = blocks[block_id].box;
            bool dominated = false;
            for (ItemPos other_id = 0;
                    other_id < (ItemPos)blocks.size() && !dominated;
                    ++other_id) {
                if (other_id == block_id)
                    continue;
                const Box& other_box = blocks[other_id].box;
                // other dominates block if it is smaller or equal in every
                // dimension, strictly smaller in at least one (or identical
                // box with a lower id, to keep exactly one representative
                // per distinct size).
                if (other_box.x <= box.x
                        && other_box.y <= box.y
                        && other_box.z <= box.z
                        && (other_box.x < box.x
                            || other_box.y < box.y
                            || other_box.z < box.z
                            || other_id < block_id)) {
                    dominated = true;
                }
            }
            if (!dominated)
                front.push_back(block_id);
        }
    }
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
    //     uncovered: add its volume to unable_volume.
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
                node.unable_volume += rem.box.volume();
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
    const std::vector<Block>& blocks = blocks_[bin_type_id];
    // empty_spaces is sorted by ascending anchor distance (bl_corner sum),
    // descending volume.  Scan from the front and return the first space that
    // has at least one feasible block; that is already the best space.
    BestSpaceResult result;
    for (ItemPos space_idx = 0;
            space_idx < (ItemPos)parent.empty_spaces.size();
            ++space_idx) {
        const EmptySpace& space = parent.empty_spaces[space_idx];
        bool has_fitting_block = false;
        for (ItemPos block_id = 0;
                block_id < (ItemPos)blocks.size() && !has_fitting_block;
                ++block_id) {
            const Block& block = blocks[block_id];
            if (block.box.x > space.box.x
                    || block.box.y > space.box.y
                    || block.box.z > space.box.z)
                continue;
            bool feasible = true;
            for (const auto& item_copy: block.item_copies) {
                if (parent.item_number_of_copies[item_copy.first] + item_copy.second
                        > instance_.item_type(item_copy.first).copies) {
                    feasible = false;
                    break;
                }
            }
            if (feasible)
                has_fitting_block = true;
        }
        if (has_fitting_block) {
            result.space_idx = space_idx;
            result.anchor = compute_anchor_info(space, bin_type.box);
            return result;
        }
    }
    return result;
}

double BranchingSchemeMaximalSpaces::compute_insertion_guide(
        const Node& parent,
        const Insertion& insertion) const
{
    BinPos bin_pos = insertion.new_bin?
        parent.number_of_bins:
        parent.number_of_bins - 1;
    BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
    const Block& block = blocks_[bin_type_id][insertion.block_id];

    Volume new_contact_area = parent.contact_area
        + block.contact_area
        + compute_new_contact_area(
                ((!insertion.new_bin)? parent.placed_blocks.back(): std::vector<Node::PlacedBlock>()),
                bin_type_id,
                insertion.bl_corner,
                block);
    Volume new_item_volume = parent.item_volume + block.item_volume;
    ItemPos new_number_of_items = parent.number_of_items + block.number_of_items;
    Volume new_block_volume = insertion.new_bin?
        instance_.previous_bin_volume(parent.number_of_bins) + block.box.volume():
        parent.block_volume + block.box.volume();
    Volume used_volume = new_block_volume + parent.unable_volume;
    double mean_item_volume = (double)new_item_volume / new_number_of_items;
    double volume_load = used_volume > 0?
        (double)new_item_volume / used_volume:
        0.0;
    return (double)new_contact_area * mean_item_volume * volume_load;
}

const std::vector<BranchingSchemeMaximalSpaces::Insertion>& BranchingSchemeMaximalSpaces::insertions(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "insertions " << parent->id
    //    << " # blocks " << parent->number_of_blocks
    //    << " # items " << parent->number_of_items
    //    << " item_volume " << parent->item_volume
    //    << " profit " << parent->profit
    //    << " guide_volume " << parent->volume_guide
    //    << " volume_load " << volume_load(*parent)
    //    << std::endl;
    insertions_.clear();

    if (!parent->empty_spaces.empty()) {
        BinPos bin_pos = parent->number_of_bins - 1;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);

        BestSpaceResult best = find_best_space(*parent, bin_type_id);

        if (best.space_idx != -1) {
            const EmptySpace& space = parent->empty_spaces[best.space_idx];
            for (ItemPos block_id = 0;
                    block_id < (ItemPos)blocks_[bin_type_id].size();
                    ++block_id) {
                const Block& block = blocks_[bin_type_id][block_id];
                if (block.box.x > space.box.x
                        || block.box.y > space.box.y
                        || block.box.z > space.box.z)
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
                insertion.new_bin = false;
                insertion.space_id = best.space_idx;
                insertion.block_id = block_id;
                insertion.bl_corner = compute_block_position(best.anchor, space, block.box);
                insertion.guide = compute_insertion_guide(*parent, insertion);
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
            insertion.guide = compute_insertion_guide(*parent, insertion);
            insertions_.push_back(insertion);
        }
    }
    std::sort(insertions_.begin(), insertions_.end(),
            [](const Insertion& ins_1, const Insertion& ins_2) {
                return ins_1.guide > ins_2.guide;
            });
    return insertions_;
}

BranchingSchemeMaximalSpaces::Insertion BranchingSchemeMaximalSpaces::best_insertion(
        Node& parent) const
{
    Insertion best;
    double best_score = -1;

    if (!parent.empty_spaces.empty()) {
        BinPos bin_pos = parent.number_of_bins - 1;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);

        BestSpaceResult best_space = find_best_space(parent, bin_type_id);

        if (best_space.space_idx != -1) {
            const EmptySpace& space = parent.empty_spaces[best_space.space_idx];
            for (ItemPos block_id = 0;
                    block_id < (ItemPos)blocks_[bin_type_id].size();
                    ++block_id) {
                const Block& block = blocks_[bin_type_id][block_id];
                if (block.box.x > space.box.x
                        || block.box.y > space.box.y
                        || block.box.z > space.box.z)
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
                Volume new_item_volume = parent.item_volume + block.item_volume;
                ItemPos new_number_of_items = parent.number_of_items + block.number_of_items;
                Volume new_block_volume = parent.block_volume + block.box.volume();
                Volume used_volume = new_block_volume + parent.unable_volume;
                double mean_item_volume = (double)new_item_volume / new_number_of_items;
                double volume_load = used_volume > 0?
                    (double)new_item_volume / used_volume:
                    0.0;
                double score = (double)mean_item_volume * volume_load;
                if (score > best_score) {
                    best_score = score;
                    best.new_bin = false;
                    best.space_id = best_space.space_idx;
                    best.block_id = block_id;
                    best.bl_corner = compute_block_position(best_space.anchor, space, block.box);
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
        const std::vector<Node::PlacedBlock> empty_placed_blocks;
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
            Volume new_item_volume = parent.item_volume + block.item_volume;
            ItemPos new_number_of_items = parent.number_of_items + block.number_of_items;
            Volume new_block_volume = instance_.previous_bin_volume(parent.number_of_bins)
                + block.box.volume();
            Volume used_volume = new_block_volume + parent.unable_volume;
            double mean_item_volume = (double)new_item_volume / new_number_of_items;
            double volume_load = used_volume > 0?
                (double)new_item_volume / used_volume:
                0.0;
            double score = (double)mean_item_volume * volume_load;
            if (score > best_score) {
                best_score = score;
                best.new_bin = true;
                best.space_id = 0;
                best.block_id = block_id;
                best.bl_corner = compute_block_position(anchor, space, block.box);
            }
        }
    }

    return best;
}

Volume BranchingSchemeMaximalSpaces::compute_new_contact_area(
        const std::vector<Node::PlacedBlock>& placed_blocks,
        BinTypeId bin_type_id,
        Point bl_corner,
        const Block& block) const
{
    const BinType& bin_type = instance_.bin_type(bin_type_id);
    Volume contact = 0;
    if (bl_corner.x == 0)
        contact += block.box.y * block.box.z;
    if (bl_corner.x + block.box.x == bin_type.box.x)
        contact += block.box.y * block.box.z;
    if (bl_corner.y == 0)
        contact += block.box.x * block.box.z;
    if (bl_corner.y + block.box.y == bin_type.box.y)
        contact += block.box.x * block.box.z;
    if (bl_corner.z == 0)
        contact += block.box.x * block.box.y;
    if (bl_corner.z + block.box.z == bin_type.box.z)
        contact += block.box.x * block.box.y;
    for (ItemPos pb_idx = (ItemPos)placed_blocks.size() - 1;
            pb_idx >= 0;
            --pb_idx) {
        const Node::PlacedBlock& prev_pb = placed_blocks[pb_idx];
        const Block& prev_block = blocks_[bin_type_id][prev_pb.block_id];
        contact += compute_contact_area(
                bl_corner, block.box,
                prev_pb.bl_corner, prev_block.box);
    }
    return contact;
}

void BranchingSchemeMaximalSpaces::apply_insertion(
        Node& node,
        const Insertion& insertion) const
{
    node.id = node_id_++;
    node.block_id = insertion.block_id;
    node.bl_corner = insertion.bl_corner;
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
    node.number_of_items += (ItemPos)block.items.size();
    node.number_of_blocks++;

    const EmptySpace& space = node.empty_spaces[insertion.space_id];
    AnchorInfo anchor = compute_anchor_info(space, bin_type.box);

    //{
    //    Length bin_mid_x = bin_type.box.x / 2;
    //    Length bin_mid_y = bin_type.box.y / 2;
    //    Length bin_mid_z = bin_type.box.z / 2;
    //    Length block_xe = insertion.bl_corner.x + block.box.x;
    //    Length block_ye = insertion.bl_corner.y + block.box.y;
    //    Length block_ze = insertion.bl_corner.z + block.box.z;
    //    // If the remaining gap in the space after placing the block is less than
    //    // 99 % of the bin length in a direction, the overflow past the octant
    //    // midpoint is negligible: treat the block as fully inside the octant.
    //    Length overlap_x = (100 * (space.box.x - block.box.x) < 99 * bin_type.box.x)
    //        ? block.box.x
    //        : (anchor.dir_x
    //            ? std::max((Length)0, block_xe - std::max(insertion.bl_corner.x, bin_mid_x))
    //            : std::max((Length)0, std::min(block_xe, bin_mid_x) - insertion.bl_corner.x));
    //    Length overlap_y = (100 * (space.box.y - block.box.y) < 99 * bin_type.box.y)
    //        ? block.box.y
    //        : (anchor.dir_y
    //            ? std::max((Length)0, block_ye - std::max(insertion.bl_corner.y, bin_mid_y))
    //            : std::max((Length)0, std::min(block_ye, bin_mid_y) - insertion.bl_corner.y));
    //    Length overlap_z = (100 * (space.box.z - block.box.z) < 99 * bin_type.box.z)
    //        ? block.box.z
    //        : (anchor.dir_z
    //            ? std::max((Length)0, block_ze - std::max(insertion.bl_corner.z, bin_mid_z))
    //            : std::max((Length)0, std::min(block_ze, bin_mid_z) - insertion.bl_corner.z));
    //    Volume volume_inside = (Volume)overlap_x * overlap_y * overlap_z;
    //    node.corner_alignment += 1.5 * volume_inside - 0.5 * block.box.volume();
    //}

    node.contact_area += block.contact_area + compute_new_contact_area(
            node.placed_blocks.back(),
            bin_type_id,
            insertion.bl_corner,
            block);

    // Extend the block's bounding box by the unreachable gap in each direction
    // (Zhu volume-loss reduction).  The gap rx in each direction splits into a
    // reachable part max_reachable[rx] (future items can fill it, left as a
    // sub-space) and an unreachable part (rx - max_reachable[rx]) that no item
    // combination can reach.  The extended box claims the block plus the
    // unreachable parts; cut_spaces removes that region from future consideration.
    Box extended_box = block.box;
    Point extended_bl_corner = insertion.bl_corner;
    if (anchor.dir_x) {
        Length remaining_length = space.xe() - block.box.x;
        Length lost_length = remaining_length - max_reachable_x_[remaining_length];
        extended_box.x += lost_length;
        extended_bl_corner.x -= lost_length;
    } else {
        Length remaining_length = bin_type.box.x - (space.xs() + block.box.x);
        Length lost_length = remaining_length - max_reachable_x_[remaining_length];
        extended_box.x += lost_length;
    }
    if (anchor.dir_y) {
        Length remaining_length = space.ye() - block.box.y;
        Length lost_length = remaining_length - max_reachable_y_[remaining_length];
        extended_box.y += lost_length;
        extended_bl_corner.y -= lost_length;
    } else {
        Length remaining_length = bin_type.box.y - (space.ys() + block.box.y);
        Length lost_length = remaining_length - max_reachable_y_[remaining_length];
        extended_box.y += lost_length;
    }
    if (anchor.dir_z) {
        Length remaining_length = space.ze() - block.box.z;
        Length lost_length = remaining_length - max_reachable_z_[remaining_length];
        extended_box.z += lost_length;
        extended_bl_corner.z -= lost_length;
    } else {
        Length remaining_length = bin_type.box.z - (space.zs() + block.box.z);
        Length lost_length = remaining_length - max_reachable_z_[remaining_length];
        extended_box.z += lost_length;
    }
    node.block_volume += extended_box.volume();

    Length ux = max_reachable_x_[space.box.x - block.box.x];
    Length uy = max_reachable_y_[space.box.y - block.box.y];
    Length uz = max_reachable_z_[space.box.z - block.box.z];
    Volume usable_volume = (Volume)(block.box.x + ux) * (block.box.y + uy) * (block.box.z + uz);
    node.volume_loss_estimate += space.box.volume() - usable_volume;

    //std::cout << "node " << node_id_
    //    << " # blocks " << node.number_of_blocks
    //    << " # items " << node.number_of_items
    //    << " item_volume " << node.item_volume
    //    << " block_volume " << node.block_volume
    //    << " contact_area " << node.contact_area
    //    << " load " << (double)node.item_volume / node.block_volume
    //    << " space " << space.box
    //    << " block " << block.box
    //    << " ext " << extended_box
    //    << std::endl;

    // Try to extend the placed block to the bin wall along each axis and pack
    // all remaining fitting items in the gap.  The three axes are handled
    // sequentially so that a successful x-extension widens the cross-section
    // used for the y- and z-checks.
    Node::PlacedBlock current_pb;
    current_pb.block_id = insertion.block_id;
    current_pb.bl_corner = insertion.bl_corner;
    //{
    //    Length eff_xs = insertion.bl_corner.x;
    //    Length eff_ys = insertion.bl_corner.y;
    //    Length eff_zs = insertion.bl_corner.z;
    //    Length eff_xe = insertion.bl_corner.x + block.box.x;
    //    Length eff_ye = insertion.bl_corner.y + block.box.y;
    //    Length eff_ze = insertion.bl_corner.z + block.box.z;
    //    try_extend_block(node, bin_type_id, bin_type, insertion, current_pb.extra_items,
    //            eff_xs, eff_ys, eff_zs, eff_xe, eff_ye, eff_ze, Direction::X);
    //    try_extend_block(node, bin_type_id, bin_type, insertion, current_pb.extra_items,
    //            eff_xs, eff_ys, eff_zs, eff_xe, eff_ye, eff_ze, Direction::Y);
    //    try_extend_block(node, bin_type_id, bin_type, insertion, current_pb.extra_items,
    //            eff_xs, eff_ys, eff_zs, eff_xe, eff_ye, eff_ze, Direction::Z);
    //    // Widen extended_bl_corner / extended_box to cover all extra-items
    //    // regions so the single cut_spaces call below removes them.
    //    Length old_cut_xe = extended_bl_corner.x + extended_box.x;
    //    Length old_cut_ye = extended_bl_corner.y + extended_box.y;
    //    Length old_cut_ze = extended_bl_corner.z + extended_box.z;
    //    extended_bl_corner.x = std::min(extended_bl_corner.x, eff_xs);
    //    extended_bl_corner.y = std::min(extended_bl_corner.y, eff_ys);
    //    extended_bl_corner.z = std::min(extended_bl_corner.z, eff_zs);
    //    extended_box.x = std::max(old_cut_xe, eff_xe) - extended_bl_corner.x;
    //    extended_box.y = std::max(old_cut_ye, eff_ye) - extended_bl_corner.y;
    //    extended_box.z = std::max(old_cut_ze, eff_ze) - extended_bl_corner.z;
    //}

    cut_spaces(node.empty_spaces, extended_bl_corner, extended_box);

    // Remove spaces too small for any remaining item.
    {
        scratch_unfillable_space_ids_.clear();
        for (ItemPos space_id = 0;
                space_id < (ItemPos)node.empty_spaces.size();
                ++space_id) {
            const EmptySpace& space = node.empty_spaces[space_id];
            bool has_fitting_item = false;
            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance_.number_of_item_types() && !has_fitting_item;
                    ++item_type_id) {
                if (node.item_number_of_copies[item_type_id] >= instance_.item_type(item_type_id).copies)
                    continue;
                const ItemType& item_type = instance_.item_type(item_type_id);
                for (Rotation rotation: item_type.rotations) {
                    Box rotated = item_type.box.rotate(rotation);
                    if (rotated.x <= space.box.x
                            && rotated.y <= space.box.y
                            && rotated.z <= space.box.z) {
                        has_fitting_item = true;
                        break;
                    }
                }
            }
            if (!has_fitting_item)
                scratch_unfillable_space_ids_.push_back(space_id);
        }
        if (!scratch_unfillable_space_ids_.empty())
            remove_spaces_and_update_waste(node, scratch_unfillable_space_ids_);
    }

    std::sort(node.empty_spaces.begin(), node.empty_spaces.end());

    node.placed_blocks.back().push_back(std::move(current_pb));
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
        return node_2->profit < node_1->profit;
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

Solution BranchingSchemeMaximalSpaces::to_solution(const std::shared_ptr<Node>& node) const
{
    Solution solution(instance_);
    if (node->number_of_bins == 0)
        return solution;

    for (BinPos bin_idx = 0;
            bin_idx < (BinPos)node->placed_blocks.size();
            ++bin_idx) {
        BinTypeId bin_type_id = instance_.bin_type_id(bin_idx);
        BinPos bin_pos = solution.add_bin(bin_type_id, 1);
        for (const Node::PlacedBlock& pb: node->placed_blocks[bin_idx]) {
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
            for (const SolutionItem& extra_item: pb.extra_items) {
                solution.add_item(
                    bin_pos,
                    extra_item.item_type_id,
                    extra_item.bl_corner,
                    extra_item.rotation);
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
    node->volume_guide = compute_guide_greedy(*node);
    return node;
}

void BranchingSchemeMaximalSpaces::try_extend_block(
        Node& node,
        BinTypeId bin_type_id,
        const BinType& bin_type,
        const Insertion& insertion,
        std::vector<SolutionItem>& extra_items,
        Length eff_xs,
        Length eff_ys,
        Length eff_zs,
        Length& eff_xe,
        Length& eff_ye,
        Length& eff_ze,
        Direction direction) const
{
    Length gap_face, bin_wall, cross1_start, cross1_end, cross2_start, cross2_end;
    if (direction == Direction::X) {
        gap_face = eff_xe;
        bin_wall = bin_type.box.x;
        cross1_start = eff_ys;
        cross1_end = eff_ye;
        cross2_start = eff_zs;
        cross2_end = eff_ze;
    } else if (direction == Direction::Y) {
        gap_face = eff_ye;
        bin_wall = bin_type.box.y;
        cross1_start = eff_xs;
        cross1_end = eff_xe;
        cross2_start = eff_zs;
        cross2_end = eff_ze;
    } else {
        gap_face = eff_ze;
        bin_wall = bin_type.box.z;
        cross1_start = eff_xs;
        cross1_end = eff_xe;
        cross2_start = eff_ys;
        cross2_end = eff_ye;
    }
    Length gap = bin_wall - gap_face;
    if (gap <= 0)
        return;

    // Check: no packed block in the current bin intersects the gap region.
    bool faces_wall = true;
    {
        const std::vector<Node::PlacedBlock>& bin_placed = node.placed_blocks.back();
        for (ItemPos pb_idx = (ItemPos)bin_placed.size() - 1;
                pb_idx >= 0;
                --pb_idx) {
            const Node::PlacedBlock& prev_pb = bin_placed[pb_idx];
            const Block& prev_block = blocks_[bin_type_id][prev_pb.block_id];
            Length prev_gap_start, prev_gap_end;
            Length prev_c1_start, prev_c1_end;
            Length prev_c2_start, prev_c2_end;
            if (direction == Direction::X) {
                prev_gap_start = prev_pb.bl_corner.x;
                prev_gap_end = prev_pb.bl_corner.x + prev_block.box.x;
                prev_c1_start = prev_pb.bl_corner.y;
                prev_c1_end = prev_pb.bl_corner.y + prev_block.box.y;
                prev_c2_start = prev_pb.bl_corner.z;
                prev_c2_end = prev_pb.bl_corner.z + prev_block.box.z;
            } else if (direction == Direction::Y) {
                prev_gap_start = prev_pb.bl_corner.y;
                prev_gap_end = prev_pb.bl_corner.y + prev_block.box.y;
                prev_c1_start = prev_pb.bl_corner.x;
                prev_c1_end = prev_pb.bl_corner.x + prev_block.box.x;
                prev_c2_start = prev_pb.bl_corner.z;
                prev_c2_end = prev_pb.bl_corner.z + prev_block.box.z;
            } else {
                prev_gap_start = prev_pb.bl_corner.z;
                prev_gap_end = prev_pb.bl_corner.z + prev_block.box.z;
                prev_c1_start = prev_pb.bl_corner.x;
                prev_c1_end = prev_pb.bl_corner.x + prev_block.box.x;
                prev_c2_start = prev_pb.bl_corner.y;
                prev_c2_end = prev_pb.bl_corner.y + prev_block.box.y;
            }
            if (prev_gap_end > gap_face && prev_gap_start < bin_wall
                    && prev_c1_end > cross1_start && prev_c1_start < cross1_end
                    && prev_c2_end > cross2_start && prev_c2_start < cross2_end) {
                faces_wall = false;
                break;
            }
        }
    }
    if (!faces_wall)
        return;

    // For each remaining item type find the rotation that fits in the gap and
    // maximises the gap-dimension extent.
    struct FittingItemType {
        ItemTypeId item_type_id;
        Rotation rotation;
        Length dim_gap;
        Length dim_c1;
        Length dim_c2;
        ItemPos copies;
    };
    std::vector<FittingItemType> fitting_item_types;
    Length c1_size = cross1_end - cross1_start;
    Length c2_size = cross2_end - cross2_start;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        ItemPos remaining = item_type.copies - node.item_number_of_copies[item_type_id];
        if (remaining <= 0)
            continue;

        bool found = false;
        Rotation best_rotation = Rotation::XYZ;
        Length best_dim_gap = 0;
        Length best_dim_c1 = 0;
        Length best_dim_c2 = 0;

        for (Rotation r: item_type.rotations) {
            Box rotated = item_type.box.rotate(r);
            Length dim_gap, dim_c1, dim_c2;
            if (direction == Direction::X) {
                dim_gap = rotated.x;
                dim_c1 = rotated.y;
                dim_c2 = rotated.z;
            } else if (direction == Direction::Y) {
                dim_gap = rotated.y;
                dim_c1 = rotated.x;
                dim_c2 = rotated.z;
            } else {
                dim_gap = rotated.z;
                dim_c1 = rotated.x;
                dim_c2 = rotated.y;
            }
            if (dim_gap <= gap && dim_c1 <= c1_size && dim_c2 <= c2_size) {
                if (!found || dim_gap > best_dim_gap) {
                    best_rotation = r;
                    best_dim_gap = dim_gap;
                    best_dim_c1 = dim_c1;
                    best_dim_c2 = dim_c2;
                    found = true;
                }
            }
        }

        if (found) {
            FittingItemType fi;
            fi.item_type_id = item_type_id;
            fi.rotation = best_rotation;
            fi.dim_gap = best_dim_gap;
            fi.dim_c1 = best_dim_c1;
            fi.dim_c2 = best_dim_c2;
            fi.copies = remaining;
            fitting_item_types.push_back(fi);
        }
    }

    if (fitting_item_types.empty())
        return;

    // First-fit 3D bottom-left packing into the gap region.
    // pos_gap: cursor along the gap direction (offset from gap_face).
    // pos_c1_curr/pos_c2_curr: cursor along the two cross axes.
    // pos_c1_next: end of the tallest item in the current c2-layer (next shelf start).
    // pos_c2_next: end of the deepest item placed so far (next layer start).
    Length pos_gap = 0;
    Length pos_c1_curr = 0;
    Length pos_c2_curr = 0;
    Length pos_c1_next = 0;
    Length pos_c2_next = 0;

    std::vector<SolutionItem> tentative_extra_items;

    for (const FittingItemType& fi: fitting_item_types) {
        for (ItemPos copy = 0; copy < fi.copies; ++copy) {
            Length dim_gap = fi.dim_gap;
            Length dim_c1 = fi.dim_c1;
            Length dim_c2 = fi.dim_c2;
            Length place_gap;
            Length place_c1;
            Length place_c2;

            if (pos_gap + dim_gap <= gap
                    && pos_c1_curr + dim_c1 <= c1_size
                    && pos_c2_curr + dim_c2 <= c2_size) {
                place_gap = pos_gap;
                place_c1 = pos_c1_curr;
                place_c2 = pos_c2_curr;
                pos_gap += dim_gap;
                pos_c1_next = std::max(pos_c1_next, pos_c1_curr + dim_c1);
                pos_c2_next = std::max(pos_c2_next, pos_c2_curr + dim_c2);
            } else if (dim_gap <= gap
                    && pos_c1_next + dim_c1 <= c1_size
                    && pos_c2_curr + dim_c2 <= c2_size) {
                pos_c1_curr = pos_c1_next;
                place_gap = 0;
                place_c1 = pos_c1_curr;
                place_c2 = pos_c2_curr;
                pos_gap = dim_gap;
                pos_c1_next = pos_c1_curr + dim_c1;
                pos_c2_next = std::max(pos_c2_next, pos_c2_curr + dim_c2);
            } else if (dim_gap <= gap
                    && dim_c1 <= c1_size
                    && pos_c2_next + dim_c2 <= c2_size) {
                pos_c2_curr = pos_c2_next;
                pos_c1_curr = 0;
                place_gap = 0;
                place_c1 = 0;
                place_c2 = pos_c2_curr;
                pos_gap = dim_gap;
                pos_c1_next = dim_c1;
                pos_c2_next = pos_c2_curr + dim_c2;
            } else {
                return;
            }

            SolutionItem extra_item;
            extra_item.item_type_id = fi.item_type_id;
            extra_item.rotation = fi.rotation;
            if (direction == Direction::X) {
                extra_item.bl_corner = {
                    gap_face + place_gap,
                    cross1_start + place_c1,
                    cross2_start + place_c2};
            } else if (direction == Direction::Y) {
                extra_item.bl_corner = {
                    cross1_start + place_c1,
                    gap_face + place_gap,
                    cross2_start + place_c2};
            } else {
                extra_item.bl_corner = {
                    cross1_start + place_c1,
                    cross2_start + place_c2,
                    gap_face + place_gap};
            }
            tentative_extra_items.push_back(extra_item);
        }
    }

    for (const SolutionItem& extra_item: tentative_extra_items) {
        extra_items.push_back(extra_item);
    }
    for (const FittingItemType& fi: fitting_item_types) {
        node.item_number_of_copies[fi.item_type_id] += fi.copies;
        node.item_volume += instance_.item_type(fi.item_type_id).box.volume() * fi.copies;
        node.number_of_items += fi.copies;
        node.profit += instance_.item_type(fi.item_type_id).profit * fi.copies;
    }
    node.block_volume += (Volume)gap * c1_size * c2_size;

    if (direction == Direction::X)
        eff_xe = bin_wall;
    else if (direction == Direction::Y)
        eff_ye = bin_wall;
    else
        eff_ze = bin_wall;
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
