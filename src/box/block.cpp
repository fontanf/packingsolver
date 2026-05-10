#include "box/block.hpp"

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

/** Check space and fill-rate validity of a block against the given bin type. */
bool is_valid_block(
    const Block& block,
    const BinType& bin_type,
    double minimum_fill_rate)
{
    if (block.box.x > bin_type.box.x
            || block.box.y > bin_type.box.y
            || block.box.z > bin_type.box.z)
        return false;

    if (block.fill_rate() < minimum_fill_rate)
        return false;

    return true;
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
            Box box = item_type.box.rotate(rotation);
            ItemPos copies_max_x = (std::min)(
                    item_type.copies,
                    (ItemPos)(bin_type.box.x / box.x));
            for (ItemPos cx = 1; cx <= copies_max_x; ++cx) {
                ItemPos copies_max_y = (std::min)(
                        item_type.copies / cx,
                        (ItemPos)(bin_type.box.y / box.y));
                for (ItemPos cy = 1; cy <= copies_max_y; ++cy) {
                    ItemPos copies_max_z = (std::min)(
                            item_type.copies / (cx * cy),
                            (ItemPos)(bin_type.box.z / box.z));
                    for (ItemPos cz = 1; cz <= copies_max_z; ++cz) {
                        Block block;
                        block.is_simple = true;
                        block.item_type_id = item_type_id;
                        block.rotation = rotation;
                        block.box = {cx * box.x, cy * box.y, cz * box.z};
                        block.item_volume = cx * cy * cz * item_type.volume();
                        block.item_copies = {{item_type_id, cx * cy * cz}};
                        block.items.reserve(cx * cy * cz);
                        for (ItemPos ccx = 0; ccx < cx; ++ccx) {
                            for (ItemPos ccy = 0; ccy < cy; ++ccy) {
                                for (ItemPos ccz = 0; ccz < cz; ++ccz) {
                                    SolutionItem solution_item;
                                    solution_item.item_type_id = item_type_id;
                                    solution_item.rotation = rotation;
                                    solution_item.bl_corner = {ccx * box.x, ccy * box.y, ccz * box.z};
                                    block.items.push_back(solution_item);
                                }
                            }
                        }
                        blocks.push_back(std::move(block));
                    }
                }
            }
        }
    }

    std::sort(blocks.begin(), blocks.end(), block_item_volume_greater);

    // Algorithm 3.5: iterative combination
    // P starts as singular blocks; after each iteration P ← N (newly generated blocks)
    std::vector<Block> previous_blocks(blocks.begin(), blocks.end());

    ItemPos number_of_simple_blocks = blocks.size();
    for (;;) {
        //std::cout << blocks.size() << std::endl;
        std::vector<Block> new_blocks;
        bool limit_reached = false;

        for (const Block& previous_block: previous_blocks) {
            if (limit_reached)
                break;
            for (const Block& existing_block: blocks) {
                if (limit_reached)
                    break;
                if (previous_block.is_simple == existing_block.is_simple
                        && previous_block.item_type_id == existing_block.item_type_id
                        && previous_block.rotation == existing_block.rotation) {
                    continue;
                }
                if (!is_valid_item_copies(
                            previous_block.item_copies,
                            existing_block.item_copies,
                            instance)) {
                    continue;
                }
                for (Direction direction: {Direction::X, Direction::Y, Direction::Z}) {
                    Block combined;
                    combined.item_volume = previous_block.item_volume + existing_block.item_volume;
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
                    if (!is_valid_block(combined, bin_type, parameters.minimum_fill_rate))
                        continue;
                    BlockKey key;
                    key.box = combined.box;
                    key.item_copies = merge_item_copies(
                            previous_block.item_copies,
                            existing_block.item_copies,
                            instance);
                    if (seen.count(key))
                        continue;
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
                    if (parameters.maximum_number_of_blocks != -1
                            && (ItemPos)(blocks.size() + new_blocks.size())
                            >= parameters.maximum_number_of_blocks) {
                        limit_reached = true;
                        break;
                    }
                    if (parameters.maximum_number_of_non_simple_blocks != -1
                            && (ItemPos)(blocks.size() - number_of_simple_blocks + new_blocks.size())
                            >= parameters.maximum_number_of_non_simple_blocks) {
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

        if (limit_reached)
            break;
    }

    //for (const Block& block: blocks)
    //    std::cout << block << std::endl;

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
