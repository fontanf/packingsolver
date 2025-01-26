#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineColumnGeneration2TestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleGuillotineColumnGeneration2TestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineColumnGeneration2Test: public testing::TestWithParam<RectangleGuillotineColumnGeneration2TestParams> { };

TEST_P(RectangleGuillotineColumnGeneration2Test, RectangleGuillotineColumnGeneration2)
{
    RectangleGuillotineColumnGeneration2TestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_column_generation_2 = true;
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
        RectangleGuillotineColumnGeneration2Test,
        testing::ValuesIn(std::vector<RectangleGuillotineColumnGeneration2TestParams>{
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_vertical" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_horizontal" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_trims" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_trims" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rr" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rr" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hr" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hr" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2ro" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2ro" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2ro" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2ro" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3ho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3ho" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3ho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3ho" / "solution.csv",
            }}));
