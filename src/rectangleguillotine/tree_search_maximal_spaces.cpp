#include "rectangleguillotine/tree_search_maximal_spaces.hpp"

#include "rectangleguillotine/solution_builder.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
//////////////////////// BranchingSchemeMaximalSpaces //////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingSchemeMaximalSpaces::BranchingSchemeMaximalSpaces(
        const Instance& instance,
        const std::vector<std::vector<Block>>& blocks,
        const Parameters& parameters):
    instance_(instance),
    blocks_(blocks),
    parameters_(parameters)
{
    cut_thickness_ = instance_.parameters().cut_thickness;
    BinTypeId bin_type_id = instance_.bin_type_id(0);
    const BinType& bin_type = instance_.bin_type(bin_type_id);
    bin_rect_ = {bin_type.rect.w, bin_type.rect.h};
}

bool BranchingSchemeMaximalSpaces::cut_is_vertical(
        Depth depth,
        CutOrientation cut_orientation) const
{
    bool even_depth = (depth % 2 == 0);
    if (cut_orientation == CutOrientation::Vertical)
        return even_depth;
    else  // Horizontal
        return !even_depth;
}

ItemPos BranchingSchemeMaximalSpaces::find_best_space(const Node& node) const
{
    ItemPos best_idx = -1;
    Area best_area = std::numeric_limits<Area>::max();
    Length best_dist = std::numeric_limits<Length>::max();
    for (ItemPos idx = 0; idx < (ItemPos)node.empty_spaces.size(); ++idx) {
        const EmptySpace& space = node.empty_spaces[idx];
        Area area = space.area();
        Length dist = space.bl_corner.x + space.bl_corner.y;
        bool is_better = (area < best_area)
            || (area == best_area && dist < best_dist);
        if (is_better) {
            best_idx = idx;
            best_area = area;
            best_dist = dist;
        }
    }
    return best_idx;
}

const std::shared_ptr<BranchingSchemeMaximalSpaces::Node>
BranchingSchemeMaximalSpaces::root() const
{
    auto node = std::make_shared<Node>();
    node->id = node_id_++;
    node->item_number_of_copies.assign(instance_.number_of_item_types(), 0);

    EmptySpace root_space;
    root_space.bl_corner = {0, 0};
    root_space.width = bin_rect_.w;
    root_space.height = bin_rect_.h;
    root_space.depth = 0;
    root_space.cut_orientation = parameters_.first_cut_orientation;
    node->empty_spaces.push_back(root_space);

    BinTypeId bin_type_id = instance_.bin_type_id(0);
    node->valid_block_ids.resize(blocks_[bin_type_id].size());
    std::iota(node->valid_block_ids.begin(), node->valid_block_ids.end(), (ItemPos)0);

    return node;
}

const std::vector<BranchingSchemeMaximalSpaces::Insertion>&
BranchingSchemeMaximalSpaces::insertions(
        const std::shared_ptr<Node>& parent) const
{
    if (!parent->cached_insertions.empty() || parent->next_child_pos > 0)
        return parent->cached_insertions;

    insertions_.clear();

    if (parent->empty_spaces.empty())
        return insertions_;

    ItemPos space_idx = find_best_space(*parent);
    const EmptySpace& space = parent->empty_spaces[space_idx];

    bool generate_both = (space.cut_orientation == CutOrientation::Any);

    BinTypeId bin_type_id = instance_.bin_type_id(0);
    for (ItemPos block_id: parent->valid_block_ids) {
        const Block& block = blocks_[bin_type_id][block_id];
        if (block.rect.w > space.width || block.rect.h > space.height)
            continue;
        double guide = (space.area() > 0)?
            (double)block.item_profit / space.area():
            0.0;
        {
            Insertion ins;
            ins.block_id = block_id;
            ins.space_id = space_idx;
            ins.cut_orientation = generate_both?
                CutOrientation::Vertical:
                space.cut_orientation;
            ins.guide = guide;
            insertions_.push_back(ins);
        }
        if (generate_both) {
            Insertion ins;
            ins.block_id = block_id;
            ins.space_id = space_idx;
            ins.cut_orientation = CutOrientation::Horizontal;
            ins.guide = guide;
            insertions_.push_back(ins);
        }
    }

    std::sort(
            insertions_.begin(),
            insertions_.end(),
            [](const Insertion& ins_1, const Insertion& ins_2) {
                if (ins_1.guide != ins_2.guide)
                    return ins_1.guide > ins_2.guide;
                // Vertical (enum value 1) > Horizontal (enum value 0): Vertical first.
                return ins_1.cut_orientation > ins_2.cut_orientation;
            });

    parent->cached_insertions = insertions_;
    return parent->cached_insertions;
}

