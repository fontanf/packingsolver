/**
 * Column generation algorithm
 *
 * Algorithm for k-staged guillotine single knapsack and open dimensions
 * problems.
 *
 * Input:
 * - a bin
 * - n item types with qⱼ copies
 * Problem:
 * - find a valid pattern such that all item j is not used more than qⱼ times.
 * Objective:
 * - maximize the profit of the cut items.
 *
 * The linear programming formulation of the knapsack rectangleguillotine
 * problem based on Dantzig–Wolfe decomposition is written as follows:
 *
 * Variables:
 * - yᵏ ∈ {0, qmax} representing a set of items fitting into a first-level
 *   subplate.
 *   yᵏ = q iff the corresponding first-level subplate is selected q times.
 *   xⱼᵏ = q iff yᵏ contains q copies of item type j, otherwise 0.
 *
 * Program:
 *
 * max  ∑ₖ (∑ⱼ pⱼ xⱼᵏ) yᵏ
 *
 * ∑ₖ (maxⱼ wⱼ xⱼᵏ) yᵏ <= W
 *                                                         (width of the bin)
 *                                                          Dual variables: u
 *
 * 0 <= ∑ₖ xⱼᵏ yᵏ <= qⱼ      for all item types j
 *                                      (each item selected at most qⱼ times)
 *                                                         Dual variables: vⱼ
 *
 * The pricing problem consists in finding a variable of negative reduced cost.
 * The reduced cost of a variable yᵏ is given by:
 * rc(yᵢᵏ) = (maxⱼ wⱼ xⱼᵏ) u + ∑ⱼ xⱼᵏ vⱼ - pⱼ
 *
 * We solve the problem for each possible width.
 * Therefore, we need to solve (k-1)-staged guillotine single knapsack
 * subproblems.
 *
 * If k-staged is 2-staged exact, 2-staged non-exact or 3-staged homogenous,
 * then solving the pricing problem reduces to solving onedimensional single
 * knapsack problems.
 *
 * Otherwise, the pricing problem is solved with the same algorithm recursively.
 *
 */

#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include "columngenerationsolver/linear_programming_solver.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct ColumnGeneration2Output: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    ColumnGeneration2Output(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }

    CutOrientation best_solution_first_stage_orientation;

    NodeId best_solution_number_of_nodes;
};

struct ColumnGeneration2Parameters: packingsolver::Parameters<Instance, Solution>
{
    bool automatic_stop = false;

    /** Linear programming solver. */
    columngenerationsolver::SolverName solver_name
        = columngenerationsolver::SolverName::CLP;
};

const ColumnGeneration2Output column_generation_2(
        const Instance& instance,
        const ColumnGeneration2Parameters& parameters = {});

}
}
