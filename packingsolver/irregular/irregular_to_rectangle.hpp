/**
 * Irregular to rectangle
 *
 * Algorithm for irregular packing problems.
 *
 * The algorithm computes an enclosing rectangle for each item type and then
 * run a rectangle packing algorithm.
 *
 * Requirements:
 * - Items must have shape type 'MultiPolygonWithHoles'
 * - Containers must have shape type 'Rectangle'
 */

#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "packingsolver/rectangle/optimize.hpp"

namespace packingsolver
{
namespace irregular
{

struct IrregularToRectangleOutput
{
    /** Constructor. */
    IrregularToRectangleOutput(const Instance& instance):
        solution_pool(instance, 1) { }

    /** Solution pool. */
    SolutionPool<Instance, Solution> solution_pool;
};

using IrregularToRectangleNewSolutionCallback = std::function<void(const IrregularToRectangleOutput&)>;

struct IrregularToRectangleOptionalParameters
{
    /** Parameters for the rectangle sub-problem. */
    rectangle::OptimizeOptionalParameters rectangle_parameters;

    /** New solution callback. */
    IrregularToRectangleNewSolutionCallback new_solution_callback
        = [](const IrregularToRectangleOutput&) { };

    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();
};

IrregularToRectangleOutput irregular_to_rectangle(
        const Instance& instance,
        IrregularToRectangleOptionalParameters parameters = {});

}
}
