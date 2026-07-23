#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "onedimensional/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

struct OneDimensionalMilpAssignmentTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const OneDimensionalMilpAssignmentTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class OneDimensionalMilpAssignmentTest: public testing::TestWithParam<OneDimensionalMilpAssignmentTestParams> { };

TEST_P(OneDimensionalMilpAssignmentTest, OneDimensionalMilpAssignment)
{
    OneDimensionalMilpAssignmentTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.use_milp_assignment = true;
    optimize_parameters.use_tree_search = false;
    optimize_parameters.use_sequential_single_knapsack = false;
    optimize_parameters.use_sequential_value_correction = false;
    optimize_parameters.use_dichotomic_search = false;
    optimize_parameters.use_column_generation = false;
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
        OneDimensionalMilpAssignmentTest,
        testing::ValuesIn(std::vector<OneDimensionalMilpAssignmentTestParams>{
            {
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bin_type" / "solution.csv",
            }, {
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_multiple_bins" / "solution.csv",
            }}));
