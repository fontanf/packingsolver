#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::boxstacks;
namespace fs = boost::filesystem;

TEST(BoxStacks, BinCopies)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    instance_builder.add_item_type(1, 1, 1, -1, 10);
    instance_builder.add_bin_type(10, 10, 10, -1, 10);
    const Instance instance = instance_builder.build();
    Solution solution(instance);
    solution.add_bin(0, 2);
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}
