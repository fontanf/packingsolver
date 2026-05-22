#include "box/block.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <algorithm>
#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::box;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Internals ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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
    bool operator()(const BlockKey& a, const BlockKey& b) const
    {
        return a.box == b.box && a.item_copies == b.item_copies;
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

/** Generate blocks for a single bin type.
 *
 * Implements Algorithm 3.5 (GENERATE-GUILLOTINE-BLOCKS):
 *   1. Generate singular blocks (one item in one rotation).
 *   2. Iteratively combine the singular set P with all existing blocks B in
 *      all three directions until N_B blocks are reached or no new block is
 *      found.
 */
std::vector<Block> compute_blocks_for_bin(
    const Instance& instance,
    BinTypeId bin_type_id,
    const BlockParameters& parameters)
{
    double minimum_fill_rate = (instance.number_of_item_types() < 30)? 1.00: 0.98;
    const BinType& bin_type = instance.bin_type(bin_type_id);

    std::vector<Block> blocks;
    BlockSet seen;

    // Algorithm 3.4: generate singular blocks
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies < 1)
            continue;
        for (Rotation rotation: item_type.rotations) {
            Block block;
            block.box = item_type.box.rotate(rotation);
            block.item_volume = item_type.volume();
            block.number_of_items = 1;
            block.item_copies = {{item_type_id, 1}};
            SolutionItem solution_item;
            solution_item.item_type_id = item_type_id;
            solution_item.rotation = rotation;
            solution_item.bl_corner = {0, 0, 0};
            block.items.push_back(solution_item);
            if (block.box.x > bin_type.box.x
                    || block.box.y > bin_type.box.y
                    || block.box.z > bin_type.box.z)
                continue;
            BlockKey key = make_key(block);
            if (!seen.insert(key).second)
                continue;
            blocks.push_back(std::move(block));
        }
    }

    std::sort(blocks.begin(), blocks.end(), block_item_volume_greater);

    // Algorithm 3.5: iterative combination
    // P starts as singular blocks; after each iteration P ← N (newly generated blocks)
    std::vector<Block> previous_blocks(blocks.begin(), blocks.end());

    while ((ItemPos)blocks.size() < parameters.maximum_number_of_blocks) {
        std::vector<Block> new_blocks;
        bool limit_reached = false;

        for (const Block& previous_block: previous_blocks) {
            if (limit_reached)
                break;
            for (const Block& existing_block: blocks) {
                if (limit_reached)
                    break;
                for (Direction direction: {Direction::X, Direction::Y, Direction::Z}) {
                    Block combined;
                    switch (direction) {
                    case Direction::X:
                        combined.box.x = previous_block.box.x + existing_block.box.x;
                        combined.box.y = std::max(previous_block.box.y, existing_block.box.y);
                        combined.box.z = std::max(previous_block.box.z, existing_block.box.z);
                        break;
                    case Direction::Y:
                        combined.box.x = std::max(previous_block.box.x, existing_block.box.x);
                        combined.box.y = previous_block.box.y + existing_block.box.y;
                        combined.box.z = std::max(previous_block.box.z, existing_block.box.z);
                        break;
                    case Direction::Z:
                        combined.box.x = std::max(previous_block.box.x, existing_block.box.x);
                        combined.box.y = std::max(previous_block.box.y, existing_block.box.y);
                        combined.box.z = previous_block.box.z + existing_block.box.z;
                        break;
                    default: break;
                    }
                    if (combined.box.x > bin_type.box.x
                            || combined.box.y > bin_type.box.y
                            || combined.box.z > bin_type.box.z) {
                        continue;
                    }
                    combined.item_volume = previous_block.item_volume + existing_block.item_volume;
                    if (combined.fill_rate() < minimum_fill_rate)
                        continue;

                    if (!is_valid_item_copies(previous_block.item_copies, existing_block.item_copies, instance))
                        continue;

                    BlockKey key = make_key(combined);
                    key.box = combined.box;
                    key.item_copies = merge_item_copies(
                            previous_block.item_copies,
                            existing_block.item_copies,
                            instance);
                    if (seen.count(key))
                        continue;
                    combined.number_of_items = previous_block.number_of_items + existing_block.number_of_items;
                    combined.item_copies = key.item_copies;
                    combined.items = previous_block.items;
                    for (const SolutionItem& item: existing_block.items) {
                        SolutionItem shifted = item;
                        switch (direction) {
                        case Direction::X: shifted.bl_corner.x += previous_block.box.x; break;
                        case Direction::Y: shifted.bl_corner.y += previous_block.box.y; break;
                        case Direction::Z: shifted.bl_corner.z += previous_block.box.z; break;
                        default: break;
                        }
                        combined.items.push_back(shifted);
                    }
                    seen.insert(key);

                    new_blocks.push_back(std::move(combined));
                    if ((ItemPos)(blocks.size() + new_blocks.size())
                            >= parameters.maximum_number_of_blocks) {
                        limit_reached = true;
                        break;
                    }
                }
            }
        }

        if (new_blocks.empty())
            break;

        std::sort(new_blocks.begin(), new_blocks.end(), block_item_volume_greater);
        previous_blocks = std::move(new_blocks);
        const ItemPos old_blocks_size = (ItemPos)blocks.size();
        for (const Block& block: previous_blocks)
            blocks.push_back(block);
        std::inplace_merge(
            blocks.begin(),
            blocks.begin() + old_blocks_size,
            blocks.end(),
            block_item_volume_greater);
    }

    return blocks;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// compute_blocks /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<Block>> packingsolver::box::compute_blocks(
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
