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

TEST(BoxStacks, VariableSizedBinPackingSolutionComparison)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(6, 5, 5);
    instance_builder.set_item_type_copies(item_type_id, 2);
    packingsolver::BinTypeId small_bin_type_id = instance_builder.add_bin_type(6, 5, 5);
    instance_builder.set_bin_type_cost(small_bin_type_id, 7);
    instance_builder.set_bin_type_copies(small_bin_type_id, 2);
    packingsolver::BinTypeId large_bin_type_id = instance_builder.add_bin_type(12, 5, 5);
    instance_builder.set_bin_type_cost(large_bin_type_id, 10);
    instance_builder.set_bin_type_copies(large_bin_type_id, 1);
    const Instance instance = instance_builder.build();

    SolutionBuilder small_bins_solution_builder(instance);
    packingsolver::BinPos bin_pos_0 = small_bins_solution_builder.add_bin(small_bin_type_id, 1);
    packingsolver::StackId stack_id_0 = small_bins_solution_builder.add_stack(bin_pos_0, 0, 6, 0, 5);
    small_bins_solution_builder.add_item(bin_pos_0, stack_id_0, item_type_id, Rotation::XYZ);
    packingsolver::BinPos bin_pos_1 = small_bins_solution_builder.add_bin(small_bin_type_id, 1);
    packingsolver::StackId stack_id_1 = small_bins_solution_builder.add_stack(bin_pos_1, 0, 6, 0, 5);
    small_bins_solution_builder.add_item(bin_pos_1, stack_id_1, item_type_id, Rotation::XYZ);
    Solution small_bins_solution = small_bins_solution_builder.build();

    SolutionBuilder large_bin_solution_builder(instance);
    packingsolver::BinPos bin_pos_2 = large_bin_solution_builder.add_bin(large_bin_type_id, 1);
    packingsolver::StackId stack_id_2 = large_bin_solution_builder.add_stack(bin_pos_2, 0, 6, 0, 5);
    large_bin_solution_builder.add_item(bin_pos_2, stack_id_2, item_type_id, Rotation::XYZ);
    packingsolver::StackId stack_id_3 = large_bin_solution_builder.add_stack(bin_pos_2, 6, 12, 0, 5);
    large_bin_solution_builder.add_item(bin_pos_2, stack_id_3, item_type_id, Rotation::XYZ);
    Solution large_bin_solution = large_bin_solution_builder.build();

    EXPECT_TRUE(small_bins_solution.feasible());
    EXPECT_TRUE(large_bin_solution.feasible());
    EXPECT_TRUE(packingsolver::equal_cost(small_bins_solution.cost(), 14.0));
    EXPECT_TRUE(packingsolver::equal_cost(large_bin_solution.cost(), 10.0));
    // 'a < b' is true when 'b' is the better solution, so the cost 10
    // solution has to compare greater than the cost 14 one.
    EXPECT_TRUE(small_bins_solution < large_bin_solution);
    EXPECT_FALSE(large_bin_solution < small_bins_solution);
}
