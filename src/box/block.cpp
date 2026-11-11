#include "box/block.hpp"

#include "box/instance_cache.hpp"
#include "packingsolver/algorithms/resource.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <algorithm>
#include <ostream>
#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::box;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Internals ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::box::operator<<(std::ostream& os, const Block& block)
{
    os << "x " << block.box.x
       << " y " << block.box.y
       << " z " << block.box.z
       << " item_volume " << block.item_volume
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
    Box box;
    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;
};

struct BlockKeyHasher
{
    std::size_t operator()(const BlockKey& key) const
    {
        std::size_t hash = 0;
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.box.x));
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.box.y));
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.box.z));
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
            const BlockKey& block_1,
            const BlockKey& block_2) const
    {
        return block_1.box == block_2.box
            && block_1.item_copies == block_2.item_copies;
    }
};

using BlockSet = std::unordered_set<BlockKey, BlockKeyHasher, BlockKeyEqual>;

BlockKey make_key(const Block& block)
{
    return {block.box, block.item_copies};
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

/**
 * Merge two sorted sparse item_copies vectors and check quantity constraints.
 *
 * Returns {true, merged} if all merged counts are within instance limits,
 * {false, {}} otherwise.
 */
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


bool block_item_volume_greater(const Block& block_1, const Block& block_2)
{
    return block_1.item_volume > block_2.item_volume;
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
    Box bin_box = bin_type.box;


    // Sorted ascending by fill_rate: begin() is worst, prev(end()) is best.
    // This allows O(log n) trimming of the worst block when the set exceeds
    // maximum_number_of_blocks (those tail blocks will never be popped anyway).
    std::multiset<Block, BlockFillRateLess> blocks_to_process;
    std::vector<Block> returned_blocks;
    BlockSet seen;

    // Helper: insert a block into the multiset and trim the worst if over capacity.
    auto enqueue = [&](Block block) {
        blocks_to_process.insert(std::move(block));
        if (parameters.maximum_number_of_blocks != -1
                && (ItemPos)blocks_to_process.size() > parameters.maximum_number_of_blocks) {
            BlockKey key = make_key(*blocks_to_process.begin());
            seen.erase(key);
            blocks_to_process.erase(blocks_to_process.begin());
        }
    };

    // Generate singular blocks and enqueue.
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

    // Process blocks in decreasing fill_rate order.  Each popped block is added
    // to returned_blocks and combined with every already-returned block to
    // generate candidate combined blocks for the queue.
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

        //std::cout << returned_blocks.size()
        //    << " " << block.box
        //    << " # items " << block.number_of_items
        //    << " frate " << block.fill_rate()
        //    << " \% bin " << (double)block.box.volume() / bin_type.box.volume()
        //    << " " << blocks_to_process.size()
        //    << std::endl;

        // Combine with every already-returned block in all three directions.
        for (const Block& existing_block: returned_blocks) {
            if (block.is_simple && existing_block.is_simple
                    && block.item_type_id == existing_block.item_type_id
                    && block.rotation == existing_block.rotation)
                continue;
            for (Direction direction: {Direction::X, Direction::Y, Direction::Z}) {
                Block combined;
                combined.item_volume = block.item_volume + existing_block.item_volume;
                combined.item_profit = block.item_profit + existing_block.item_profit;
                combined.weight = block.weight + existing_block.weight;
                switch (direction) {
                case Direction::X:
                    combined.box.x = block.box.x + existing_block.box.x;
                    combined.box.y = std::max(block.box.y, existing_block.box.y);
                    combined.box.z = std::max(block.box.z, existing_block.box.z);
                    break;
                case Direction::Y:
                    combined.box.x = std::max(block.box.x, existing_block.box.x);
                    combined.box.y = block.box.y + existing_block.box.y;
                    combined.box.z = std::max(block.box.z, existing_block.box.z);
                    break;
                case Direction::Z:
                    combined.box.x = std::max(block.box.x, existing_block.box.x);
                    combined.box.y = std::max(block.box.y, existing_block.box.y);
                    combined.box.z = block.box.z + existing_block.box.z;
                    break;
                default: break;
                }
                if (combined.box.x > bin_box.x
                        || combined.box.y > bin_box.y
                        || combined.box.z > bin_box.z)
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
                key.box = combined.box;
                key.item_copies = merge_item_copies(
                        block.item_copies,
                        existing_block.item_copies,
                        instance);
                if (seen.count(key))
                    continue;
                combined.item_copies = key.item_copies;
                combined.items = block.items;
                for (const SolutionItem& item: existing_block.items) {
                    SolutionItem shifted = item;
                    switch (direction) {
                    case Direction::X: shifted.bl_corner.x += block.box.x; break;
                    case Direction::Y: shifted.bl_corner.y += block.box.y; break;
                    case Direction::Z: shifted.bl_corner.z += block.box.z; break;
                    default: break;
                    }
                    combined.items.push_back(shifted);
                }
                seen.insert(key);
                combined.resource_consumption = compute_block_resource_consumption(
                        instance, bin_type_id, combined.item_copies);
                if (!block_resource_feasible(instance, bin_type_id, combined))
                    continue;
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

MaxReachableLengths packingsolver::box::compute_max_reachable_lengths(
    const Instance& instance)
{
    Length max_x = 0;
    Length max_y = 0;
    Length max_z = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        max_x = std::max(max_x, bin_type.box.x);
        max_y = std::max(max_y, bin_type.box.y);
        max_z = std::max(max_z, bin_type.box.z);
    }

    std::vector<uint8_t> reachable_x(max_x + 1, false);
    std::vector<uint8_t> reachable_y(max_y + 1, false);
    std::vector<uint8_t> reachable_z(max_z + 1, false);
    std::vector<uint8_t> temp_x(max_x + 1, false);
    std::vector<uint8_t> temp_y(max_y + 1, false);
    std::vector<uint8_t> temp_z(max_z + 1, false);
    reachable_x[0] = reachable_y[0] = reachable_z[0] = true;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy_idx = 0; copy_idx < item_type.copies; ++copy_idx) {
            temp_x = reachable_x;
            temp_y = reachable_y;
            temp_z = reachable_z;
            for (Rotation rotation: item_type.rotations) {
                Box box = item_type.box.rotate(rotation);
                if (box.x <= max_x)
                    for (Length v = max_x; v >= box.x; --v)
                        if (reachable_x[v - box.x]) temp_x[v] = true;
                if (box.y <= max_y)
                    for (Length v = max_y; v >= box.y; --v)
                        if (reachable_y[v - box.y]) temp_y[v] = true;
                if (box.z <= max_z)
                    for (Length v = max_z; v >= box.z; --v)
                        if (reachable_z[v - box.z]) temp_z[v] = true;
            }
            reachable_x = temp_x;
            reachable_y = temp_y;
            reachable_z = temp_z;
        }
    }

    MaxReachableLengths result;
    result.x.resize(max_x + 1, 0);
    result.y.resize(max_y + 1, 0);
    result.z.resize(max_z + 1, 0);
    for (Length v = 1; v <= max_x; ++v)
        result.x[v] = reachable_x[v] ? v : result.x[v - 1];
    for (Length v = 1; v <= max_y; ++v)
        result.y[v] = reachable_y[v] ? v : result.y[v - 1];
    for (Length v = 1; v <= max_z; ++v)
        result.z[v] = reachable_z[v] ? v : result.z[v - 1];
    return result;
}

std::vector<double> packingsolver::box::compute_block_resource_consumption(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& item_copies)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    std::vector<double> resource_consumption(bin_type.number_of_resources(), 0.0);
    for (const std::pair<ItemTypeId, ItemPos>& item_copy: item_copies) {
        const ItemType& item_type = instance.item_type(item_copy.first);
        for (const ItemResourceConsumption& consumption: item_type.resources[bin_type_id]) {
            const std::vector<double>& schedule = bin_type.resource(consumption.resource_id)
                .item_consumptions[consumption.consumption_pos].second;
            for (ItemPos copy = 0; copy < item_copy.second; ++copy)
                resource_consumption[consumption.resource_id] += schedule_consumption(schedule, copy);
        }
    }
    return resource_consumption;
}

