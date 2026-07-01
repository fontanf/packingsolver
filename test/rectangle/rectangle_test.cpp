#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>

using namespace packingsolver::rectangle;

TEST(Rectangle, BinCopies)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    instance_builder.add_item_type(1, 1, -1, 10);
    instance_builder.add_bin_type(10, 10, -1, 10);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 2);
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}
