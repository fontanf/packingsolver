#include "packingsolver/irregular/post_process.hpp"

#include "irregular/linear_programming.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

AnchorOutput packingsolver::irregular::anchor(
        const Solution& solution,
        double x_weight,
        double y_weight,
        const AnchorParameters& parameters)
{
    AnchorOutput output(solution.instance());

    LinearProgrammingAnchorParameters linear_programming_anchor_parameters;
    linear_programming_anchor_parameters.verbosity_level = 0;
    LinearProgrammingAnchorOutput linear_programming_anchor_output = linear_programming_anchor(
            solution,
            x_weight,
            y_weight,
            linear_programming_anchor_parameters);
    output.solution_pool.add(linear_programming_anchor_output.solution);

    return output;
}
