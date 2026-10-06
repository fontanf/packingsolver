/**
 * Sequential onedimensional rectangle algorithm
 *
 * Algorithm for boxstacks single-bin Knapsack, Feasibility and OpenDimension*
 * problems.
 *
 * The algorithm first generates stacks by solving a onedimensional
 * Variable-sized Bin Packing subproblem. Then, it packs these stacks by
 * solving a rectangle Knapsack subproblem (a rectangle OpenDimensionX
 * subproblem for the OpenDimensionX objective). If the solution found violates
 * the axle weight constraints, the boxstacks tree search is run afterwards.
 *
 * The bin is filled along X. To fill it along Y, the algorithm is run on the
 * flipped instance.
 *
 * This algorithms is designed for boxstacks problems where the axle weight
 * constraints don't force to have sparse packings.
 */

#pragma once

#include "packingsolver/boxstacks/optimize.hpp"

#include "packingsolver/onedimensional/optimize.hpp"

namespace packingsolver
{
namespace boxstacks
{

struct SequentialOneDimensionalRectangleOutput: Output
{
    /** Constructor. */
    SequentialOneDimensionalRectangleOutput(const Instance& instance):
        Output(instance) { }


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

    /**
     * True if, on the first iteration (see 'number_of_iterations') of the
     * last pass, at least one rectangle branching scheme call explored its
     * whole reachable search space without the queue size ever being the
     * limiting factor (the underlying 'iterative_beam_search_2' call's own
     * 'optimal' output field) - i.e. a larger queue size is guaranteed to
     * yield the exact same rectangle subproblem result.
     *
     * Lets the 'Anytime' mode detect that growing the queue size further is
     * pointless - which on a small/easy instance can otherwise happen so
     * many times, so fast, that the queue size overflows - without having to
     * pick an arbitrary cap.
     */
    bool rectangle_subproblem_explored_exhaustively = false;

    /** Number of passes (see 'anytime_rectangle_initial_queue_size'). */
    Counter number_of_passes = 0;

    /**
     * Boolean indicating if the solution found by the first pass did not
     * satisfy the axle weight constraints before the repair step.
     */
    bool first_pass_failed = false;

    /** Boolean indicating if the first pass packed all items. */
    bool first_pass_full = false;

    /** Number of calls to the tree search. */
    Counter number_of_tree_search_calls = 0;

    /**
     * Number of calls to the tree search after which all items are packed.
     */
    Counter number_of_tree_search_perfect = 0;

    /**
     * Number of calls to the tree search which improved the solution found by
     * the sequential onedimensional rectangle algorithm.
     */
    Counter number_of_tree_search_better = 0;

    /** Time spent in the tree search. */
    double tree_search_time = 0.0;
};

struct SequentialOneDimensionalRectangleParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    bool sequential = true;

    /**
     * Directions in which the bin is filled.
     *
     * An empty vector means "decide automatically" - see
     * 'sequential_onedimensional_rectangle''s own body for the automatic
     * logic.
     */
    std::vector<Direction> directions;

    /**
     * Optimization mode.
     *
     * In 'Anytime' mode, successive passes are performed with growing queue
     * sizes (see 'anytime_rectangle_initial_queue_size'). Otherwise, a
     * single pass is performed.
     *
     * In 'NotAnytimeSequential' mode, the directions are run sequentially;
     * in 'NotAnytimeDeterministic' mode, their solutions are reported in a
     * deterministic order.
     */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Parameters for the onedimensional sub-problem. */
    onedimensional::OptimizeParameters onedimensional_parameters;

    /**
     * Initial size of the queue of the rectangle subproblem in 'Anytime'
     * mode.
     *
     * In 'Anytime' mode, the algorithm performs successive passes, growing
     * the rectangle and tree search queue sizes from their initial sizes by
     * the same factor at each pass.
     *
     * The rectangle and tree search queue sizes both grow from their own
     * initial size, rather than from a shared '1': their non-anytime sizes
     * differ (1024 vs 512 by default), so growing both from the same
     * starting point would have one reach its own non-anytime size, and stop
     * mattering, well before the other. Scaling the initial sizes by that
     * same ratio (2:1 by default) keeps the fraction of "full effort"
     * reached at any given pass the same for both.
     */
    NodeId anytime_rectangle_initial_queue_size = 2;

    /** Initial size of the queue of the tree search in 'Anytime' mode. */
    NodeId anytime_tree_search_initial_queue_size = 1;

    /**
     * Size of the queue of the rectangle subproblem when not in 'Anytime'
     * mode.
     */
    NodeId not_anytime_rectangle_queue_size = 1024;

    /** Size of the queue of the tree search when not in 'Anytime' mode. */
    NodeId not_anytime_tree_search_queue_size = 512;

    /** Guides of the tree search. */
    std::vector<GuideId> tree_search_guides;

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
