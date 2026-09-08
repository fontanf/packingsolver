#include "rectangle/item_subset_incompatibility.hpp"

#include "rectangle/solution_builder.hpp"

#include <algorithm>
#include <functional>
#include <numeric>
#include <stdexcept>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/** The (width, height) pairs an item type may take on. */
std::vector<std::pair<Length, Length>> item_type_orientations(
        const ItemType& item_type)
{
    std::vector<std::pair<Length, Length>> orientations;
    orientations.push_back({item_type.rect.x, item_type.rect.y});
    if (!item_type.oriented)
        orientations.push_back({item_type.rect.y, item_type.rect.x});
    return orientations;
}

/**
 * 'true' iff, with items 1 and 2 placed side by side along one axis, the
 * resulting block and item 3 fit together in the bin, split along the
 * other axis - one of the "pair split from the third" shapes (see this
 * file's own header comment).
 */
bool pair_split_fits(
        Length width_1,
        Length height_1,
        Length width_2,
        Length height_2,
        Length width_3,
        Length height_3,
        const BinType& bin_type)
{
    // Pair side by side horizontally; block and item 3 stacked vertically.
    if (std::max(width_1 + width_2, width_3) <= bin_type.rect.x
            && std::max(height_1, height_2) + height_3 <= bin_type.rect.y) {
        return true;
    }
    // Pair side by side vertically; block and item 3 side by side horizontally.
    if (std::max(height_1 + height_2, height_3) <= bin_type.rect.y
            && std::max(width_1, width_2) + width_3 <= bin_type.rect.x) {
        return true;
    }
    return false;
}

/**
 * 'true' iff none of the 8 shapes described in this file's own header
 * comment fits, in any combination of the three item types' orientations.
 */
bool triplet_incompatible(
        const ItemType& item_type_1,
        const ItemType& item_type_2,
        const ItemType& item_type_3,
        const BinType& bin_type)
{
    std::vector<std::pair<Length, Length>> orientations_1 = item_type_orientations(item_type_1);
    std::vector<std::pair<Length, Length>> orientations_2 = item_type_orientations(item_type_2);
    std::vector<std::pair<Length, Length>> orientations_3 = item_type_orientations(item_type_3);

    for (const std::pair<Length, Length>& orientation_1: orientations_1) {
        Length width_1 = orientation_1.first;
        Length height_1 = orientation_1.second;
        for (const std::pair<Length, Length>& orientation_2: orientations_2) {
            Length width_2 = orientation_2.first;
            Length height_2 = orientation_2.second;
            for (const std::pair<Length, Length>& orientation_3: orientations_3) {
                Length width_3 = orientation_3.first;
                Length height_3 = orientation_3.second;

                // All three in a single row.
                if (width_1 + width_2 + width_3 <= bin_type.rect.x
                        && std::max({height_1, height_2, height_3}) <= bin_type.rect.y) {
                    return false;
                }
                // All three in a single column.
                if (height_1 + height_2 + height_3 <= bin_type.rect.y
                        && std::max({width_1, width_2, width_3}) <= bin_type.rect.x) {
                    return false;
                }
                // A pair split from the third, for each choice of the odd
                // one out.
                if (pair_split_fits(width_1, height_1, width_2, height_2, width_3, height_3, bin_type))
                    return false;
                if (pair_split_fits(width_1, height_1, width_3, height_3, width_2, height_2, bin_type))
                    return false;
                if (pair_split_fits(width_2, height_2, width_3, height_3, width_1, height_1, bin_type))
                    return false;
            }
        }
    }
    return true;
}

/**
 * Like 'item_type_orientations', but also recording which rotation flag
 * each orientation corresponds to - needed to build an actual placement
 * (see 'find_triplet_placement'), not just decide compatibility.
 */
struct OrientedSize
{
    Length width;
    Length height;
    bool rotate;
};

std::vector<OrientedSize> item_type_oriented_sizes(
        const ItemType& item_type)
{
    std::vector<OrientedSize> sizes;
    sizes.push_back({item_type.rect.x, item_type.rect.y, false});
    if (!item_type.oriented)
        sizes.push_back({item_type.rect.y, item_type.rect.x, true});
    return sizes;
}

