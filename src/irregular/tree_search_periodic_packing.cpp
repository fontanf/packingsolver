#include "irregular/tree_search_periodic_packing.hpp"
#include "irregular/sequential_feasibility.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "irregular/periodic_packing.hpp"
#include "irregular/rotations.hpp"
#include "irregular/shape_simplification.hpp"
#include "irregular/solution_builder.hpp"
#include "algorithms/thread_pool.hpp"
#include "treesearchsolver/iterative_beam_search.hpp"

#include "optimizationtools/containers/indexed_map.hpp"

#include "shape/rectangular_approximation.hpp"

#include <thread>
#include <forward_list>
#include <algorithm>
#include <cmath>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{
/** Defined further below, alongside tree_search_periodic_packing(). */
std::vector<PeriodicItemPacking> assemble_periodic_packings(const Instance& instance);
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////// compute_periodic_blocks ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * Return true iff 'block_1' dominates 'block_2': it contains the same item
 * types, at least as many copies of each of them, and takes no more space in
 * both dimensions.
 *
 * Blocks with different sets of item types are never compared: a block
 * using copies of an item type that 'block_2' doesn't use might not be
 * usable when 'block_2' is.
 */
static bool dominates(
        const PeriodicBlock& block_1,
        const PeriodicBlock& block_2)
{
    if (block_1.item_copies.size() != block_2.item_copies.size())
        return false;
    for (const std::pair<ItemTypeId, ItemPos>& copies_2: block_2.item_copies) {
        bool found = false;
        for (const std::pair<ItemTypeId, ItemPos>& copies_1: block_1.item_copies) {
            if (copies_1.first == copies_2.first) {
                if (copies_1.second < copies_2.second)
                    return false;
                found = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return !shape::strictly_greater(block_1.bx, block_2.bx)
        && !shape::strictly_greater(block_1.by, block_2.by);
}

/**
 * Return true iff 'block' is dominated by a block of 'blocks' (see
 * 'add_block_if_not_dominated'), without needing its items.
 */
static bool is_dominated(
        const std::vector<PeriodicBlock>& blocks,
        const PeriodicBlock& block)
{
    if (block.number_of_items == 1)
        return false;
    for (const PeriodicBlock& existing: blocks)
        if (dominates(existing, block))
            return true;
    return false;
}

/**
 * Add 'block' to 'blocks' unless it is dominated by a block already present,
 * pruning greedily as candidates are generated rather than collecting every
 * candidate and filtering the whole list afterwards.
 *
 * A block is dominated by another block that packs the same item types, at
 * least as many copies of each, into no more space in both dimensions (see
 * 'dominates') -- the tree search can then always prefer that other block,
 * since it is never worse in items, bx, or by. This also removes any
 * existing blocks that the newly added one now dominates.
 *
 * A single-item block is exempted from all of this and always kept
 * unconditionally: it is the only way to place an item type's very last
 * remaining copy, so it must never be discarded as "dominated" by a block
 * that needs more copies than might still be available -- e.g. when only 1
 * copy is left of an item type whose only small block interlocks 2 copies
 * per unit cell. Discarding it would leave that last copy with no block
 * able to place it at all, even though there may be plenty of free space
 * for it individually.
 */
static void add_block_if_not_dominated(
        std::vector<PeriodicBlock>& blocks,
        PeriodicBlock&& block)
{
    if (block.number_of_items == 1) {
        blocks.push_back(std::move(block));
        return;
    }

    for (const PeriodicBlock& existing: blocks)
        if (dominates(existing, block))
            return;

    // Drop existing blocks now dominated by 'block' (except single-item
    // ones, which are never removed), swapping each one with the last
    // element and popping instead of erase/remove_if, since order doesn't
    // matter here. When 'i' is itself the last element, skip the
    // self-move-assignment (blocks[i] = std::move(blocks[i])) and just pop:
    // moving a vector-holding struct into itself is not guaranteed safe and
    // was observed to corrupt the heap (double free) in practice.
    for (int i = 0; i < (int)blocks.size(); ) {
        if (blocks[i].number_of_items != 1
                && dominates(block, blocks[i])) {
            if (i != (int)blocks.size() - 1)
                blocks[i] = std::move(blocks.back());
            blocks.pop_back();
        } else {
            ++i;
        }
    }

    blocks.push_back(std::move(block));
}

/**
 * Usable bin bounding-box dimension (scaled) along one axis: shrunk on
 * every side by instance.bin_spacing_scaled(bin_type_id), then grown back
 * by one extra instance.item_spacing_scaled(). See
 * BranchingSchemePeriodicPacking::bin_bx()/by()'s comment for why: that
 * extra margin exactly cancels the trailing item_spacing_scaled() every
 * generated block's own bx/by bakes in (see compute_periodic_blocks_for_item_type
 * / add_aabb_grid_blocks_for_item_type below), so a chain of blocks that
 * truly fits the bin is not rejected just because each block, considered in
 * isolation, over-counts by one spacing past its own last copy.
 *
 * Shared by those two block-construction functions (which run before any
 * BranchingSchemePeriodicPacking exists, since they build the blocks_ its
 * constructor needs) and by the class's own bin_bx()/by() methods, which
 * just forward here.
 */
static LengthDbl usable_bin_bx(const Instance& instance, BinTypeId bin_type_id)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    return bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min
        - 2.0 * instance.bin_spacing_scaled(bin_type_id)
        + instance.item_spacing_scaled();
}

static LengthDbl usable_bin_by(const Instance& instance, BinTypeId bin_type_id)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    return bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min
        - 2.0 * instance.bin_spacing_scaled(bin_type_id)
        + instance.item_spacing_scaled();
}

/**
 * Add plain AABB-grid blocks for a single item type and its rotations, as a
 * fallback alongside the periodic-packing-derived blocks: for a rotation
 * whose shape has no valid periodic packing (e.g. the NFP-based lattice
 * search finds nothing, or none of the pre-filtered candidates pass
 * check_periodic_packing), compute_periodic_blocks_for_item_type would
 * otherwise never produce a single block for it, making it impossible for
 * the branching scheme to ever place it at all. A simple non-overlapping
 * nx x ny grid tiling of the item's own bounding box (no interlocking, so no
 * per-cell overhang to account for) is always valid regardless of shape, and
 * gives every item type at least some usable blocks. Blocks that are worse
 * than a periodic-packing-derived block with the same items are pruned by
 * the domination filter in compute_periodic_blocks_for_item_type like any
 * other block.
 */
static void add_aabb_grid_blocks_for_item_type(
        const Instance& instance,
        ItemTypeId item_type_id,
        const std::vector<ItemTypeRotation>& rotations,
        std::vector<PeriodicBlock>& blocks)
{
    const ItemType& item_type = instance.item_type(item_type_id);
    if (item_type.copies < 1)
        return;

    BinTypeId bin_type_id = instance.bin_type_id(0);
    LengthDbl item_spacing = instance.item_spacing_scaled();
    LengthDbl bin_bx = usable_bin_bx(instance, bin_type_id);
    LengthDbl bin_by = usable_bin_by(instance, bin_type_id);

    for (const ItemTypeRotation& rotation: rotations) {
        ShapeWithHoles shape = get_item_combined_shape(instance, item_type_id, rotation);
        AxisAlignedBoundingBox aabb = shape.compute_min_max();
        LengthDbl item_bx = aabb.x_max - aabb.x_min;
        LengthDbl item_by = aabb.y_max - aabb.y_min;
        if (shape::strictly_greater(item_bx, bin_bx)
                || shape::strictly_greater(item_by, bin_by))
            continue;

        // Each item occupies a pitch_x x pitch_y cell (its own size plus a
        // trailing item_spacing gap), so an n-copy grid reports a footprint
        // of n * pitch_x: n - 1 gaps between consecutive copies, plus one
        // more trailing gap past the last copy. That trailing gap is never
        // needed by the items *within* this block, but the branching scheme
        // treats block.bx/by as this block's whole reserved footprint, so
        // whatever ends up placed flush against this block's high edge --
        // another block, most commonly -- is guaranteed to land at least
        // item_spacing away from this block's actual items (mirroring how
        // bin_bx()/bin_by() attribute the full item_bin_minimum_spacing to
        // one side only, rather than splitting it between both neighbors).
        LengthDbl pitch_x = item_bx + item_spacing;
        LengthDbl pitch_y = item_by + item_spacing;
        int max_nx = 0;
        while (max_nx < item_type.copies
                && !shape::strictly_greater((max_nx + 1) * pitch_x, bin_bx))
            ++max_nx;
        int max_ny = 0;
        while (max_ny < item_type.copies
                && !shape::strictly_greater((max_ny + 1) * pitch_y, bin_by))
            ++max_ny;

        for (int nx = 1; nx <= max_nx; ++nx) {
            for (int ny = 1; ny <= max_ny; ++ny) {
                int total_cells = nx * ny;
                if (total_cells > item_type.copies)
                    break;

                PeriodicBlock block;
                block.nx = nx;
                block.ny = ny;
                block.bx = nx * pitch_x;
                block.by = ny * pitch_y;
                block.number_of_items = total_cells;
                block.item_area = total_cells * item_type.area_scaled;
                block.item_profit = total_cells * item_type.profit;
                block.item_copies.push_back({item_type_id, total_cells});
                block.items.reserve(total_cells);

                for (int i = 0; i < nx; ++i) {
                    for (int j = 0; j < ny; ++j) {
                        SolutionItem placed_item;
                        placed_item.item_type_id = item_type_id;
                        placed_item.angle = rotation.angle;
                        placed_item.mirror = rotation.mirror;
                        placed_item.bl_corner.x = i * pitch_x - aabb.x_min;
                        placed_item.bl_corner.y = j * pitch_y - aabb.y_min;
                        block.items.push_back(placed_item);
                    }
                }

                add_block_if_not_dominated(blocks, std::move(block));
            }
        }
    }
}

/**
 * Add the periodic blocks of a periodic packing, built from the periodic
 * packing clipped to the bin.
 *
 * The bin is filled with the periodic packing: every copy of an item of the
 * unit cell, translated by a lattice vector, which fits in the bin. Then a
 * block is made, for each pair (H, W) of the top of an item and of the
 * right of an item, of the items whose top is at most H and whose right is
 * at most W: these are the blocks obtained by removing repeatedly the
 * highest or the most right items. Unlike tilings of whole unit cells, these
 * blocks may contain partial unit cells, and no row of unit cells sticks out
 * of the others when vector_2 is not perpendicular to vector_1. Any set of
 * copies of the items translated by lattice vectors is part of the periodic
 * packing, so the blocks are valid.
 *
 * The number of items fitting in the bin depends on the position of the
 * lattice, so the bin is filled for several positions, each one with an
 * item of the unit cell against the left side of the bin and an item
 * against its bottom side.
 *
 * Return false, without adding any block, if the bin holds too many copies
 * of the items: the blocks are then built from whole unit cells instead
 * (see 'add_periodic_blocks_for_packing').
 */
static bool add_clipped_lattice_blocks(
        const Instance& instance,
        int packing_id,
        const PeriodicItemPacking& packing,
        std::vector<PeriodicBlock>& blocks)
{
    // Maximum number of copies of the items in the bin.
    const ItemPos maximum_number_of_items = 500;

    BinTypeId bin_type_id = instance.bin_type_id(0);
    LengthDbl item_spacing = instance.item_spacing_scaled();
    // Dimensions available for the items: the dimensions of a block include
    // a trailing item-item spacing (see 'add_periodic_blocks_for_packing').
    LengthDbl width = usable_bin_bx(instance, bin_type_id) - item_spacing;
    LengthDbl height = usable_bin_by(instance, bin_type_id) - item_spacing;

    const Point& vector_1 = packing.vector_1;
    const Point& vector_2 = packing.vector_2;
    LengthDbl determinant = vector_1.x * vector_2.y - vector_1.y * vector_2.x;
    if (shape::equal(determinant, 0.0))
        return false;

    // Bounding boxes of the items of the unit cell.
    ItemPos number_of_cell_items = packing.items.size();
    std::vector<AxisAlignedBoundingBox> item_aabbs;
    for (const SolutionItem& item: packing.items) {
        ShapeWithHoles shape = get_item_combined_shape(
                instance,
                item.item_type_id,
                {item.angle, item.mirror});
        AxisAlignedBoundingBox aabb = shape.compute_min_max();
        aabb.shift(item.bl_corner);
        item_aabbs.push_back(aabb);
    }
    LengthDbl cell_size = (std::max)(
            packing.aabb_scaled.x_max - packing.aabb_scaled.x_min,
            packing.aabb_scaled.y_max - packing.aabb_scaled.y_min);

    // A copy of an item of the unit cell: the position of the item in the
    // unit cell and the lattice vector it is translated by.
    struct LatticeItem
    {
        ItemPos item_pos;
        Point translation;
        AxisAlignedBoundingBox aabb;
    };

    // Copies of the items of the unit cell, for a given position of the
    // lattice, inside [x_min, x_max] x [y_min, y_max].
    auto lattice_items = [&](
            const Point& lattice_position,
            LengthDbl x_min,
            LengthDbl x_max,
            LengthDbl y_min,
            LengthDbl y_max,
            std::vector<LatticeItem>& output)
    {
        output.clear();
        // Range of the lattice coordinates (n, m) of the translations n *
        // vector_1 + m * vector_2 to consider.
        LengthDbl n_min = +std::numeric_limits<LengthDbl>::infinity();
        LengthDbl n_max = -std::numeric_limits<LengthDbl>::infinity();
        LengthDbl m_min = +std::numeric_limits<LengthDbl>::infinity();
        LengthDbl m_max = -std::numeric_limits<LengthDbl>::infinity();
        for (LengthDbl x: {x_min - 2 * cell_size, x_max + 2 * cell_size}) {
            for (LengthDbl y: {y_min - 2 * cell_size, y_max + 2 * cell_size}) {
                LengthDbl dx = x - lattice_position.x;
                LengthDbl dy = y - lattice_position.y;
                LengthDbl n = (dx * vector_2.y - dy * vector_2.x) / determinant;
                LengthDbl m = (vector_1.x * dy - vector_1.y * dx) / determinant;
                n_min = (std::min)(n_min, n);
                n_max = (std::max)(n_max, n);
                m_min = (std::min)(m_min, m);
                m_max = (std::max)(m_max, m);
            }
        }
        for (long long n = (long long)std::floor(n_min); n <= (long long)std::ceil(n_max); ++n) {
            for (long long m = (long long)std::floor(m_min); m <= (long long)std::ceil(m_max); ++m) {
                Point translation = {
                    lattice_position.x + n * vector_1.x + m * vector_2.x,
                    lattice_position.y + n * vector_1.y + m * vector_2.y};
                for (ItemPos item_pos = 0; item_pos < number_of_cell_items; ++item_pos) {
                    AxisAlignedBoundingBox aabb = item_aabbs[item_pos];
                    aabb.shift(translation);
                    if (shape::strictly_lesser(aabb.x_min, x_min)
                            || shape::strictly_greater(aabb.x_max, x_max)
                            || shape::strictly_lesser(aabb.y_min, y_min)
                            || shape::strictly_greater(aabb.y_max, y_max)) {
                        continue;
                    }
                    output.push_back({item_pos, translation, aabb});
                    if ((ItemPos)output.size() > maximum_number_of_items)
                        return false;
                }
            }
        }
        return true;
    };

    // Positions of the lattice.
    std::vector<Point> lattice_positions;
    std::vector<LatticeItem> items;
    for (ItemPos item_pos_left = 0; item_pos_left < number_of_cell_items; ++item_pos_left) {
        for (ItemPos item_pos_bottom = 0; item_pos_bottom < number_of_cell_items; ++item_pos_bottom) {
            // The copy of 'item_pos_left' at the origin of the lattice
            // against the left side of the bin, and the lowest copy of
            // 'item_pos_bottom' in the bin against its bottom side.
            Point lattice_position = {-item_aabbs[item_pos_left].x_min, 0.0};
            // Lowest copy of 'item_pos_bottom' above the origin, among those
            // between the left and the right sides of the bin.
            LengthDbl lowest = +std::numeric_limits<LengthDbl>::infinity();
            std::vector<LatticeItem> strip_items;
            if (!lattice_items(
                        lattice_position,
                        0.0, width,
                        0.0, height,
                        strip_items)) {
                return false;
            }
            for (const LatticeItem& lattice_item: strip_items) {
                if (lattice_item.item_pos == item_pos_bottom
                        && lattice_item.aabb.y_min < lowest) {
                    lowest = lattice_item.aabb.y_min;
                }
            }
            if (lowest == std::numeric_limits<LengthDbl>::infinity())
                continue;
            lattice_position.y -= lowest;
            bool duplicate = false;
            for (const Point& previous_position: lattice_positions) {
                // Same lattice, up to a lattice vector.
                LengthDbl dx = lattice_position.x - previous_position.x;
                LengthDbl dy = lattice_position.y - previous_position.y;
                LengthDbl n = (dx * vector_2.y - dy * vector_2.x) / determinant;
                LengthDbl m = (vector_1.x * dy - vector_1.y * dx) / determinant;
                if (shape::equal(n, std::round(n)) && shape::equal(m, std::round(m)))
                    duplicate = true;
            }
            if (!duplicate)
                lattice_positions.push_back(lattice_position);
        }
    }
    if (lattice_positions.empty())
        return false;

    // Fill the bin for each position of the lattice, then move the items
    // against the bottom and the left sides of the bin, which may make room
    // for more items. All the fillings are computed before adding any block,
    // so that no block is added if there are too many items.
    std::vector<std::vector<LatticeItem>> fillings;
    for (Point lattice_position: lattice_positions) {
        for (int iteration = 0; iteration < 3; ++iteration) {
            if (!lattice_items(lattice_position, 0.0, width, 0.0, height, items))
                return false;
            if (items.empty())
                break;
            LengthDbl items_x_min = std::numeric_limits<LengthDbl>::infinity();
            LengthDbl items_y_min = std::numeric_limits<LengthDbl>::infinity();
            for (const LatticeItem& lattice_item: items) {
                items_x_min = (std::min)(items_x_min, lattice_item.aabb.x_min);
                items_y_min = (std::min)(items_y_min, lattice_item.aabb.y_min);
            }
            if (!shape::strictly_greater(items_x_min, 0.0)
                    && !shape::strictly_greater(items_y_min, 0.0)) {
                break;
            }
            lattice_position.x -= items_x_min;
            lattice_position.y -= items_y_min;
        }
        if (!items.empty())
            fillings.push_back(items);
    }

    for (const std::vector<LatticeItem>& items: fillings) {

        // Blocks: for each top H, the items whose top is at most H, sorted
        // by right; then for each right W, those whose right is at most W.
        std::vector<LengthDbl> tops;
        for (const LatticeItem& lattice_item: items)
            tops.push_back(lattice_item.aabb.y_max);
        std::sort(tops.begin(), tops.end());
        tops.erase(std::unique(tops.begin(), tops.end(),
                    [](LengthDbl a, LengthDbl b) { return shape::equal(a, b); }),
                tops.end());
        std::vector<ItemPos> sorted_items(items.size());
        for (ItemPos pos = 0; pos < (ItemPos)items.size(); ++pos)
            sorted_items[pos] = pos;
        std::sort(sorted_items.begin(), sorted_items.end(),
                [&items](ItemPos pos_1, ItemPos pos_2)
                {
                    return items[pos_1].aabb.x_max < items[pos_2].aabb.x_max;
                });
        for (LengthDbl top: tops) {
            optimizationtools::IndexedMap<ItemPos> copies_map(
                    instance.number_of_item_types(), 0);
            AxisAlignedBoundingBox block_aabb;
            std::vector<ItemPos> block_items;
            AreaDbl item_area = 0.0;
            Profit item_profit = 0;
            bool too_many_copies = false;
            // Items whose top is at most 'top', sorted by right.
            std::vector<ItemPos> top_items;
            for (ItemPos pos: sorted_items)
                if (!shape::strictly_greater(items[pos].aabb.y_max, top))
                    top_items.push_back(pos);
            for (ItemPos sorted_pos = 0; sorted_pos < (ItemPos)top_items.size(); ++sorted_pos) {
                const LatticeItem& lattice_item = items[top_items[sorted_pos]];
                const SolutionItem& item = packing.items[lattice_item.item_pos];
                const ItemType& item_type = instance.item_type(item.item_type_id);
                copies_map.set(item.item_type_id, copies_map[item.item_type_id] + 1);
                if (copies_map[item.item_type_id] > item_type.copies) {
                    too_many_copies = true;
                    break;
                }
                block_items.push_back(top_items[sorted_pos]);
                block_aabb = shape::merge(block_aabb, lattice_item.aabb);
                item_area += item_type.area_scaled;
                item_profit += item_type.profit;
                // Only build a block once all the items with the same right
                // have been added.
                if (sorted_pos + 1 < (ItemPos)top_items.size()
                        && shape::equal(
                            items[top_items[sorted_pos + 1]].aabb.x_max,
                            lattice_item.aabb.x_max)) {
                    continue;
                }

                PeriodicBlock block;
                block.packing_id = packing_id;
                block.bx = block_aabb.x_max - block_aabb.x_min + item_spacing;
                block.by = block_aabb.y_max - block_aabb.y_min + item_spacing;
                block.number_of_items = block_items.size();
                block.item_area = item_area;
                block.item_profit = item_profit;
                block.item_copies = std::vector<std::pair<ItemTypeId, ItemPos>>(
                        copies_map.begin(),
                        copies_map.end());
                if (is_dominated(blocks, block))
                    continue;
                block.items.reserve(block_items.size());
                for (ItemPos pos: block_items) {
                    const LatticeItem& block_item = items[pos];
                    SolutionItem placed_item = packing.items[block_item.item_pos];
                    placed_item.bl_corner.x += block_item.translation.x - block_aabb.x_min;
                    placed_item.bl_corner.y += block_item.translation.y - block_aabb.y_min;
                    block.items.push_back(placed_item);
                }
                add_block_if_not_dominated(blocks, std::move(block));
            }
            if (too_many_copies)
                continue;
        }
    }
    return true;
}

/**
 * Add the periodic blocks of a periodic packing: nx × ny tilings of its unit
 * cell, bounded both by the available copies of its item types and by the
 * bin dimensions.
 */
static void add_periodic_blocks_for_packing(
        const Instance& instance,
        int packing_id,
        const PeriodicItemPacking& packing,
        std::vector<PeriodicBlock>& blocks)
{
    BinTypeId bin_type_id = instance.bin_type_id(0);
    LengthDbl item_spacing = instance.item_spacing_scaled();
    LengthDbl bin_bx = usable_bin_bx(instance, bin_type_id);
    LengthDbl bin_by = usable_bin_by(instance, bin_type_id);


    // Unit-cell bounding box dimensions.
    LengthDbl unit_bx = packing.aabb_scaled.x_max - packing.aabb_scaled.x_min;
    LengthDbl unit_by = packing.aabb_scaled.y_max - packing.aabb_scaled.y_min;

    if (shape::strictly_greater(unit_bx, bin_bx) || shape::strictly_greater(unit_by, bin_by))
        return;

    // Precompute item copies and profit per unit cell. Item type ids are
    // dense small integers, so an IndexedMap (O(1) get/set, no per-key
    // allocation) is a better fit here than std::map.
    optimizationtools::IndexedMap<ItemPos> unit_copies_map(
            instance.number_of_item_types(), 0);
    AreaDbl unit_item_area = 0.0;
    Profit unit_item_profit = 0;
    for (const SolutionItem& item: packing.items) {
        unit_copies_map.set(item.item_type_id, unit_copies_map[item.item_type_id] + 1);
        unit_item_area += instance.item_type(item.item_type_id).area_scaled;
        unit_item_profit += instance.item_type(item.item_type_id).profit;
    }
    std::vector<std::pair<ItemTypeId, ItemPos>> unit_copies(
            unit_copies_map.begin(), unit_copies_map.end());

    // A block with total_cells unit-cell repetitions must not use more
    // copies of any item type than the instance provides.
    int max_total_cells = std::numeric_limits<int>::max();
    for (const std::pair<ItemTypeId, ItemPos>& kv: unit_copies) {
        ItemPos limit = instance.item_type(kv.first).copies;
        max_total_cells = std::min(max_total_cells, (int)(limit / kv.second));
    }
    if (max_total_cells < 1)
        return;

    if (add_clipped_lattice_blocks(instance, packing_id, packing, blocks))
        return;

    // Row (index j, stepped by vector_2) offset, with drift along
    // vector_1's direction wrapped back by an integer number of
    // vector_1 translations. Without this, a vector_2 that is not
    // parallel/perpendicular to vector_1 (i.e. not axis-aligned once
    // vector_1 is) shears the tiling into a parallelogram: each row is
    // offset further along vector_1's direction than the last, forcing
    // the block's rectangular AABB to grow to contain the full drift
    // and leaving two opposite corners empty. Since columns (index i)
    // already repeat every vector_1, translating an entire row by a
    // whole multiple of vector_1 just selects a different (but equally
    // valid, still gapless) window of the same infinite periodic row;
    // choosing the multiple that cancels the row's drift keeps every
    // row's window aligned with the others, so the AABB stays close to
    // the items' actual footprint instead of growing with ny.
    LengthDbl v1_dot_v1 = packing.vector_1.x * packing.vector_1.x
        + packing.vector_1.y * packing.vector_1.y;
    LengthDbl v1_dot_v2 = packing.vector_1.x * packing.vector_2.x
        + packing.vector_1.y * packing.vector_2.y;
    std::vector<LengthDbl> row_offset_x = {0.0};
    std::vector<LengthDbl> row_offset_y = {0.0};
    auto ensure_row_offset = [&](int j)
    {
        while ((int)row_offset_x.size() <= j) {
            int jj = (int)row_offset_x.size();
            double k = (v1_dot_v1 > 0.0)?
                std::round(jj * v1_dot_v2 / v1_dot_v1): 0.0;
            row_offset_x.push_back(jj * packing.vector_2.x - k * packing.vector_1.x);
            row_offset_y.push_back(jj * packing.vector_2.y - k * packing.vector_1.y);
        }
    };

    for (int nx = 1; ; ++nx) {
        for (int ny = 1; ; ++ny) {
            // Compute the AABB of the (nx, ny) tiling iteratively.
            // Copy (i, j) is offset by i * v1 + j * v2 from the reference.
            // Each copy's AABB is the AABB of the unit cell shifted by the
            // offset: the AABB of the unit cell doesn't necessarily start
            // at (0, 0) (e.g. for the periodic packings of
            // compute_wrapping_periodic_packings).
            LengthDbl tiling_x_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl tiling_x_max = -std::numeric_limits<LengthDbl>::infinity();
            LengthDbl tiling_y_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl tiling_y_max = -std::numeric_limits<LengthDbl>::infinity();
            for (int j = 0; j < ny; ++j)
                ensure_row_offset(j);
            for (int i = 0; i < nx; ++i) {
                for (int j = 0; j < ny; ++j) {
                    LengthDbl cx = i * packing.vector_1.x + row_offset_x[j];
                    LengthDbl cy = i * packing.vector_1.y + row_offset_y[j];
                    tiling_x_min = std::min(tiling_x_min, cx + packing.aabb_scaled.x_min);
                    tiling_x_max = std::max(tiling_x_max, cx + packing.aabb_scaled.x_max);
                    tiling_y_min = std::min(tiling_y_min, cy + packing.aabb_scaled.y_min);
                    tiling_y_max = std::max(tiling_y_max, cy + packing.aabb_scaled.y_max);
                }
            }
            // The + item_spacing here is a trailing margin past the
            // tiling's own tight footprint, not needed by the items
            // within this block (packing.vector_1/vector_2 already keep
            // periodic copies item_spacing apart, that's what
            // compute_periodic_packings solved for) but reserved so that
            // whatever the branching scheme places flush against this
            // block's high edge -- another block, most commonly -- ends
            // up at least item_spacing away from this block's actual
            // items (see add_aabb_grid_blocks_for_item_type's comment
            // for the same convention applied to its grid blocks).
            LengthDbl block_bx = tiling_x_max - tiling_x_min + item_spacing;
            LengthDbl block_by = tiling_y_max - tiling_y_min + item_spacing;

            // Stop growing ny once the block no longer fits in the bin.
            if (shape::strictly_greater(block_by, bin_by))
                break;
            // Still check x: skip this (nx, ny) but continue ny loop if bx fits.
            if (shape::strictly_greater(block_bx, bin_bx))
                break;
            // Stop growing ny once the block would use more copies of an
            // item type than the instance provides.
            if (nx * ny > max_total_cells)
                break;

            // Build the PeriodicBlock.
            PeriodicBlock block;
            block.packing_id = packing_id;
            block.nx = nx;
            block.ny = ny;
            block.bx = block_bx;
            block.by = block_by;

            int total_cells = nx * ny;
            block.items.reserve(total_cells * (int)packing.items.size());
            block.number_of_items = total_cells * (int)packing.items.size();
            block.item_area = total_cells * unit_item_area;
            block.item_profit = total_cells * unit_item_profit;

            for (const std::pair<ItemTypeId, ItemPos>& kv: unit_copies)
                block.item_copies.push_back({kv.first, total_cells * kv.second});

            for (int i = 0; i < nx; ++i) {
                for (int j = 0; j < ny; ++j) {
                    LengthDbl offset_x = i * packing.vector_1.x + row_offset_x[j]
                        - tiling_x_min;
                    LengthDbl offset_y = i * packing.vector_1.y + row_offset_y[j]
                        - tiling_y_min;
                    for (const SolutionItem& item: packing.items) {
                        SolutionItem placed_item = item;
                        placed_item.bl_corner.x = item.bl_corner.x + offset_x;
                        placed_item.bl_corner.y = item.bl_corner.y + offset_y;
                        block.items.push_back(placed_item);
                    }
                }
            }

            add_block_if_not_dominated(blocks, std::move(block));
        }

        // Stop growing nx once even the smallest possible block (ny = 1)
        // in this nx direction no longer fits the bin in either dimension:
        // growing nx further can only make the block larger, never
        // smaller, so there is no point trying larger values.
        {
            LengthDbl col_x_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl col_x_max = -std::numeric_limits<LengthDbl>::infinity();
            LengthDbl col_y_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl col_y_max = -std::numeric_limits<LengthDbl>::infinity();
            for (int i = 0; i < nx; ++i) {
                LengthDbl cx = i * packing.vector_1.x;
                LengthDbl cy = i * packing.vector_1.y;
                col_x_min = std::min(col_x_min, cx + packing.aabb_scaled.x_min);
                col_x_max = std::max(col_x_max, cx + packing.aabb_scaled.x_max);
                col_y_min = std::min(col_y_min, cy + packing.aabb_scaled.y_min);
                col_y_max = std::max(col_y_max, cy + packing.aabb_scaled.y_max);
            }
            LengthDbl candidate_bx = col_x_max - col_x_min;
            LengthDbl candidate_by = col_y_max - col_y_min;
            if (shape::strictly_greater(candidate_bx, bin_bx)
                    || shape::strictly_greater(candidate_by, bin_by)
                    || nx >= max_total_cells)
                break;
        }
    }
}

std::vector<PeriodicBlock> packingsolver::irregular::compute_periodic_blocks_for_item_type(
        const Instance& instance,
        ItemTypeId item_type_id,
        const std::vector<PeriodicItemPacking>& packings,
        const std::vector<ItemTypeRotation>& rotations)
{
    if (instance.number_of_bin_types() == 0)
        return {};

    std::vector<PeriodicBlock> blocks;

    for (int packing_id = 0; packing_id < (int)packings.size(); ++packing_id)
        add_periodic_blocks_for_packing(instance, packing_id, packings[packing_id], blocks);

    // Blocks are already pruned as they are generated (see
    // add_block_if_not_dominated): 'blocks' only ever holds the current
    // Pareto-optimal set (across bx, by, and number of copies of this item
    // type), so there is nothing left to filter here.
    add_aabb_grid_blocks_for_item_type(instance, item_type_id, rotations, blocks);

    return blocks;
}

/**
 * An item type in a rotation which can frame blocks: its bounding box has a
 * large empty rectangle touching one of its sides (see
 * compute_largest_empty_side_rectangle), like an L-shape.
 */
struct FrameItem
{
    ItemTypeId item_type_id;

    ItemTypeRotation rotation;

    /** Bounding box of the item in this rotation. */
    AxisAlignedBoundingBox aabb;

    /** Empty rectangle. */
    AxisAlignedBoundingBox rectangle;

    /** Sides of 'rectangle' which are not on the sides of 'aabb'. */
    bool left;
    bool right;
    bool bottom;
    bool top;

    /** Rectangles covering 'aabb' outside of 'rectangle', so containing the item. */
    std::vector<AxisAlignedBoundingBox> occupied;
};

/**
 * Return true iff two rectangles are closer than 'spacing'.
 */
static bool rectangles_too_close(
        const AxisAlignedBoundingBox& rectangle_1,
        const AxisAlignedBoundingBox& rectangle_2,
        LengthDbl spacing)
{
    return shape::strictly_lesser(rectangle_1.x_min, rectangle_2.x_max + spacing)
        && shape::strictly_lesser(rectangle_2.x_min, rectangle_1.x_max + spacing)
        && shape::strictly_lesser(rectangle_1.y_min, rectangle_2.y_max + spacing)
        && shape::strictly_lesser(rectangle_2.y_min, rectangle_1.y_max + spacing);
}

static AxisAlignedBoundingBox shifted(
        AxisAlignedBoundingBox rectangle,
        const Point& translation)
{
    rectangle.shift(translation);
    return rectangle;
}

/**
 * Compute the frame items of the instance.
 */
static std::vector<FrameItem> compute_frame_items(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations)
{
    // Minimum ratio between the area of the empty rectangle and the area of
    // the bounding box.
    const double minimum_ratio = 0.5;

    std::vector<FrameItem> frame_items;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        for (const ItemTypeRotation& rotation: item_type_rotations[item_type_id]) {
            ShapeWithHoles shape = get_item_combined_shape(instance, item_type_id, rotation);
            AxisAlignedBoundingBox aabb = shape.compute_min_max();
            AreaDbl aabb_area = (aabb.x_max - aabb.x_min) * (aabb.y_max - aabb.y_min);
            LengthDbl segment_length = (std::max)(
                    aabb.x_max - aabb.x_min,
                    aabb.y_max - aabb.y_min) / 128;
            AxisAlignedBoundingBox best_rectangle;
            AreaDbl best_area = 0.0;
            for (BoundingBoxSide side: {
                    BoundingBoxSide::Bottom,
                    BoundingBoxSide::Top,
                    BoundingBoxSide::Left,
                    BoundingBoxSide::Right}) {
                AxisAlignedBoundingBox rectangle = compute_largest_empty_side_rectangle(
                        shape, side, segment_length);
                if (rectangle.x_min > rectangle.x_max)
                    continue;
                AreaDbl area = (rectangle.x_max - rectangle.x_min) * (rectangle.y_max - rectangle.y_min);
                if (area > best_area) {
                    best_area = area;
                    best_rectangle = rectangle;
                }
            }
            if (best_area < minimum_ratio * aabb_area)
                continue;
            FrameItem frame_item;
            frame_item.item_type_id = item_type_id;
            frame_item.rotation = rotation;
            frame_item.aabb = aabb;
            frame_item.rectangle = best_rectangle;
            frame_item.left = shape::strictly_greater(best_rectangle.x_min, aabb.x_min);
            frame_item.right = shape::strictly_lesser(best_rectangle.x_max, aabb.x_max);
            frame_item.bottom = shape::strictly_greater(best_rectangle.y_min, aabb.y_min);
            frame_item.top = shape::strictly_lesser(best_rectangle.y_max, aabb.y_max);
            if (frame_item.left)
                frame_item.occupied.push_back({aabb.x_min, best_rectangle.x_min, aabb.y_min, aabb.y_max});
            if (frame_item.right)
                frame_item.occupied.push_back({best_rectangle.x_max, aabb.x_max, aabb.y_min, aabb.y_max});
            if (frame_item.bottom)
                frame_item.occupied.push_back({best_rectangle.x_min, best_rectangle.x_max, aabb.y_min, best_rectangle.y_min});
            if (frame_item.top)
                frame_item.occupied.push_back({best_rectangle.x_min, best_rectangle.x_max, best_rectangle.y_max, aabb.y_max});
            frame_items.push_back(frame_item);
        }
    }
    return frame_items;
}

