#include "packingsolver/boxstacks/post_process.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

GroupIdenticalBinsOutput packingsolver::boxstacks::group_identical_bins(
        const Solution& solution)
{
    GroupIdenticalBinsOutput output(solution.instance());
    Solution grouped_solution = packingsolver::group_identical_bins(solution);
    output.solution_pool.add(grouped_solution);
    return output;
}
