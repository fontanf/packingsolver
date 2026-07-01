#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineColumnGenerationStripsTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleGuillotineColumnGenerationStripsTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineColumnGenerationStripsTest: public testing::TestWithParam<RectangleGuillotineColumnGenerationStripsTestParams> { };

TEST_P(RectangleGuillotineColumnGenerationStripsTest, RectangleGuillotineColumnGenerationStrips)
{
    RectangleGuillotineColumnGenerationStripsTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    //optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_column_generation_strips = true;
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
        RectangleGuillotineColumnGenerationStrips,
        RectangleGuillotineColumnGenerationStripsTest,
        testing::ValuesIn(std::vector<RectangleGuillotineColumnGenerationStripsTestParams>{
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nho" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2nvo_trims" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr_cut_thickness" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_1rvr_cut_thickness" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr_cut_thickness" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2hvr_cut_thickness" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_2rvo_cut_thickness" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo_cut_thickness" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo_cut_thickness" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo_cut_thickness" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_3hvo_cut_thickness" / "solution.csv",
            }}));
