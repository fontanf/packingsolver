#include "rectangle/block.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <algorithm>
#include <ostream>
#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::rectangle;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Internals ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::rectangle::operator<<(std::ostream& os, const Block& block)
{
    os << "x " << block.rect.x
       << " y " << block.rect.y
       << " item_area " << block.item_area
       << " fill_rate " << block.fill_rate()
       << " item_copies";
    for (const auto& item_copy: block.item_copies)
        os << " " << item_copy.first << "*" << item_copy.second;
    return os;
}

namespace
{

struct BlockKey
{
    Rectangle rect;
    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;
};

struct BlockKeyHasher
{
    std::size_t operator()(const BlockKey& key) const
    {
        std::size_t hash = 0;
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.rect.x));
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.rect.y));
        for (const auto& item_copy: key.item_copies) {
            optimizationtools::hash_combine(hash, std::hash<ItemTypeId>{}(item_copy.first));
            optimizationtools::hash_combine(hash, std::hash<ItemPos>{}(item_copy.second));
        }
        return hash;
    }
};

struct BlockKeyEqual
{
    bool operator()(
            const BlockKey& block_key_1,
            const BlockKey& block_key_2) const
    {
        return block_key_1.rect.x == block_key_2.rect.x
            && block_key_1.rect.y == block_key_2.rect.y
            && block_key_1.item_copies == block_key_2.item_copies;
    }
};

using BlockSet = std::unordered_set<BlockKey, BlockKeyHasher, BlockKeyEqual>;

BlockKey make_key(const Block& block)
{
    return {block.rect, block.item_copies};
}

bool is_valid_item_copies(
    const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies_1,
    const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies_2,
    const Instance& instance)
{
    auto it1 = item_copies_1.begin();
    auto it2 = item_copies_2.begin();
    while (it1 != item_copies_1.end() || it2 != item_copies_2.end()) {
        ItemTypeId item_type_id;
        ItemPos copies;
        if (it1 == item_copies_1.end()) {
            item_type_id = it2->first; copies = it2->second; ++it2;
        } else if (it2 == item_copies_2.end()) {
            item_type_id = it1->first; copies = it1->second; ++it1;
        } else if (it1->first < it2->first) {
            item_type_id = it1->first; copies = it1->second; ++it1;
        } else if (it1->first > it2->first) {
            item_type_id = it2->first; copies = it2->second; ++it2;
        } else {
            item_type_id = it1->first; copies = it1->second + it2->second; ++it1; ++it2;
        }
        if (copies > instance.item_type(item_type_id).copies)
            return false;
    }
    return true;
}

std::vector<std::pair<ItemTypeId, ItemPos>> merge_item_copies(
    const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies_1,
    const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies_2,
    const Instance& instance)
{
    std::vector<std::pair<ItemTypeId, ItemPos>> result;
    auto it1 = item_copies_1.begin();
    auto it2 = item_copies_2.begin();
    while (it1 != item_copies_1.end() || it2 != item_copies_2.end()) {
        ItemTypeId item_type_id;
        ItemPos copies;
        if (it1 == item_copies_1.end()) {
            item_type_id = it2->first; copies = it2->second; ++it2;
        } else if (it2 == item_copies_2.end()) {
            item_type_id = it1->first; copies = it1->second; ++it1;
        } else if (it1->first < it2->first) {
            item_type_id = it1->first; copies = it1->second; ++it1;
        } else if (it1->first > it2->first) {
            item_type_id = it2->first; copies = it2->second; ++it2;
        } else {
            item_type_id = it1->first; copies = it1->second + it2->second; ++it1; ++it2;
        }
        result.push_back({item_type_id, copies});
    }
    return result;
}

struct BlockFillRateLess {
    bool operator()(const Block& block_1, const Block& block_2) const
    {
        if (block_1.fill_rate() != block_2.fill_rate())
            return block_1.fill_rate() < block_2.fill_rate();
        return block_1.number_of_items > block_2.number_of_items;
    }
};

