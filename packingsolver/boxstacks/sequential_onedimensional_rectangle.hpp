/**
 * Sequential onedimensional rectangle algorithm
 *
 * Algorithm for boxstacks Single Knpasck problems.
 *
 * The algorithm first generates stacks by solving a onedimensional
 * Variable-sized Bin Packing subproblem. Then, it packs these stacks by
 * solving a rectangle Knapsack subproblem.
 *
 * This algorithms is designed for boxstacks problems where the axle weight
 * constraints don't force to have sparse packings.
 */

#pragma once

#include "packingsolver/boxstacks/solution.hpp"

#include "packingsolver/onedimensional/optimize.hpp"
#include "packingsolver/rectangle/optimize.hpp"

namespace packingsolver
{
namespace boxstacks
{

struct SequentialOneDimensionalRectangleOutput
{
    /** Constructor. */
    SequentialOneDimensionalRectangleOutput(const Instance& instance):
        solution_pool(instance, 1) { }

    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;

    /**
     * Number of items in the solution found by the algorithm, before the
     * repair step.
     */
    ItemPos maximum_number_of_items = 0;

    /**
     * Boolean indicating if the solution found by the algorithm did not
     * satisfy the axle weight constraints before the repair step.
     */
    bool failed = false;

    /** Number of calls to the onedimensional subproblem. */
    Counter number_of_onedimensional_calls = 0;

    /** Time spent in the onedimensional subproblem. */
    double onedimensional_time = 0.0;

    /** Number of calls to the rectangle subproblem. */
    Counter number_of_rectangle_calls = 0;

    /** Time spent in the rectangle subproblem. */
    double rectangle_time = 0.0;
};

struct SequentialOneDimensionalRectangleOptionalParameters
{
    /** Parameters for the onedimensional sub-problem. */
    onedimensional::OptimizeOptionalParameters onedimensional_parameters;

    /** Size of the queue in the rectangle subproblem. */
    NodeId rectangle_queue_size = 1024;

    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();

    bool move_intra_shift = false;
    bool move_intra_swap = false;
    bool move_add = true;
    bool move_inter_swap = false;
};

SequentialOneDimensionalRectangleOutput sequential_onedimensional_rectangle(
        const Instance& instance,
        SequentialOneDimensionalRectangleOptionalParameters parameters = {});

}
}