/**
 * Translation of a frame item such that a rectangle 'content' is in its
 * empty rectangle, against the sides of the empty rectangle which are along
 * the item, at the item-item minimum spacing.
 *
 * Return false if 'content' doesn't fit between two opposite such sides.
 */
static bool frame_item_position(
        const FrameItem& frame_item,
        const AxisAlignedBoundingBox& content,
        LengthDbl spacing,
        Point& position)
{
    const AxisAlignedBoundingBox& rectangle = frame_item.rectangle;
    if (frame_item.left && frame_item.right
            && shape::strictly_greater(
                content.x_max - content.x_min + 2 * spacing,
                rectangle.x_max - rectangle.x_min)) {
        return false;
    }
    if (frame_item.bottom && frame_item.top
            && shape::strictly_greater(
                content.y_max - content.y_min + 2 * spacing,
                rectangle.y_max - rectangle.y_min)) {
        return false;
    }
    if (frame_item.left) {
        position.x = content.x_min - spacing - rectangle.x_min;
    } else if (frame_item.right) {
        position.x = content.x_max + spacing - rectangle.x_max;
    } else {
        position.x = content.x_min - rectangle.x_min;
    }
    if (frame_item.bottom) {
        position.y = content.y_min - spacing - rectangle.y_min;
    } else if (frame_item.top) {
        position.y = content.y_max + spacing - rectangle.y_max;
    } else {
        position.y = content.y_min - rectangle.y_min;
    }
    return true;
}

