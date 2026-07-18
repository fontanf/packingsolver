/**
 * MILP raster
 *
 * The goal of the MILP raster algorithm is to find a packing solution by
 * rasterizing the bin and solving a mixed-integer linear program.
 */

#pragma once

#include "packingsolver/irregular/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace irregular
{

struct MilpRasterOutput: Output
{
    /** Constructor. */
    MilpRasterOutput(const Instance& instance):
        Output(instance) { }
};

struct MilpRasterParameters: packingsolver::Parameters<Instance, Solution, Output>
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
