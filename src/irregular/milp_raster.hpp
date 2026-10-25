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

    /**
     * Maximum number of cell sizes: the algorithm solves the MILP of a cell
     * size, then halves it, until the timer ends or this number of cell sizes
     * is reached.
     *
     * -1 means no limit.
     */
    Counter maximum_number_of_cell_sizes = -1;

    /**
     * Maximum number of nodes of the branch-and-bound of the MILP solver,
     * for each cell size.
     *
     * -1 means no limit.
     */
    Counter maximum_number_of_nodes = -1;
};

MilpRasterOutput milp_raster(
        const Instance& instance,
        const MilpRasterParameters& parameters = {});

}
}
