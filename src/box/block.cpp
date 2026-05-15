#include "box/block.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <algorithm>
#include <ostream>
#include <queue>
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
        return block_1.fill_rate() < block_2.fill_rate();
    }
};

inline Box lift_box(
    const Instance& instance,
    BinTypeId bin_type_id,
    const Box& box)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Box box_lifted;
    box_lifted.x = (bin_type.box.x - box.x >= instance.smallest_item_x())? box.x: bin_type.box.x;
    box_lifted.y = (bin_type.box.y - box.y >= instance.smallest_item_y())? box.y: bin_type.box.y;
    box_lifted.z = (bin_type.box.z - box.z >= instance.smallest_item_z())? box.z: bin_type.box.z;
    return box_lifted;
}

std::vector<Block> compute_blocks_for_bin(
    const Instance& instance,
    BinTypeId bin_type_id,
    const BlockParameters& parameters)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);

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
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies < 1)
            continue;
        for (Rotation rotation: item_type.rotations) {
            Box rotated_box = item_type.box.rotate(rotation);
            ItemPos copies_max_x = (std::min)(
                    item_type.copies,
                    (ItemPos)(bin_type.box.x / rotated_box.x));
            for (ItemPos cx = 1; cx <= copies_max_x; ++cx) {
                ItemPos copies_max_y = (std::min)(
                        item_type.copies / cx,
                        (ItemPos)(bin_type.box.y / rotated_box.y));
                for (ItemPos cy = 1; cy <= copies_max_y; ++cy) {
                    ItemPos copies_max_z = (std::min)(
                            item_type.copies / (cx * cy),
                            (ItemPos)(bin_type.box.z / rotated_box.z));
                    for (ItemPos cz = 1; cz <= copies_max_z; ++cz) {
                        Block block;
                        block.is_simple = true;
                        block.item_type_id = item_type_id;
                        block.rotation = rotation;
                        block.box = {cx * rotated_box.x, cy * rotated_box.y, cz * rotated_box.z};
                        block.box = lift_box(instance, bin_type_id, block.box);
                        block.item_volume = cx * cy * cz * item_type.volume();
                        block.item_copies = {{item_type_id, cx * cy * cz}};
                        block.number_of_items = cx * cy * cz;
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
                        BlockKey key = make_key(block);
                        if (!seen.count(key)) {
                            seen.insert(key);
                            enqueue(std::move(block));
                        }
                    }
                }
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

        auto it = std::prev(blocks_to_process.end());
        Block block = *it;
        blocks_to_process.erase(it);

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
                if (combined.box.x > bin_type.box.x
                        || combined.box.y > bin_type.box.y
                        || combined.box.z > bin_type.box.z)
                    continue;
                combined.box = lift_box(instance, bin_type_id, combined.box);

                if (parameters.maximum_number_of_blocks != -1
                        && (ItemPos)blocks_to_process.size() >= parameters.maximum_number_of_blocks) {
                    if (combined.fill_rate() <= blocks_to_process.begin()->fill_rate())
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
                combined.number_of_items = block.number_of_items + existing_block.number_of_items;
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
