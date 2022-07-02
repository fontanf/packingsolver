#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

#include <sstream>

namespace packingsolver
{
namespace rectangleguillotine
{

struct OptimizeOptionalParameters
{
    optimizationtools::Info info = optimizationtools::Info();
};

struct Output
{
    Output(const Instance& instance):
        solution_pool(instance, 1) { }

    SolutionPool<Instance, Solution> solution_pool;
};

Output optimize(
        const Instance& instance,
        OptimizeOptionalParameters parameters = {});

}
}

