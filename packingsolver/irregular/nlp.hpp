/**
 * Nonlinear programming based algorithms
 *
 * Algorithm for irregular single Knapsack, Open Dimension problems.
 * - Problem type: 'irregular'
 *   - Item types must have a shape of type 'Polygon'
 *   - Bin types must have a shape of the 'Rectangle'
 * - Objective: 'OpenDimensionX'
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

struct NlpOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    NlpOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct NlpParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Path of the .nl output file. */
    std::string output_nl_path;
};

const NlpOutput nlp(
        const Instance& instance,
        const NlpParameters& parameters = {});

}
}
