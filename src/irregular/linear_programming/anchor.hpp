/**
 * Linear programming anchor
 *
 * The goal of the linear programming anchor algorithm is to push the items of
 * a given solution towards a given corner.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace irregular
{

struct LinearProgrammingAnchorOutput
{
    /** Constructor. */
    LinearProgrammingAnchorOutput(const Instance& instance):
        solution(instance) { }

    Solution solution;

    Counter number_of_iterations = 0;
};

struct LinearProgrammingAnchorParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Linear programming solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;
};

LinearProgrammingAnchorOutput linear_programming_anchor(
        const Solution& initial_solution,
        double x_weight,
        double y_weight,
        const LinearProgrammingAnchorParameters& parameters = {});

}
}
