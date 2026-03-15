#include "packingsolver/irregular/post_process.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/linear_programming.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

AnchorToCornerOutput packingsolver::irregular::anchor_to_corner(
        const Solution& solution,
        const AnchorToCornerParameters& parameters)
{
    AnchorToCornerOutput output(solution.instance());

    return output;
}
