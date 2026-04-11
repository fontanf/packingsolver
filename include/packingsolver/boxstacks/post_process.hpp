#pragma once

#include "packingsolver/boxstacks/solution.hpp"

namespace packingsolver
{
namespace boxstacks
{

struct GroupIdenticalBinsOutput: packingsolver::Output<Instance, Solution>
{
    GroupIdenticalBinsOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

/**
 * Group identical bins of a solution together.
 *
 * Bins with the same bin type and the same stacks at the same positions are
 * merged into a single entry with the combined copy count.
 */
GroupIdenticalBinsOutput group_identical_bins(const Solution& solution);

}
}
