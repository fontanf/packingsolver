#include "rectangleguillotine/labeling.hpp"

#include "packingsolver/rectangleguillotine/algorithm_formatter.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Dynamic programming ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::vector<Profit> compute_dp(const Instance& instance)
{
    const BinType& bin_type = instance.bin_type(0);
    Length eff_width  = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    Length cut_thickness = instance.parameters().cut_thickness;

    std::vector<Profit> dp_values((eff_width + 1) * (eff_height + 1), 0.0);

    auto dp = [&](Length width, Length height) -> Profit& {
        return dp_values[width + (eff_width + 1) * height];
    };

    for (Length width = 1; width <= eff_width; ++width) {
        for (Length height = 1; height <= eff_height; ++height) {
            Profit& dp_current = dp(width, height);

            for (ItemTypeId item_type_id = 0;
                    item_type_id < instance.number_of_item_types();
                    ++item_type_id) {
                const ItemType& item_type = instance.item_type(item_type_id);
                if (item_type.rect.w == width && item_type.rect.h == height)
                    dp_current = std::max(dp_current, item_type.profit);
                if (!item_type.oriented
                        && item_type.rect.h == width
                        && item_type.rect.w == height)
                    dp_current = std::max(dp_current, item_type.profit);
            }

            for (Length cut = 1; 2 * cut + cut_thickness <= width; ++cut)
                dp_current = std::max(dp_current,
                        dp(cut, height) + dp(width - cut - cut_thickness, height));

            for (Length cut = 1; 2 * cut + cut_thickness <= height; ++cut)
                dp_current = std::max(dp_current,
                        dp(width, cut) + dp(width, height - cut - cut_thickness));
        }
    }

    return dp_values;
}

std::vector<Profit> compute_rdp(
        const std::vector<Profit>& dp_values,
        Length eff_width,
        Length eff_height,
        Length cut_thickness)
{
    std::vector<Profit> rdp_values(
            (eff_width + 1) * (eff_height + 1),
            -std::numeric_limits<Profit>::infinity());

    auto dp = [&](Length width, Length height) {
        return dp_values[width + (eff_width + 1) * height];
    };
    auto rdp = [&](Length width, Length height) -> Profit& {
        return rdp_values[width + (eff_width + 1) * height];
    };

    rdp(eff_width, eff_height) = 0.0;

    for (Length width = eff_width; width >= 1; --width) {
        for (Length height = eff_height; height >= 1; --height) {
            if (width == eff_width && height == eff_height)
                continue;

            Profit& rdp_current = rdp(width, height);

            for (Length total_width = width + cut_thickness + 1;
                    total_width <= eff_width;
                    ++total_width) {
                Length other_width = total_width - width - cut_thickness;
                rdp_current = std::max(
                        rdp_current,
                        rdp(total_width, height) + dp(other_width, height));
            }

            for (Length total_height = height + cut_thickness + 1;
                    total_height <= eff_height;
                    ++total_height) {
                Length other_height = total_height - height - cut_thickness;
                rdp_current = std::max(
                        rdp_current,
                        rdp(width, total_height) + dp(width, other_height));
            }
        }
    }

    return rdp_values;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// StateBlock ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct StateBlock
{
    /** Tight bounding box of all items in the block. */
    Rectangle rect;

    /** Sum of profits of all items, respecting copy limits. */
    Profit profit = 0.0;

    /**
     * Number of copies of each item type used, as a sorted sparse list of
     * (item_type_id, count) pairs.
     */
    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;

    ItemPos number_of_items = 0;

    /**
     * Direction of the outermost guillotine cut that combines parent_1 and
     * parent_2.  Set to Vertical by convention for single-item seed blocks.
     */
    CutOrientation first_cut_orientation = CutOrientation::Vertical;

    /** True for single-item seed blocks. */
    bool is_simple = false;

    /** Item type; valid only when is_simple == true. */
    ItemTypeId item_type_id = -1;

    /** Whether the item is rotated; valid only when is_simple == true. */
    bool rotate = false;

    /**
     * Indices into the returned_blocks vector of the two parent blocks.
     * parent_1 is placed at (0, 0) relative to this block's origin.
     * parent_2 is placed at (parent_1.rect.w + cut_thickness, 0) for
     * Vertical, or at (0, parent_1.rect.h + cut_thickness) for Horizontal.
     * -1 when is_simple == true.
     */
    ItemPos parent_1_id = -1;
    ItemPos parent_2_id = -1;

    /** Priority for the processing queue: profit + rdp(rect.w, rect.h). */
    double priority = 0.0;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Seen-set infrastructure /////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct StateBlockKey
{
    Rectangle rect;
    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;
    CutOrientation first_cut_orientation;
};

struct StateBlockKeyHasher
{
    std::size_t operator()(const StateBlockKey& key) const
    {
        std::size_t hash = 0;
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.rect.w));
        optimizationtools::hash_combine(hash, std::hash<Length>{}(key.rect.h));
        optimizationtools::hash_combine(
                hash,
                std::hash<int>{}(static_cast<int>(key.first_cut_orientation)));
        for (const auto& item_copy: key.item_copies) {
            optimizationtools::hash_combine(
                    hash, std::hash<ItemTypeId>{}(item_copy.first));
            optimizationtools::hash_combine(
                    hash, std::hash<ItemPos>{}(item_copy.second));
        }
        return hash;
    }
};

