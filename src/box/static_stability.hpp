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

}
}
