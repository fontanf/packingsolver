#include "rectangle/block.hpp"

#include "rectangle/instance_cache.hpp"

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

/**
 * Append the simple blocks of item type 'item_type_id' for bin type
 * 'bin_type_id' to 'blocks'; see 'compute_simple_blocks'.
 */
void append_simple_blocks(
        const Instance& instance,
        BinTypeId bin_type_id,
        ItemTypeId item_type_id,
        std::vector<Block>& blocks)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Rectangle bin_rect = bin_type.rect;
    const ItemType& item_type = instance.item_type(item_type_id);
    if (item_type.copies < 1)
        return;
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

                if (block.weight > bin_type.maximum_weight)
                    continue;
                block.resource_consumption = compute_resource_consumption(
                        instance, bin_type_id, block.item_copies);
                if (!resource_feasible(instance, bin_type_id, {}, block.resource_consumption))
                    continue;

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
                blocks.push_back(std::move(block));
            }
        }
    }
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
    Rectangle bin_rect = bin_type.rect;

    std::multiset<Block, BlockFillRateLess> blocks_to_process;
    std::vector<Block> returned_blocks;
    BlockSet seen;
    // Ids of the returned blocks sorted by length along x and along y.
    std::vector<ItemPos> returned_block_ids_by_x;
    std::vector<ItemPos> returned_block_ids_by_y;
    // For each returned block, bit 0 (resp. 1) is set if it is combined with
    // the current block along x (resp. y).
    std::vector<char> candidate_directions;
    std::vector<ItemPos> candidate_block_ids;
    auto block_x_less = [&returned_blocks](Length x, ItemPos block_id) {
        return x < returned_blocks[block_id].rect.x;
    };
    auto block_y_less = [&returned_blocks](Length y, ItemPos block_id) {
        return y < returned_blocks[block_id].rect.y;
    };

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
        std::vector<Block> simple_blocks;
        append_simple_blocks(
                instance,
                bin_type_id,
                item_type_id,
                simple_blocks);
        for (Block& block: simple_blocks) {
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

        // Combined blocks must fit in the bin: along x (resp. y), only the
        // returned blocks in [returned_block_ids_by_x.begin(), x_end) (resp.
        // [returned_block_ids_by_y.begin(), y_end)) fit next to the block.
        auto x_end = std::upper_bound(
                returned_block_ids_by_x.begin(),
                returned_block_ids_by_x.end(),
                bin_rect.x - block.rect.x,
                block_x_less);
        auto y_end = std::upper_bound(
                returned_block_ids_by_y.begin(),
                returned_block_ids_by_y.end(),
                bin_rect.y - block.rect.y,
                block_y_less);
        // If many of the returned blocks fit, considering all of them is
        // faster; otherwise, only consider the ones which fit, in the order
        // of their ids.
        bool use_candidates = (
                2 * ((x_end - returned_block_ids_by_x.begin())
                    + (y_end - returned_block_ids_by_y.begin()))
                < (ItemPos)returned_blocks.size());
        candidate_block_ids.clear();
        if (use_candidates) {
            for (auto it = returned_block_ids_by_x.begin(); it != x_end; ++it) {
                candidate_directions[*it] |= 1;
                candidate_block_ids.push_back(*it);
            }
            for (auto it = returned_block_ids_by_y.begin(); it != y_end; ++it) {
                if (candidate_directions[*it] == 0)
                    candidate_block_ids.push_back(*it);
                candidate_directions[*it] |= 2;
            }
            std::sort(candidate_block_ids.begin(), candidate_block_ids.end());
        }

        ItemPos number_of_considered_blocks = (use_candidates)?
            (ItemPos)candidate_block_ids.size():
            (ItemPos)returned_blocks.size();
        for (ItemPos considered_block_pos = 0;
                considered_block_pos < number_of_considered_blocks;
                ++considered_block_pos) {
            ItemPos existing_block_id = (use_candidates)?
                candidate_block_ids[considered_block_pos]:
                considered_block_pos;
            const Block& existing_block = returned_blocks[existing_block_id];
            char existing_block_directions = 3;
            if (use_candidates) {
                existing_block_directions = candidate_directions[existing_block_id];
                candidate_directions[existing_block_id] = 0;
            }
            if (block.is_simple && existing_block.is_simple
                    && block.item_type_id == existing_block.item_type_id
                    && block.rotate == existing_block.rotate)
                continue;
            for (Direction direction: {Direction::X, Direction::Y}) {
                if (!(existing_block_directions & ((direction == Direction::X)? 1: 2)))
                    continue;
                Weight combined_weight = block.weight + existing_block.weight;
                if (combined_weight > bin_type.maximum_weight)
                    continue;
                Rectangle combined_rect = {0, 0};
                switch (direction) {
                case Direction::X:
                    combined_rect.x = block.rect.x + existing_block.rect.x;
                    combined_rect.y = std::max(block.rect.y, existing_block.rect.y);
                    break;
                case Direction::Y:
                    combined_rect.x = std::max(block.rect.x, existing_block.rect.x);
                    combined_rect.y = block.rect.y + existing_block.rect.y;
                    break;
                default: break;
                }
                if (combined_rect.x > bin_rect.x
                        || combined_rect.y > bin_rect.y)
                    continue;
                Profit combined_profit = block.item_profit + existing_block.item_profit;
                ItemPos combined_number_of_items = block.number_of_items + existing_block.number_of_items;

                // If the queue is full, the combined block must be better
                // than the worst block of the queue (see 'BlockFillRateLess').
                if (parameters.maximum_number_of_blocks != -1
                        && (ItemPos)blocks_to_process.size() >= parameters.maximum_number_of_blocks) {
                    const Block& worst_block = *blocks_to_process.begin();
                    if (worst_block.number_of_items == 1)
                        continue;
                    double combined_efficiency = (double)combined_profit / combined_rect.area();
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
                key.rect = combined_rect;
                key.item_copies = merge_item_copies(
                        block.item_copies,
                        existing_block.item_copies,
                        instance);
                if (seen.count(key))
                    continue;

                Block combined;
                combined.rect = combined_rect;
                combined.item_area = block.item_area + existing_block.item_area;
                combined.item_profit = combined_profit;
                combined.weight = combined_weight;
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

                combined.items = block.items;
                combined.items.reserve(block.items.size() + existing_block.items.size());
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

        ItemPos block_id = (ItemPos)returned_blocks.size();
        returned_blocks.push_back(std::move(block));
        candidate_directions.push_back(0);
        returned_block_ids_by_x.insert(
                std::upper_bound(
                    returned_block_ids_by_x.begin(),
                    returned_block_ids_by_x.end(),
                    returned_blocks.back().rect.x,
                    block_x_less),
                block_id);
        returned_block_ids_by_y.insert(
                std::upper_bound(
                    returned_block_ids_by_y.begin(),
                    returned_block_ids_by_y.end(),
                    returned_blocks.back().rect.y,
                    block_y_less),
                block_id);
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

std::vector<Block> packingsolver::rectangle::compute_simple_blocks(
        const Instance& instance,
        BinTypeId bin_type_id,
        ItemTypeId item_type_id)
{
    std::vector<Block> blocks;
    append_simple_blocks(
            instance,
            bin_type_id,
            item_type_id,
            blocks);
    return blocks;
}

std::vector<Block> packingsolver::rectangle::copy_blocks(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<Block>& original_blocks,
        const std::vector<ItemTypeId>& orig_to_sub_item_type_ids,
        const std::vector<bool>& new_item_types)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    // With at most 3 item types, 'compute_blocks' only keeps the blocks
    // without waste.
    bool only_full_blocks = (instance.number_of_item_types() <= 3);

    std::vector<Block> blocks;
    for (const Block& original_block: original_blocks) {
        if (original_block.rect.x > bin_type.rect.x
                || original_block.rect.y > bin_type.rect.y) {
            continue;
        }
        if (original_block.weight > bin_type.maximum_weight)
            continue;
        if (only_full_blocks && original_block.fill_rate() != 1.0)
            continue;
        Block block = original_block;
        bool available = true;
        for (std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies) {
            item_copy.first = (item_copy.first < (ItemTypeId)orig_to_sub_item_type_ids.size())?
                orig_to_sub_item_type_ids[item_copy.first]:
                -1;
            if (item_copy.first == -1
                    || item_copy.second > instance.item_type(item_copy.first).copies) {
                available = false;
                break;
            }
        }
        if (!available)
            continue;
        std::sort(block.item_copies.begin(), block.item_copies.end());
        block.resource_consumption = compute_resource_consumption(
                instance, bin_type_id, block.item_copies);
        if (!resource_feasible(instance, bin_type_id, {}, block.resource_consumption))
            continue;
        block.item_profit = 0;
        for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies)
            block.item_profit += item_copy.second * instance.item_type(item_copy.first).profit;
        if (block.is_simple)
            block.item_type_id = block.item_copies.front().first;
        for (SolutionItem& item: block.items)
            item.item_type_id = orig_to_sub_item_type_ids[item.item_type_id];
        blocks.push_back(std::move(block));
    }

    // Simple blocks of the new item types.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (!new_item_types[item_type_id])
            continue;
        std::vector<Block> simple_blocks;
        append_simple_blocks(
                instance,
                bin_type_id,
                item_type_id,
                simple_blocks);
        for (Block& block: simple_blocks) {
            if (only_full_blocks && block.fill_rate() != 1.0)
                continue;
            blocks.push_back(std::move(block));
        }
    }
    return blocks;
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

std::vector<std::vector<Block>> packingsolver::rectangle::compute_blocks(
    const Instance& instance,
    const BlockParameters& parameters,
    const bool* end)
{
    std::vector<std::vector<Block>> result(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        result[bin_type_id] = compute_blocks_for_bin(
                instance,
                bin_type_id,
                parameters,
                end);
    }
    return result;
}
