#pragma once

#include "packingsolver/irregular/solution.hpp"

#include "packingsolver/irregular/irregular_to_rectangle.hpp"

namespace packingsolver
{
namespace irregular
{

struct Output: packingsolver::Output<Instance, Solution>
{
    Output(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }
};

using NewSolutionCallback = std::function<void(const Output&)>;

struct OptimizeParameters: packingsolver::Parameters<Instance, Solution>
{
    /** Parameters of the algorithm 'IrregularToRectangle'. */
    IrregularToRectangleParameters irregular_to_rectangle_parameters;


    /** Path of the .nl output file. */
    std::string output_nl_path;
};

const Output optimize(
        const Instance& instance,
        const OptimizeParameters& parameters = {});

}
}
