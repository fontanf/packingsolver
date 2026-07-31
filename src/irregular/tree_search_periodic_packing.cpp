#include "irregular/tree_search_periodic_packing.hpp"
#include "irregular/sequential_feasibility.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "irregular/periodic_packing.hpp"
#include "irregular/rotations.hpp"
#include "irregular/solution_builder.hpp"
#include "algorithms/thread_pool.hpp"
#include "treesearchsolver/iterative_beam_search.hpp"

#include "optimizationtools/containers/indexed_map.hpp"

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
 * Add 'block' to 'blocks' unless it is dominated by a block already present,
 * pruning greedily as candidates are generated rather than collecting every
 * candidate and filtering the whole list afterwards.
 *
 * A block is dominated by another block that packs at least as many copies
 * of this item type (all blocks handled by a single
 * compute_periodic_blocks_for_item_type call concern the same, single item
 * type) into no more space in both dimensions -- the tree search can then
 * always prefer that other block, since it is never worse in items, bx, or
 * by. This also removes any existing blocks that the newly added one now
 * dominates.
 *
 * A single-copy block is exempted from all of this and always kept
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
    ItemPos count = block.item_copies[0].second;

    if (count == 1) {
        blocks.push_back(std::move(block));
        return;
    }

    for (const PeriodicBlock& existing: blocks) {
        if (existing.item_copies[0].second >= count
                && !shape::strictly_greater(existing.bx, block.bx)
                && !shape::strictly_greater(existing.by, block.by)) {
            return;
        }
    }

    // Drop existing blocks now dominated by 'block' (except single-copy
    // ones, which are never removed), swapping each one with the last
    // element and popping instead of erase/remove_if, since order doesn't
    // matter here. When 'i' is itself the last element, skip the
    // self-move-assignment (blocks[i] = std::move(blocks[i])) and just pop:
    // moving a vector-holding struct into itself is not guaranteed safe and
    // was observed to corrupt the heap (double free) in practice.
    for (int i = 0; i < (int)blocks.size(); ) {
        if (blocks[i].item_copies[0].second != 1
                && count >= blocks[i].item_copies[0].second
                && !shape::strictly_greater(block.bx, blocks[i].bx)
                && !shape::strictly_greater(block.by, blocks[i].by)) {
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

std::vector<PeriodicBlock> packingsolver::irregular::compute_periodic_blocks_for_item_type(
        const Instance& instance,
        ItemTypeId item_type_id,
        const std::vector<PeriodicItemPacking>& packings,
        const std::vector<ItemTypeRotation>& rotations)
{
    if (instance.number_of_bin_types() == 0)
        return {};

    BinTypeId bin_type_id = instance.bin_type_id(0);
    LengthDbl item_spacing = instance.item_spacing_scaled();
    LengthDbl bin_bx = usable_bin_bx(instance, bin_type_id);
    LengthDbl bin_by = usable_bin_by(instance, bin_type_id);

    std::vector<PeriodicBlock> blocks;

    for (int packing_id = 0; packing_id < (int)packings.size(); ++packing_id) {
        const PeriodicItemPacking& packing = packings[packing_id];

        // Unit-cell bounding box dimensions.
        LengthDbl unit_bx = packing.aabb_scaled.x_max - packing.aabb_scaled.x_min;
        LengthDbl unit_by = packing.aabb_scaled.y_max - packing.aabb_scaled.y_min;

        if (shape::strictly_greater(unit_bx, bin_bx) || shape::strictly_greater(unit_by, bin_by))
            continue;

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
            continue;

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
                // Each copy's AABB is [offset_x, offset_x + unit_bx] × [offset_y, offset_y + unit_by].
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
                        tiling_x_min = std::min(tiling_x_min, cx);
                        tiling_x_max = std::max(tiling_x_max, cx + unit_bx);
                        tiling_y_min = std::min(tiling_y_min, cy);
                        tiling_y_max = std::max(tiling_y_max, cy + unit_by);
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
                    col_x_min = std::min(col_x_min, cx);
                    col_x_max = std::max(col_x_max, cx + unit_bx);
                    col_y_min = std::min(col_y_min, cy);
                    col_y_max = std::max(col_y_max, cy + unit_by);
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

    // Blocks are already pruned as they are generated (see
    // add_block_if_not_dominated): 'blocks' only ever holds the current
    // Pareto-optimal set (across bx, by, and number of copies of this item
    // type), so there is nothing left to filter here.
    add_aabb_grid_blocks_for_item_type(instance, item_type_id, rotations, blocks);

    return blocks;
}

std::vector<PeriodicBlock> packingsolver::irregular::compute_periodic_blocks(
        const Instance& instance,
        const std::vector<PeriodicItemPacking>& packings,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations)
{
    // Group packings by item type (every PeriodicItemPacking's items all
    // share the same item_type_id, since packings only ever pair a single
    // item type's own rotations against itself).
    std::vector<std::vector<PeriodicItemPacking>> packings_by_item_type(
            instance.number_of_item_types());
    for (const PeriodicItemPacking& packing: packings)
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
    ItemPos number_of_blocks = (ItemPos)blocks_.size();
    node->valid_block_ids.resize(number_of_blocks);
    std::iota(node->valid_block_ids.begin(), node->valid_block_ids.end(), (ItemPos)0);
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

bool BranchingSchemePeriodicPacking::overlaps(
        const EmptySpace& space,
        Point bl_corner,
        LengthDbl bx,
        LengthDbl by)
{
    return shape::strictly_lesser(bl_corner.x, space.xe())
        && shape::strictly_greater(bl_corner.x + bx, space.xs())
        && shape::strictly_lesser(bl_corner.y, space.ye())
        && shape::strictly_greater(bl_corner.y + by, space.ys());
}

void BranchingSchemePeriodicPacking::add_empty_space(
        std::vector<EmptySpace>& spaces,
        const EmptySpace& new_space)
{
    if (!shape::strictly_greater(new_space.bx, 0.0) || !shape::strictly_greater(new_space.by, 0.0))
        return;
    for (const EmptySpace& existing: spaces)
        if (existing.contains(new_space))
            return;
    spaces.push_back(new_space);
}

void BranchingSchemePeriodicPacking::cut_spaces(
        std::vector<EmptySpace>& spaces,
        Point bl_corner,
        LengthDbl bx,
        LengthDbl by)
{
    for (ItemPos i = 0; i < (ItemPos)spaces.size(); ) {
        if (!overlaps(spaces[i], bl_corner, bx, by)) {
            ++i;
            continue;
        }
        EmptySpace space = spaces[i];
        spaces[i] = spaces.back();
        spaces.pop_back();

        // Left (x-)
        if (shape::strictly_greater(bl_corner.x, space.xs())) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bx = bl_corner.x - space.bl_corner.x;
            sub.by = space.by;
            add_empty_space(spaces, sub);
        }
        // Right (x+)
        if (shape::strictly_lesser(bl_corner.x + bx, space.xe())) {
            EmptySpace sub;
            sub.bl_corner.x = bl_corner.x + bx;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bx = space.xe() - (bl_corner.x + bx);
            sub.by = space.by;
            add_empty_space(spaces, sub);
        }
        // Bottom (y-)
        if (shape::strictly_greater(bl_corner.y, space.ys())) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = space.bl_corner.y;
            sub.bx = space.bx;
            sub.by = bl_corner.y - space.bl_corner.y;
            add_empty_space(spaces, sub);
        }
        // Top (y+)
        if (shape::strictly_lesser(bl_corner.y + by, space.ye())) {
            EmptySpace sub;
            sub.bl_corner.x = space.bl_corner.x;
            sub.bl_corner.y = bl_corner.y + by;
            sub.bx = space.bx;
            sub.by = space.ye() - (bl_corner.y + by);
            add_empty_space(spaces, sub);
        }
        // Do not increment i: the element swapped into position i must be checked.
    }
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
    cut_spaces(node.empty_spaces, insertion.bl_corner, block.bx, block.by);

    // Remove empty spaces that no longer fit any currently valid block:
    // otherwise find_best_space could pick one of these (e.g. because it is
    // closest to a bin corner) and find no insertion there, wrongly making
    // the node infertile even though other, larger spaces remain usable.
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
 * Assemble periodic packings for every item type, reading from the
 * ItemType::periodic_packings cache when available (see
 * InstanceBuilder::build()): sub-instances built by copying item types from
 * a source instance (e.g. successive sequential_feasibility iterations, or
 * the last-bin post-process) inherit the cache for free instead of
 * recomputing from scratch, which can otherwise dominate the running time
 * for items with complex shapes. Falls back to computing on the fly for the
 * rare item type whose cache wasn't populated (e.g. an Instance not built
 * through InstanceBuilder).
 */
std::vector<PeriodicItemPacking> assemble_periodic_packings(const Instance& instance)
{
    std::vector<PeriodicItemPacking> packings;
    std::vector<std::vector<ItemTypeRotation>> item_type_rotations;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (item_type.periodic_packings_computed) {
            packings.insert(
                    packings.end(),
                    item_type.periodic_packings.begin(),
                    item_type.periodic_packings.end());
            continue;
        }
        if (item_type_rotations.empty()) {
            auto all_item_type_rotations = compute_item_type_rotations(instance);
            item_type_rotations = all_item_type_rotations[instance.bin_type_id(0)];
        }
        std::vector<PeriodicItemPacking> item_type_packings
            = compute_periodic_packings_for_item_type(
                    instance, item_type_id, item_type_rotations[item_type_id]);
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
                const Instance& sub_instance)
        {
            TreeSearchPeriodicPackingParameters inner_parameters;
            inner_parameters.verbosity_level = 0;
            inner_parameters.timer = parameters.timer;
            inner_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            inner_parameters.optimization_mode = parameters.optimization_mode;
            inner_parameters.not_anytime_tree_search_queue_size
                = parameters.not_anytime_tree_search_queue_size;
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
    std::vector<packingsolver::Output<Instance, Solution>> local_outputs;
    {
        BranchingSchemePeriodicPacking::Parameters branching_scheme_parameters;
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
        ibs_parameters_list.push_back(ibs_parameters);
        local_outputs.push_back(packingsolver::Output<Instance, Solution>(instance));
    }

    std::vector<std::thread> threads;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Counter scheme_idx = 0; scheme_idx < (Counter)branching_schemes.size(); ++scheme_idx) {
        if (parameters.optimization_mode != OptimizationMode::NotAnytimeDeterministic) {
            ibs_parameters_list[scheme_idx].new_solution_callback
                = [&algorithm_formatter, &branching_schemes, scheme_idx](
                        const treesearchsolver::Output<BranchingSchemePeriodicPacking>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemePeriodicPacking>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemePeriodicPacking>&>(tss_output);
                    Solution solution = branching_schemes[scheme_idx].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "n " << tssibs_output.maximum_size_of_the_queue;
                    algorithm_formatter.update_solution(solution, ss.str());
                };
        } else {
            ibs_parameters_list[scheme_idx].new_solution_callback
                = [&local_outputs, &branching_schemes, scheme_idx](
                        const treesearchsolver::Output<BranchingSchemePeriodicPacking>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemePeriodicPacking>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemePeriodicPacking>&>(tss_output);
                    Solution solution = branching_schemes[scheme_idx].to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "n " << tssibs_output.maximum_size_of_the_queue;
                    local_outputs[(size_t)scheme_idx].solution_pool.add(solution, ss.str());
                };
        }
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
    if (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic) {
        for (Counter scheme_idx = 0; scheme_idx < (Counter)branching_schemes.size(); ++scheme_idx) {
            algorithm_formatter.update_solution(
                    local_outputs[(size_t)scheme_idx].solution_pool.best(),
                    local_outputs[(size_t)scheme_idx].solution_pool.best_label());
        }
    }

    algorithm_formatter.end();
    return output;
}