BranchingSchemeMaximalSpaces::Insertion
BranchingSchemeMaximalSpaces::best_insertion(Node& node) const
{
    if (node.empty_spaces.empty())
        return {};

    ItemPos space_idx = find_best_space(node);
    const EmptySpace& space = node.empty_spaces[space_idx];

    // When the space is Any, the greedy always picks Vertical.
    CutOrientation co = (space.cut_orientation == CutOrientation::Any)?
        CutOrientation::Vertical:
        space.cut_orientation;

    BinTypeId bin_type_id = instance_.bin_type_id(0);
    Insertion best;
    best.block_id = -1;
    double best_guide = -1.0;

    for (ItemPos block_id: node.valid_block_ids) {
        const Block& block = blocks_[bin_type_id][block_id];
        if (block.rect.w > space.width || block.rect.h > space.height)
            continue;
        double guide = (space.area() > 0)?
            (double)block.item_profit / space.area():
            0.0;
        if (guide > best_guide) {
            best_guide = guide;
            best.block_id = block_id;
            best.space_id = space_idx;
            best.cut_orientation = co;
            best.guide = guide;
        }
    }
    return best;
}

void BranchingSchemeMaximalSpaces::apply_insertion(
        Node& node,
        const Insertion& insertion) const
{
    BinTypeId bin_type_id = instance_.bin_type_id(0);
    const EmptySpace space = node.empty_spaces[insertion.space_id];
    const Block& block = blocks_[bin_type_id][insertion.block_id];
    Length w_b = block.rect.w;
    Length h_b = block.rect.h;
    Coord bl = space.bl_corner;
    Depth d = space.depth;
    CutOrientation co = insertion.cut_orientation;

    // Record placement.
    Node::PlacedBlock pb;
    pb.block_id = insertion.block_id;
    pb.bl_corner = bl;
    pb.cut_orientation = co;
    node.placed_blocks.push_back(pb);

    // Update item counts and profit.
    for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies) {
        node.item_number_of_copies[item_copy.first] += item_copy.second;
    }
    node.item_area += block.item_area;
    node.profit += block.item_profit;
    node.number_of_items += block.number_of_items;
    node.number_of_blocks++;
    node.id = node_id_++;

    // Remove selected space.
    node.empty_spaces.erase(node.empty_spaces.begin() + insertion.space_id);

    // Generate the two new URSs based on the forced cut direction.
    bool vertical_first = cut_is_vertical(d, co);

    if (vertical_first) {
        // Right URS: full height, to the right of the block.
        if (space.xe() > bl.x + w_b + cut_thickness_) {
            EmptySpace right_space;
            right_space.bl_corner = {bl.x + w_b + cut_thickness_, bl.y};
            right_space.width = space.xe() - bl.x - w_b - cut_thickness_;
            right_space.height = space.height;
            right_space.depth = d + 1;
            right_space.cut_orientation = co;
            node.empty_spaces.push_back(right_space);
        }
        // Above URS: same width as block, above the block.
        if (space.ye() > bl.y + h_b + cut_thickness_) {
            EmptySpace above_space;
            above_space.bl_corner = {bl.x, bl.y + h_b + cut_thickness_};
            above_space.width = w_b;
            above_space.height = space.ye() - bl.y - h_b - cut_thickness_;
            above_space.depth = d + 2;
            above_space.cut_orientation = co;
            node.empty_spaces.push_back(above_space);
        }
    } else {
        // Above URS: full width, above the block.
        if (space.ye() > bl.y + h_b + cut_thickness_) {
            EmptySpace above_space;
            above_space.bl_corner = {bl.x, bl.y + h_b + cut_thickness_};
            above_space.width = space.width;
            above_space.height = space.ye() - bl.y - h_b - cut_thickness_;
            above_space.depth = d + 1;
            above_space.cut_orientation = co;
            node.empty_spaces.push_back(above_space);
        }
        // Right URS: same height as block, to the right of the block.
        if (space.xe() > bl.x + w_b + cut_thickness_) {
            EmptySpace right_space;
            right_space.bl_corner = {bl.x + w_b + cut_thickness_, bl.y};
            right_space.width = space.xe() - bl.x - w_b - cut_thickness_;
            right_space.height = h_b;
            right_space.depth = d + 2;
            right_space.cut_orientation = co;
            node.empty_spaces.push_back(right_space);
        }
    }

    // Update valid block ids based on new item copy counts.
    std::vector<ItemPos> new_valid;
    new_valid.reserve(node.valid_block_ids.size());
    for (ItemPos block_id: node.valid_block_ids) {
        const Block& candidate = blocks_[bin_type_id][block_id];
        bool valid = true;
        for (const std::pair<ItemTypeId, ItemPos>& item_copy: candidate.item_copies) {
            if (node.item_number_of_copies[item_copy.first] + item_copy.second
                    > instance_.item_type(item_copy.first).copies) {
                valid = false;
                break;
            }
        }
        if (valid)
            new_valid.push_back(block_id);
    }
    node.valid_block_ids = std::move(new_valid);

    // Remove empty spaces in which no valid block fits.
    ItemPos space_idx = 0;
    while (space_idx < (ItemPos)node.empty_spaces.size()) {
        bool has_fitting_block = false;
        for (ItemPos block_id: node.valid_block_ids) {
            const Block& candidate = blocks_[bin_type_id][block_id];
            if (candidate.rect.w <= node.empty_spaces[space_idx].width
                    && candidate.rect.h <= node.empty_spaces[space_idx].height) {
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

Profit BranchingSchemeMaximalSpaces::compute_guide_greedy(const Node& node) const
{
    Node greedy_node = node;
    while (true) {
        Insertion ins = best_insertion(greedy_node);
        if (ins.block_id == -1)
            break;
        apply_insertion(greedy_node, ins);
    }
    return greedy_node.profit;
}

std::shared_ptr<BranchingSchemeMaximalSpaces::Node>
BranchingSchemeMaximalSpaces::next_child(
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

bool BranchingSchemeMaximalSpaces::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    return node_1->profit > node_2->profit;
}

bool BranchingSchemeMaximalSpaces::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    return false;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// to_solution ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Solution BranchingSchemeMaximalSpaces::to_solution(
        const std::shared_ptr<Node>& node_orig) const
{
    // Run greedy to completion.
    Node greedy_node = *node_orig;
    while (true) {
        Insertion ins = best_insertion(greedy_node);
        if (ins.block_id == -1)
            break;
        apply_insertion(greedy_node, ins);
    }

    BinTypeId bin_type_id = instance_.bin_type_id(0);
    const BinType& bin_type = instance_.bin_type(bin_type_id);

    // All placed blocks share the same cut_orientation (inherited from first placement).
    CutOrientation fco = greedy_node.placed_blocks.empty()?
        CutOrientation::Vertical:
        greedy_node.placed_blocks[0].cut_orientation;

    // Local cut-tree node used to build the tree in memory before feeding it
    // to SolutionBuilder via DFS.
    struct CutNode {
        Depth depth = 0;
        Length l = 0, r = 0, b = 0, t = 0;
        // -1 = waste/empty, >= 0 = item leaf
        ItemTypeId item_type_id = -1;
        std::vector<ItemPos> children;
    };
    std::vector<CutNode> cut_nodes;

    // Root covers the full bin.
    {
        CutNode root;
        root.depth = 0;
        root.l = 0; root.r = bin_type.rect.w;
        root.b = 0; root.t = bin_type.rect.h;
        cut_nodes.push_back(root);
    }

    // place_simple_block: emit cut nodes for a cx×cy grid of one item type.
    // (bx0, by0): absolute bin coordinates of the block's BL corner.
    std::function<void(ItemPos, const Block&, Depth, Length, Length)> place_simple_block;
    place_simple_block = [&](
            ItemPos parent_idx,
            const Block& block,
            Depth d_parent,
            Length bx0, Length by0)
    {
        const ItemType& item_type = instance_.item_type(block.item_type_id);
        Length bw = item_type.width(block.rotate);
        Length bh = item_type.height(block.rotate);
        ItemPos cx = (bw > 0) ? (block.rect.w + cut_thickness_) / (bw + cut_thickness_) : 1;
        ItemPos cy = (bh > 0) ? (block.rect.h + cut_thickness_) / (bh + cut_thickness_) : 1;

        if (cx == 1 && cy == 1) {
            cut_nodes[parent_idx].item_type_id = block.item_type_id;
            return;
        }

        bool required_vertical = cut_is_vertical(d_parent, fco);

        if (cx == 1) {
            // Only horizontal cuts; inject wrapper if parity requires vertical.
            if (required_vertical) {
                ItemPos wrap_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+bw; cn.b=by0; cn.t=by0+block.rect.h; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(wrap_idx);
                place_simple_block(wrap_idx, block, d_parent+1, bx0, by0);
                return;
            }
            for (ItemPos ccy = 0; ccy < cy; ++ccy) {
                Length cell_b = by0 + ccy * (bh + cut_thickness_);
                ItemPos child_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+bw; cn.b=cell_b; cn.t=cell_b+bh; cn.item_type_id=block.item_type_id; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(child_idx);
            }
            return;
        }

        if (cy == 1) {
            // Only vertical cuts; inject wrapper if parity requires horizontal.
            if (!required_vertical) {
                ItemPos wrap_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+block.rect.w; cn.b=by0; cn.t=by0+bh; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(wrap_idx);
                place_simple_block(wrap_idx, block, d_parent+1, bx0, by0);
                return;
            }
            for (ItemPos ccx = 0; ccx < cx; ++ccx) {
                Length cell_l = bx0 + ccx * (bw + cut_thickness_);
                ItemPos child_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=cell_l; cn.r=cell_l+bw; cn.b=by0; cn.t=by0+bh; cn.item_type_id=block.item_type_id; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(child_idx);
            }
            return;
        }

        // Both cx > 1 and cy > 1: outer cut matches required_vertical.
        if (required_vertical) {
            for (ItemPos ccx = 0; ccx < cx; ++ccx) {
                Length col_l = bx0 + ccx * (bw + cut_thickness_);
                ItemPos col_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=col_l; cn.r=col_l+bw; cn.b=by0; cn.t=by0+block.rect.h; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(col_idx);
                for (ItemPos ccy = 0; ccy < cy; ++ccy) {
                    Length cell_b = by0 + ccy * (bh + cut_thickness_);
                    ItemPos cell_idx = (ItemPos)cut_nodes.size();
                    { CutNode cn; cn.depth=d_parent+2; cn.l=col_l; cn.r=col_l+bw; cn.b=cell_b; cn.t=cell_b+bh; cn.item_type_id=block.item_type_id; cut_nodes.push_back(cn); }
                    cut_nodes[col_idx].children.push_back(cell_idx);
                }
            }
        } else {
            for (ItemPos ccy = 0; ccy < cy; ++ccy) {
                Length row_b = by0 + ccy * (bh + cut_thickness_);
                ItemPos row_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+block.rect.w; cn.b=row_b; cn.t=row_b+bh; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(row_idx);
                for (ItemPos ccx = 0; ccx < cx; ++ccx) {
                    Length cell_l = bx0 + ccx * (bw + cut_thickness_);
                    ItemPos cell_idx = (ItemPos)cut_nodes.size();
                    { CutNode cn; cn.depth=d_parent+2; cn.l=cell_l; cn.r=cell_l+bw; cn.b=row_b; cn.t=row_b+bh; cn.item_type_id=block.item_type_id; cut_nodes.push_back(cn); }
                    cut_nodes[row_idx].children.push_back(cell_idx);
                }
            }
        }
    };

    // place_block: recursively expand a block into cut_nodes.
    // avail_w/avail_h is the region assigned to this block at parent_idx;
    // block.rect.w/h is the block's actual footprint (avail >= rect).
    std::function<void(ItemPos, ItemPos, Depth, Length, Length, Length, Length)> place_block;
    place_block = [&](
            ItemPos parent_idx,
            ItemPos block_id,
            Depth d_parent,
            Length bx0, Length by0,
            Length avail_w, Length avail_h)
    {
        const Block& block = blocks_[bin_type_id][block_id];

        // Excess width: emit a vertical cut, block on the left, waste on the right.
        // Only create a waste node when the gap exceeds cut_thickness (otherwise
        // the solution_builder cannot fit a valid sibling after the cut).
        if (avail_w > block.rect.w + cut_thickness_) {
            bool required_vertical = cut_is_vertical(d_parent, fco);
            if (!required_vertical) {
                ItemPos wrap_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+avail_w; cn.b=by0; cn.t=by0+avail_h; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(wrap_idx);
                place_block(wrap_idx, block_id, d_parent+1, bx0, by0, avail_w, avail_h);
                return;
            }
            ItemPos left_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+block.rect.w; cn.b=by0; cn.t=by0+avail_h; cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back(left_idx);
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0+block.rect.w; cn.r=bx0+avail_w; cn.b=by0; cn.t=by0+avail_h; cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back((ItemPos)cut_nodes.size() - 1);
            place_block(left_idx, block_id, d_parent+1, bx0, by0, block.rect.w, avail_h);
            return;
        }

        // Excess height: emit a horizontal cut, block below, waste above.
        // Only create a waste node when the gap exceeds cut_thickness (otherwise
        // the solution_builder cannot fit a valid sibling after the cut).
        if (avail_h > block.rect.h + cut_thickness_) {
            bool required_vertical = cut_is_vertical(d_parent, fco);
            if (required_vertical) {
                ItemPos wrap_idx = (ItemPos)cut_nodes.size();
                { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+avail_w; cn.b=by0; cn.t=by0+avail_h; cut_nodes.push_back(cn); }
                cut_nodes[parent_idx].children.push_back(wrap_idx);
                place_block(wrap_idx, block_id, d_parent+1, bx0, by0, avail_w, avail_h);
                return;
            }
            ItemPos bottom_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+avail_w; cn.b=by0; cn.t=by0+block.rect.h; cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back(bottom_idx);
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+avail_w; cn.b=by0+block.rect.h; cn.t=by0+avail_h; cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back((ItemPos)cut_nodes.size() - 1);
            place_block(bottom_idx, block_id, d_parent+1, bx0, by0, avail_w, block.rect.h);
            return;
        }

        // No excess: avail == rect.
        if (block.is_simple) {
            place_simple_block(parent_idx, block, d_parent, bx0, by0);
            return;
        }

        // Combined block: split into child_1 and child_2.
        const Block& child_1 = blocks_[bin_type_id][block.child_1_id];
        const Block& child_2 = blocks_[bin_type_id][block.child_2_id];
        bool required_vertical = cut_is_vertical(d_parent, fco);

        if (block.cut_is_vertical != required_vertical) {
            ItemPos wrap_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+block.rect.w; cn.b=by0; cn.t=by0+block.rect.h; cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back(wrap_idx);
            place_block(wrap_idx, block_id, d_parent+1, bx0, by0, block.rect.w, block.rect.h);
            return;
        }

        if (block.cut_is_vertical) {
            Length cut_x = bx0 + child_1.rect.w;
            Length right_x = cut_x + cut_thickness_;
            ItemPos left_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=cut_x; cn.b=by0; cn.t=by0+block.rect.h; cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back(left_idx);
            ItemPos right_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d_parent+1; cn.l=right_x; cn.r=bx0+block.rect.w; cn.b=by0; cn.t=by0+block.rect.h; cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back(right_idx);
            place_block(left_idx,  block.child_1_id, d_parent+1, bx0,     by0, child_1.rect.w, block.rect.h);
            place_block(right_idx, block.child_2_id, d_parent+1, right_x, by0, child_2.rect.w, block.rect.h);
        } else {
            Length cut_y = by0 + child_1.rect.h;
            Length top_y = cut_y + cut_thickness_;
            ItemPos bottom_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+block.rect.w; cn.b=by0;   cn.t=cut_y;              cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back(bottom_idx);
            ItemPos top_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d_parent+1; cn.l=bx0; cn.r=bx0+block.rect.w; cn.b=top_y; cn.t=by0+block.rect.h;  cut_nodes.push_back(cn); }
            cut_nodes[parent_idx].children.push_back(top_idx);
            place_block(bottom_idx, block.child_1_id, d_parent+1, bx0, by0,   block.rect.w, child_1.rect.h);
            place_block(top_idx,    block.child_2_id, d_parent+1, bx0, top_y, block.rect.w, child_2.rect.h);
        }
    };

    // Track each URS with the index of its CutNode.
    struct UrsEntry {
        EmptySpace space;
        ItemPos node_idx;
    };

    std::vector<UrsEntry> urs_list;
    {
        EmptySpace root_space;
        root_space.bl_corner = {0, 0};
        root_space.width  = bin_rect_.w;
        root_space.height = bin_rect_.h;
        root_space.depth  = 0;
        root_space.cut_orientation = parameters_.first_cut_orientation;
        urs_list.push_back({root_space, 0});
    }

    // Mirror apply_insertion's valid-block pruning so that the replay's
    // min-area selection is identical to the search's find_best_space.
    std::vector<ItemPos> replay_item_copies(instance_.number_of_item_types(), 0);
    std::vector<ItemPos> replay_valid_block_ids((ItemPos)blocks_[bin_type_id].size());
    std::iota(replay_valid_block_ids.begin(), replay_valid_block_ids.end(), 0);

    // Replay each placed block in placement order (same min-area selection
    // as the search) and build the cut tree in cut_nodes.
    for (const Node::PlacedBlock& pb: greedy_node.placed_blocks) {
        // Select min-area URS (mirrors find_best_space).
        ItemPos best_idx = -1;
        Area best_area = std::numeric_limits<Area>::max();
        Length best_dist = std::numeric_limits<Length>::max();
        for (ItemPos idx = 0; idx < (ItemPos)urs_list.size(); ++idx) {
            const EmptySpace& space = urs_list[idx].space;
            Area area  = space.area();
            Length dist = space.bl_corner.x + space.bl_corner.y;
            bool is_better = (area < best_area)
                || (area == best_area && dist < best_dist);
            if (is_better) { best_idx = idx; best_area = area; best_dist = dist; }
        }

        const EmptySpace& urs = urs_list[best_idx].space;
        ItemPos urs_node_idx  = urs_list[best_idx].node_idx;

        const Block& block = blocks_[bin_type_id][pb.block_id];
        Length w_b = block.rect.w;
        Length h_b = block.rect.h;
        Depth d    = urs.depth;
        CutOrientation co = pb.cut_orientation;
        bool vertical_first = cut_is_vertical(d, co);

        std::vector<UrsEntry> new_urs;
        ItemPos block_area_idx = -1;

        if (vertical_first) {
            // depth d+1: vertical strip covering block column
            ItemPos strip_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d+1; cn.l=urs.xs(); cn.r=urs.xs()+w_b; cn.b=urs.ys(); cn.t=urs.ye(); cut_nodes.push_back(cn); }
            cut_nodes[urs_node_idx].children.push_back(strip_idx);

            // depth d+1: right URS
            if (urs.xe() > urs.xs() + w_b + cut_thickness_) {
                ItemPos right_idx = (ItemPos)cut_nodes.size();
                Length rl = urs.xs() + w_b + cut_thickness_;
                { CutNode cn; cn.depth=d+1; cn.l=rl; cn.r=urs.xe(); cn.b=urs.ys(); cn.t=urs.ye(); cut_nodes.push_back(cn); }
                cut_nodes[urs_node_idx].children.push_back(right_idx);
                EmptySpace rs; rs.bl_corner={rl, urs.ys()}; rs.width=urs.xe()-rl; rs.height=urs.height; rs.depth=d+1; rs.cut_orientation=co;
                new_urs.push_back({rs, right_idx});
            }

            // depth d+2: block area
            block_area_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d+2; cn.l=urs.xs(); cn.r=urs.xs()+w_b; cn.b=urs.ys(); cn.t=urs.ys()+h_b; cut_nodes.push_back(cn); }
            cut_nodes[strip_idx].children.push_back(block_area_idx);

            // depth d+2: above URS
            if (urs.ye() > urs.ys() + h_b + cut_thickness_) {
                ItemPos above_idx = (ItemPos)cut_nodes.size();
                Length ab = urs.ys() + h_b + cut_thickness_;
                { CutNode cn; cn.depth=d+2; cn.l=urs.xs(); cn.r=urs.xs()+w_b; cn.b=ab; cn.t=urs.ye(); cut_nodes.push_back(cn); }
                cut_nodes[strip_idx].children.push_back(above_idx);
                EmptySpace as; as.bl_corner={urs.xs(), ab}; as.width=w_b; as.height=urs.ye()-ab; as.depth=d+2; as.cut_orientation=co;
                new_urs.push_back({as, above_idx});
            }
        } else {
            // depth d+1: horizontal slab covering block row
            ItemPos slab_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d+1; cn.l=urs.xs(); cn.r=urs.xe(); cn.b=urs.ys(); cn.t=urs.ys()+h_b; cut_nodes.push_back(cn); }
            cut_nodes[urs_node_idx].children.push_back(slab_idx);

            // depth d+1: above URS
            if (urs.ye() > urs.ys() + h_b + cut_thickness_) {
                ItemPos above_idx = (ItemPos)cut_nodes.size();
                Length ab = urs.ys() + h_b + cut_thickness_;
                { CutNode cn; cn.depth=d+1; cn.l=urs.xs(); cn.r=urs.xe(); cn.b=ab; cn.t=urs.ye(); cut_nodes.push_back(cn); }
                cut_nodes[urs_node_idx].children.push_back(above_idx);
                EmptySpace as; as.bl_corner={urs.xs(), ab}; as.width=urs.width; as.height=urs.ye()-ab; as.depth=d+1; as.cut_orientation=co;
                new_urs.push_back({as, above_idx});
            }

            // depth d+2: block area
            block_area_idx = (ItemPos)cut_nodes.size();
            { CutNode cn; cn.depth=d+2; cn.l=urs.xs(); cn.r=urs.xs()+w_b; cn.b=urs.ys(); cn.t=urs.ys()+h_b; cut_nodes.push_back(cn); }
            cut_nodes[slab_idx].children.push_back(block_area_idx);

            // depth d+2: right URS
            if (urs.xe() > urs.xs() + w_b + cut_thickness_) {
                ItemPos right_idx = (ItemPos)cut_nodes.size();
                Length rl = urs.xs() + w_b + cut_thickness_;
                { CutNode cn; cn.depth=d+2; cn.l=rl; cn.r=urs.xe(); cn.b=urs.ys(); cn.t=urs.ys()+h_b; cut_nodes.push_back(cn); }
                cut_nodes[slab_idx].children.push_back(right_idx);
                EmptySpace rs; rs.bl_corner={rl, urs.ys()}; rs.width=urs.xe()-rl; rs.height=h_b; rs.depth=d+2; rs.cut_orientation=co;
                new_urs.push_back({rs, right_idx});
            }
        }

        // Expand the block's cut tree into cut_nodes.
        place_block(block_area_idx, pb.block_id, d + 2, pb.bl_corner.x, pb.bl_corner.y, block.rect.w, block.rect.h);

        urs_list.erase(urs_list.begin() + best_idx);
        for (UrsEntry& new_entry: new_urs)
            urs_list.push_back(new_entry);

        // Mirror apply_insertion: update valid blocks then prune dead URSs.
        for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies)
            replay_item_copies[item_copy.first] += item_copy.second;
        std::vector<ItemPos> new_valid;
        new_valid.reserve(replay_valid_block_ids.size());
        for (ItemPos block_id: replay_valid_block_ids) {
            const Block& candidate = blocks_[bin_type_id][block_id];
            bool valid = true;
            for (const std::pair<ItemTypeId, ItemPos>& item_copy: candidate.item_copies) {
                if (replay_item_copies[item_copy.first] + item_copy.second
                        > instance_.item_type(item_copy.first).copies) {
                    valid = false;
                    break;
                }
            }
            if (valid)
                new_valid.push_back(block_id);
        }
        replay_valid_block_ids = std::move(new_valid);

        ItemPos prune_idx = 0;
        while (prune_idx < (ItemPos)urs_list.size()) {
            bool has_fitting_block = false;
            for (ItemPos block_id: replay_valid_block_ids) {
                const Block& candidate = blocks_[bin_type_id][block_id];
                if (candidate.rect.w <= urs_list[prune_idx].space.width
                        && candidate.rect.h <= urs_list[prune_idx].space.height) {
                    has_fitting_block = true;
                    break;
                }
            }
            if (!has_fitting_block) {
                urs_list[prune_idx] = urs_list.back();
                urs_list.pop_back();
            } else {
                ++prune_idx;
            }
        }
    }

    // DFS traversal of cut_nodes feeding SolutionBuilder.
    // Mirrors the traversal in SolutionBuilder::read(): for each non-root
    // node call add_node(depth, cut_position) then optionally set_last_node_item.
    SolutionBuilder solution_builder(instance_);
    solution_builder.add_bin(bin_type_id, 1, fco);

    std::function<void(ItemPos)> dfs = [&](ItemPos node_idx) {
        const CutNode& node = cut_nodes[node_idx];
        if (node.depth > 0) {
            bool node_is_vertical = (
                (fco == CutOrientation::Vertical  && node.depth % 2 == 1)
                || (fco == CutOrientation::Horizontal && node.depth % 2 == 0));
            Length cut_pos = node_is_vertical ? node.r : node.t;
            solution_builder.add_node(node.depth, cut_pos);
            if (node.item_type_id >= 0) {
                solution_builder.set_last_node_item(node.item_type_id);
                return;
            }
        }
        for (ItemPos child_idx: node.children)
            dfs(child_idx);
    };
    dfs(0);

    return solution_builder.build();
}