/**
 * Move a frame item at 'position' away from the content of the block, along
 * the direction 'direction' (a unit axis-aligned vector), until its occupied
 * rectangles are not too close to the obstacles anymore.
 *
 * Return false if it is not possible within a reasonable distance.
 */
static bool slide_frame_item(
        const FrameItem& frame_item,
        const std::vector<AxisAlignedBoundingBox>& obstacles,
        LengthDbl spacing,
        const Point& direction,
        Point& position)
{
    for (int iteration = 0; iteration < 32; ++iteration) {
        LengthDbl shift = 0.0;
        for (const AxisAlignedBoundingBox& occupied: frame_item.occupied) {
            AxisAlignedBoundingBox placed = shifted(occupied, position);
            for (const AxisAlignedBoundingBox& obstacle: obstacles) {
                if (!rectangles_too_close(placed, obstacle, spacing))
                    continue;
                LengthDbl needed = 0.0;
                if (direction.x > 0)
                    needed = obstacle.x_max + spacing - placed.x_min;
                else if (direction.x < 0)
                    needed = placed.x_max - (obstacle.x_min - spacing);
                else if (direction.y > 0)
                    needed = obstacle.y_max + spacing - placed.y_min;
                else
                    needed = placed.y_max - (obstacle.y_min - spacing);
                shift = (std::max)(shift, needed);
            }
        }
        if (!shape::strictly_greater(shift, 0.0))
            return true;
        position.x += shift * direction.x;
        position.y += shift * direction.y;
    }
    return false;
}

