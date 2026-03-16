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

    LinearProgrammingAnchorToCornerParameters lpatc_parameters;
    lpatc_parameters.verbosity_level = 0;
    lpatc_parameters.anchor_corner = parameters.anchor_corner;
    LinearProgrammingAnchorToCornerOutput atc_output = linear_programming_anchor_to_corner(
            solution,
            lpatc_parameters);
    output.solution_pool.add(atc_output.solution_pool.best());

    return output;
}
