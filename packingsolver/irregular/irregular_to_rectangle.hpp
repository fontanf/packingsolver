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

struct IrregularToRectangleOutput: packingsolver::Output<Instance, Solution>
{
    /** Constructor. */
    IrregularToRectangleOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct IrregularToRectangleParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Parameters for the rectangle sub-problem. */
    rectangle::OptimizeParameters rectangle_parameters;
};

const IrregularToRectangleOutput irregular_to_rectangle(
        const Instance& instance,
        const IrregularToRectangleParameters& parameters = {});

}
}
