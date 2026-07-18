/**
 * Dual feasible functions
 *
 * Formulas to get bounds on bin packing problems (with a single bin type and
 * without item rotation).
 *
 * Generalization to three dimensions of the two-dimensional functions used in
 * 'rectangle/dual_feasible_functions.hpp'.
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
