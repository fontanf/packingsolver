#include "box/branching_scheme_maximal_spaces.hpp"

#include "packingsolver/box/solution.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Internals ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

struct AnchorInfo
{
    Length distance;
    bool dir_x;
    bool dir_y;
    bool dir_z;
    Volume space_volume;
};

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
        const Parameters& parameters):
    instance_(instance),
    blocks_(blocks),
    parameters_(parameters)
{
    compute_reachable_lengths();
}

void BranchingSchemeMaximalSpaces::compute_reachable_lengths()
{
    Length max_x = 0;
    Length max_y = 0;
    Length max_z = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        max_x = std::max(max_x, bin_type.box.x);
        max_y = std::max(max_y, bin_type.box.y);
        max_z = std::max(max_z, bin_type.box.z);
    }

    // Multiple-choice subset-sum per axis: reachable[v] = true iff length v
    // is achievable by stacking copies of items, where each copy contributes
    // the dimension of exactly one chosen rotation (not multiple rotations).
    std::vector<uint8_t> reachable_x(max_x + 1, false);
    std::vector<uint8_t> reachable_y(max_y + 1, false);
    std::vector<uint8_t> reachable_z(max_z + 1, false);
    std::vector<uint8_t> temp_x(max_x + 1, false);
    std::vector<uint8_t> temp_y(max_y + 1, false);
    std::vector<uint8_t> temp_z(max_z + 1, false);
    reachable_x[0] = true;
    reachable_y[0] = true;
    reachable_z[0] = true;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        const ItemPos copies = item_type.copies;
        for (ItemPos copy_idx = 0; copy_idx < copies; ++copy_idx) {
            // Multiple-choice update: initialize temp from the pre-copy
            // reachable (the "skip this copy" option), then propagate each
            // rotation's contribution using the same pre-copy reachable as
            // source so that at most one rotation is counted per copy.
            temp_x = reachable_x;
            temp_y = reachable_y;
            temp_z = reachable_z;
            for (Rotation rotation: item_type.rotations) {
                Box box = item_type.box.rotate(rotation);
                if (box.x <= max_x)
                    for (Length v = max_x; v >= box.x; --v)
                        if (reachable_x[v - box.x])
                            temp_x[v] = true;
                if (box.y <= max_y)
                    for (Length v = max_y; v >= box.y; --v)
                        if (reachable_y[v - box.y])
                            temp_y[v] = true;
                if (box.z <= max_z)
                    for (Length v = max_z; v >= box.z; --v)
                        if (reachable_z[v - box.z])
                            temp_z[v] = true;
            }
            reachable_x = temp_x;
            reachable_y = temp_y;
            reachable_z = temp_z;
        }
    }

    // max_reachable_x_[r] = largest reachable length <= r.
    max_reachable_x_.resize(max_x + 1);
    max_reachable_y_.resize(max_y + 1);
    max_reachable_z_.resize(max_z + 1);
    max_reachable_x_[0] = 0;
    max_reachable_y_[0] = 0;
    max_reachable_z_[0] = 0;
    for (Length v = 1; v <= max_x; ++v)
        max_reachable_x_[v] = reachable_x[v] ? v : max_reachable_x_[v - 1];
    for (Length v = 1; v <= max_y; ++v)
        max_reachable_y_[v] = reachable_y[v] ? v : max_reachable_y_[v - 1];
    for (Length v = 1; v <= max_z; ++v)
        max_reachable_z_[v] = reachable_z[v] ? v : max_reachable_z_[v - 1];
}

const std::shared_ptr<BranchingSchemeMaximalSpaces::Node> BranchingSchemeMaximalSpaces::root() const
{
    auto node = std::make_shared<Node>();
    node->id = node_id_++;
    node->item_number_of_copies.assign(instance_.number_of_item_types(), 0);
    node->number_of_bins = 0;
    return node;
}

