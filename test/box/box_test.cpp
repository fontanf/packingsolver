#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"
#include "box/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::box;
namespace fs = boost::filesystem;

struct BoxOptimizeTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const BoxOptimizeTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class BoxOptimizeTest: public testing::TestWithParam<BoxOptimizeTestParams> { };

TEST_P(BoxOptimizeTest, BoxOptimize)
{
    BoxOptimizeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
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

    // The bound must not exceed the value of the reference solution, which
    // is feasible.
    switch (instance.objective()) {
    case packingsolver::Objective::BinPacking:
        EXPECT_LE(output.bin_packing_bound, solution.number_of_bins());
        break;
    case packingsolver::Objective::VariableSizedBinPacking:
        EXPECT_FALSE(packingsolver::strictly_lesser_cost(
                    solution.cost(),
                    output.variable_sized_bin_packing_bound));
        break;
    case packingsolver::Objective::Knapsack:
        EXPECT_FALSE(packingsolver::strictly_greater_profit(
                    solution.profit(),
                    output.knapsack_bound));
        break;
    default:
        break;
    }
}

INSTANTIATE_TEST_SUITE_P(
        Box,
        BoxOptimizeTest,
        testing::ValuesIn(std::vector<BoxOptimizeTestParams>{
            {
                // The lower bound of 'VariableSizedBinPacking' (the
                // one-dimensional trivial bound, which every problem type
                // uses through its one-dimensional relaxation) used to
                // require the optional bins alone to cover the items,
                // ignoring that the mandatory bins of a bin type with a
                // positive 'copies_min' hold items too. No two items fit in
                // the same bin: the optimum uses three bins of type 1
                // (cost 3), but the bound was 4.
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "items.csv",
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "bins.csv",
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "solution.csv",
            }, {
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_two_bin_types" / "bins.csv",
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_two_bin_types" / "parameters.csv",
                fs::path("data") / "box" / "tests" / "variable_sized_bin_packing_two_bin_types" / "solution.csv",
            }}));
