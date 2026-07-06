#pragma once

#include "packingsolver/rectangleguillotine/instance.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

/**
 * A rectangleguillotine block: a rectangular envelope filled by items
 * packed via guillotine cuts.
 *
 * Leaf blocks (is_simple = true) are uniform cx×cy grids of a single item
 * type.  Combined blocks (is_simple = false) are formed by placing two
 * sub-blocks side by side along either axis; child_1_id and child_2_id are
 * indices into the per-bin block list and always refer to earlier entries
 * (the list is built bottom-up).  The cut tree is reconstructed on demand
 * in to_solution from this hierarchical structure.
 */
struct Block
{
    /** Envelope dimensions (.w = width, .h = height). */
    Rectangle rect = {0, 0};

    Area item_area = 0;
    Profit item_profit = 0;
    Weight weight = 0;
    ItemPos number_of_items = 0;

    std::vector<std::pair<ItemTypeId, ItemPos>> item_copies;

    /**
     * Leaf block (is_simple = true): a cx×cy grid of one item type.
     * rect.w = cx * item_type.width(rotate) + (cx-1) * cut_thickness
     * rect.h = cy * item_type.height(rotate) + (cy-1) * cut_thickness
     */
    bool is_simple = false;
    ItemTypeId item_type_id = -1;
    bool rotate = false;

    /**
     * Combined block (is_simple = false): two sub-blocks placed side by side.
     * child_1_id and child_2_id are indices into the flat block list for the
     * bin; they are always smaller than this block's own index (children are
     * appended before the parent).
     * cut_is_vertical: true = vertical cut (children placed left/right, X-direction).
     *                  false = horizontal cut (children placed bottom/top, Y-direction).
     */
    bool cut_is_vertical = false;
    ItemPos child_1_id = -1;
    ItemPos child_2_id = -1;

    double fill_rate() const
    {
        return (rect.w > 0 && rect.h > 0)?
            (double)item_area / ((Area)rect.w * rect.h):
            0.0;
    }

    double efficiency() const
    {
        return (rect.w > 0 && rect.h > 0)?
            (double)item_profit / ((Area)rect.w * rect.h):
            0.0;
    }
};

std::ostream& operator<<(std::ostream& os, const Block& block);

/**
 * Parameters for compute_blocks().
 */
struct BlockParameters
{
    /** Maximum number of blocks to generate. */
    ItemPos maximum_number_of_blocks = 10000;
};

/**
 * Generate guillotine blocks for each bin type of the instance.
 *
 * Returns one vector of blocks per bin type (indexed by BinTypeId).
 */
std::vector<std::vector<Block>> compute_blocks(
        const Instance& instance,
        const BlockParameters& parameters = {});

} // namespace rectangleguillotine
} // namespace packingsolver
