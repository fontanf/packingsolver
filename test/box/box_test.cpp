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

TEST(Box, UnlimitedItemCopies)
{
    // Item copies -1: as many copies as the total volume of the bins allows,
    // ceil(3 * 1000 / 64) = 47.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(4, 4, 4);
    instance_builder.set_item_type_copies(item_type_id, -1);
    const Instance instance = instance_builder.build();
    EXPECT_EQ(instance.item_type(item_type_id).copies, 47);
}

TEST(Box, LeftoverModes)
{
    // A 100 x 50 x 40 bin whose items end at x 60, y 20, z 10: the volume
    // used in each leftover mode is the box from the origin to the items
    // along the dimensions of the mode, and the whole bin along the others.
    struct Case { LeftoverMode leftover_mode; packingsolver::Volume used; };
    for (Case c: std::vector<Case>{
            {LeftoverMode::X, 60 * 50 * 40},
            {LeftoverMode::Y, 100 * 20 * 40},
            {LeftoverMode::Z, 100 * 50 * 10},
            {LeftoverMode::XY, 60 * 20 * 40},
            {LeftoverMode::XZ, 60 * 50 * 10},
            {LeftoverMode::YZ, 100 * 20 * 10},
            {LeftoverMode::XYZ, 60 * 20 * 10}}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
        instance_builder.set_leftover_mode(c.leftover_mode);
        instance_builder.add_bin_type(100, 50, 40);
        packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(30, 20, 10);
        instance_builder.set_item_type_copies(item_type_id, 2);
        const Instance instance = instance_builder.build();
        SolutionBuilder solution_builder(instance);
        solution_builder.add_bin(0, 1);
        solution_builder.add_item(0, item_type_id, {0, 0, 0}, Rotation::XYZ);
        solution_builder.add_item(0, item_type_id, {30, 0, 0}, Rotation::XYZ);
        const Solution solution = solution_builder.build();
        EXPECT_TRUE(solution.feasible());
        EXPECT_EQ(solution.leftover_value(), 100 * 50 * 40 - c.used) << c.leftover_mode;
        EXPECT_EQ(solution.waste(), c.used - 2 * 30 * 20 * 10) << c.leftover_mode;
    }
}

TEST(Box, LeftoverModeJson)
{
    // The leftover mode is read from the JSON format, and written to it.
    std::stringstream ss(R"({"objective": "bin-packing-with-leftovers", "leftover_mode": "XZ",
        "bin_types": [{"x": 10, "y": 10, "z": 10}], "item_types": [{"x": 5, "y": 5, "z": 5}]})");
    InstanceBuilder instance_builder;
    instance_builder.read(ss);
    const Instance instance = instance_builder.build();
    EXPECT_EQ(instance.parameters().leftover_mode, LeftoverMode::XZ);
    // The default.
    InstanceBuilder default_builder;
    default_builder.add_bin_type(10, 10, 10);
    default_builder.add_item_type(5, 5, 5);
    EXPECT_EQ(default_builder.build().parameters().leftover_mode, LeftoverMode::XYZ);
}
