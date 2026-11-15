#pragma once

#include "packingsolver/box/solution.hpp"

namespace packingsolver
{
namespace box
{

/**
 * Return true iff the items of a bin are in static mechanical equilibrium.
 *
 * The items are rigid bodies whose only external force is their weight,
 * applied at their geometric center. An item is supported through the
 * contacts between its bottom face and the floor of the bin or the top faces
 * of the items below it. A contact is a rectangle, and any pressure on it
 * which only pushes is equivalent to non-negative forces at its four
 * corners. The items are in equilibrium iff there exist such forces for
 * which, for each item, the vertical forces and the moments about the
 * horizontal axes are balanced (Galrão Ramos, Oliveira, Gonçalves and Lopes,
 * 2016, "A container loading algorithm with static mechanical equilibrium
 * stability constraints"). The existence of these forces is checked by
 * solving a linear program.
 *
 * Contrary to the checks on the support of each item alone, the forces that
 * an item transmits to the items below it are taken into account. Lateral
 * contacts are not: an item leaning against another one is not stable.
 */
bool is_statically_stable(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items);

/**
 * Return true iff the items of a bin satisfy the maximum weights above of
 * their item types.
 *
 * The forces are those of 'is_statically_stable'. The items satisfy the
 * maximum weights above iff there exist such forces which balance the
 * vertical forces of each item (and its moments, with the static stability)
 * and for which the total force on the top face of each item is at most the
 * maximum weight above of its item type. So, each item with a weight must
 * rest on the floor of the bin or on other items.
 */
bool satisfies_maximum_weights_above(
        const Instance& instance,
        const BinType& bin_type,
        const std::vector<SolutionItem>& items);

/**
 * Let the items of the bins of a solution which aren't stable, or which don't
 * satisfy the maximum weights above, fall.
 *
 * Each item is moved down until it rests on the floor of the bin or on the
 * top face of another item. The items only move down and the items whose
 * footprints intersect keep their vertical order, so the solution remains
 * valid, with the same items in the same bins. No item floats any more, but
 * an item which overhangs too much may still tip.
 *
 * The items are moved in a single pass, from the lowest to the highest one:
 * an item lands on the highest top face of the items already moved whose
 * footprints intersect its footprint.
 */
Solution drop_items(const Solution& solution);

/**
 * Remove items from the bins of a solution which aren't stable, or which
 * don't satisfy the maximum weights above, until they do.
 *
 * While a bin doesn't, the items which can't be balanced are those whose
 * balance rows are violated in an optimal solution of the equilibrium
 * linear program in which these rows may be violated at a cost, with the
 * maximum weights above. The highest of them is removed with the items which
 * rest on it, directly or not; the other items keep their positions, so they
 * remain supported.
 *
 * It is meant to be called after 'drop_items', so that the floating items
 * have fallen instead of being removed.
 */
Solution remove_unbalanced_items(const Solution& solution);

}
}
