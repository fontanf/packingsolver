#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct LabelingOutput: packingsolver::Output<Instance, Solution>
{
    LabelingOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct LabelingParameters: packingsolver::Parameters<Instance, Solution>
{
};

/**
 * Labeling algorithm for the (bounded-copies) guillotine 2-dimensional
 * knapsack problem.
 *
 * This is a simple version of the algorithm of Léonard and Clautiaux
 * (2025): the upper bounds guiding the labeling algorithm are computed
 * using the classical dynamic programming approach of Beasley (1985) and
 * Claut et al. (2018) (functions 'DP' and 'RDP' in the paper), without the
 * linear programming model described in the paper.
 *
 * Limitations of this initial implementation:
 * - only the first bin type of the instance is considered;
 * - defects, stacks, minimum waste, minimum/maximum distance between cuts
 *   and the number of stages are not taken into account, similarly to
 *   'dynamic_programming_infinite_copies_array';
 * - the destructive lower bound described by Dolatabadi, Lodi and Monaci
 *   (2012), used to further strengthen the profit criterion, is not
 *   implemented yet.
 */
const LabelingOutput labeling(
        const Instance& instance,
        const LabelingParameters& parameters = {});

}
}
