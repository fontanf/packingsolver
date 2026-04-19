/**
 * MILP raster
 *
 * The goal of the MILP raster algorithm is to find a packing solution by
 * rasterizing the bin and solving a mixed-integer linear program.
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace irregular
{

struct MilpRasterOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    MilpRasterOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct MilpRasterParameters: packingsolver::Parameters<Instance, Solution>
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;
};

MilpRasterOutput milp_raster(
        const Instance& instance,
        const MilpRasterParameters& parameters = {});

}
}
