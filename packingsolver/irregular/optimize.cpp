#include "packingsolver/irregular/optimize.hpp"

#include "packingsolver/irregular/nlp.hpp"
#include "packingsolver/irregular/nlp_circle.hpp"
#include "packingsolver/algorithms/column_generation.hpp"

#include "treesearchsolver/iterative_beam_search.hpp"
#include "treesearchsolver/anytime_column_search.hpp"

#include <thread>

using namespace packingsolver;
using namespace packingsolver::irregular;

Output packingsolver::irregular::optimize(
        const Instance& instance,
        OptimizeOptionalParameters parameters)
{
    Output output(instance);

    Algorithm algorithm = Algorithm::IrregularToRectangle;
    if (parameters.algorithm != Algorithm::Auto) {
        algorithm = parameters.algorithm;
    } else {
        algorithm = Algorithm::IrregularToRectangle;
    }
    std::stringstream ss;
    ss << algorithm;
    parameters.info.add_to_json("Algorithm", "Algorithm", ss.str());

    output.solution_pool.best().algorithm_start(parameters.info, algorithm);

    if (algorithm == Algorithm::IrregularToRectangle) {
        IrregularToRectangleOptionalParameters i2r_parameters
            = parameters.irregular_to_rectangle_parameters;
        i2r_parameters.new_solution_callback = [&parameters, &output](
                const IrregularToRectangleOutput& i2r_output)
        {
            // Lock mutex.
            parameters.info.lock();

            std::stringstream ss;
            bool new_best = output.solution_pool.add(
                    i2r_output.solution_pool.best(),
                    ss,
                    parameters.info);
            if (new_best)
                parameters.new_solution_callback(output);

            // Unlock mutex.
            parameters.info.unlock();
        };
        i2r_parameters.info = Info(parameters.info, false, "");

        irregular_to_rectangle(
                instance,
                i2r_parameters);

    } else if (algorithm == Algorithm::Nlp
            && instance.number_of_circular_items() == instance.number_of_items()) {
        NlpCircleOptionalParameters nlp_parameters;
        nlp_parameters.output_nl_path = parameters.output_nl_path;
        nlp_parameters.info = Info(parameters.info, false, "");
        auto nlp_output = nlp_circle(instance, nlp_parameters);

        std::stringstream ss;
        bool new_best = output.solution_pool.add(
                nlp_output.solution_pool.best(),
                ss,
                parameters.info);
        if (new_best)
            parameters.new_solution_callback(output);

    } else if (algorithm == Algorithm::Nlp) {
        NlpOptionalParameters nlp_parameters;
        nlp_parameters.output_nl_path = parameters.output_nl_path;
        nlp_parameters.info = Info(parameters.info, false, "");

        auto nlp_output = nlp(instance, nlp_parameters);

        std::stringstream ss;
        bool new_best = output.solution_pool.add(
                nlp_output.solution_pool.best(),
                ss,
                parameters.info);
        if (new_best)
            parameters.new_solution_callback(output);
    }

    output.solution_pool.best().algorithm_end(parameters.info);
    return output;
}

