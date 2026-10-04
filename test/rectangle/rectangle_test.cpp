#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

TEST(Rectangle, BinCopies)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(1, 1);
    instance_builder.set_item_type_copies(item_type_id, 10);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 10);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 2);
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

struct RectangleOptimizeTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleOptimizeTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleOptimizeTest: public testing::TestWithParam<RectangleOptimizeTestParams> { };

TEST_P(RectangleOptimizeTest, RectangleOptimize)
{
    RectangleOptimizeTestParams test_params = GetParam();
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
        Rectangle,
        RectangleOptimizeTest,
        testing::ValuesIn(std::vector<RectangleOptimizeTestParams>{
            {
                // The lower bound of 'VariableSizedBinPacking' (the
                // one-dimensional trivial bound, which every problem type
                // uses through its one-dimensional relaxation) used to
                // require the optional bins alone to cover the items,
                // ignoring that the mandatory bins of a bin type with a
                // positive 'copies_min' hold items too. No two items fit in
                // the same bin: the optimum uses three bins of type 1
                // (cost 3), but the bound was 4.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "solution.csv",
            }}));

TEST(Rectangle, ReadUnknownEnumValues)
{
    // Unrecognized values are rejected instead of being ignored.
    for (const std::string& parameter: {
            R"("unloading_constraint": "only-z-movements")",
            R"("leftover_mode": "volume")"}) {
        std::stringstream ss(
                "{\"objective\": \"bin-packing\", " + parameter + ", "
                "\"bin_types\": [{\"x\": 100, \"y\": 50}], "
                "\"item_types\": [{\"x\": 10, \"y\": 10}]}");
        InstanceBuilder instance_builder;
        EXPECT_THROW(instance_builder.read(ss), std::invalid_argument);
    }
    std::stringstream ss(R"({
            "objective": "bin-packing",
            "unloading_constraint": "increasing-x",
            "bin_types": [{"x": 100, "y": 50}],
            "item_types": [{"x": 10, "y": 10}]})");
    InstanceBuilder instance_builder;
    instance_builder.read(ss);
    Instance instance = instance_builder.build();
    EXPECT_EQ(instance.unloading_constraint(), UnloadingConstraint::IncreasingX);
}

TEST(Rectangle, ItemTypeDimensionsMustBePositive)
{
    InstanceBuilder instance_builder;
    EXPECT_THROW(instance_builder.add_item_type(0, 10), std::invalid_argument);
    EXPECT_THROW(instance_builder.add_item_type(10, 0), std::invalid_argument);
    EXPECT_NO_THROW(instance_builder.add_item_type(10, 10));
}
