#include "rectangleguillotine/block.hpp"

#include "rectangleguillotine/instance_cache.hpp"

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
        const BlockParameters& parameters,
        const bool* end)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Length bin_w = bin_type.rect.w;
    Length bin_h = bin_type.rect.h;
    Length cut_thickness = instance.parameters().cut_thickness;

    std::multiset<Block, BlockFillRateLess> blocks_to_process;
    std::vector<Block> returned_blocks;
    BlockSet seen;
    // Ids of the returned blocks sorted by width and by height.
    std::vector<ItemPos> returned_block_ids_by_width;
    std::vector<ItemPos> returned_block_ids_by_height;
    // For each returned block, bit 0 (resp. 1) is set if it is combined with
    // the current block in direction 0 (resp. 1).
    std::vector<char> candidate_directions;
    std::vector<ItemPos> candidate_block_ids;

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
        for (Block& block: compute_simple_blocks(instance, bin_type_id, item_type_id)) {
            BlockKey key = make_key(block);
            if (!seen.count(key)) {
                seen.insert(key);
                enqueue(std::move(block));
            }
        }
    }

    while (!blocks_to_process.empty()) {
        if (parameters.maximum_number_of_blocks != -1
                && (ItemPos)returned_blocks.size() >= parameters.maximum_number_of_blocks)
            break;
        if (end != nullptr && *end)
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

        // Most of the returned blocks don't fit next to the block in the bin.
        // Only consider the ones which fit, in the order of their ids.
        candidate_block_ids.clear();
        auto width_end = std::upper_bound(
                returned_block_ids_by_width.begin(),
                returned_block_ids_by_width.end(),
                bin_w - block.rect.w - cut_thickness,
                [&returned_blocks](Length width, ItemPos block_id) {
                    return width < returned_blocks[block_id].rect.w;
                });
        for (auto it = returned_block_ids_by_width.begin(); it != width_end; ++it) {
            candidate_directions[*it] |= 1;
            candidate_block_ids.push_back(*it);
        }
        auto height_end = std::upper_bound(
                returned_block_ids_by_height.begin(),
                returned_block_ids_by_height.end(),
                bin_h - block.rect.h - cut_thickness,
                [&returned_blocks](Length height, ItemPos block_id) {
                    return height < returned_blocks[block_id].rect.h;
                });
        for (auto it = returned_block_ids_by_height.begin(); it != height_end; ++it) {
            if (candidate_directions[*it] == 0)
                candidate_block_ids.push_back(*it);
            candidate_directions[*it] |= 2;
        }
        if (8 * candidate_block_ids.size() > returned_blocks.size()) {
            // Many candidates: retrieving them in order from
            // 'candidate_directions' is faster than sorting them.
            candidate_block_ids.clear();
            for (ItemPos block_id = 0;
                    block_id < (ItemPos)returned_blocks.size();
                    ++block_id) {
                if (candidate_directions[block_id] != 0)
                    candidate_block_ids.push_back(block_id);
            }
        } else {
            std::sort(candidate_block_ids.begin(), candidate_block_ids.end());
        }

        for (ItemPos existing_idx: candidate_block_ids) {
            const Block& existing_block = returned_blocks[existing_idx];
            char existing_directions = candidate_directions[existing_idx];
            candidate_directions[existing_idx] = 0;
            if (block.is_simple && existing_block.is_simple
                    && block.item_type_id == existing_block.item_type_id
                    && block.rotate == existing_block.rotate)
                continue;
            // direction 0 = X (vertical cut), direction 1 = Y (horizontal cut)
            for (int direction = 0; direction <= 1; ++direction) {
                if (!(existing_directions & (1 << direction)))
                    continue;
                Length combined_width = 0;
                Length combined_height = 0;
                if (direction == 0) {
                    combined_width = block.rect.w + existing_block.rect.w + cut_thickness;
                    combined_height = std::max(block.rect.h, existing_block.rect.h);
                } else {
                    combined_width = std::max(block.rect.w, existing_block.rect.w);
                    combined_height = block.rect.h + existing_block.rect.h + cut_thickness;
                }
                Profit combined_profit = block.item_profit + existing_block.item_profit;
                ItemPos combined_number_of_items = block.number_of_items + existing_block.number_of_items;

                // If the queue is full, the combined block must be better
                // than the worst block of the queue (see 'BlockFillRateLess').
                if (parameters.maximum_number_of_blocks != -1
                        && (ItemPos)blocks_to_process.size() >= parameters.maximum_number_of_blocks) {
                    const Block& worst_block = *blocks_to_process.begin();
                    if (worst_block.number_of_items == 1)
                        continue;
                    double combined_efficiency = (double)combined_profit
                        / ((Area)combined_width * combined_height);
                    double worst_efficiency = worst_block.efficiency();
                    if (combined_efficiency < worst_efficiency)
                        continue;
                    if (combined_efficiency == worst_efficiency
                            && worst_block.number_of_items <= combined_number_of_items)
                        continue;
                }

                if (!is_valid_item_copies(block.item_copies, existing_block.item_copies, instance))
                    continue;

                BlockKey key;
                key.width = combined_width;
                key.height = combined_height;
                key.item_copies = merge_item_copies(block.item_copies, existing_block.item_copies);
                if (seen.count(key))
                    continue;
                seen.insert(key);
                Block combined;
                combined.rect.w = combined_width;
                combined.rect.h = combined_height;
                combined.item_area = block.item_area + existing_block.item_area;
                combined.item_profit = combined_profit;
                combined.number_of_items = combined_number_of_items;
                combined.item_copies = key.item_copies;
                if (combined.item_copies.size()
                        == block.item_copies.size() + existing_block.item_copies.size()) {
                    // The blocks don't share any item type: the consumption
                    // of the combined block is the sum of their consumptions.
                    combined.resource_consumption = block.resource_consumption;
                    for (ResourceId resource_id = 0;
                            resource_id < (ResourceId)combined.resource_consumption.size();
                            ++resource_id) {
                        combined.resource_consumption[resource_id]
                            += existing_block.resource_consumption[resource_id];
                    }
                } else {
                    combined.resource_consumption = compute_resource_consumption(
                            instance, bin_type_id, combined.item_copies);
                }
                if (!resource_feasible(instance, bin_type_id, {}, combined.resource_consumption))
                    continue;
                combined.cut_is_vertical = (direction == 0);
                combined.child_1_id = current_block_id;
                combined.child_2_id = existing_idx;
                enqueue(std::move(combined));
            }
        }

        returned_blocks.push_back(std::move(block));
        candidate_directions.push_back(0);
        returned_block_ids_by_width.insert(
                std::upper_bound(
                    returned_block_ids_by_width.begin(),
                    returned_block_ids_by_width.end(),
                    returned_blocks.back().rect.w,
                    [&returned_blocks](Length width, ItemPos block_id) {
                        return width < returned_blocks[block_id].rect.w;
                    }),
                current_block_id);
        returned_block_ids_by_height.insert(
                std::upper_bound(
                    returned_block_ids_by_height.begin(),
                    returned_block_ids_by_height.end(),
                    returned_blocks.back().rect.h,
                    [&returned_blocks](Length height, ItemPos block_id) {
                        return height < returned_blocks[block_id].rect.h;
                    }),
                current_block_id);
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

std::vector<Block> packingsolver::rectangleguillotine::compute_simple_blocks(
        const Instance& instance,
        BinTypeId bin_type_id,
        ItemTypeId item_type_id)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Length bin_w = bin_type.rect.w;
    Length bin_h = bin_type.rect.h;
    Length cut_thickness = instance.parameters().cut_thickness;
    const ItemType& item_type = instance.item_type(item_type_id);

    std::vector<Block> blocks;
    if (item_type.copies < 1)
        return blocks;
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
                block.resource_consumption = compute_resource_consumption(
                        instance, bin_type_id, block.item_copies);
                if (!resource_feasible(instance, bin_type_id, {}, block.resource_consumption))
                    continue;
                blocks.push_back(std::move(block));
            }
        }
    }
    return blocks;
}

std::vector<std::vector<Block>> packingsolver::rectangleguillotine::compute_blocks(
        const Instance& instance,
        const BlockParameters& parameters,
        const bool* end)
{
    std::vector<std::vector<Block>> result(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        result[bin_type_id] = compute_blocks_for_bin(instance, bin_type_id, parameters, end);
    }
    return result;
}

const std::vector<std::vector<Block>>* Instance::get_blocks(
        const bool& end) const
{
    if (blocks_cache_->computed.load(std::memory_order_acquire))
        return &blocks_cache_->blocks;
    if (end)
        return nullptr;
    // Only one thread computes the blocks; the other ones wait for it.
    std::lock_guard<std::mutex> lock(blocks_cache_->mutex);
    if (blocks_cache_->computed.load(std::memory_order_acquire))
        return &blocks_cache_->blocks;
    std::vector<std::vector<Block>> blocks = compute_blocks(*this, BlockParameters(), &end);
    if (end)
        return nullptr;
    blocks_cache_->blocks = std::move(blocks);
    blocks_cache_->computed.store(true, std::memory_order_release);
    return &blocks_cache_->blocks;
}
