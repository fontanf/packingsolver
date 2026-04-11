#include "packingsolver/onedimensional/post_process.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

GroupIdenticalBinsOutput packingsolver::onedimensional::group_identical_bins(
        const Solution& solution)
{
    GroupIdenticalBinsOutput output(solution.instance());
    Solution grouped_solution = packingsolver::group_identical_bins(solution);
    output.solution_pool.add(grouped_solution);
    return output;
}
