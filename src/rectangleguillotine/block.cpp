#include "rectangleguillotine/block.hpp"

#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/block.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

/** Absolute item bounds in block-local coordinates. */
struct AbsItem {
    ItemTypeId item_type_id = -1;
    bool rotate = false;
    Length l = 0, r = 0, b = 0, t = 0;
};

/**
 * Recursively build a guillotine cut sub-tree for the region
 * [x0, x0+width] x [y0, y0+height] that contains exactly the given items.
 *
 * Tries vertical cuts first (collecting item.r values as candidate cut
 * positions), then horizontal cuts.  For a single-item region, trim-waste
 * children are appended when the item does not fill the full extent.
 *
 * Returns the index of the newly created root node in block.nodes.
 */
int build_node(
        Block& block,
        Length x0, Length y0, Length width, Length height,
        const std::vector<AbsItem>& items)
{
    int idx = (int)block.nodes.size();
    {
        Block::Node node;
        node.x0 = x0; node.y0 = y0;
        node.width = width; node.height = height;
        block.nodes.push_back(node);
    }

    if (items.size() == 1) {
        const AbsItem& item = items[0];

        if (item.l > x0) {
            // Left gap: vertical cut at item.l, waste on the left.
            block.nodes[idx].cut_is_vertical = true;
            block.nodes[idx].cut_pos = item.l;

            int left_idx = (int)block.nodes.size();
            {
                Block::Node waste;
                waste.x0 = x0; waste.y0 = y0;
                waste.width = item.l - x0; waste.height = height;
                block.nodes.push_back(waste);
            }
            int right_idx = build_node(block, item.l, y0, x0 + width - item.l, height, items);
            block.nodes[idx].first_child = left_idx;
            block.nodes[idx].second_child = right_idx;
        } else if (item.b > y0) {
            // Bottom gap: horizontal cut at item.b, waste at the bottom.
            block.nodes[idx].cut_is_vertical = false;
            block.nodes[idx].cut_pos = item.b;

            int bottom_idx = (int)block.nodes.size();
            {
                Block::Node waste;
                waste.x0 = x0; waste.y0 = y0;
                waste.width = width; waste.height = item.b - y0;
                block.nodes.push_back(waste);
            }
            int top_idx = build_node(block, x0, item.b, width, y0 + height - item.b, items);
            block.nodes[idx].first_child = bottom_idx;
            block.nodes[idx].second_child = top_idx;
        } else if (item.r < x0 + width) {
            // Right trim: item.l == x0; vertical cut at item.r, waste to the right.
            block.nodes[idx].cut_is_vertical = true;
            block.nodes[idx].cut_pos = item.r;

            int left_idx = build_node(block, x0, y0, item.r - x0, height, items);
            int right_idx = (int)block.nodes.size();
            {
                Block::Node waste;
                waste.x0 = item.r; waste.y0 = y0;
                waste.width = x0 + width - item.r; waste.height = height;
                block.nodes.push_back(waste);
            }
            block.nodes[idx].first_child = left_idx;
            block.nodes[idx].second_child = right_idx;
        } else if (item.t < y0 + height) {
            // Top trim: item.l==x0, item.b==y0, item.r==x0+width; horizontal cut at item.t.
            block.nodes[idx].cut_is_vertical = false;
            block.nodes[idx].cut_pos = item.t;

            int bottom_idx = (int)block.nodes.size();
            {
                Block::Node leaf;
                leaf.x0 = x0; leaf.y0 = y0;
                leaf.width = width; leaf.height = item.t - y0;
                leaf.item_type_id = item.item_type_id;
                leaf.rotate = item.rotate;
                block.nodes.push_back(leaf);
            }
            int top_idx = (int)block.nodes.size();
            {
                Block::Node waste;
                waste.x0 = x0; waste.y0 = item.t;
                waste.width = width; waste.height = y0 + height - item.t;
                block.nodes.push_back(waste);
            }
            block.nodes[idx].first_child = bottom_idx;
            block.nodes[idx].second_child = top_idx;
        } else {
            // Perfect fit.
            block.nodes[idx].item_type_id = item.item_type_id;
            block.nodes[idx].rotate = item.rotate;
        }
        return idx;
    }

    // Try vertical cuts (candidate positions: right edges of items).
    {
        std::vector<Length> cut_xs;
        for (const AbsItem& item: items)
            cut_xs.push_back(item.r);
        std::sort(cut_xs.begin(), cut_xs.end());
        cut_xs.erase(std::unique(cut_xs.begin(), cut_xs.end()), cut_xs.end());

        for (Length cut_x: cut_xs) {
            if (cut_x <= x0 || cut_x >= x0 + width)
                continue;
            std::vector<AbsItem> left_items, right_items;
            bool valid = true;
            for (const AbsItem& item: items) {
                if (item.r <= cut_x)
                    left_items.push_back(item);
                else if (item.l >= cut_x)
                    right_items.push_back(item);
                else { valid = false; break; }
            }
            if (!valid || left_items.empty() || right_items.empty())
                continue;

            block.nodes[idx].cut_is_vertical = true;
            block.nodes[idx].cut_pos = cut_x;

            int left_idx = build_node(block, x0, y0, cut_x - x0, height, left_items);
            int right_idx = build_node(block, cut_x, y0, x0 + width - cut_x, height, right_items);

            block.nodes[idx].first_child = left_idx;
            block.nodes[idx].second_child = right_idx;
            return idx;
        }
    }

    // Try horizontal cuts (candidate positions: top edges of items).
    {
        std::vector<Length> cut_ys;
        for (const AbsItem& item: items)
            cut_ys.push_back(item.t);
        std::sort(cut_ys.begin(), cut_ys.end());
        cut_ys.erase(std::unique(cut_ys.begin(), cut_ys.end()), cut_ys.end());

        for (Length cut_y: cut_ys) {
            if (cut_y <= y0 || cut_y >= y0 + height)
                continue;
            std::vector<AbsItem> bottom_items, top_items;
            bool valid = true;
            for (const AbsItem& item: items) {
                if (item.t <= cut_y)
                    bottom_items.push_back(item);
                else if (item.b >= cut_y)
                    top_items.push_back(item);
                else { valid = false; break; }
            }
            if (!valid || bottom_items.empty() || top_items.empty())
                continue;

            block.nodes[idx].cut_is_vertical = false;
            block.nodes[idx].cut_pos = cut_y;

            int bottom_idx = build_node(block, x0, y0, width, cut_y - y0, bottom_items);
            int top_idx = build_node(block, x0, cut_y, width, y0 + height - cut_y, top_items);

            block.nodes[idx].first_child = bottom_idx;
            block.nodes[idx].second_child = top_idx;
            return idx;
        }
    }

    // Should never be reached for valid guillotine blocks.
    return idx;
}

