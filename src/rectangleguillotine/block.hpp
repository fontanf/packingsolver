#pragma once

#include "packingsolver/rectangleguillotine/instance.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

std::ostream& operator<<(std::ostream& os, const Block& block);

/**
 * Generate the simple blocks of an item type for a bin type: the grids of
 * copies of the item type, in each of its orientations, which fit in the
 * bin.
 */
std::vector<Block> compute_simple_blocks(
        const Instance& instance,
        BinTypeId bin_type_id,
        ItemTypeId item_type_id);

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
