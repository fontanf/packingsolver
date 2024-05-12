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

namespace packingsolver
{
namespace boxstacks
{

struct SequentialOneDimensionalRectangleOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    SequentialOneDimensionalRectangleOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }


    /** Number of iterations. */
    Counter number_of_iterations = 0;

    /** Number of stack splits. */
    Counter number_of_stack_splits = 0;

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

struct SequentialOneDimensionalRectangleParameters: packingsolver::Parameters<Instance, Solution>
{
    bool sequential = true;

    /** Parameters for the onedimensional sub-problem. */
    onedimensional::OptimizeParameters onedimensional_parameters;

    /** Size of the queue in the rectangle subproblem. */
    NodeId rectangle_queue_size = 1024;

    bool move_intra_shift = false;
    bool move_intra_swap = false;
    bool move_add = true;
    bool move_inter_swap = false;
};

const SequentialOneDimensionalRectangleOutput sequential_onedimensional_rectangle(
        const Instance& instance,
        const SequentialOneDimensionalRectangleParameters& parameters = {});

}
}
