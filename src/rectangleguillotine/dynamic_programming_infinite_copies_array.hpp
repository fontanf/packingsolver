#pragma once

#include "packingsolver/rectangleguillotine/solution.hpp"

namespace packingsolver
{
namespace rectangleguillotine
{

struct DynamicProgrammingInfiniteCopiesArrayOutput: packingsolver::Output<Instance, Solution>
{
    DynamicProgrammingInfiniteCopiesArrayOutput(const Instance& instance):
        packingsolver::Output<Instance, Solution>(instance) { }

    /**
     * DP values indexed by width + (eff_width + 1) * height, where
     * eff_width  = bin_width  - left_trim - right_trim and
     * eff_height = bin_height - bottom_trim - top_trim.
     * dp_values[w + (eff_width + 1) * h] is the maximum profit achievable by
     * a guillotine packing (unbounded copies) into a w x h rectangle within
     * the trimmed area.
     */
    std::vector<Profit> dp_values;
};

struct DynamicProgrammingInfiniteCopiesArrayParameters: packingsolver::Parameters<Instance, Solution>
{
};

const DynamicProgrammingInfiniteCopiesArrayOutput dynamic_programming_infinite_copies_array(
        const Instance& instance,
        const DynamicProgrammingInfiniteCopiesArrayParameters& parameters = {});

}
}
