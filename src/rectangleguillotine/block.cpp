#include "rectangleguillotine/block.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <algorithm>
#include <ostream>
#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

struct BlockKey
{
    Length width;
    Length height;
    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;
};

struct BlockKeyHasher
{
    std::size_t operator()(const BlockKey& key) const
    {
        std::size_t hash = 0;
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.width));
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.height));
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
        return block_key_1.width == block_key_2.width
            && block_key_1.height == block_key_2.height
            && block_key_1.item_copies == block_key_2.item_copies;
    }
};

using BlockSet = std::unordered_set<BlockKey, BlockKeyHasher, BlockKeyEqual>;

BlockKey make_key(const Block& block)
{
    return {block.rect.w, block.rect.h, block.item_copies};
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
    const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies_2)
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
        if (block_1.number_of_items == 1) {
            if (block_2.number_of_items != 1)
                return false;
        } else if (block_2.number_of_items == 1) {
            return true;
        }
        if (block_1.efficiency() != block_2.efficiency())
            return block_1.efficiency() < block_2.efficiency();
        return block_1.number_of_items > block_2.number_of_items;
    }
};

std::vector<Block> compute_blocks_for_bin(
        const Instance& instance,
        BinTypeId bin_type_id,
        const BlockParameters& parameters)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Length bin_w = bin_type.rect.w;
    Length bin_h = bin_type.rect.h;
    Length cut_thickness = instance.parameters().cut_thickness;

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
            Length bw = item_type.width(rotate);
            Length bh = item_type.height(rotate);
            ItemPos copies_max_x = (std::min)(
                    item_type.copies,
                    (ItemPos)((bin_w + cut_thickness) / (bw + cut_thickness)));
            for (ItemPos cx = 1; cx <= copies_max_x; ++cx) {
                ItemPos copies_max_y = (std::min)(
                        item_type.copies / cx,
                        (ItemPos)((bin_h + cut_thickness) / (bh + cut_thickness)));
                for (ItemPos cy = 1; cy <= copies_max_y; ++cy) {
                    Block block;
                    block.is_simple = true;
                    block.item_type_id = item_type_id;
                    block.rotate = rotate;
                    block.rect = {
                        cx * bw + (cx - 1) * cut_thickness,
                        cy * bh + (cy - 1) * cut_thickness};
                    block.item_area = cx * cy * item_type.area();
                    block.item_profit = (double)(cx * cy) * item_type.profit;
                    block.item_copies = {{item_type_id, cx * cy}};
                    block.number_of_items = cx * cy;
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
            continue;
        }

        // Index that block will occupy once pushed to returned_blocks.
        ItemPos current_block_id = (ItemPos)returned_blocks.size();

        for (ItemPos existing_idx = 0;
                existing_idx < (ItemPos)returned_blocks.size();
                ++existing_idx) {
            const Block& existing_block = returned_blocks[existing_idx];
            if (block.is_simple && existing_block.is_simple
                    && block.item_type_id == existing_block.item_type_id
                    && block.rotate == existing_block.rotate)
                continue;
            // direction 0 = X (vertical cut), direction 1 = Y (horizontal cut)
            for (int direction = 0; direction <= 1; ++direction) {
                Block combined;
                combined.item_area = block.item_area + existing_block.item_area;
                combined.item_profit = block.item_profit + existing_block.item_profit;
                if (direction == 0) {
                    combined.rect.w = block.rect.w + existing_block.rect.w + cut_thickness;
                    combined.rect.h = std::max(block.rect.h, existing_block.rect.h);
                } else {
                    combined.rect.w = std::max(block.rect.w, existing_block.rect.w);
                    combined.rect.h = block.rect.h + existing_block.rect.h + cut_thickness;
                }
                if (combined.rect.w > bin_w || combined.rect.h > bin_h)
                    continue;

                combined.number_of_items = block.number_of_items + existing_block.number_of_items;
                if (parameters.maximum_number_of_blocks != -1
                        && (ItemPos)blocks_to_process.size() >= parameters.maximum_number_of_blocks) {
                    if (!BlockFillRateLess{}(*blocks_to_process.begin(), combined))
                        continue;
                }

                if (!is_valid_item_copies(block.item_copies, existing_block.item_copies, instance))
                    continue;

                BlockKey key;
                key.width = combined.rect.w;
                key.height = combined.rect.h;
                key.item_copies = merge_item_copies(block.item_copies, existing_block.item_copies);
                if (seen.count(key))
                    continue;
                combined.item_copies = key.item_copies;
                combined.cut_is_vertical = (direction == 0);
                combined.child_1_id = current_block_id;
                combined.child_2_id = existing_idx;
                seen.insert(key);
                enqueue(std::move(combined));
            }
        }

        returned_blocks.push_back(std::move(block));
    }

    return returned_blocks;
}

} // anonymous namespace

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const Block& block)
{
    os << "Block rect=(" << block.rect.w << "x" << block.rect.h << ")"
       << " items=" << block.number_of_items;
    return os;
}

std::vector<std::vector<Block>> packingsolver::rectangleguillotine::compute_blocks(
        const Instance& instance,
        const BlockParameters& parameters)
{
    std::vector<std::vector<Block>> result(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        result[bin_type_id] = compute_blocks_for_bin(instance, bin_type_id, parameters);
    }
    return result;
}
