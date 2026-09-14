#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/solution.hpp"
#include "boxstacks/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::boxstacks;
namespace fs = boost::filesystem;

TEST(BoxStacks, BinCopies)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(1, 1, 1);
    instance_builder.set_item_type_copies(item_type_id, 10);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 10);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 2);
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

TEST(BoxStacks, SolutionBuilderTwoBinsBuiltAtOnce)
{
    // 'SolutionBuilder::build' computes the indicators of a multi-bin
    // solution one bin at a time, and each pass recomputes the aggregate
    // feasibility over every bin, including the ones not sized yet; this
    // read past the end of their empty per-group weight vectors (an access
    // violation on Windows, caught by _GLIBCXX_ASSERTIONS elsewhere). Every
    // multi-bin solution rebuilt from another instance goes through here:
    // Reduction::unreduce_solution, InstanceFlipper::unflip_solution.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(4, 4, 4);
    instance_builder.set_item_type_copies(item_type_id, 2);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(5, 5, 5);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    for (packingsolver::BinPos bin_pos = 0; bin_pos < 2; ++bin_pos) {
        solution_builder.add_bin(bin_type_id, 1);
        solution_builder.add_stack(bin_pos, 0, 4, 0, 4);
        solution_builder.add_item(bin_pos, 0, item_type_id, packingsolver::boxstacks::Rotation::XYZ);
    }
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.number_of_items(), 2);
    EXPECT_TRUE(solution.feasible());
    EXPECT_TRUE(solution.full());
}
