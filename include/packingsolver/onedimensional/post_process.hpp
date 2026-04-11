#pragma once

#include "packingsolver/onedimensional/solution.hpp"

namespace packingsolver
{
namespace onedimensional
{

struct GroupIdenticalBinsOutput: packingsolver::Output<Instance, Solution>
{
    GroupIdenticalBinsOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

/**
 * Group identical bins of a solution together.
 *
 * Bins with the same bin type and the same items at the same positions are
 * merged into a single entry with the combined copy count.
 */
GroupIdenticalBinsOutput group_identical_bins(const Solution& solution);

}
}