/**
 * Add the blocks obtained by framing the blocks of 'blocks' with frame
 * items (see 'FrameItem'): with 1 frame item in a rotation r, 2 frame items
 * in rotations r and r + 180, or 4 frame items in rotations r, r + 90,
 * r + 180 and r + 270. The frame items are placed using only their
 * bounding box and their empty rectangle, and the bounding box of the
 * block, so the framed blocks are valid without computing any no-fit
 * polygon.
 *
 * A framed block is only kept if it takes no more space than the block plus
 * the frame items packed alone, at the best density of the blocks made of
 * this item type only. The framed blocks are framed again, up to a depth of
 * 2.
 */
static void add_framed_blocks(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations,
        std::vector<PeriodicBlock>& blocks)
{
    const int maximum_depth = 2;

    if (instance.number_of_bin_types() == 0)
        return;
    std::vector<FrameItem> frame_items = compute_frame_items(instance, item_type_rotations);
    if (frame_items.empty())
        return;

    BinTypeId bin_type_id = instance.bin_type_id(0);
    LengthDbl spacing = instance.item_spacing_scaled();
    LengthDbl bin_bx = usable_bin_bx(instance, bin_type_id);
    LengthDbl bin_by = usable_bin_by(instance, bin_type_id);

    // Best density of the blocks made of a single item type, for each item
    // type.
    std::vector<double> alone_densities(instance.number_of_item_types(), 0.0);
    for (const PeriodicBlock& block: blocks) {
        if (block.item_copies.size() != 1)
            continue;
        ItemTypeId item_type_id = block.item_copies[0].first;
        alone_densities[item_type_id] = (std::max)(
                alone_densities[item_type_id],
                block.item_area / (block.bx * block.by));
    }

    auto rotation_of = [&](
            ItemTypeId item_type_id,
            Angle angle,
            bool mirror) -> const FrameItem*
    {
        for (const FrameItem& frame_item: frame_items) {
            if (frame_item.item_type_id == item_type_id
                    && frame_item.rotation.mirror == mirror
                    && shape::equal(
                        shape::normalize_angle(frame_item.rotation.angle),
                        shape::normalize_angle(angle))) {
                return &frame_item;
            }
        }
        return nullptr;
    };

    // Configurations: lists of frame items, the first one in rotation r.
    std::vector<std::vector<const FrameItem*>> configurations;
    for (const FrameItem& frame_item: frame_items) {
        configurations.push_back({&frame_item});
        const FrameItem* frame_item_180 = rotation_of(
                frame_item.item_type_id,
                frame_item.rotation.angle + 180,
                frame_item.rotation.mirror);
        if (frame_item_180 != nullptr)
            configurations.push_back({&frame_item, frame_item_180});
        const FrameItem* frame_item_90 = rotation_of(
                frame_item.item_type_id,
                frame_item.rotation.angle + 90,
                frame_item.rotation.mirror);
        const FrameItem* frame_item_270 = rotation_of(
                frame_item.item_type_id,
                frame_item.rotation.angle + 270,
                frame_item.rotation.mirror);
        if (frame_item_90 != nullptr && frame_item_180 != nullptr && frame_item_270 != nullptr)
            configurations.push_back({&frame_item, frame_item_90, frame_item_180, frame_item_270});
    }

    std::vector<PeriodicBlock> blocks_to_frame = blocks;
    for (int depth = 0; depth < maximum_depth && !blocks_to_frame.empty(); ++depth) {
        std::vector<PeriodicBlock> framed_blocks;
        for (const PeriodicBlock& block: blocks_to_frame) {
            AxisAlignedBoundingBox content = {0.0, block.bx - spacing, 0.0, block.by - spacing};
            for (const std::vector<const FrameItem*>& configuration: configurations) {
                ItemTypeId frame_item_type_id = configuration[0]->item_type_id;
                const ItemType& frame_item_type = instance.item_type(frame_item_type_id);
                // Copies.
                ItemPos frame_copies = configuration.size();
                for (const auto& kv: block.item_copies)
                    if (kv.first == frame_item_type_id)
                        frame_copies += kv.second;
                if (frame_copies > frame_item_type.copies)
                    continue;

                // Place the frame items one after the other. For each one,
                // the candidate positions are its position against the
                // content, and the positions aligning its bounding box with
                // the bounding box of the previous frame items; each one
                // moved away from the content if needed. The one giving the
                // smallest bounding box is kept.
                std::vector<AxisAlignedBoundingBox> obstacles = {content};
                AxisAlignedBoundingBox framed_aabb = content;
                std::vector<Point> positions;
                bool ok = true;
                for (const FrameItem* frame_item: configuration) {
                    Point anchor;
                    if (!frame_item_position(*frame_item, content, spacing, anchor)) {
                        ok = false;
                        break;
                    }
                    std::vector<Point> candidates = {anchor};
                    if (!positions.empty()) {
                        candidates.push_back({anchor.x, framed_aabb.y_min - frame_item->aabb.y_min});
                        candidates.push_back({anchor.x, framed_aabb.y_max - frame_item->aabb.y_max});
                        candidates.push_back({framed_aabb.x_min - frame_item->aabb.x_min, anchor.y});
                        candidates.push_back({framed_aabb.x_max - frame_item->aabb.x_max, anchor.y});
                    }
                    std::vector<Point> directions;
                    if (frame_item->left)
                        directions.push_back({-1.0, 0.0});
                    if (frame_item->right)
                        directions.push_back({1.0, 0.0});
                    if (frame_item->bottom)
                        directions.push_back({0.0, -1.0});
                    if (frame_item->top)
                        directions.push_back({0.0, 1.0});
                    bool found = false;
                    Point best_position;
                    AreaDbl best_area = std::numeric_limits<AreaDbl>::infinity();
                    for (const Point& candidate: candidates) {
                        std::vector<Point> moved_candidates;
                        bool valid = true;
                        for (const AxisAlignedBoundingBox& occupied: frame_item->occupied)
                            for (const AxisAlignedBoundingBox& obstacle: obstacles)
                                if (rectangles_too_close(shifted(occupied, candidate), obstacle, spacing))
                                    valid = false;
                        if (valid) {
                            moved_candidates.push_back(candidate);
                        } else {
                            for (const Point& direction: directions) {
                                Point moved = candidate;
                                if (slide_frame_item(*frame_item, obstacles, spacing, direction, moved))
                                    moved_candidates.push_back(moved);
                            }
                        }
                        for (const Point& moved: moved_candidates) {
                            AxisAlignedBoundingBox aabb = shape::merge(
                                    framed_aabb,
                                    shifted(frame_item->aabb, moved));
                            AreaDbl area = (aabb.x_max - aabb.x_min) * (aabb.y_max - aabb.y_min);
                            if (area < best_area) {
                                best_area = area;
                                best_position = moved;
                                found = true;
                            }
                        }
                    }
                    if (!found) {
                        ok = false;
                        break;
                    }
                    positions.push_back(best_position);
                    framed_aabb = shape::merge(framed_aabb, shifted(frame_item->aabb, best_position));
                    for (const AxisAlignedBoundingBox& occupied: frame_item->occupied)
                        obstacles.push_back(shifted(occupied, best_position));
                }
                if (!ok)
                    continue;

                LengthDbl bx = framed_aabb.x_max - framed_aabb.x_min + spacing;
                LengthDbl by = framed_aabb.y_max - framed_aabb.y_min + spacing;
                if (shape::strictly_greater(bx, bin_bx) || shape::strictly_greater(by, bin_by))
                    continue;
                // Acceptance: no more space than the block and the frame
                // items packed alone.
                AreaDbl frame_items_area = configuration.size() * frame_item_type.area_scaled;
                double alone_density = alone_densities[frame_item_type_id];
                if (alone_density <= 0.0) {
                    alone_density = frame_item_type.area_scaled / (
                            (configuration[0]->aabb.x_max - configuration[0]->aabb.x_min)
                            * (configuration[0]->aabb.y_max - configuration[0]->aabb.y_min));
                }
                if (shape::strictly_greater(
                            bx * by,
                            block.bx * block.by + frame_items_area / alone_density)) {
                    continue;
                }

                PeriodicBlock framed_block;
                framed_block.bx = bx;
                framed_block.by = by;
                framed_block.number_of_items = block.number_of_items + configuration.size();
                framed_block.item_area = block.item_area + frame_items_area;
                framed_block.item_profit = block.item_profit + configuration.size() * frame_item_type.profit;
                framed_block.item_copies = block.item_copies;
                bool found_type = false;
                for (auto& kv: framed_block.item_copies) {
                    if (kv.first == frame_item_type_id) {
                        kv.second += configuration.size();
                        found_type = true;
                    }
                }
                if (!found_type)
                    framed_block.item_copies.push_back({frame_item_type_id, (ItemPos)configuration.size()});
                if (is_dominated(blocks, framed_block))
                    continue;
                framed_block.items.reserve(framed_block.number_of_items);
                for (const SolutionItem& item: block.items) {
                    SolutionItem placed_item = item;
                    placed_item.bl_corner.x -= framed_aabb.x_min;
                    placed_item.bl_corner.y -= framed_aabb.y_min;
                    framed_block.items.push_back(placed_item);
                }
                for (int pos = 0; pos < (int)configuration.size(); ++pos) {
                    SolutionItem placed_item;
                    placed_item.item_type_id = frame_item_type_id;
                    placed_item.angle = configuration[pos]->rotation.angle;
                    placed_item.mirror = configuration[pos]->rotation.mirror;
                    placed_item.bl_corner.x = positions[pos].x - framed_aabb.x_min;
                    placed_item.bl_corner.y = positions[pos].y - framed_aabb.y_min;
                    framed_block.items.push_back(placed_item);
                }
                framed_blocks.push_back(framed_block);
                add_block_if_not_dominated(blocks, std::move(framed_block));
            }
        }
        blocks_to_frame.swap(framed_blocks);
    }
}

