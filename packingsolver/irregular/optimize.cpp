#include "packingsolver/irregular/optimize.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/nlp.hpp"
#include "packingsolver/irregular/nlp_circle.hpp"
#include "packingsolver/algorithms/column_generation.hpp"

#include "treesearchsolver/iterative_beam_search.hpp"
#include "treesearchsolver/anytime_column_search.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::irregular;

const packingsolver::irregular::Output packingsolver::irregular::optimize(
        const Instance& instance,
        const OptimizeParameters& parameters)
{
    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.number_of_circular_items() == instance.number_of_items()) {

        NlpCircleParameters nlp_parameters;
        nlp_parameters.verbosity_level = 0;
        nlp_parameters.timer = parameters.timer;
        nlp_parameters.output_nl_path = parameters.output_nl_path;
        auto nlp_output = nlp_circle(instance, nlp_parameters);

        std::stringstream ss;
        algorithm_formatter.update_solution(nlp_output.solution_pool.best(), ss.str());

    } else {

        IrregularToRectangleParameters i2r_parameters
            = parameters.irregular_to_rectangle_parameters;
        i2r_parameters.verbosity_level = 0;
        i2r_parameters.timer = parameters.timer;
        i2r_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& ps_output)
        {
            std::stringstream ss;
            algorithm_formatter.update_solution(ps_output.solution_pool.best(), ss.str());
        };

        irregular_to_rectangle(
                instance,
                i2r_parameters);

        /*
        NlpParameters nlp_parameters;
        nlp_parameters.verbosity_level = 0;
        nlp_parameters.timer = parameters.timer;
        nlp_parameters.output_nl_path = parameters.output_nl_path;

        auto nlp_output = nlp(instance, nlp_parameters);

        std::stringstream ss;
        algorithm_formatter.update_solution(nlp_output.solution_pool.best(), ss.str());
        */

    }

    algorithm_formatter.end();
    return output;
}
