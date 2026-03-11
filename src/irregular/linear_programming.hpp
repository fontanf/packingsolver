/**
 * Linear programming
 *
 * The goal of the linear programming algorithm is to push the items of a given
 * solution towards a given corner.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "mathoptsolverscmake/common.hpp"

namespace packingsolver
{
namespace irregular
{

struct LinearProgrammingOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    LinearProgrammingOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }

    Counter number_of_iterations = 0;
};

struct LinearProgrammingParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Linear programming solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;
};

LinearProgrammingOutput linear_programming(
        const Instance& instance,
        const Solution& initial_solution,
        const LinearProgrammingParameters& parameters = {});

}
}
