#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "columngenerationsolver/commons.hpp"

namespace packingsolver
{
namespace irregular
{

struct AnchorToCornerOutput: packingsolver::Output<Instance, Solution>
{
    AnchorToCornerOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct AnchorToCornerParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Anchor corner. */
    Corner anchor_corner = Corner::BottomLeft;

    /** Linear programming solver. */
    columngenerationsolver::SolverName linear_programming_solver_name
        = columngenerationsolver::SolverName::Highs;
};

AnchorToCornerOutput anchor_to_corner(
        const Solution& solution,
        const AnchorToCornerParameters& parameters = {});

}
}
