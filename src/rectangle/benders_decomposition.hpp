#pragma once

#include "packingsolver/rectangle/optimize.hpp"

#include "mathoptsolverscmake/mathopt.hpp"

namespace packingsolver
{
namespace rectangle
{

struct BendersDecompositionOutput: Output
{
    /** Constructor. */
    BendersDecompositionOutput(const Instance& instance):
        Output(instance) { }

    /** Number of iterations. */
    Counter number_of_iterations = 0;
};

struct BendersDecompositionParameters: packingsolver::Parameters<Instance, Solution, Output>
{
    /** MILP solver. */
    mathoptsolverscmake::SolverName solver = mathoptsolverscmake::SolverName::Highs;

    /** Optimization mode. */
    OptimizationMode optimization_mode = OptimizationMode::Anytime;

    /** Use the tree search algorithm to solve the master problem. */
    bool master_problem_use_tree_search = false;

    /** Use the MILP assignment algorithm to solve the master problem. */
    bool master_problem_use_milp_assignment = true;

    /**
     * Maximum number of iterations, in non-'Anytime' optimization modes
     * only (-1: unlimited). 'Anytime' mode instead relies on the timer (or
     * full convergence, i.e. every bin of the master's candidate found
     * geometrically feasible) to stop, since it is expected to keep
     * improving for as long as it is given to run.
     */
    Counter not_anytime_maximum_number_of_iterations = -1;

    /**
     * Maximum number of iterations that actually solve at least one
     * feasibility subproblem, in non-'Anytime' optimization modes only
     * (-1: unlimited). Unlike 'not_anytime_maximum_number_of_iterations',
     * an iteration whose master candidate is cut by a dual-feasible-
     * function inequality before any subproblem is solved (see
     * 'find_most_violated_dual_feasible_function_cut' in
     * 'benders_decomposition.cpp') doesn't count towards this limit, since
     * it never actually pays for a subproblem solve.
     */
    Counter not_anytime_maximum_number_of_subproblem_solves = 0;

    /** Size of the queue for the knapsack subproblem. */
    NodeId subproblem_queue_size = 512;

    /**
     * Maximum number of minimal-infeasible-subset no-good cuts to generate
     * per infeasible bin found (see 'enumerate_minimal_infeasible_subsets'
     * in 'benders_decomposition.cpp'); bounds how much the search for
     * additional cuts can cost per Benders iteration.
     */
    Counter maximum_number_of_no_good_cuts_per_bin = 8;
};

BendersDecompositionOutput benders_decomposition(
        const Instance& instance,
        const BendersDecompositionParameters& parameters);

}
}