std::vector<PeriodicBlock> packingsolver::irregular::compute_periodic_blocks(
        const Instance& instance,
        const std::vector<PeriodicItemPacking>& packings,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations)
{
    // Group the packings of a single item type by item type, to build their
    // blocks together with the AABB-grid blocks of this item type. The
    // packings of several item types get their own blocks, built after.
    std::vector<std::vector<PeriodicItemPacking>> packings_by_item_type(
            instance.number_of_item_types());
    for (const PeriodicItemPacking& packing: packings)
        if (periodic_packing_has_single_item_type(packing))
            packings_by_item_type[packing.items[0].item_type_id].push_back(packing);

    std::vector<PeriodicBlock> output;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        std::vector<PeriodicBlock> item_type_output
            = compute_periodic_blocks_for_item_type(
                    instance,
                    item_type_id,
                    packings_by_item_type[item_type_id],
                    item_type_rotations[item_type_id]);
        output.insert(
                output.end(),
                std::make_move_iterator(item_type_output.begin()),
                std::make_move_iterator(item_type_output.end()));
    }

    if (instance.number_of_bin_types() > 0) {
        std::vector<PeriodicBlock> multiple_item_types_output;
        for (int packing_id = 0; packing_id < (int)packings.size(); ++packing_id) {
            if (periodic_packing_has_single_item_type(packings[packing_id]))
                continue;
            add_periodic_blocks_for_packing(
                    instance,
                    packing_id,
                    packings[packing_id],
                    multiple_item_types_output);
        }
        output.insert(
                output.end(),
                std::make_move_iterator(multiple_item_types_output.begin()),
                std::make_move_iterator(multiple_item_types_output.end()));
    }

    add_framed_blocks(instance, item_type_rotations, output);

    return output;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////// BranchingSchemePeriodicPacking /////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingSchemePeriodicPacking::BranchingSchemePeriodicPacking(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters)
{
    BinTypeId bin_type_id = instance_.bin_type_id(0);

    std::vector<PeriodicItemPacking> packings = assemble_periodic_packings(instance_);

    // compute_item_type_rotations returns [bin_type_id][item_type_id][rotation_idx].
    // Use the first bin type's rotations. This is cheap relative to
    // compute_periodic_packings, so it is recomputed here rather than
    // threaded through as a constructor argument.
    auto all_item_type_rotations = compute_item_type_rotations(instance_);
    const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations
        = all_item_type_rotations[bin_type_id];
    blocks_ = compute_periodic_blocks(instance_, packings, item_type_rotations);

    // Approximate the obstacles by rectangles. The empty spaces span the
    // bin's full bounding box (see usable_bin_bx()/by()'s comment), which
    // for a non-rectangular bin shape extends past the shape itself: the
    // borders -- the parts of that bounding box outside of the shape, built
    // alongside the defects in InstanceBuilder::build() -- are obstacles
    // too. The fixed items are placed as in the solution (mirrored, rotated,
    // then shifted by their 'bl_corner') and inflated by the item-item
    // spacing.
    std::vector<shape::OuterApproximationByRectanglesInputShape> obstacles;
    for (const std::vector<Defect>& defects: {bin_type().defects, bin_type().borders}) {
        for (const Defect& defect: defects) {
            shape::OuterApproximationByRectanglesInputShape obstacle;
            obstacle.shape = defect.shape_inflated;
            obstacles.push_back(obstacle);
        }
    }
    for (const FixedItem& fixed_item: bin_type().fixed_items) {
        const ItemType& item_type = instance_.item_type(fixed_item.item_type_id);
        Point bl_corner = instance_.parameters().scale_value * fixed_item.bl_corner;
        for (const ItemShape& item_shape: item_type.shapes) {
            shape::OuterApproximationByRectanglesInputShape obstacle;
            obstacle.shape = item_shape.shape_inflated;
            if (fixed_item.mirror)
                obstacle.shape = obstacle.shape.axial_symmetry_y_axis();
            obstacle.shape = obstacle.shape.rotate(fixed_item.angle);
            obstacle.shape.shift(bl_corner.x, bl_corner.y);
            obstacles.push_back(obstacle);
        }
    }
    if (!obstacles.empty()) {
        // Same maximum waste as for the shape simplification.
        AreaDbl total_item_area = 0.0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            total_item_area += item_type.copies * item_type.area_scaled;
        }
        AreaDbl maximum_waste = parameters_.maximum_approximation_ratio
            * (std::min)(bin_type().area_scaled, total_item_area);
        // The extent of a block ends one item-item spacing past its items
        // (see usable_bin_bx()/by()'s comment): a block on the left of (or
        // below) an obstacle only needs to end at the obstacle for its items
        // to be at the required distance. So the left and bottom sides of
        // the rectangles are moved by one item-item spacing (at most to their
        // middle, which is enough for the narrower rectangles).
        LengthDbl item_spacing = instance_.item_spacing_scaled();
        for (const std::vector<AxisAlignedBoundingBox>& rectangles:
                shape::outer_approximation_by_rectangles(obstacles, maximum_waste)) {
            for (AxisAlignedBoundingBox rectangle: rectangles) {
                rectangle.x_min = (std::min)(
                        rectangle.x_min + item_spacing,
                        (rectangle.x_min + rectangle.x_max) / 2);
                rectangle.y_min = (std::min)(
                        rectangle.y_min + item_spacing,
                        (rectangle.y_min + rectangle.y_max) / 2);
                obstacles_.push_back(rectangle);
            }
        }
    }
}