bool packingsolver::box::block_resource_feasible(
        const Instance& instance,
        BinTypeId bin_type_id,
        const Block& block)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        if (!resource.penalize
                && block.resource_consumption[resource_id] > resource.capacity) {
            return false;
        }
    }
    return true;
}

std::vector<Block> packingsolver::box::compute_simple_blocks(
        const Instance& instance,
        BinTypeId bin_type_id,
        ItemTypeId item_type_id)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Box bin_box = bin_type.box;
    const ItemType& item_type = instance.item_type(item_type_id);

    std::vector<Block> blocks;
    if (item_type.copies < 1)
        return blocks;
    for (Rotation rotation: item_type.rotations) {
        Box rotated_box = item_type.box.rotate(rotation);
        ItemPos copies_max_x = (std::min)(
                item_type.copies,
                (ItemPos)(bin_box.x / rotated_box.x));
        for (ItemPos cx = 1; cx <= copies_max_x; ++cx) {
            ItemPos copies_max_y = (std::min)(
                    item_type.copies / cx,
                    (ItemPos)(bin_box.y / rotated_box.y));
            for (ItemPos cy = 1; cy <= copies_max_y; ++cy) {
                ItemPos copies_max_z = (std::min)(
                        item_type.copies / (cx * cy),
                        (ItemPos)(bin_box.z / rotated_box.z));
                for (ItemPos cz = 1; cz <= copies_max_z; ++cz) {
                    Block block;
                    block.is_simple = true;
                    block.item_type_id = item_type_id;
                    block.rotation = rotation;
                    block.box = {cx * rotated_box.x, cy * rotated_box.y, cz * rotated_box.z};
                    block.item_volume = cx * cy * cz * item_type.volume();
                    block.item_profit = (double)(cx * cy * cz) * item_type.profit;
                    block.weight = cx * cy * cz * item_type.weight;
                    block.item_copies = {{item_type_id, cx * cy * cz}};
                    block.number_of_items = cx * cy * cz;
                    block.resource_consumption = compute_block_resource_consumption(
                            instance, bin_type_id, block.item_copies);
                    if (!block_resource_feasible(instance, bin_type_id, block))
                        continue;
                    block.items.reserve(cx * cy * cz);
                    for (ItemPos ccx = 0; ccx < cx; ++ccx) {
                        for (ItemPos ccy = 0; ccy < cy; ++ccy) {
                            for (ItemPos ccz = 0; ccz < cz; ++ccz) {
                                SolutionItem solution_item;
                                solution_item.item_type_id = item_type_id;
                                solution_item.rotation = rotation;
                                solution_item.bl_corner = {
                                    ccx * rotated_box.x,
                                    ccy * rotated_box.y,
                                    ccz * rotated_box.z};
                                block.items.push_back(solution_item);
                            }
                        }
                    }
                    blocks.push_back(std::move(block));
                }
            }
        }
    }
    return blocks;
}

std::vector<Block> packingsolver::box::copy_blocks(
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
        if (original_block.box.x > bin_type.box.x
                || original_block.box.y > bin_type.box.y
                || original_block.box.z > bin_type.box.z) {
            continue;
        }
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
        block.item_profit = 0;
        for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies)
            block.item_profit += item_copy.second * instance.item_type(item_copy.first).profit;
        // The resources of the bin types may differ from the original ones.
        block.resource_consumption = compute_block_resource_consumption(
                instance, bin_type_id, block.item_copies);
        if (!block_resource_feasible(instance, bin_type_id, block))
            continue;
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
        for (Block& block: compute_simple_blocks(instance, bin_type_id, item_type_id)) {
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

std::vector<std::vector<Block>> packingsolver::box::compute_blocks(
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
