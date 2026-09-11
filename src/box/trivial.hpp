/**
 * Trivial bounds
 *
 * Cheap, closed-form upper bounds (no search): a volume-based Dantzig bound
 * for 'Knapsack', a volume-based bin count for 'BinPacking', and an
 * area/item-extent bound for the 'OpenDimension*' objectives.
 */

#pragma once

#include "packingsolver/box/optimize.hpp"

namespace packingsolver
{
namespace box
{

struct TrivialBoundsOutput: Output
{
    /** Constructor. */
    TrivialBoundsOutput(const Instance& instance):
        Output(instance) { }
};

struct TrivialBoundsParameters: packingsolver::Parameters<Instance, Solution, Output>
{
};

TrivialBoundsOutput trivial_bounds(
        const Instance& instance,
        const TrivialBoundsParameters& parameters);

}
}