LengthDbl BranchingSchemePeriodicPacking::bin_bx() const
{
    return usable_bin_bx(instance_, instance_.bin_type_id(0));
}

LengthDbl BranchingSchemePeriodicPacking::bin_by() const
{
    return usable_bin_by(instance_, instance_.bin_type_id(0));
}

const std::shared_ptr<BranchingSchemePeriodicPacking::Node>
BranchingSchemePeriodicPacking::root() const
{
    auto node = std::make_shared<Node>();
    node->id = node_id_++;
    node->item_number_of_copies.assign(instance_.number_of_item_types(), 0);
    EmptySpace space;
    space.bl_corner = {bin_x_min(), bin_y_min()};
    space.bx = bin_bx();
    space.by = bin_by();
    node->empty_spaces.push_back(space);
    // Cut out the obstacles (defects, borders and fixed items) from the
    // initial empty space.
    for (const AxisAlignedBoundingBox& obstacle: obstacles_) {
        cut_empty_spaces(
                node->empty_spaces,
                {obstacle.x_min, obstacle.y_min},
                obstacle.x_max - obstacle.x_min,
                obstacle.y_max - obstacle.y_min);
    }
    // The fixed items are part of every solution: they consume copies of
    // their item types.
    for (const FixedItem& fixed_item: bin_type().fixed_items) {
        const ItemType& item_type = instance_.item_type(fixed_item.item_type_id);
        node->item_number_of_copies[fixed_item.item_type_id]++;
        node->item_area += item_type.area_scaled;
        node->number_of_items++;
        node->profit += item_type.profit;
    }
    ItemPos number_of_blocks = (ItemPos)blocks_.size();
    for (ItemPos block_id = 0; block_id < number_of_blocks; ++block_id) {
        // Skip the blocks which would exceed the copies left by the fixed
        // items.
        bool feasible = true;
        for (const std::pair<ItemTypeId, ItemPos>& kv: blocks_[block_id].item_copies) {
            if (node->item_number_of_copies[kv.first] + kv.second
                    > instance_.item_type(kv.first).copies) {
                feasible = false;
                break;
            }
        }
        if (feasible)
            node->valid_block_ids.push_back(block_id);
    }
    if (!obstacles_.empty())
        remove_unusable_spaces(*node);
    return node;
}

BranchingSchemePeriodicPacking::BestSpaceResult
BranchingSchemePeriodicPacking::find_best_space(const Node& parent) const
{
    BestSpaceResult result;
    LengthDbl best_distance = std::numeric_limits<LengthDbl>::max();
    AreaDbl best_area = 0.0;
    int best_corner = std::numeric_limits<int>::min();

    for (ItemPos space_idx = 0;
            space_idx < (ItemPos)parent.empty_spaces.size();
            ++space_idx) {
        const EmptySpace& space = parent.empty_spaces[space_idx];
        LengthDbl dist_x_start = space.xs() - bin_x_min();
        LengthDbl dist_x_end = (bin_x_min() + bin_bx()) - space.xe();
        LengthDbl dist_y_start = space.ys() - bin_y_min();
        LengthDbl dist_y_end = (bin_y_min() + bin_by()) - space.ye();
        bool dir_x = shape::strictly_lesser(dist_x_end, dist_x_start);
        bool dir_y = shape::strictly_lesser(dist_y_end, dist_y_start);
        LengthDbl distance = std::min(dist_x_start, dist_x_end)
            + std::min(dist_y_start, dist_y_end);
        AreaDbl area = space.area();
        int corner = (dir_x? 2: 0) | (dir_y? 1: 0);
        bool is_better = shape::strictly_lesser(distance, best_distance)
            || (shape::equal(distance, best_distance) && shape::strictly_greater(area, best_area))
            || (shape::equal(distance, best_distance) && shape::equal(area, best_area) && corner > best_corner);
        if (is_better) {
            result.space_idx = space_idx;
            result.anchor.distance = distance;
            result.anchor.dir_x = dir_x;
            result.anchor.dir_y = dir_y;
            result.anchor.space_area = area;
            best_distance = distance;
            best_area = area;
            best_corner = corner;
        }
    }
    return result;
}

BranchingSchemePeriodicPacking::SpaceContactInfo
BranchingSchemePeriodicPacking::compute_space_contact_info(
        const std::vector<Node::PlacedBlock>& placed_blocks,
        const EmptySpace& space,
        double delta) const
{
    SpaceContactInfo info;
    info.space_xs = space.bl_corner.x;
    info.space_ys = space.bl_corner.y;
    info.space_bx = space.bx;
    info.space_by = space.by;
    info.tol_x = delta * space.bx;
    info.tol_y = delta * space.by;

    LengthDbl xl = space.xs(), xh = space.xe();
    LengthDbl yl = space.ys(), yh = space.ye();
    info.xl_wall = !shape::strictly_greater(xl - bin_x_min(), info.tol_x);
    info.yl_wall = !shape::strictly_greater(yl - bin_y_min(), info.tol_y);
    info.xh_wall = !shape::strictly_greater((bin_x_min() + bin_bx()) - xh, info.tol_x);
    info.yh_wall = !shape::strictly_greater((bin_y_min() + bin_by()) - yh, info.tol_y);

    for (const Node::PlacedBlock& pb: placed_blocks) {
        const PeriodicBlock& block = blocks_[pb.block_id];
        LengthDbl pb_xl = pb.bl_corner.x;
        LengthDbl pb_xh = pb_xl + block.bx;
        LengthDbl pb_yl = pb.bl_corner.y;
        LengthDbl pb_yh = pb_yl + block.by;

        // Block touches the left edge of the space. Uses a delta-scaled
        // tolerance (like the wall checks above), not strict equality:
        // irregular's blocks come from NFP-based periodic-packing geometry,
        // which accumulates far more floating-point residue than
        // rectangle's axis-aligned cuts, so a genuinely-touching neighbor
        // can miss a strict equality check by a tiny epsilon. Missing a
        // neighbor here doesn't just under-count contact: since
        // compute_relative_contact_area only credits an edge as touching
        // when the *sum* of its wall/neighbor contacts reaches that edge's
        // full length, one missed neighbor can zero out that edge's
        // contribution entirely, and if it's the only contributing edge,
        // the relative contact area C -- and thus the whole guide score
        // V * C^alpha * ... -- becomes exactly 0 even for an otherwise
        // perfectly valid placement.
        if (!shape::strictly_greater(std::abs(pb_xh - xl), info.tol_x)) {
            if (shape::strictly_lesser(pb_yl, yh) && shape::strictly_greater(pb_yh, yl)) {
                SpaceContactInfo::Neighbor n;
                n.lo1 = pb_yl;
                n.hi1 = pb_yh;
                n.orthogonal_pos = pb_xh;
                info.xl_neighbors.push_back(n);
            }
        }
        // Block touches the right edge of the space.
        if (!shape::strictly_greater(std::abs(pb_xl - xh), info.tol_x)) {
            if (shape::strictly_lesser(pb_yl, yh) && shape::strictly_greater(pb_yh, yl)) {
                SpaceContactInfo::Neighbor n;
                n.lo1 = pb_yl;
                n.hi1 = pb_yh;
                n.orthogonal_pos = pb_xl;
                info.xh_neighbors.push_back(n);
            }
        }
        // Block touches the bottom edge of the space.
        if (!shape::strictly_greater(std::abs(pb_yh - yl), info.tol_y)) {
            if (shape::strictly_lesser(pb_xl, xh) && shape::strictly_greater(pb_xh, xl)) {
                SpaceContactInfo::Neighbor n;
                n.lo1 = pb_xl;
                n.hi1 = pb_xh;
                n.orthogonal_pos = pb_yh;
                info.yl_neighbors.push_back(n);
            }
        }
        // Block touches the top edge of the space.
        if (!shape::strictly_greater(std::abs(pb_yl - yh), info.tol_y)) {
            if (shape::strictly_lesser(pb_xl, xh) && shape::strictly_greater(pb_xh, xl)) {
                SpaceContactInfo::Neighbor n;
                n.lo1 = pb_xl;
                n.hi1 = pb_xh;
                n.orthogonal_pos = pb_yl;
                info.yh_neighbors.push_back(n);
            }
        }
    }
    return info;
}

