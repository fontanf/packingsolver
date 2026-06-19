#include "rectangleguillotine/block.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <algorithm>
#include <ostream>
#include <set>
#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// operator<< /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const Block& block)
{
    os << "w " << block.rect.w
       << " h " << block.rect.h
       << " item_area " << block.item_area
       << " fill_rate " << block.fill_rate()
       << " stages " << block.number_of_stages
       << " first_cut " << block.first_cut_orientation
       << " item_copies";
    for (const auto& item_copy: block.item_copies)
        os << " " << item_copy.first << "*" << item_copy.second;
    return os;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Internals ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

struct BlockKey
{
    Rectangle rect;
    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;
    CutOrientation first_cut_orientation;
};

struct BlockKeyHasher
{
    std::size_t operator()(const BlockKey& key) const
    {
        std::size_t hash = 0;
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.rect.w));
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.rect.h));
        optimizationtools::hash_combine(
                hash,
                std::hash<int>{}(static_cast<int>(key.first_cut_orientation)));
        for (const auto& item_copy: key.item_copies) {
            optimizationtools::hash_combine(
                    hash,
                    std::hash<ItemTypeId>{}(item_copy.first));
            optimizationtools::hash_combine(
                    hash,
                    std::hash<ItemPos>{}(item_copy.second));
        }
        return hash;
    }
};

struct BlockKeyEqual
{
    bool operator()(const BlockKey& key_1, const BlockKey& key_2) const
    {
        return key_1.rect.w == key_2.rect.w
            && key_1.rect.h == key_2.rect.h
            && key_1.first_cut_orientation == key_2.first_cut_orientation
            && key_1.item_copies == key_2.item_copies;
    }
};

using BlockSet = std::unordered_set<BlockKey, BlockKeyHasher, BlockKeyEqual>;

BlockKey make_key(const Block& block)
{
    return {block.rect, block.item_copies, block.first_cut_orientation};
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
            item_type_id = it1->first;
            copies = it1->second + it2->second;
            ++it1; ++it2;
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
        if (it1 == item_copies_1.end()) {
            result.push_back(*it2); ++it2;
        } else if (it2 == item_copies_2.end()) {
            result.push_back(*it1); ++it1;
        } else if (it1->first < it2->first) {
            result.push_back(*it1); ++it1;
        } else if (it1->first > it2->first) {
            result.push_back(*it2); ++it2;
        } else {
            result.push_back({it1->first, it1->second + it2->second});
            ++it1; ++it2;
        }
    }
    return result;
}

struct BlockFillRateLess
{
    bool operator()(const Block& block_1, const Block& block_2) const
    {
        return block_1.fill_rate() < block_2.fill_rate();
    }
};

Counter compute_number_of_stages(
        CutType cut_type,
        const Block& block_1,
        const Block& block_2,
        CutOrientation cut_orientation)
{
    Counter max = (std::max)(
            block_1.number_of_stages,
            block_2.number_of_stages);
    if (max >= 1) {
        if ((block_1.number_of_stages == max
                    && block_1.first_cut_orientation != cut_orientation)
            || (block_2.number_of_stages == max
                && block_2.first_cut_orientation != cut_orientation)) {
            return max + 1;
        } else {
            return max;
        }
    } else {
        if (cut_type ==  CutType::Exact) {
            if ((cut_orientation == CutOrientation::Vertical && block_1.rect.h != block_2.rect.h)
                    || (cut_orientation == CutOrientation::Horizontal && block_1.rect.w != block_2.rect.w)) {
                return 2;
            } else {
                return 1;
            }
        } else if (cut_type == CutType::NonExact) {
            return 1;
        }
    }
    return -1;
}

