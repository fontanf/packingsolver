/**
 * Dual feasible functions
 *
 * Formulas to get bounds on bin packing problems (with a single bin type and
 * without item rotation).
 *
 * References:
 * - "New reduction procedures and lower bounds for the two-dimensional bin
 *   packing problem with fixed orientation" (Carlier et al., 2007)
 *   https://doi.org/10.1016/j.cor.2005.08.012
 * - "A theoretical and experimental study of fast lower bounds for the
 *   two-dimensional bin packing problem" (Serairi1 et Haouari, 2018)
 *   https://doi.org/10.1051/ro/2017019
 * - "A new lower bound for the non-oriented two-dimensional bin-packing
 *   problem☆" (Clautiaux et al., 2007)
 *   https://doi.org/10.1016/j.orl.2006.07.001
 */

#pragma once

#include "packingsolver/rectangle/optimize.hpp"

namespace packingsolver
{
namespace rectangle
{

struct DualFeasibleFunctionsOutput: Output
{
    /** Constructor. */
    DualFeasibleFunctionsOutput(const Instance& instance):
        Output(instance) { }
};

struct DualFeasibleFunctionsParameters: packingsolver::Parameters<Instance, Solution, Output>
{
};

DualFeasibleFunctionsOutput dual_feasible_functions(
        const Instance& instance,
        const DualFeasibleFunctionsParameters& parameters);

struct DualFeasibleFunctionsCut
{
    /** 'true' if a violated dual-feasible-function inequality was found. */
    bool found = false;

    /**
     * Coefficient of each item type in the cut (0 for item types that do
     * not appear in it), indexed by item type id.
     */
    std::vector<double> coefficients;

    /** Right-hand side of the cut. */
    double bound = 0.0;

    /**
     * Amount by which the cut is violated by 'selected_items' (sum of the
     * coefficients over 'selected_items' minus 'bound').
     *
     * Only meaningful if 'found' is 'true'; used to rank candidate cuts
     * against each other.
     */
    double violation = 0.0;
};

/**
 * Look for the most violated dual-feasible-function inequality for a given
 * selection of items assigned to a single bin.
 *
 * The returned cut, if any, is valid for any selection of items from
 * 'instance' assigned to a single bin of its (unique) bin type, not only
 * for 'selected_items' - so it may be added as a standalone, permanently
 * reusable constraint (e.g. in a Benders decomposition master problem),
 * not just a one-off no-good cut on this exact selection.
 *
 * 'found = false' does not prove that 'selected_items' fits into the bin,
 * only that this necessary condition does not disprove it.
 */
DualFeasibleFunctionsCut find_most_violated_dual_feasible_function_cut(
        const Instance& instance,
        const std::vector<std::pair<ItemTypeId, ItemPos>>& selected_items);

}
}