double BranchingSchemePeriodicPacking::compute_relative_contact_area(
        const SpaceContactInfo& info,
        const PeriodicBlock& block,
        Point bl_corner,
        double delta) const
{
    LengthDbl rel_x = bl_corner.x - info.space_xs;
    LengthDbl rel_y = bl_corner.y - info.space_ys;

    LengthDbl block_xl = bl_corner.x;
    LengthDbl block_xh = bl_corner.x + block.bx;
    LengthDbl block_yl = bl_corner.y;
    LengthDbl block_yh = bl_corner.y + block.by;

    LengthDbl tol_x = delta * block.bx;
    LengthDbl tol_y = delta * block.by;

    LengthDbl contact = 0.0;
    LengthDbl perimeter = 2.0 * (block.bx + block.by);

    // Left edge of block.
    if (!shape::strictly_greater(rel_x, tol_x)) {
        if (info.xl_wall) {
            contact += block.by;
        } else {
            for (const SpaceContactInfo::Neighbor& n: info.xl_neighbors) {
                LengthDbl overlap = std::min(block_yh, n.hi1) - std::max(block_yl, n.lo1);
                if (shape::strictly_greater(overlap, 0.0))
                    contact += overlap;
            }
        }
    }
    // Right edge of block.
    if (!shape::strictly_greater(info.space_bx - rel_x - block.bx, tol_x)) {
        if (info.xh_wall) {
            contact += block.by;
        } else {
            for (const SpaceContactInfo::Neighbor& n: info.xh_neighbors) {
                LengthDbl overlap = std::min(block_yh, n.hi1) - std::max(block_yl, n.lo1);
                if (shape::strictly_greater(overlap, 0.0))
                    contact += overlap;
            }
        }
    }
    // Bottom edge of block.
    if (!shape::strictly_greater(rel_y, tol_y)) {
        if (info.yl_wall) {
            contact += block.bx;
        } else {
            for (const SpaceContactInfo::Neighbor& n: info.yl_neighbors) {
                LengthDbl overlap = std::min(block_xh, n.hi1) - std::max(block_xl, n.lo1);
                if (shape::strictly_greater(overlap, 0.0))
                    contact += overlap;
            }
        }
    }
    // Top edge of block.
    if (!shape::strictly_greater(info.space_by - rel_y - block.by, tol_y)) {
        if (info.yh_wall) {
            contact += block.bx;
        } else {
            for (const SpaceContactInfo::Neighbor& n: info.yh_neighbors) {
                LengthDbl overlap = std::min(block_xh, n.hi1) - std::max(block_xl, n.lo1);
                if (shape::strictly_greater(overlap, 0.0))
                    contact += overlap;
            }
        }
    }

    return contact / perimeter;
}

double BranchingSchemePeriodicPacking::compute_insertion_guide(
        const Node& parent,
        const Insertion& insertion,
        const SpaceContactInfo& info) const
{
    const PeriodicBlock& block = blocks_[insertion.block_id];

    double node_fill_rate = (double)parent.item_area / (bin_bx() * bin_by());
    double v = block.item_profit;
    double f = block.fill_rate();
    double n = (double)block.number_of_items;
    if (node_fill_rate < parameters_.configuration_switch_threshold) {
        double c = compute_relative_contact_area(
                info, block, insertion.bl_corner, parameters_.delta);
        return v
            * std::pow(c, parameters_.alpha)
            * std::pow(f, parameters_.beta)
            * std::pow(n, -parameters_.gamma);
    } else {
        double c = compute_relative_contact_area(
                info, block, insertion.bl_corner, parameters_.delta_2);
        return v
            * std::pow(c, parameters_.alpha_2)
            * std::pow(f, parameters_.beta_2)
            * std::pow(n, -parameters_.gamma_2);
    }
}

double BranchingSchemePeriodicPacking::active_delta(const Node& node) const
{
    double fill_rate = (double)node.item_area / (bin_bx() * bin_by());
    return (fill_rate < parameters_.configuration_switch_threshold)?
        parameters_.delta:
        parameters_.delta_2;
}

const std::vector<BranchingSchemePeriodicPacking::Insertion>&
BranchingSchemePeriodicPacking::insertions(
        const std::shared_ptr<Node>& parent) const
{
    insertions_.clear();

    double delta = active_delta(*parent);

    if (!parent->empty_spaces.empty()) {
        BestSpaceResult best = find_best_space(*parent);
        if (best.space_idx != -1) {
            const EmptySpace& space = parent->empty_spaces[best.space_idx];
            SpaceContactInfo contact_info = compute_space_contact_info(
                    parent->placed_blocks, space, delta);

            LengthDbl anchor_x = best.anchor.dir_x? space.xe(): space.xs();
            LengthDbl anchor_y = best.anchor.dir_y? space.ye(): space.ys();

            for (ItemPos block_id: parent->valid_block_ids) {
                const PeriodicBlock& block = blocks_[block_id];
                if (shape::strictly_greater(block.bx, space.bx)
                        || shape::strictly_greater(block.by, space.by))
                    continue;

                Insertion insertion;
                insertion.space_id = best.space_idx;
                insertion.block_id = block_id;
                insertion.bl_corner.x = best.anchor.dir_x?
                    anchor_x - block.bx: anchor_x;
                insertion.bl_corner.y = best.anchor.dir_y?
                    anchor_y - block.by: anchor_y;
                insertion.guide = compute_insertion_guide(*parent, insertion, contact_info);
                insertions_.push_back(insertion);
            }
        }
    }

    std::sort(insertions_.begin(), insertions_.end(),
            [](const Insertion& insertion_1, const Insertion& insertion_2) {
                return insertion_1.guide > insertion_2.guide;
            });
    return insertions_;
}

BranchingSchemePeriodicPacking::Insertion
BranchingSchemePeriodicPacking::best_insertion(Node& parent) const
{
    Insertion best;
    double best_score = 0;

    double delta = active_delta(parent);

    if (!parent.empty_spaces.empty()) {
        BestSpaceResult best_space = find_best_space(parent);
        if (best_space.space_idx != -1) {
            const EmptySpace& space = parent.empty_spaces[best_space.space_idx];
            SpaceContactInfo contact_info = compute_space_contact_info(
                    parent.placed_blocks, space, delta);

            LengthDbl anchor_x = best_space.anchor.dir_x? space.xe(): space.xs();
            LengthDbl anchor_y = best_space.anchor.dir_y? space.ye(): space.ys();

            for (ItemPos block_id: parent.valid_block_ids) {
                const PeriodicBlock& block = blocks_[block_id];
                if (shape::strictly_greater(block.bx, space.bx)
                        || shape::strictly_greater(block.by, space.by))
                    continue;

                Insertion insertion;
                insertion.space_id = best_space.space_idx;
                insertion.block_id = block_id;
                insertion.bl_corner.x = best_space.anchor.dir_x?
                    anchor_x - block.bx: anchor_x;
                insertion.bl_corner.y = best_space.anchor.dir_y?
                    anchor_y - block.by: anchor_y;
                double score = compute_insertion_guide(parent, insertion, contact_info);
                if (score > best_score) {
                    best_score = score;
                    best = insertion;
                }
            }
        }
    }

    return best;
}

void BranchingSchemePeriodicPacking::apply_insertion(
        Node& node,
        const Insertion& insertion) const
{
    node.id = node_id_++;
    node.cached_insertions.clear();
    node.next_child_pos = 0;

    const PeriodicBlock& block = blocks_[insertion.block_id];

    // Record placement.
    Node::PlacedBlock pb;
    pb.block_id = insertion.block_id;
    pb.bl_corner = insertion.bl_corner;
    node.placed_blocks.push_back(pb);

    // Update item copy counts.
    for (const std::pair<ItemTypeId, ItemPos>& kv: block.item_copies)
        node.item_number_of_copies[kv.first] += kv.second;

    // Update valid block ids: remove blocks that can no longer be placed.
    for (ItemPos i = 0; i < (ItemPos)node.valid_block_ids.size(); ) {
        const PeriodicBlock& candidate = blocks_[node.valid_block_ids[i]];
        bool feasible = true;
        for (const std::pair<ItemTypeId, ItemPos>& kv: candidate.item_copies) {
            if (node.item_number_of_copies[kv.first] + kv.second
                    > instance_.item_type(kv.first).copies) {
                feasible = false;
                break;
            }
        }
        if (feasible) {
            ++i;
        } else {
            node.valid_block_ids[i] = node.valid_block_ids.back();
            node.valid_block_ids.pop_back();
        }
    }

    // Update statistics.
    node.item_area += block.item_area;
    node.number_of_items += block.number_of_items;
    node.number_of_blocks++;
    node.profit += block.item_profit;

    // Update empty spaces.
    cut_empty_spaces(node.empty_spaces, insertion.bl_corner, block.bx, block.by);
    remove_unusable_spaces(node);
}

void BranchingSchemePeriodicPacking::remove_unusable_spaces(Node& node) const
{
    for (ItemPos space_idx = 0; space_idx < (ItemPos)node.empty_spaces.size(); ) {
        const EmptySpace& space = node.empty_spaces[space_idx];
        bool has_fitting_block = false;
        for (ItemPos block_id: node.valid_block_ids) {
            const PeriodicBlock& candidate = blocks_[block_id];
            if (!shape::strictly_greater(candidate.bx, space.bx)
                    && !shape::strictly_greater(candidate.by, space.by)) {
                has_fitting_block = true;
                break;
            }
        }
        if (!has_fitting_block) {
            node.empty_spaces[space_idx] = node.empty_spaces.back();
            node.empty_spaces.pop_back();
        } else {
            ++space_idx;
        }
    }
}

Profit BranchingSchemePeriodicPacking::compute_guide_greedy(const Node& node) const
{
    Node greedy_node = node;
    while (true) {
        Insertion insertion = best_insertion(greedy_node);
        if (insertion.block_id == -1)
            break;
        apply_insertion(greedy_node, insertion);
    }
    return greedy_node.profit;
}

std::shared_ptr<BranchingSchemePeriodicPacking::Node>
BranchingSchemePeriodicPacking::next_child(
        const std::shared_ptr<Node>& parent) const
{
    if (parent->next_child_pos == 0)
        parent->cached_insertions = insertions(parent);
    NodeId pos = parent->next_child_pos;
    parent->next_child_pos++;
    if (pos >= (NodeId)parent->cached_insertions.size())
        return nullptr;
    const Insertion& insertion = parent->cached_insertions[pos];

    std::vector<Insertion> saved_insertions;
    std::swap(parent->cached_insertions, saved_insertions);
    auto node = std::make_shared<Node>(*parent);
    std::swap(parent->cached_insertions, saved_insertions);

    node->cached_insertions.clear();
    node->next_child_pos = 0;
    apply_insertion(*node, insertion);
    node->greedy_value = compute_guide_greedy(*node);

    return node;
}

