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


struct EdgeSeparationConstraintParameters
{
    /** Shape to which the separating line belongs. */
    ShapePos edge_shape_pos = -1;

    /** Separating element. */
    ElementPos edge_element_pos = -1;

    /** Element of the closest point. */
    ElementPos point_element_pos = -1;

    /** Closest point from the separating element. */
    Point point;

    /** Signed distance between the closest element and the separating line. */
    LengthDbl distance;

    /**
     * Signed distance with the unscaled point.
     *
     * Used as tie-breaker when the shape scale is 0.0.
     */
    LengthDbl distance_unscaled;


    LengthDbl coef_x1;
    LengthDbl coef_y1;
    LengthDbl coef_x2;
    LengthDbl coef_y2;
    LengthDbl coef_lambda1;
    LengthDbl coef_lambda2;
};

/**
 * Find an edge to separate two given shapes.
 *
 * The selected edge is the less constraining one.
 */
EdgeSeparationConstraintParameters find_best_edge_separator(
        const Shape& shape_1,
        const Point& shift_1,
        double scale_1,
        const Shape& shape_2,
        const Point& shift_2,
        double scale_2);

struct LinearProgrammingMinimizeShrinkageOutput
{
    /** Constructor. */
    LinearProgrammingMinimizeShrinkageOutput(const Instance& instance):
        solution(instance) { }

    Solution solution;

    /** Boolean indicating if the final solution is feasible (all items at full scale). */
    bool feasible = false;

    /** For each item position, true if the item was scaled below 1 in the final LP. */
    std::vector<bool> items_shrunken;

    /** Final lambda value for each item (scale factor in [0, 1]). */
    std::vector<double> final_lambda;

    Counter number_of_iterations = 0;
};

struct LinearProgrammingMinimizeShrinkageParameters: packingsolver::Parameters<Instance, Solution>
{

    /** Linear programming solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;
};

LinearProgrammingMinimizeShrinkageOutput linear_programming_minimize_shrinkage(
        const Solution& initial_solution,
        const std::vector<double>& initial_lambda,
        const std::vector<Profit>& item_penalties,
        const LinearProgrammingMinimizeShrinkageParameters& parameters = {});


struct LinearProgrammingAnchorOutput
{
    /** Constructor. */
    LinearProgrammingAnchorOutput(const Instance& instance):
        solution(instance) { }

    Solution solution;

    Counter number_of_iterations = 0;
};

struct LinearProgrammingAnchorParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Linear programming solver. */
    mathoptsolverscmake::SolverName solver
        = mathoptsolverscmake::SolverName::Highs;
};

LinearProgrammingAnchorOutput linear_programming_anchor(
        const Solution& initial_solution,
        double x_weight,
        double y_weight,
        const LinearProgrammingAnchorParameters& parameters = {});

}
}