const std::vector<BranchingSchemeMaximalSpaces::Insertion>& BranchingSchemeMaximalSpaces::insertions(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "node_id " << parent->id
    //    << " # items " << parent->number_of_items
    //    << " # vol " << parent->item_volume
    //    << " load " << (double)parent->item_volume / parent->block_volume
    //    << " " << (double)parent->item_volume / instance_.bin_volume()
    //    << std::endl;
    insertions_.clear();

    // Try spaces in anchor-distance order (K3) until one yields a feasible
    // insertion (K4).  Spaces that cannot fit any remaining block are skipped:
    // item counts only increase as we go deeper, so a space that cannot fit
    // anything now can never fit anything later.
    if (!parent->empty_spaces.empty()) {
        BinPos bin_pos = parent->number_of_bins - 1;
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        const BinType& bin_type = instance_.bin_type(bin_type_id);

        // Find the best space that can fit at least one block (K3).
        // Spaces that fit no block are skipped permanently: item counts only
        // increase deeper, so they will never become useful.
        ItemPos best_space_idx = -1;
        AnchorInfo best_anchor;
        for (ItemPos space_idx = 0;
                space_idx < (ItemPos)parent->empty_spaces.size();
                ++space_idx) {
            const EmptySpace& space = parent->empty_spaces[space_idx];
            bool has_fitting_block = false;
            for (ItemPos block_id = 0; block_id < (ItemPos)blocks_[bin_type_id].size(); ++block_id) {
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
                if (feasible) {
                    has_fitting_block = true;
                    break;
                }
            }
            if (!has_fitting_block)
                continue;
            AnchorInfo anchor = compute_anchor_info(space, bin_type.box);
            if (best_space_idx == -1
                    || anchor.distance < best_anchor.distance
                    || (anchor.distance == best_anchor.distance
                        && anchor.space_volume > best_anchor.space_volume)) {
                best_space_idx = space_idx;
                best_anchor = anchor;
            }
        }

        // Generate one insertion per fitting block in the best space (K4).
        if (best_space_idx != -1) {
            const EmptySpace& space = parent->empty_spaces[best_space_idx];
            for (ItemPos block_id = 0; block_id < (ItemPos)blocks_[bin_type_id].size(); ++block_id) {
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
                insertion.space_id = best_space_idx;
                insertion.block_id = block_id;
                insertion.bl_corner = compute_block_position(best_anchor, space, block.box);
                insertions_.push_back(insertion);
            }
        }
    }

    // Insert in a new bin.
    if (insertions_.empty()
            && parent->number_of_bins < instance_.number_of_bins()) {
        BinPos bin_pos = parent->number_of_bins;
        BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        EmptySpace space;
        space.bl_corner.x = 0;
        space.bl_corner.y = 0;
        space.bl_corner.z = 0;
        space.box = bin_type.box;
        AnchorInfo anchor = compute_anchor_info(space, bin_type.box);
        for (ItemPos block_id = 0; block_id < (ItemPos)blocks_[bin_type_id].size(); ++block_id) {
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
            insertions_.push_back(insertion);
        }
    }
    return insertions_;
}