struct StateBlockKeyEqual
{
    bool operator()(const StateBlockKey& key_1, const StateBlockKey& key_2) const
    {
        return key_1.rect.w == key_2.rect.w
            && key_1.rect.h == key_2.rect.h
            && key_1.first_cut_orientation == key_2.first_cut_orientation
            && key_1.item_copies == key_2.item_copies;
    }
};

using StateBlockSet = std::unordered_set<
        StateBlockKey, StateBlockKeyHasher, StateBlockKeyEqual>;

StateBlockKey make_key(const StateBlock& block)
{
    return {block.rect, block.item_copies, block.first_cut_orientation};
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Item-copy helpers ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////// Solution reconstruction /////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * Reconstruct a block into the SolutionBuilder, given the available node
 * dimensions (which may exceed block.rect when this block is the shorter
 * sub-block of a max-dimension merge).
 *
 * When available_w > block.rect.w or available_h > block.rect.h, a
 * normalization cut is emitted to trim the excess before descending into
 * the block's content, mirroring the NormX / NormY labels in labeling.cpp.
 */
void reconstruct_block(
        SolutionBuilder& builder,
        const std::vector<StateBlock>& returned_blocks,
        const StateBlock& block,
        CutOrientation bin_first_cut_orientation,
        Length cut_thickness,
        Depth depth,
        Length x_offset,
        Length y_offset,
        Length available_w,
        Length available_h)
{
    bool cut_is_vertical = (bin_first_cut_orientation == CutOrientation::Vertical)
        == (depth % 2 == 1);

    // Normalization: trim excess width with a vertical cut.
    if (available_w > block.rect.w) {
        if (cut_is_vertical) {
            builder.add_node(depth, x_offset + block.rect.w);
            reconstruct_block(builder, returned_blocks, block,
                    bin_first_cut_orientation, cut_thickness,
                    depth + 1, x_offset, y_offset, block.rect.w, available_h);
        } else {
            builder.add_node(depth, y_offset + available_h);
            reconstruct_block(builder, returned_blocks, block,
                    bin_first_cut_orientation, cut_thickness,
                    depth + 1, x_offset, y_offset, available_w, available_h);
        }
        return;
    }

    // Normalization: trim excess height with a horizontal cut.
    if (available_h > block.rect.h) {
        if (!cut_is_vertical) {
            builder.add_node(depth, y_offset + block.rect.h);
            reconstruct_block(builder, returned_blocks, block,
                    bin_first_cut_orientation, cut_thickness,
                    depth + 1, x_offset, y_offset, available_w, block.rect.h);
        } else {
            builder.add_node(depth, x_offset + available_w);
            reconstruct_block(builder, returned_blocks, block,
                    bin_first_cut_orientation, cut_thickness,
                    depth + 1, x_offset, y_offset, available_w, available_h);
        }
        return;
    }

    // No excess: available_w == block.rect.w and available_h == block.rect.h.

    if (block.is_simple) {
        builder.set_last_node_item(block.item_type_id);
        return;
    }

    bool block_cut_is_vertical =
        (block.first_cut_orientation == CutOrientation::Vertical);

    if (cut_is_vertical != block_cut_is_vertical) {
        if (cut_is_vertical) {
            builder.add_node(depth, x_offset + block.rect.w);
        } else {
            builder.add_node(depth, y_offset + block.rect.h);
        }
        reconstruct_block(builder, returned_blocks, block,
                bin_first_cut_orientation, cut_thickness,
                depth + 1, x_offset, y_offset, block.rect.w, block.rect.h);
        return;
    }

    const StateBlock& parent_1 = returned_blocks[block.parent_1_id];
    const StateBlock& parent_2 = returned_blocks[block.parent_2_id];

    if (block.first_cut_orientation == CutOrientation::Vertical) {
        builder.add_node(depth, x_offset + parent_1.rect.w);
        reconstruct_block(builder, returned_blocks, parent_1,
                bin_first_cut_orientation, cut_thickness,
                depth + 1, x_offset, y_offset,
                parent_1.rect.w, block.rect.h);
        builder.add_node(depth, x_offset + block.rect.w);
        reconstruct_block(builder, returned_blocks, parent_2,
                bin_first_cut_orientation, cut_thickness,
                depth + 1, x_offset + parent_1.rect.w + cut_thickness, y_offset,
                parent_2.rect.w, block.rect.h);
    } else {
        builder.add_node(depth, y_offset + parent_1.rect.h);
        reconstruct_block(builder, returned_blocks, parent_1,
                bin_first_cut_orientation, cut_thickness,
                depth + 1, x_offset, y_offset,
                block.rect.w, parent_1.rect.h);
        builder.add_node(depth, y_offset + block.rect.h);
        reconstruct_block(builder, returned_blocks, parent_2,
                bin_first_cut_orientation, cut_thickness,
                depth + 1, x_offset, y_offset + parent_1.rect.h + cut_thickness,
                block.rect.w, parent_2.rect.h);
    }
}

Solution reconstruct_solution(
        const Instance& instance,
        const std::vector<StateBlock>& returned_blocks,
        const StateBlock& root_block,
        Length eff_width,
        Length eff_height,
        Length cut_thickness)
{
    const BinType& bin_type = instance.bin_type(0);
    Length x_offset = (bin_type.left_trim_type  == TrimType::Hard) ? bin_type.left_trim  : 0;
    Length y_offset = (bin_type.bottom_trim_type == TrimType::Hard) ? bin_type.bottom_trim : 0;

    CutOrientation bin_first_cut_orientation = instance.parameters().first_stage_orientation;
    if (bin_first_cut_orientation == CutOrientation::Any)
        bin_first_cut_orientation = CutOrientation::Vertical;

    SolutionBuilder builder(instance);
    builder.add_bin(0, 1, bin_first_cut_orientation);

    if (root_block.profit > 0.0) {
        builder.add_node(1, x_offset + eff_width);
        reconstruct_block(builder, returned_blocks, root_block,
                bin_first_cut_orientation, cut_thickness,
                2, x_offset, y_offset, eff_width, eff_height);
    }

    return builder.build();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Main function ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const LabelingOutput packingsolver::rectangleguillotine::labeling(
        const Instance& instance,
        const LabelingParameters& parameters)
{
    LabelingOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    const BinType& bin_type = instance.bin_type(0);
    Length eff_width  = bin_type.rect.w - bin_type.left_trim - bin_type.right_trim;
    Length eff_height = bin_type.rect.h - bin_type.bottom_trim - bin_type.top_trim;
    Length cut_thickness = instance.parameters().cut_thickness;

    std::vector<Profit> dp_values = compute_dp(instance);
    std::vector<Profit> rdp_values = compute_rdp(
            dp_values, eff_width, eff_height, cut_thickness);

    auto rdp_value = [&](Length width, Length height) -> Profit {
        return rdp_values[width + (eff_width + 1) * height];
    };

    // Bucket queue: bucket k holds blocks with priority in
    // [0.99^k · init_bound, 0.99^(k-1) · init_bound).
    // No sorting within a bucket; we stop when a bucket's threshold
    // falls below best_solution_profit.
    double init_bound = dp_values[eff_width + (eff_width + 1) * eff_height];
    const int number_of_buckets = 2000;
    const double bucket_factor = std::log(1.0 / 0.99);

    auto bucket_index = [&](double priority) -> int {
        if (init_bound <= 0.0 || priority >= init_bound)
            return 0;
        if (priority <= 0.0)
            return number_of_buckets - 1;
        int k = (int)(std::log(init_bound / priority) / bucket_factor);
        return std::min(k, number_of_buckets - 1);
    };

    StateBlockSet seen;
    std::vector<std::vector<StateBlock>> blocks_to_process(number_of_buckets);
    std::vector<StateBlock> returned_blocks;

    Profit best_solution_profit = 0.0;

    // Seed: one block per item type per orientation (single copy, tight box).
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.copies < 1)
            continue;
        for (int rotation = 0; rotation < 2; ++rotation) {
            bool rotate_item = (rotation == 1);
            if (rotate_item && item_type.oriented)
                continue;
            if (rotate_item && item_type.rect.w == item_type.rect.h)
                continue;
            Length item_w = item_type.width(rotate_item);
            Length item_h = item_type.height(rotate_item);
            if (item_w > eff_width || item_h > eff_height)
                continue;
            Profit block_priority = item_type.profit + rdp_value(item_w, item_h);
            if (block_priority <= best_solution_profit)
                continue;

            StateBlock block;
            block.is_simple = true;
            block.item_type_id = item_type_id;
            block.rotate = rotate_item;
            block.rect = {item_w, item_h};
            block.profit = item_type.profit;
            block.item_copies = {{item_type_id, 1}};
            block.number_of_items = 1;
            block.first_cut_orientation = CutOrientation::Vertical;
            block.priority = block.profit + rdp_value(block.rect.w, block.rect.h);

            if (block.profit > best_solution_profit) {
                best_solution_profit = block.profit;
                Solution solution = reconstruct_solution(
                        instance, returned_blocks, block,
                        eff_width, eff_height, cut_thickness);
                algorithm_formatter.update_solution(solution, "b " + std::to_string(returned_blocks.size()));
            }

            if (block.priority > best_solution_profit) {
                blocks_to_process[bucket_index(block.priority)].push_back(block);
            }
        }
    }

    // Main loop: process buckets in decreasing priority order.
    // Within each bucket, blocks are processed in LIFO order (no sort needed).
    for (int bucket_k = 0; bucket_k < number_of_buckets; ++bucket_k) {
        if (parameters.timer.needs_to_end())
            break;
        double bucket_threshold = init_bound * std::pow(0.99, bucket_k);
        if (bucket_threshold <= best_solution_profit)
            break;

        while (!blocks_to_process[bucket_k].empty()) {
            if (parameters.timer.needs_to_end())
                break;
            if (bucket_threshold <= best_solution_profit)
                break;

            StateBlock block = blocks_to_process[bucket_k].back();
            blocks_to_process[bucket_k].pop_back();

            if (block.priority <= best_solution_profit)
                continue;

            StateBlockKey key = make_key(block);
            if (seen.count(key))
                continue;
            seen.insert(key);

            ItemPos current_id = (ItemPos)returned_blocks.size();
            returned_blocks.push_back(block);

            for (ItemPos existing_idx = 0; existing_idx <= current_id; ++existing_idx) {
                const StateBlock& existing_block = returned_blocks[existing_idx];

                Profit combined_profit = block.profit + existing_block.profit;

                // Compute dimensions and priority for each orientation before
                // the expensive item-copy validation.
                Length vert_w = block.rect.w + existing_block.rect.w + cut_thickness;
                Length vert_h = std::max(block.rect.h, existing_block.rect.h);
                Length horiz_w = std::max(block.rect.w, existing_block.rect.w);
                Length horiz_h = block.rect.h + existing_block.rect.h + cut_thickness;

                double vert_priority = (vert_w <= eff_width && vert_h <= eff_height)?
                    combined_profit + rdp_value(vert_w, vert_h):
                    -1.0;
                double horiz_priority = (horiz_w <= eff_width && horiz_h <= eff_height)?
                    combined_profit + rdp_value(horiz_w, horiz_h):
                    -1.0;

                if (vert_priority <= best_solution_profit
                        && horiz_priority <= best_solution_profit)
                    continue;

                if (!is_valid_item_copies(
                        block.item_copies, existing_block.item_copies, instance))
                    continue;

                std::vector<std::pair<ItemTypeId, ItemPos>> merged_copies =
                    merge_item_copies(block.item_copies, existing_block.item_copies);

                ItemPos combined_number_of_items =
                    block.number_of_items + existing_block.number_of_items;

                if (vert_priority > best_solution_profit) {
                    StateBlock combined;
                    combined.first_cut_orientation = CutOrientation::Vertical;
                    combined.parent_1_id = current_id;
                    combined.parent_2_id = existing_idx;
                    combined.profit = combined_profit;
                    combined.number_of_items = combined_number_of_items;
                    combined.item_copies = merged_copies;
                    combined.rect = {vert_w, vert_h};
                    combined.priority = vert_priority;
                    if (combined_profit > best_solution_profit) {
                        best_solution_profit = combined_profit;
                        Solution solution = reconstruct_solution(
                                instance, returned_blocks, combined,
                                eff_width, eff_height, cut_thickness);
                        algorithm_formatter.update_solution(solution, "b " + std::to_string(returned_blocks.size()));
                    }
                    blocks_to_process[bucket_index(combined.priority)].push_back(
                            std::move(combined));
                }

                if (horiz_priority > best_solution_profit) {
                    StateBlock combined;
                    combined.first_cut_orientation = CutOrientation::Horizontal;
                    combined.parent_1_id = current_id;
                    combined.parent_2_id = existing_idx;
                    combined.profit = combined_profit;
                    combined.number_of_items = combined_number_of_items;
                    combined.item_copies = std::move(merged_copies);
                    combined.rect = {horiz_w, horiz_h};
                    combined.priority = horiz_priority;
                    if (combined_profit > best_solution_profit) {
                        best_solution_profit = combined_profit;
                        Solution solution = reconstruct_solution(
                                instance, returned_blocks, combined,
                                eff_width, eff_height, cut_thickness);
                        algorithm_formatter.update_solution(solution, "n " + std::to_string(returned_blocks.size()));
                    }
                    blocks_to_process[bucket_index(combined.priority)].push_back(
                            std::move(combined));
                }
            }
        }
    }

    algorithm_formatter.end();
    return output;
}
