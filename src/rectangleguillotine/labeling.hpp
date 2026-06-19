#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct LabelingOutput: packingsolver::Output<Instance, Solution>
{
    LabelingOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

struct LabelingParameters: packingsolver::Parameters<Instance, Solution>
{
};

/**
 * Labeling algorithm for the (bounded-copies) guillotine 2-dimensional
 * knapsack problem.
 *
 * An alternative implementation of the labeling algorithm that generates
 * states (blocks) bottom-up using a best-first priority queue ordered by
 * profit + rdp(w, h), analogous to the block generation algorithm for box
 * packing (src/rectangleguillotine/block.cpp).
 *
 * Each state is a rectangular block with a tight bounding box containing
 * items packed via guillotine cuts.  Two blocks are merged by placing them
 * side-by-side (vertical or horizontal cut); the combined bounding box takes
 * max on the non-cut axis, so sub-blocks do not need matching dimensions.
 *
 * The algorithm processes states in decreasing order of
 * profit + rdp(width, height) and stops as soon as no remaining state in
 * the queue can improve the best root block found so far.
 *
 * Same limitations as the labeling algorithm: only the first bin type is
 * considered; defects, stacks, minimum waste, minimum/maximum distance
 * between cuts and the number of stages are not taken into account.
 */
const LabelingOutput labeling(
        const Instance& instance,
        const LabelingParameters& parameters = {});

}
}