BranchingSchemeMaximalSpaces::Node BranchingSchemeMaximalSpaces::child_tmp(
        const std::shared_ptr<Node>& parent,
        const Insertion& insertion) const
{
    Node node;
    node.id = node_id_++;
    node.parent = parent;
    node.block_id = insertion.block_id;
    node.bl_corner = insertion.bl_corner;
    node.new_bin = insertion.new_bin;
    node.item_number_of_copies = parent->item_number_of_copies;
    node.item_volume = parent->item_volume;
    node.number_of_items = parent->number_of_items;
    node.number_of_blocks = parent->number_of_blocks;
    node.number_of_bins = parent->number_of_bins;
    node.profit = parent->profit;

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
    } else {
        node.empty_spaces = parent->empty_spaces;
        node.block_volume = parent->block_volume;
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

    {
        Length bin_mid_x = bin_type.box.x / 2;
        Length bin_mid_y = bin_type.box.y / 2;
        Length bin_mid_z = bin_type.box.z / 2;
        Length block_xe = insertion.bl_corner.x + block.box.x;
        Length block_ye = insertion.bl_corner.y + block.box.y;
        Length block_ze = insertion.bl_corner.z + block.box.z;
        // If the remaining gap in the space after placing the block is less than
        // 99 % of the bin length in a direction, the overflow past the octant
        // midpoint is negligible: treat the block as fully inside the octant.
        Length overlap_x = (100 * (space.box.x - block.box.x) < 99 * bin_type.box.x)
            ? block.box.x
            : (anchor.dir_x
                ? std::max((Length)0, block_xe - std::max(insertion.bl_corner.x, bin_mid_x))
                : std::max((Length)0, std::min(block_xe, bin_mid_x) - insertion.bl_corner.x));
        Length overlap_y = (100 * (space.box.y - block.box.y) < 99 * bin_type.box.y)
            ? block.box.y
            : (anchor.dir_y
                ? std::max((Length)0, block_ye - std::max(insertion.bl_corner.y, bin_mid_y))
                : std::max((Length)0, std::min(block_ye, bin_mid_y) - insertion.bl_corner.y));
        Length overlap_z = (100 * (space.box.z - block.box.z) < 99 * bin_type.box.z)
            ? block.box.z
            : (anchor.dir_z
                ? std::max((Length)0, block_ze - std::max(insertion.bl_corner.z, bin_mid_z))
                : std::max((Length)0, std::min(block_ze, bin_mid_z) - insertion.bl_corner.z));
        Volume volume_inside = (Volume)overlap_x * overlap_y * overlap_z;
        node.corner_alignment = parent->corner_alignment
            + 1.5 * volume_inside - 0.5 * block.box.volume();
    }

    {
        Volume new_contact = 0;
        // Contact with bin walls.
        if (insertion.bl_corner.x == 0)
            new_contact += block.box.y * block.box.z;
        if (insertion.bl_corner.x + block.box.x == bin_type.box.x)
            new_contact += block.box.y * block.box.z;
        if (insertion.bl_corner.y == 0)
            new_contact += block.box.x * block.box.z;
        if (insertion.bl_corner.y + block.box.y == bin_type.box.y)
            new_contact += block.box.x * block.box.z;
        if (insertion.bl_corner.z == 0)
            new_contact += block.box.x * block.box.y;
        if (insertion.bl_corner.z + block.box.z == bin_type.box.z)
            new_contact += block.box.x * block.box.y;
        // Contact with previously placed blocks in the same bin.
        if (!insertion.new_bin) {
            for (const Node* current = parent.get();
                    current->parent != nullptr;
                    current = current->parent.get()) {
                const Block& prev_block = blocks_[bin_type_id][current->block_id];
                new_contact += compute_contact_area(
                        insertion.bl_corner, block.box,
                        current->bl_corner, prev_block.box);
                if (current->new_bin)
                    break;
            }
        }
        node.contact_area = parent->contact_area + new_contact;
    }

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
    node.volume_loss_estimate = parent->volume_loss_estimate + space.box.volume() - usable_volume;

    std::cout << "node " << node_id_
        << " # blocks " << node.number_of_blocks
        << " # items " << node.number_of_items
        << " item_volume " << node.item_volume
        << " block_volume " << node.block_volume
        << " contact_area " << node.contact_area
        << " load " << (double)node.item_volume / node.block_volume
        << " space " << space.box
        << " block " << block.box
        << " ext " << extended_box
        << std::endl;

    cut_spaces(node.empty_spaces, extended_bl_corner, extended_box);

    return node;
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

    BinPos bin_pos = -1;
    BinPos bin_count = 0;

    std::vector<const Node*> path;
    for (const Node* current = node.get();
            current->parent != nullptr;
            current = current->parent.get())
        path.push_back(current);
    std::reverse(path.begin(), path.end());

    for (const Node* current: path) {
        if (current->new_bin) {
            BinTypeId bin_type_id = instance_.bin_type_id(bin_count);
            bin_pos = solution.add_bin(bin_type_id, 1);
            bin_count++;
        }
        BinTypeId bin_type_id = instance_.bin_type_id(bin_count - 1);
        const Block& block = blocks_[bin_type_id][current->block_id];
        for (const SolutionItem& solution_item: block.items) {
            Point item_bl_corner;
            item_bl_corner.x = current->bl_corner.x + solution_item.bl_corner.x;
            item_bl_corner.y = current->bl_corner.y + solution_item.bl_corner.y;
            item_bl_corner.z = current->bl_corner.z + solution_item.bl_corner.z;
            solution.add_item(
                bin_pos,
                solution_item.item_type_id,
                item_bl_corner,
                solution_item.rotation);
        }
    }

    return solution;
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