/**
 * If items a and b, placed side by side along one axis, and item c,
 * placed along the other axis, fit together in the bin - one of the "pair
 * split from the third" shapes (see this file's own header comment) -
 * fill 'corner_a'/'corner_b'/'corner_c' with a concrete, non-overlapping
 * placement for them and return 'true'; return 'false' otherwise. Mirrors
 * 'pair_split_fits' exactly (same 2 branches, same conditions), but also
 * constructs coordinates: whichever axis is split on, the two sides only
 * need to be offset from one another along *that* axis - their ranges
 * along it become disjoint, so they cannot overlap regardless of how they
 * line up on the other axis, which is why both can simply start at 0
 * there.
 */
bool pair_split_placement(
        Length width_a,
        Length height_a,
        Length width_b,
        Length height_b,
        Length width_c,
        Length height_c,
        const BinType& bin_type,
        Point& corner_a,
        Point& corner_b,
        Point& corner_c)
{
    // Pair side by side horizontally; block and item c stacked vertically.
    if (std::max(width_a + width_b, width_c) <= bin_type.rect.x
            && std::max(height_a, height_b) + height_c <= bin_type.rect.y) {
        corner_a = {0, 0};
        corner_b = {width_a, 0};
        corner_c = {0, std::max(height_a, height_b)};
        return true;
    }
    // Pair side by side vertically; block and item c side by side horizontally.
    if (std::max(height_a + height_b, height_c) <= bin_type.rect.y
            && std::max(width_a, width_b) + width_c <= bin_type.rect.x) {
        corner_a = {0, 0};
        corner_b = {0, height_a};
        corner_c = {std::max(width_a, width_b), 0};
        return true;
    }
    return false;
}

/**
 * If none of the 8 shapes described in this file's own header comment
 * fits, in any combination of the three item types' orientations, return
 * 'false' (mirrors 'triplet_incompatible' exactly - same shapes, same
 * orientations, same order). Otherwise fill 'bl_corners'/'rotate' with a
 * concrete, non-overlapping placement for whichever shape and orientation
 * combination is found first (not searched for or optimized in any way)
 * and return 'true'.
 */
bool find_triplet_shape_placement(
        const ItemType& item_type_1,
        const ItemType& item_type_2,
        const ItemType& item_type_3,
        const BinType& bin_type,
        std::array<Point, 3>& bl_corners,
        std::array<bool, 3>& rotate)
{
    std::vector<OrientedSize> sizes_1 = item_type_oriented_sizes(item_type_1);
    std::vector<OrientedSize> sizes_2 = item_type_oriented_sizes(item_type_2);
    std::vector<OrientedSize> sizes_3 = item_type_oriented_sizes(item_type_3);

    for (const OrientedSize& size_1: sizes_1) {
        Length width_1 = size_1.width;
        Length height_1 = size_1.height;
        for (const OrientedSize& size_2: sizes_2) {
            Length width_2 = size_2.width;
            Length height_2 = size_2.height;
            for (const OrientedSize& size_3: sizes_3) {
                Length width_3 = size_3.width;
                Length height_3 = size_3.height;

                // All three in a single row.
                if (width_1 + width_2 + width_3 <= bin_type.rect.x
                        && std::max({height_1, height_2, height_3}) <= bin_type.rect.y) {
                    bl_corners[0] = {0, 0};
                    bl_corners[1] = {width_1, 0};
                    bl_corners[2] = {width_1 + width_2, 0};
                    rotate = {size_1.rotate, size_2.rotate, size_3.rotate};
                    return true;
                }
                // All three in a single column.
                if (height_1 + height_2 + height_3 <= bin_type.rect.y
                        && std::max({width_1, width_2, width_3}) <= bin_type.rect.x) {
                    bl_corners[0] = {0, 0};
                    bl_corners[1] = {0, height_1};
                    bl_corners[2] = {0, height_1 + height_2};
                    rotate = {size_1.rotate, size_2.rotate, size_3.rotate};
                    return true;
                }
                // A pair split from the third, for each choice of the odd
                // one out - mirrors 'triplet_incompatible''s own 3 calls
                // to 'pair_split_fits' exactly.
                if (pair_split_placement(
                        width_1, height_1, width_2, height_2, width_3, height_3,
                        bin_type, bl_corners[0], bl_corners[1], bl_corners[2])) {
                    rotate = {size_1.rotate, size_2.rotate, size_3.rotate};
                    return true;
                }
                if (pair_split_placement(
                        width_1, height_1, width_3, height_3, width_2, height_2,
                        bin_type, bl_corners[0], bl_corners[2], bl_corners[1])) {
                    rotate = {size_1.rotate, size_2.rotate, size_3.rotate};
                    return true;
                }
                if (pair_split_placement(
                        width_2, height_2, width_3, height_3, width_1, height_1,
                        bin_type, bl_corners[1], bl_corners[2], bl_corners[0])) {
                    rotate = {size_1.rotate, size_2.rotate, size_3.rotate};
                    return true;
                }
            }
        }
    }
    return false;
}

