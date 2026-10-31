#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "onedimensional/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

struct OneDimensionalSequentialValueCorrectionTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const OneDimensionalSequentialValueCorrectionTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class OneDimensionalSequentialValueCorrectionTest: public testing::TestWithParam<OneDimensionalSequentialValueCorrectionTestParams> { };

TEST_P(OneDimensionalSequentialValueCorrectionTest, OneDimensionalSequentialValueCorrection)
{
    OneDimensionalSequentialValueCorrectionTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_sequential_value_correction = true;
    optimize_parameters.use_tree_search = false;
    optimize_parameters.use_sequential_single_knapsack = false;
    optimize_parameters.use_dichotomic_search = false;
    optimize_parameters.use_column_generation = false;
    optimize_parameters.use_milp_assignment = false;
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
        OneDimensional,
        OneDimensionalSequentialValueCorrectionTest,
        testing::ValuesIn(std::vector<OneDimensionalSequentialValueCorrectionTestParams>{
            {
                fs::path("data") / "onedimensional" / "users" / "2024-04-07_items.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-07_bins.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-07_parameters.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-07_solution.csv",
            }, {
                fs::path("data") / "onedimensional" / "users" / "2024-04-09_items.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-09_bins.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-09_parameters.csv",
                fs::path("data") / "onedimensional" / "users" / "2024-04-09_solution.csv",
            }}));
