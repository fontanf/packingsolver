#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::boxstacks;
namespace fs = boost::filesystem;

struct BoxStacksOptimizeTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;

    /** Expected cost of the returned solution. */
    packingsolver::Profit cost;
};

inline std::ostream& operator<<(std::ostream& os, const BoxStacksOptimizeTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxStacksOptimizeTest: public testing::TestWithParam<BoxStacksOptimizeTestParams> { };

TEST_P(BoxStacksOptimizeTest, BoxStacksOptimize)
{
    BoxStacksOptimizeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);

    // 'boxstacks' has no 'SolutionBuilder::read', so the returned solution is
    // checked against the expected cost instead of against a reference
    // certificate.
    EXPECT_TRUE(output.solution_pool.best().feasible());
    EXPECT_TRUE(output.solution_pool.best().full());
    EXPECT_TRUE(packingsolver::equal_cost(output.solution_pool.best().cost(), test_params.cost));
}

INSTANTIATE_TEST_SUITE_P(
        BoxStacks,
        BoxStacksOptimizeTest,
        testing::ValuesIn(std::vector<BoxStacksOptimizeTestParams>{
            {
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "bins.csv",
                fs::path("data") / "boxstacks" / "tests" / "variable_sized_bin_packing_two_bin_types" / "parameters.csv",
                10,
            }}));