/** An item type's smallest possible width, across every orientation it may use. */
Length item_type_minimum_width(const ItemType& item_type)
{
    return (item_type.oriented)? item_type.rect.x: item_type.rect.min();
}

/** An item type's smallest possible height, across every orientation it may use. */
Length item_type_minimum_height(const ItemType& item_type)
{
    return (item_type.oriented)? item_type.rect.y: item_type.rect.min();
}

/**
 * Grow 'triplets' with every triplet of 3 *distinct* item types among
 * 'type_ids' provably incompatible - case 1 of 'find_incompatible_
 * triplets''s own doc comment.
 *
 * 'key'/'bin_extent' are this pass's own axis (width or height);
 * 'other_key'/'other_bin_extent' are the other axis's. A candidate that
 * also exceeds 'other_bin_extent' along the other axis is skipped
 * whenever 'skip_if_other_exceeds' is set: the width pass (called with it
 * clear) handles every triple whose width-sum exceeds the bin's width,
 * regardless of height, so the height pass (called with it set) only
 * needs to contribute the triples the width pass could not have reached -
 * a clean partition between the two passes' candidates instead of a cache
 * that would otherwise need to recognize the same triple found twice.
 *
 * 'type_ids' is visited as the standard increasing triple
 * type_pos_1 < type_pos_2 < type_pos_3 (over 'order', sorted by
 * decreasing 'key') - the usual way to enumerate every 3-combination of a
 * list exactly once.
 */
void find_incompatible_triplets_of_distinct_types(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<ItemTypeId>& type_ids,
        const std::function<Length(const ItemType&)>& key,
        Length bin_extent,
        const std::function<Length(const ItemType&)>& other_key,
        Length other_bin_extent,
        bool skip_if_other_exceeds,
        std::vector<IncompatibleTriplet>& triplets)
{
    std::vector<ItemPos> order(type_ids.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(
            order.begin(),
            order.end(),
            [&instance, &type_ids, &key](ItemPos type_pos_1, ItemPos type_pos_2)
            {
                return key(instance.item_type(type_ids[type_pos_1]))
                    > key(instance.item_type(type_ids[type_pos_2]));
            });

    for (ItemPos order_pos_1 = 0;
            order_pos_1 < (ItemPos)order.size();
            ++order_pos_1) {
        ItemTypeId item_type_id_1 = type_ids[order[order_pos_1]];
        Length key_1 = key(instance.item_type(item_type_id_1));

        for (ItemPos order_pos_2 = order_pos_1 + 1;
                order_pos_2 < (ItemPos)order.size();
                ++order_pos_2) {
            ItemTypeId item_type_id_2 = type_ids[order[order_pos_2]];
            Length key_2 = key(instance.item_type(item_type_id_2));
            Length threshold = bin_extent - key_1 - key_2;

            for (ItemPos order_pos_3 = order_pos_2 + 1;
                    order_pos_3 < (ItemPos)order.size();
                    ++order_pos_3) {
                ItemTypeId item_type_id_3 = type_ids[order[order_pos_3]];
                Length key_3 = key(instance.item_type(item_type_id_3));
                if (key_3 <= threshold)
                    break;

                const ItemType& item_type_1 = instance.item_type(item_type_id_1);
                const ItemType& item_type_2 = instance.item_type(item_type_id_2);
                const ItemType& item_type_3 = instance.item_type(item_type_id_3);
                if (skip_if_other_exceeds
                        && other_key(item_type_1) + other_key(item_type_2) + other_key(item_type_3) > other_bin_extent) {
                    continue;
                }
                if (triplet_incompatible(item_type_1, item_type_2, item_type_3, bin_type)) {
                    IncompatibleTriplet triplet;
                    triplet.item_type_ids = {item_type_id_1, item_type_id_2, item_type_id_3};
                    std::sort(triplet.item_type_ids.begin(), triplet.item_type_ids.end());
                    triplets.push_back(triplet);
                }
            }
        }
    }
}

}