bool BranchingSchemePeriodicPacking::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Knapsack: {
        return node_2->greedy_value < node_1->greedy_value;
    } case Objective::Feasibility: {
        return node_2->greedy_value < node_1->greedy_value;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "Branching scheme 'irregular::BranchingSchemePeriodicPacking' "
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingSchemePeriodicPacking::bound(
        const std::shared_ptr<Node>&,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Knapsack: {
        if (leaf(node_2))
            return true;
        return false;
    } case Objective::Feasibility: {
        if (leaf(node_2))
            return true;
        return false;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "Branching scheme 'irregular::BranchingSchemePeriodicPacking' "
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

Solution BranchingSchemePeriodicPacking::to_solution(
        const std::shared_ptr<Node>& node_orig) const
{
    // Fill the node greedily before converting to a solution.
    Node greedy_node = *node_orig;
    while (true) {
        Insertion insertion = best_insertion(greedy_node);
        if (insertion.block_id == -1)
            break;
        apply_insertion(greedy_node, insertion);
    }

    SolutionBuilder solution_builder(instance_);
    BinTypeId bin_type_id = instance_.bin_type_id(0);
    BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);

    // Node placements are tracked directly in the bin type's own (scaled)
    // frame (see bin_x_min()/bin_y_min()'s comment in the header), so all
    // that is left here is to unscale them into absolute solution coordinates.
    double scale = 1.0 / instance_.parameters().scale_value;
    for (const Node::PlacedBlock& pb: greedy_node.placed_blocks) {
        const PeriodicBlock& block = blocks_[pb.block_id];
        for (const SolutionItem& solution_item: block.items) {
            Point item_bl_corner;
            item_bl_corner.x = (pb.bl_corner.x + solution_item.bl_corner.x) * scale;
            item_bl_corner.y = (pb.bl_corner.y + solution_item.bl_corner.y) * scale;
            solution_builder.add_item(
                    bin_pos,
                    solution_item.item_type_id,
                    item_bl_corner,
                    solution_item.angle,
                    solution_item.mirror);
        }
    }

    // Fixed items.
    for (const FixedItem& fixed_item: bin_type().fixed_items) {
        solution_builder.add_item(
                bin_pos,
                fixed_item.item_type_id,
                fixed_item.bl_corner,
                fixed_item.angle,
                fixed_item.mirror,
                true);
    }

    return solution_builder.build();
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////// tree_search_periodic_packing ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

void validate_tree_search_periodic_packing_instance(const Instance& instance)
{
    if (instance.number_of_bins() > 1) {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "algorithm 'irregular::tree_search_periodic_packing' "
            << "does not support instances with more than one bin.";
        throw std::logic_error(ss.str());
    }
    if (instance.objective() == Objective::VariableSizedBinPacking) {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "algorithm 'irregular::tree_search_periodic_packing' "
            << "does not support objective '" << instance.objective() << "'.";
        throw std::logic_error(ss.str());
    }
}

/**
 * Assemble the periodic packings of the instance, reading from
 * Instance::periodic_packings() (see InstanceBuilder::build()):
 * sub-instances built by copying item types from a source instance (e.g.
 * successive sequential_feasibility iterations, or the last-bin
 * post-process) inherit them for free instead of recomputing from scratch,
 * which can otherwise dominate the running time for items with complex
 * shapes. Falls back to computing on the fly the periodic packings of the
 * rare item type whose ones weren't computed (e.g. an Instance not built
 * through InstanceBuilder).
 */
std::vector<PeriodicItemPacking> assemble_periodic_packings(const Instance& instance)
{
    std::vector<PeriodicItemPacking> packings = instance.periodic_packings();
    std::vector<std::vector<ItemTypeRotation>> item_type_rotations;
    // Computed lazily (only if some item type's cache actually needs it).
    // See instance_builder.cpp's periodic_packings_simplification_minimum_
    // number_of_vertices for why 64.
    SimplifiedInstance simplified_instance;
    bool simplified_instance_computed = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.periodic_packings_computed)
            continue;
        if (item_type_rotations.empty()) {
            auto all_item_type_rotations = compute_item_type_rotations(instance);
            item_type_rotations = all_item_type_rotations[instance.bin_type_id(0)];
        }
        if (!simplified_instance_computed) {
            simplified_instance = shape_simplification(instance, 0.001, 64);
            simplified_instance_computed = true;
        }
        std::vector<ShapeWithHoles> item_shapes;
        for (const SimplifiedShape& simplified_shape:
                simplified_instance.item_types[item_type_id].shapes) {
            item_shapes.push_back(simplified_shape.shape);
        }
        std::vector<PeriodicItemPacking> item_type_packings
            = compute_periodic_packings_for_item_type(
                    item_shapes,
                    item_type_rotations[item_type_id],
                    instance.item_spacing_scaled());
        for (PeriodicItemPacking& item_packing: item_type_packings)
            for (SolutionItem& item: item_packing.items)
                item.item_type_id = item_type_id;
        packings.insert(
                packings.end(),
                std::make_move_iterator(item_type_packings.begin()),
                std::make_move_iterator(item_type_packings.end()));
    }
    return packings;
}

}

const packingsolver::irregular::TreeSearchPeriodicPackingOutput
packingsolver::irregular::tree_search_periodic_packing(
        const Instance& instance,
        const TreeSearchPeriodicPackingParameters& parameters)
{
    validate_tree_search_periodic_packing_instance(instance);

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers
            || instance.objective() == Objective::OpenDimensionX
            || instance.objective() == Objective::OpenDimensionY
            || instance.objective() == Objective::OpenDimensionXY) {
        TreeSearchPeriodicPackingOutput output(instance);
        AlgorithmFormatter algorithm_formatter(instance, parameters, output);
        algorithm_formatter.start();
        algorithm_formatter.print_header();

        SequentialFeasibilitySolver solver = [&parameters, &algorithm_formatter](
                const Instance& sub_instance,
                const Solution*)
        {
            TreeSearchPeriodicPackingParameters inner_parameters;
            inner_parameters.verbosity_level = 0;
            inner_parameters.timer = parameters.timer;
            inner_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            inner_parameters.optimization_mode = parameters.optimization_mode;
            inner_parameters.not_anytime_tree_search_queue_size
                = parameters.not_anytime_tree_search_queue_size;
            inner_parameters.maximum_approximation_ratio = parameters.maximum_approximation_ratio;
            return tree_search_periodic_packing(sub_instance, inner_parameters).solution_pool;
        };

        SequentialFeasibilityParameters sf_parameters;
        sf_parameters.verbosity_level = 0;
        sf_parameters.timer = parameters.timer;
        sf_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sf_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& sf_output)
        {
            algorithm_formatter.update_solution(
                    sf_output.solution_pool.best(),
                    sf_output.solution_pool.best_label());
        };

        sequential_feasibility(instance, solver, sf_parameters);
        algorithm_formatter.end();
        return output;
    }

    TreeSearchPeriodicPackingOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    std::vector<BranchingSchemePeriodicPacking> branching_schemes;
    std::vector<treesearchsolver::IterativeBeamSearchParameters<BranchingSchemePeriodicPacking>> ibs_parameters_list;
    // In 'NotAnytimeDeterministic' mode, each scheme reports to its own
    // algorithm formatter, writing into its own output, instead of the shared
    // 'algorithm_formatter', so that the scheme can stop as soon as its own
    // solution is proven optimal while the order in which solutions reach
    // the shared one stays deterministic (they are replayed at the end).
    // These local formatters share silenced parameters (no messages, no JSON
    // output, no new solution callback). A 'unique_ptr' is used since an
    // 'AlgorithmFormatter' is neither copyable nor movable.
    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    packingsolver::Parameters<Instance, Solution, Output> local_parameters;
    local_parameters.verbosity_level = 0;
    local_parameters.timer = parameters.timer;
    std::vector<std::unique_ptr<Output>> local_outputs;
    std::vector<std::unique_ptr<AlgorithmFormatter>> local_algorithm_formatters;
    {
        BranchingSchemePeriodicPacking::Parameters branching_scheme_parameters;
        branching_scheme_parameters.maximum_approximation_ratio = parameters.maximum_approximation_ratio;
        branching_schemes.push_back(BranchingSchemePeriodicPacking(
                instance, branching_scheme_parameters));
        if (branching_schemes.back().empty()) {
            algorithm_formatter.end();
            return output;
        }
        treesearchsolver::IterativeBeamSearchParameters<BranchingSchemePeriodicPacking> ibs_parameters;
        ibs_parameters.verbosity_level = 0;
        ibs_parameters.timer = parameters.timer;
        ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        ibs_parameters.global_history = true;
        if (parameters.optimization_mode != OptimizationMode::Anytime) {
            ibs_parameters.minimum_size_of_the_queue = 1;
            ibs_parameters.growth_factor = parameters.not_anytime_tree_search_queue_size;
            ibs_parameters.maximum_size_of_the_queue = parameters.not_anytime_tree_search_queue_size;
        }
        if (deterministic) {
            local_outputs.push_back(std::make_unique<Output>(instance));
            local_algorithm_formatters.push_back(std::make_unique<AlgorithmFormatter>(
                        instance,
                        local_parameters,
                        *local_outputs.back()));
            ibs_parameters.timer.add_end_boolean(
                    &local_algorithm_formatters.back()->end_boolean());
        }
        ibs_parameters_list.push_back(ibs_parameters);
    }

    std::vector<std::thread> threads;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Counter scheme_idx = 0; scheme_idx < (Counter)branching_schemes.size(); ++scheme_idx) {
        AlgorithmFormatter* scheme_algorithm_formatter = (deterministic)?
            local_algorithm_formatters[(size_t)scheme_idx].get():
            &algorithm_formatter;
        ibs_parameters_list[scheme_idx].new_solution_callback
            = [scheme_algorithm_formatter, &branching_schemes, scheme_idx](
                    const treesearchsolver::Output<BranchingSchemePeriodicPacking>& tss_output)
            {
                const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemePeriodicPacking>& tssibs_output
                    = static_cast<const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemePeriodicPacking>&>(tss_output);
                Solution solution = branching_schemes[scheme_idx].to_solution(
                        tssibs_output.solution_pool.best());
                std::stringstream ss;
                ss << "n " << tssibs_output.maximum_size_of_the_queue;
                scheme_algorithm_formatter->update_solution(solution, ss.str());
            };
        exception_ptr_list.push_front(std::exception_ptr());
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeSequential) {
            threads.push_back(std::thread(
                        wrapper<decltype(&treesearchsolver::iterative_beam_search<BranchingSchemePeriodicPacking>), treesearchsolver::iterative_beam_search<BranchingSchemePeriodicPacking>>,
                        std::ref(exception_ptr_list.front()),
                        std::ref(branching_schemes[scheme_idx]),
                        ibs_parameters_list[scheme_idx]));
        } else {
            try {
                treesearchsolver::iterative_beam_search<BranchingSchemePeriodicPacking>(
                        branching_schemes[scheme_idx],
                        ibs_parameters_list[scheme_idx]);
            } catch (...) {
                exception_ptr_list.front() = std::current_exception();
            }
        }
    }
    for (Counter thread_idx = 0; thread_idx < (Counter)threads.size(); ++thread_idx)
        threads[thread_idx].join();
    for (const std::exception_ptr& exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
    if (deterministic) {
        for (const auto& local_output: local_outputs) {
            algorithm_formatter.update_solution(
                    local_output->solution_pool.best(),
                    local_output->solution_pool.best_label());
        }
    }

    algorithm_formatter.end();
    return output;
}