Block build_block(
        const Instance& instance,
        const rectangle::Block& rb)
{
    Block block;
    block.rect = rb.rect;
    block.item_area = rb.item_area;
    block.item_profit = rb.item_profit;
    block.weight = rb.weight;
    block.number_of_items = rb.number_of_items;
    block.item_copies = rb.item_copies;

    if (rb.items.empty())
        return block;

    // Build absolute item bounds in block-local coordinates.
    std::vector<AbsItem> abs_items;
    abs_items.reserve(rb.items.size());
    for (const rectangle::SolutionItem& si: rb.items) {
        const ItemType& item_type = instance.item_type(si.item_type_id);
        AbsItem ai;
        ai.item_type_id = si.item_type_id;
        ai.rotate = si.rotate;
        ai.l = si.bl_corner.x;
        ai.b = si.bl_corner.y;
        ai.r = ai.l + item_type.width(si.rotate);
        ai.t = ai.b + item_type.height(si.rotate);
        abs_items.push_back(ai);
    }

    block.nodes.reserve(2 * (int)rb.items.size() + 1);
    build_node(block, 0, 0, rb.rect.x, rb.rect.y, abs_items);
    return block;
}

} // anonymous namespace

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const Block& block)
{
    os << "Block rect=(" << block.rect.x << "x" << block.rect.y << ")"
       << " items=" << block.number_of_items
       << " nodes=" << block.nodes.size();
    return os;
}

std::vector<std::vector<Block>> packingsolver::rectangleguillotine::compute_blocks(
        const Instance& instance,
        const rectangle::BlockParameters& parameters)
{
    // Build a rectangle::Instance mirroring this guillotine instance so that
    // rectangle::compute_blocks can generate the block shapes.
    rectangle::InstanceBuilder rb;
    rb.set_objective(instance.objective());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        rb.add_bin_type(
                bin_type.rect.w,
                bin_type.rect.h,
                bin_type.cost,
                bin_type.copies);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        rb.add_item_type(
                item_type.rect.w,
                item_type.rect.h,
                item_type.profit,
                item_type.copies,
                item_type.oriented);
    }
    rectangle::Instance rectangle_instance = rb.build();

    std::vector<std::vector<rectangle::Block>> all_rect_blocks
        = rectangle::compute_blocks(rectangle_instance, parameters);

    std::vector<std::vector<Block>> result(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        if (bin_type_id >= (BinTypeId)all_rect_blocks.size())
            break;
        result[bin_type_id].reserve(all_rect_blocks[bin_type_id].size());
        for (const rectangle::Block& rb_block: all_rect_blocks[bin_type_id])
            result[bin_type_id].push_back(build_block(instance, rb_block));
    }
    return result;
}
