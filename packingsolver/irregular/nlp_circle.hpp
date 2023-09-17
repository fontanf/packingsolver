/**
 * Nonlinear programming based algorithms
 *
 * Algorithm for
 * - Problem type: 'irregular'
 *   - Item types must have a shape of type 'Circle'
 *   - Bin types must have shape of type 'Rectangle'
 * - Objective: 'OpenDimensionX'
 *
 * Some references from which this implementation is inspired:
 * - "Packing unequal circles into a strip of minimal length with a jump
 *   algorithm" (Stoyan et Yaskov, 2014)
 *   https://doi.org/10.1007/s11590-013-0646-1
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

namespace packingsolver
{
namespace irregular
{

struct NlpCircleOutput
{
    /** Constructor. */
    NlpCircleOutput(const Instance& instance):
        solution_pool(instance, 1) { }

    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;
};

struct NlpCircleOptionalParameters
{
    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();

    /** Path of the .nl output file. */
    std::string output_nl_path;
};

NlpCircleOutput nlp_circle(
        const Instance& instance,
        NlpCircleOptionalParameters parameters = {});

}
}
