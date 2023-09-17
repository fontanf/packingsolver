#pragma once

#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/solution.hpp"

#include "packingsolver/irregular/irregular_to_rectangle.hpp"

namespace packingsolver
{
namespace irregular
{

struct Output
{
    Output(const Instance& instance):
        solution_pool(instance, 1) { }

    SolutionPool<Instance, Solution> solution_pool;
};

using NewSolutionCallback = std::function<void(const Output&)>;

struct OptimizeOptionalParameters
{
    /** Algorithm. */
    Algorithm algorithm = Algorithm::Auto;

    /** New solution callback. */
    NewSolutionCallback new_solution_callback = [](const Output&) { };


    /** Parameters of the algorithm 'IrregularToRectangle'. */
    IrregularToRectangleOptionalParameters irregular_to_rectangle_parameters;


    /** Path of the .nl output file. */
    std::string output_nl_path;


    /** Info structure. */
    optimizationtools::Info info = optimizationtools::Info();
};

Output optimize(
        const Instance& instance,
        OptimizeOptionalParameters parameters = {});

}
}

