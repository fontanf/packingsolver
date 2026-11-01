/**
 * Local nonlinear programming
 *
 * The goal of the local nonlinear programming algorithm is to reduce the
 * shrinkage of items in a given solution by iteratively solving a nonlinear
 * program.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

namespace packingsolver
{
namespace irregular
{

struct NonlinearProgrammingMinimizeShrinkageOutput
{
    /** Constructor. */
    NonlinearProgrammingMinimizeShrinkageOutput(const Instance& instance):
        solution(instance) { }

    Solution solution;

    /** Boolean indicating if the final solution is feasible (all items at full scale). */
    bool feasible = false;

    /** For each item position, true if the item was scaled below 1 in the final solution. */
    std::vector<bool> items_shrunken;

    /** Final lambda value for each item (scale factor in [0, 1]). */
    std::vector<double> final_lambda;

    Counter number_of_iterations = 0;
};

struct NonlinearProgrammingMinimizeShrinkageParameters: packingsolver::Parameters<Instance, Solution>
{
};

NonlinearProgrammingMinimizeShrinkageOutput nonlinear_programming_minimize_shrinkage(
        const Solution& initial_solution,
        const std::vector<double>& initial_lambda,
        const std::vector<Profit>& item_penalties,
        const NonlinearProgrammingMinimizeShrinkageParameters& parameters = {});

}
}
