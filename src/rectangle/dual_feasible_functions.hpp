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

#include "packingsolver/rectangle/solution.hpp"

namespace packingsolver
{
namespace rectangle
{

struct DualFeasibleFunctionsOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    DualFeasibleFunctionsOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct DualFeasibleFunctionsParameters: packingsolver::Parameters<Instance, Solution>
{
};

DualFeasibleFunctionsOutput dual_feasible_functions(
        const Instance& instance,
        const DualFeasibleFunctionsParameters& parameters);

}
}
