/**
 * Mixed-integer nonlinear programming
 *
 * Algorithm for irregular single Knapsack, Open Dimension problems.
 *
 * Some references from which this implementation is inspired:
 * - "Mathematical model and efficient algorithms for object packing problem"
 *   (Chernov et al., 2010)
 *   https://doi.org/10.1016/j.comgeo.2009.12.003
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

namespace packingsolver
{
namespace irregular
{

struct NlpOutput
{
    /** Constructor. */
    NlpOutput(const Instance& instance):
        solution_pool(instance, 1) { }

    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;
};

struct NlpOptionalParameters
{
    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();

    /** Path of the .nl output file. */
    std::string output_nl_path;
};

NlpOutput nlp(
        const Instance& instance,
        NlpOptionalParameters parameters = {});

}
}
