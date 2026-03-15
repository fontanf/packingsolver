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

struct LinearProgrammingMinimizeOverlapOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    LinearProgrammingMinimizeOverlapOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }

    Counter number_of_iterations = 0;
};

struct LinearProgrammingMinimizeOverlapParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Linear programming solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;
};

LinearProgrammingMinimizeOverlapOutput linear_programming_minimize_overlap(
        const Solution& initial_solution,
        const LinearProgrammingMinimizeOverlapParameters& parameters = {});


struct LinearProgrammingAnchorToCornerOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    LinearProgrammingAnchorToCornerOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }

    Counter number_of_iterations = 0;
};

struct LinearProgrammingAnchorToCornerParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Anchor corner. */
    Corner anchor_corner = Corner::BottomLeft;

    /** Linear programming solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;
};

LinearProgrammingAnchorToCornerOutput linear_programming_anchor_to_corner(
        const Solution& initial_solution,
        const LinearProgrammingAnchorToCornerParameters& parameters = {});

}
}
