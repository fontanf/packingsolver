#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::box;
namespace fs = boost::filesystem;

struct BoxTreeSearchTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const BoxTreeSearchTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxTreeSearchTest: public testing::TestWithParam<BoxTreeSearchTestParams> { };

TEST_P(BoxTreeSearchTest, BoxTreeSearch)
{
    BoxTreeSearchTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    //optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, test_params.certificate_path.string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        BoxTreeSearch,
        BoxTreeSearchTest,
        testing::ValuesIn(std::vector<BoxTreeSearchTestParams>{
            {
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "items.csv",
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "bins.csv",
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "knapsack_1_item" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "items.csv",
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "bins.csv",
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "knapsack_4_items" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "items.csv",
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "bins.csv",
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "knapsack_20_items" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xy" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_xz" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "items.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "bins.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "open_dimension_x_4_different_items_yz" / "solution.csv",
            }}));