std::vector<Block> compute_blocks_for_bin(
        const Instance& instance,
        BinTypeId bin_type_id,
        Counter max_internal_stages,
        const BlockParameters& parameters)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Length bin_w = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length bin_h = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;

    // Sorted ascending by fill_rate: begin() is worst, prev(end()) is best.
    std::multiset<Block, BlockFillRateLess> blocks_to_process;
    std::vector<Block> returned_blocks;
    BlockSet seen;

    auto enqueue = [&](Block block) {
        blocks_to_process.insert(block);
        if (parameters.maximum_number_of_blocks != -1
                && (ItemPos)blocks_to_process.size()
                > parameters.maximum_number_of_blocks) {
            BlockKey key = make_key(*blocks_to_process.begin());
            seen.erase(key);
            blocks_to_process.erase(blocks_to_process.begin());
        }
    };

    // Seed with uniform cx x cy grids of a single item type.
    // These always have fill_rate == 1.0 and cannot be recovered by the
    // combining loop (same-type self-combination is never attempted there).
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies < 1)
            continue;
        for (int rotation = 0; rotation < 2; ++rotation) {
            bool rotate = (rotation == 1);
            if (rotate && item_type.oriented)
                continue;
            if (rotate && item_type.rect.w == item_type.rect.h)
                continue;
            Length item_w = item_type.width(rotate);
            Length item_h = item_type.height(rotate);
            ItemPos copies_max_x = std::min(
                    item_type.copies,
                    (ItemPos)(bin_w / item_w));
            for (ItemPos cx = 1; cx <= copies_max_x; ++cx) {
                ItemPos copies_max_y = std::min(
                        item_type.copies / cx,
                        (ItemPos)(bin_h / item_h));
                for (ItemPos cy = 1; cy <= copies_max_y; ++cy) {
                    Counter stages;
                    if (cx == 1 && cy == 1) {
                        stages = 0;
                    } else if (cy == 1 || cx == 1) {
                        stages = 1;
                    } else {
                        stages = 2;
                    }
                    if (stages > max_internal_stages)
                        continue;
                    // For cx > 1 && cy > 1, generate both cut orientations.
                    int orientation_count = (cx > 1 && cy > 1) ? 2 : 1;
                    for (int orientation_idx = 0;
                            orientation_idx < orientation_count;
                            ++orientation_idx) {
                        Block block;
                        block.is_simple = true;
                        block.item_type_id = item_type_id;
                        block.rotate = rotate;
                        block.copies_x = cx;
                        block.copies_y = cy;
                        block.rect = {cx * item_w, cy * item_h};
                        block.item_area = cx * cy * item_type.area();
                        block.item_copies = {{item_type_id, cx * cy}};
                        block.number_of_items = cx * cy;
                        block.number_of_stages = stages;
                        if (stages == 0) {
                            block.first_cut_orientation = CutOrientation::Vertical;
                        } else if (cx > 1 && cy == 1) {
                            block.first_cut_orientation = CutOrientation::Vertical;
                        } else if (cx == 1 && cy > 1) {
                            block.first_cut_orientation = CutOrientation::Horizontal;
                        } else {
                            block.first_cut_orientation = (orientation_idx == 0)?
                                CutOrientation::Vertical:
                                CutOrientation::Horizontal;
                        }
                        BlockKey key = make_key(block);
                        if (!seen.count(key)) {
                            seen.insert(key);
                            enqueue(block);
                        }
                    }
                }
            }
        }
    }

    // Process blocks in decreasing fill_rate order.
    while (!blocks_to_process.empty()) {
        if (parameters.maximum_number_of_blocks != -1
                && (ItemPos)returned_blocks.size()
                >= parameters.maximum_number_of_blocks)
            break;

        auto it = std::prev(blocks_to_process.end());
        Block block = *it;
        blocks_to_process.erase(it);

        // Push block to returned_blocks before the combining loop so that
        // compute_block_stages can retrieve it by its index.
        ItemPos current_id = (ItemPos)returned_blocks.size();
        returned_blocks.push_back(block);

        // Combine with every previously returned block in both cut orientations.
        for (ItemPos existing_idx = 0;
                existing_idx < current_id;
                ++existing_idx) {
            const Block& existing_block = returned_blocks[existing_idx];
            // Uniform same-type grids are fully pre-generated in the seed phase.
            if (block.is_simple && existing_block.is_simple
                    && block.item_type_id == existing_block.item_type_id
                    && block.rotate == existing_block.rotate)
                continue;
            for (CutOrientation combining_orientation:
                    {CutOrientation::Vertical, CutOrientation::Horizontal}) {
                Block combined;
                combined.parent_1_id = current_id;
                combined.parent_2_id = existing_idx;
                combined.first_cut_orientation = combining_orientation;
                combined.item_area = block.item_area + existing_block.item_area;
                if (combining_orientation == CutOrientation::Vertical) {
                    combined.rect.w = block.rect.w + existing_block.rect.w;
                    combined.rect.h = std::max(block.rect.h, existing_block.rect.h);
                } else {
                    combined.rect.w = std::max(block.rect.w, existing_block.rect.w);
                    combined.rect.h = block.rect.h + existing_block.rect.h;
                }
                if (combined.rect.w > bin_w || combined.rect.h > bin_h)
                    continue;

                combined.number_of_stages = compute_number_of_stages(
                        instance.parameters().cut_type,
                        block,
                        existing_block,
                        combining_orientation);
                if (combined.number_of_stages > max_internal_stages)
                    continue;

                if (!is_valid_item_copies(
                        block.item_copies,
                        existing_block.item_copies,
                        instance))
                    continue;

                combined.item_copies = merge_item_copies(
                        block.item_copies,
                        existing_block.item_copies);
                BlockKey key = make_key(combined);
                if (seen.count(key))
                    continue;

                if (parameters.maximum_number_of_blocks != -1
                        && (ItemPos)blocks_to_process.size()
                        >= parameters.maximum_number_of_blocks) {
                    if (combined.fill_rate()
                            <= blocks_to_process.begin()->fill_rate())
                        continue;
                }

                combined.number_of_items =
                        block.number_of_items + existing_block.number_of_items;

                seen.insert(key);
                enqueue(combined);
            }
        }
    }

    return returned_blocks;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// compute_blocks /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::vector<std::vector<Block>> packingsolver::rectangleguillotine::compute_blocks(
        const Instance& instance,
        const BlockParameters& parameters)
{
    Counter max_internal_stages = std::max(
            (Counter)0,
            instance.parameters().number_of_stages - 3);

    std::vector<std::vector<Block>> result(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        result[bin_type_id] = compute_blocks_for_bin(
                instance,
                bin_type_id,
                max_internal_stages,
                parameters);
    }
    return result;
}