std::vector<IncompatibleTriplet> packingsolver::rectangle::find_incompatible_triplets(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items)
{
    std::vector<IncompatibleTriplet> triplets;
    const BinType& bin_type = instance.bin_type(bin_type_id);

    std::vector<ItemTypeId> type_ids;
    for (const std::pair<ItemTypeId, ItemPos>& selected_item: selected_items)
        type_ids.push_back(selected_item.first);

    // Case: three copies of the same item type.
    for (const std::pair<ItemTypeId, ItemPos>& selected_item: selected_items) {
        if (selected_item.second < 3)
            continue;
        const ItemType& item_type = instance.item_type(selected_item.first);
        if (3 * item_type_minimum_width(item_type) <= bin_type.rect.x
                && 3 * item_type_minimum_height(item_type) <= bin_type.rect.y) {
            continue;
        }
        if (triplet_incompatible(item_type, item_type, item_type, bin_type)) {
            IncompatibleTriplet triplet;
            triplet.item_type_ids = {selected_item.first, selected_item.first, selected_item.first};
            triplets.push_back(triplet);
        }
    }

    // Case: two copies of one item type, plus one of another (any type
    // present).
    for (const std::pair<ItemTypeId, ItemPos>& doubled_item: selected_items) {
        if (doubled_item.second < 2)
            continue;
        const ItemType& doubled_item_type = instance.item_type(doubled_item.first);

        for (const std::pair<ItemTypeId, ItemPos>& other_item: selected_items) {
            if (other_item.first == doubled_item.first)
                continue;
            const ItemType& other_item_type = instance.item_type(other_item.first);
            if (2 * item_type_minimum_width(doubled_item_type) + item_type_minimum_width(other_item_type) <= bin_type.rect.x
                    && 2 * item_type_minimum_height(doubled_item_type) + item_type_minimum_height(other_item_type) <= bin_type.rect.y) {
                continue;
            }
            if (triplet_incompatible(doubled_item_type, doubled_item_type, other_item_type, bin_type)) {
                IncompatibleTriplet triplet;
                triplet.item_type_ids = {doubled_item.first, doubled_item.first, other_item.first};
                std::sort(triplet.item_type_ids.begin(), triplet.item_type_ids.end());
                triplets.push_back(triplet);
            }
        }
    }

    // Case: three distinct item types.
    if (type_ids.size() >= 3) {
        find_incompatible_triplets_of_distinct_types(
                instance, bin_type, type_ids,
                item_type_minimum_width, bin_type.rect.x,
                item_type_minimum_height, bin_type.rect.y,
                false, triplets);
        find_incompatible_triplets_of_distinct_types(
                instance, bin_type, type_ids,
                item_type_minimum_height, bin_type.rect.y,
                item_type_minimum_width, bin_type.rect.x,
                true, triplets);
    }

    return triplets;
}

Solution packingsolver::rectangle::find_triplet_placement(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items)
{
    std::vector<ItemTypeId> units;
    for (const std::pair<ItemTypeId, ItemPos>& selected_item: selected_items) {
        for (ItemPos copy = 0; copy < selected_item.second; ++copy)
            units.push_back(selected_item.first);
    }
    if (units.size() != 3) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "'selected_items' must sum to exactly 3 item copies; "
                "found " + std::to_string(units.size()) + ".");
    }

    const BinType& bin_type = instance.bin_type(bin_type_id);
    std::array<Point, 3> bl_corners;
    std::array<bool, 3> rotate;
    if (!find_triplet_shape_placement(
            instance.item_type(units[0]),
            instance.item_type(units[1]),
            instance.item_type(units[2]),
            bin_type,
            bl_corners,
            rotate)) {
        return Solution(instance);
    }

    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);
    for (ItemPos item_pos = 0; item_pos < 3; ++item_pos) {
        solution_builder.add_item(
                bin_pos,
                units[item_pos],
                bl_corners[item_pos],
                rotate[item_pos]);
    }
    return solution_builder.build();
}