std::vector<Block> compute_blocks_for_bin(
        const Instance& instance,
        BinTypeId bin_type_id,
        const BlockParameters& parameters)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Rectangle bin_rect = bin_type.rect;

    std::multiset<Block, BlockFillRateLess> blocks_to_process;
    std::vector<Block> returned_blocks;
    BlockSet seen;

    auto enqueue = [&](Block block) {
        blocks_to_process.insert(std::move(block));
        if (parameters.maximum_number_of_blocks != -1
                && (ItemPos)blocks_to_process.size() > parameters.maximum_number_of_blocks) {
            BlockKey key = make_key(*blocks_to_process.begin());
            seen.erase(key);
            blocks_to_process.erase(blocks_to_process.begin());
        }
    };

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies < 1)
            continue;
        int number_of_rotations = item_type.oriented ? 1 : 2;
        for (int rot_idx = 0; rot_idx < number_of_rotations; ++rot_idx) {
            bool rotate = (rot_idx == 1);
            Length bx = item_type.x(rotate);
            Length by = item_type.y(rotate);
            ItemPos copies_max_x = (std::min)(
                    item_type.copies,
                    (ItemPos)(bin_rect.x / bx));
            for (ItemPos cx = 1; cx <= copies_max_x; ++cx) {
                ItemPos copies_max_y = (std::min)(
                        item_type.copies / cx,
                        (ItemPos)(bin_rect.y / by));
                for (ItemPos cy = 1; cy <= copies_max_y; ++cy) {
                    Block block;
                    block.is_simple = true;
                    block.item_type_id = item_type_id;
                    block.rotate = rotate;
                    block.rect = {cx * bx, cy * by};
                    block.item_area = cx * cy * item_type.area();
                    block.item_profit = (double)(cx * cy) * item_type.profit;
                    block.weight = cx * cy * item_type.weight;
                    block.item_copies = {{item_type_id, cx * cy}};
                    block.number_of_items = cx * cy;
                    block.items.reserve(cx * cy);
                    for (ItemPos ccx = 0; ccx < cx; ++ccx) {
                        for (ItemPos ccy = 0; ccy < cy; ++ccy) {
                            SolutionItem solution_item;
                            solution_item.item_type_id = item_type_id;
                            solution_item.rotate = rotate;
                            solution_item.bl_corner = {ccx * bx, ccy * by};
                            block.items.push_back(solution_item);
                        }
                    }
                    BlockKey key = make_key(block);
                    if (!seen.count(key)) {
                        seen.insert(key);
                        enqueue(std::move(block));
                    }
                }
            }
        }
    }

    while (!blocks_to_process.empty()) {
        if (parameters.maximum_number_of_blocks != -1
                && (ItemPos)returned_blocks.size() >= parameters.maximum_number_of_blocks)
            break;

        auto it = std::prev(blocks_to_process.end());
        Block block = *it;
        blocks_to_process.erase(it);

        if (instance.number_of_item_types() <= 3
                && block.fill_rate() != 1.0) {
            break;
        }

        for (const Block& existing_block: returned_blocks) {
            if (block.is_simple && existing_block.is_simple
                    && block.item_type_id == existing_block.item_type_id
                    && block.rotate == existing_block.rotate)
                continue;
            for (Direction direction: {Direction::X, Direction::Y}) {
                Block combined;
                combined.item_area = block.item_area + existing_block.item_area;
                combined.item_profit = block.item_profit + existing_block.item_profit;
                combined.weight = block.weight + existing_block.weight;
                switch (direction) {
                case Direction::X:
                    combined.rect.x = block.rect.x + existing_block.rect.x;
                    combined.rect.y = std::max(block.rect.y, existing_block.rect.y);
                    break;
                case Direction::Y:
                    combined.rect.x = std::max(block.rect.x, existing_block.rect.x);
                    combined.rect.y = block.rect.y + existing_block.rect.y;
                    break;
                default: break;
                }
                if (combined.rect.x > bin_rect.x
                        || combined.rect.y > bin_rect.y)
                    continue;

                if (parameters.maximum_number_of_blocks != -1
                        && (ItemPos)blocks_to_process.size() >= parameters.maximum_number_of_blocks) {
                    if (combined.fill_rate() <= blocks_to_process.begin()->fill_rate())
                        continue;
                }

                if (!is_valid_item_copies(block.item_copies, existing_block.item_copies, instance))
                    continue;

                BlockKey key;
                key.rect = combined.rect;
                key.item_copies = merge_item_copies(
                        block.item_copies,
                        existing_block.item_copies,
                        instance);
                if (seen.count(key))
                    continue;
                combined.number_of_items = block.number_of_items + existing_block.number_of_items;
                combined.item_copies = key.item_copies;
                combined.items = block.items;
                for (const SolutionItem& item: existing_block.items) {
                    SolutionItem shifted = item;
                    switch (direction) {
                    case Direction::X: shifted.bl_corner.x += block.rect.x; break;
                    case Direction::Y: shifted.bl_corner.y += block.rect.y; break;
                    default: break;
                    }
                    combined.items.push_back(shifted);
                }
                seen.insert(key);
                enqueue(std::move(combined));
            }
        }

        returned_blocks.push_back(std::move(block));
    }

    return returned_blocks;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// compute_blocks /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

MaxReachableLengths packingsolver::rectangle::compute_max_reachable_lengths(
    const Instance& instance)
{
    Length max_x = 0;
    Length max_y = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        max_x = std::max(max_x, bin_type.rect.x);
        max_y = std::max(max_y, bin_type.rect.y);
    }

    std::vector<uint8_t> reachable_x(max_x + 1, false);
    std::vector<uint8_t> reachable_y(max_y + 1, false);
    std::vector<uint8_t> temp_x(max_x + 1, false);
    std::vector<uint8_t> temp_y(max_y + 1, false);
    reachable_x[0] = reachable_y[0] = true;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        int number_of_rotations = item_type.oriented ? 1 : 2;
        for (ItemPos copy_idx = 0; copy_idx < item_type.copies; ++copy_idx) {
            temp_x = reachable_x;
            temp_y = reachable_y;
            for (int rot_idx = 0; rot_idx < number_of_rotations; ++rot_idx) {
                bool rotate = (rot_idx == 1);
                Length rx = item_type.x(rotate);
                Length ry = item_type.y(rotate);
                if (rx <= max_x)
                    for (Length v = max_x; v >= rx; --v)
                        if (reachable_x[v - rx]) temp_x[v] = true;
                if (ry <= max_y)
                    for (Length v = max_y; v >= ry; --v)
                        if (reachable_y[v - ry]) temp_y[v] = true;
            }
            reachable_x = temp_x;
            reachable_y = temp_y;
        }
    }

    MaxReachableLengths result;
    result.x.resize(max_x + 1, 0);
    result.y.resize(max_y + 1, 0);
    for (Length v = 1; v <= max_x; ++v)
        result.x[v] = reachable_x[v] ? v : result.x[v - 1];
    for (Length v = 1; v <= max_y; ++v)
        result.y[v] = reachable_y[v] ? v : result.y[v - 1];
    return result;
}

std::vector<std::vector<Block>> packingsolver::rectangle::compute_blocks(
    const Instance& instance,
    const BlockParameters& parameters)
{
    std::vector<std::vector<Block>> result(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        result[bin_type_id] = compute_blocks_for_bin(
                instance,
                bin_type_id,
                parameters);
    }
    return result;
}
