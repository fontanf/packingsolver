/**
 * Dual feasible functions
 *
 * Formulas to get bounds on bin packing problems (with a single bin type).
 *
 * Generalization to three dimensions of the two-dimensional functions used in
 * 'rectangle/dual_feasible_functions.hpp', including that file's handling of
 * item rotation (there, up to 2 orientations; here, up to
 * 'NUMBER_OF_ROTATIONS' == 6): each item's coefficient is the minimum over
 * all of its allowed rotations, a valid lower bound on its true contribution
 * regardless of which rotation actually ends up being used.
 *
 * References:
 * - "New reduction procedures and lower bounds for the two-dimensional bin
 *   packing problem with fixed orientation" (Carlier et al., 2007)
 *   https://doi.org/10.1016/j.cor.2005.08.012
 * - "A general framework for bounds for higher-dimensional orthogonal
 *   packing problems" (Fekete and Schepers, 2004)
 *   https://doi.org/10.1002/mcda.350
 */

#pragma once

#include "packingsolver/box/optimize.hpp"

namespace packingsolver
{
namespace box
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

}
}
