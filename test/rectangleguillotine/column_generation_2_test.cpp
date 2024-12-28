#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct TestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const TestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class AlgorithmTest: public testing::TestWithParam<TestParams> { };

TEST_P(AlgorithmTest, Algorithm)
{
    TestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        RectangleGuillotineColumnGeneration2,
        AlgorithmTest,
        testing::ValuesIn(std::vector<TestParams>{
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical_items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical_bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical_parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical_solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal_items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal_bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal_parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal_solution.csv",
            }}));
