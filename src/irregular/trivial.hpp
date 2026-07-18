#pragma once

#include "packingsolver/irregular/optimize.hpp"

namespace packingsolver
{
namespace irregular
{

struct TrivialSingleItemOutput: Output
{
    TrivialSingleItemOutput(const Instance& instance):
        Output(instance) { }
};

struct TrivialSingleItemParameters: packingsolver::Parameters<Instance, Solution, Output>
{
};

/**
 * Trivial single-item algorithm
 *
 * This algorithm handles Knapsack and BinPacking problems with a single bin
 * and a single item. It tries to place the item by aligning the center of
 * its AABB with the center of the bin's AABB. The placement is validated
 * by checking intersections with the bin borders and defects using a
 * shape::IntersectionTree. If the item does not fit, no solution is returned.
 */
TrivialSingleItemOutput trivial_single_item(
        const Instance& instance,
        const TrivialSingleItemParameters& parameters = {});

}
}
