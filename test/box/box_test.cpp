#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/solution.hpp"
#include "box/solution_builder.hpp"

#include <gtest/gtest.h>

using namespace packingsolver::box;

//#include "packingsolver/box/instance_builder.hpp"
//#include "packingsolver/box/optimize.hpp"

//#include <gtest/gtest.h>
//#include <boost/filesystem.hpp>

//using namespace packingsolver::box;
//namespace fs = boost::filesystem;

//struct BoxOptimizeTestParams
//{
//    fs::path items_path;
//    fs::path bins_path;
//    fs::path defects_path;
//    fs::path parameters_path;
//    fs::path certificate_path;
//};

//inline std::ostream& operator<<(std::ostream& os, const BoxOptimizeTestParams& test_params)
//{
//    os << test_params.items_path;
//    return os;
//}

//class BoxOptimizeTest: public testing::TestWithParam<BoxOptimizeTestParams> { };

//TEST_P(BoxOptimizeTest, BoxOptimize)
//{
//    BoxOptimizeTestParams test_params = GetParam();
//    InstanceBuilder instance_builder;
//    instance_builder.read_item_types(test_params.items_path.string());
//    instance_builder.read_bin_types(test_params.bins_path.string());
//    instance_builder.read_parameters(test_params.parameters_path.string());
//    Instance instance = instance_builder.build();

//    OptimizeParameters optimize_parameters;
//    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
//    Output output = optimize(instance, optimize_parameters);

//    Solution solution(instance, test_params.certificate_path.string());
//    std::cout << std::endl
//        << "Reference solution" << std::endl
//        << "------------------" << std::endl;
//    solution.format(std::cout);

//    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
//    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
//}

//INSTANTIATE_TEST_SUITE_P(
//        Box,
//        BoxOptimizeTest,
//        testing::ValuesIn(std::vector<BoxOptimizeTestParams>{
//            {
//                fs::path("data") / "box" / "users" / "2024-11-24" / "items.csv",
//                fs::path("data") / "box" / "users" / "2024-11-24" / "bins.csv",
//                fs::path(""),
//                fs::path("data") / "box" / "users" / "2024-11-24" / "parameters.csv",
//                fs::path("data") / "box" / "users" / "2024-11-24" / "solution.csv",
//            }}));

TEST(Box, VariableSizedBinPackingSolutionComparison)
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
    small_bins_solution_builder.add_item(bin_pos_0, item_type_id, {0, 0, 0}, Rotation::XYZ);
    packingsolver::BinPos bin_pos_1 = small_bins_solution_builder.add_bin(small_bin_type_id, 1);
    small_bins_solution_builder.add_item(bin_pos_1, item_type_id, {0, 0, 0}, Rotation::XYZ);
    Solution small_bins_solution = small_bins_solution_builder.build();

    SolutionBuilder large_bin_solution_builder(instance);
    packingsolver::BinPos bin_pos_2 = large_bin_solution_builder.add_bin(large_bin_type_id, 1);
    large_bin_solution_builder.add_item(bin_pos_2, item_type_id, {0, 0, 0}, Rotation::XYZ);
    large_bin_solution_builder.add_item(bin_pos_2, item_type_id, {6, 0, 0}, Rotation::XYZ);
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
