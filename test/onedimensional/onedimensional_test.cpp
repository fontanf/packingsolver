#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "onedimensional/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

namespace
{

/**
 * Test-only helper: the consumption of the 'copy'-th (0-indexed) unit of
 * 'item_type_id' in 'resource' - a linear scan over
 * 'Resource::item_consumptions', fine here since this is not a hot path
 * (see that field's own doc comment for why production code instead looks
 * this up in O(1) via 'ItemType::resources').
 */
double item_consumption_for_test(
        const packingsolver::Resource& resource,
        packingsolver::ItemTypeId item_type_id,
        packingsolver::ItemPos copy)
{
    for (const auto& entry: resource.item_consumptions) {
        if (entry.first == item_type_id)
            return packingsolver::schedule_consumption(entry.second, copy);
    }
    return 0.0;
}

}

TEST(OneDimensional, BinCopies)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(1);
    instance_builder.set_item_type_copies(item_type_id, 10);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10);
    instance_builder.set_bin_type_copies(bin_type_id, 10);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 2);
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

TEST(OneDimensional, ResourceConsumptionCopiedFromOriginalInstance)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 10);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(10);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, {5.0});
    const Instance instance = instance_builder.build();

    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_original_instance(instance);
    packingsolver::BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(bin_type_id);
    packingsolver::ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type_from_original_instance(item_type_id);
    const Instance sub_instance = sub_instance_builder.build();

    EXPECT_EQ(sub_instance.bin_type(sub_bin_type_id).number_of_resources(), 1);
    EXPECT_EQ(
            item_consumption_for_test(sub_instance.bin_type(sub_bin_type_id).resource(0), sub_item_type_id, 0),
            5.0);
}

TEST(OneDimensional, WriteCsvRoundTrip)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    instance_builder.set_bin_type_cost(bin_type_id, 7);
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(10);
    instance_builder.set_item_type_profit(item_type_id, 42);
    instance_builder.set_item_type_copies(item_type_id, 5);
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    instance.write(path.string(), packingsolver::InstanceFormat::Csv);

    InstanceBuilder read_instance_builder;
    read_instance_builder.read_item_types(path.string() + "_items.csv");
    read_instance_builder.read_bin_types(path.string() + "_bins.csv");
    read_instance_builder.read_parameters(path.string() + "_parameters.csv");
    const Instance read_instance = read_instance_builder.build();

    fs::remove(path.string() + "_items.csv");
    fs::remove(path.string() + "_bins.csv");
    fs::remove(path.string() + "_parameters.csv");

    EXPECT_EQ(read_instance.objective(), packingsolver::Objective::BinPacking);
    ASSERT_EQ(read_instance.number_of_bin_types(), 1);
    EXPECT_EQ(read_instance.bin_type(0).length, 100);
    EXPECT_EQ(read_instance.bin_type(0).cost, 7);
    EXPECT_EQ(read_instance.bin_type(0).copies, 3);
    ASSERT_EQ(read_instance.number_of_item_types(), 1);
    EXPECT_EQ(read_instance.item_type(0).length, 10);
    EXPECT_EQ(read_instance.item_type(0).profit, 42);
    EXPECT_EQ(read_instance.item_type(0).copies, 5);
}

TEST(OneDimensional, WriteJsonRoundTripWithResource)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 10);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(10);
    instance_builder.set_item_type_copies(item_type_id, 3);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, {5.0});
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    std::string json_path = path.string() + ".json";
    instance.write(json_path, packingsolver::InstanceFormat::Json);

    InstanceBuilder read_instance_builder;
    read_instance_builder.read(json_path);
    const Instance read_instance = read_instance_builder.build();

    fs::remove(json_path);

    EXPECT_EQ(read_instance.objective(), packingsolver::Objective::BinPacking);
    ASSERT_EQ(read_instance.number_of_bin_types(), 1);
    EXPECT_EQ(read_instance.bin_type(0).number_of_resources(), 1);
    EXPECT_EQ(read_instance.bin_type(0).resource(0).capacity, 10.0);
    EXPECT_EQ(item_consumption_for_test(read_instance.bin_type(0).resource(0), 0, 0), 5.0);
}

TEST(OneDimensional, WriteCsvThrowsOnResource)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    instance_builder.add_bin_type_resource(bin_type_id, 10);
    instance_builder.add_item_type(10);
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    EXPECT_THROW(
            instance.write(path.string(), packingsolver::InstanceFormat::Csv),
            std::invalid_argument);
}

TEST(OneDimensional, WriteCsvThrowsOnEligibility)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    instance_builder.add_bin_type_eligibility(bin_type_id, 0);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(10);
    instance_builder.set_item_type_eligibility(item_type_id, 0);
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    EXPECT_THROW(
            instance.write(path.string(), packingsolver::InstanceFormat::Csv),
            std::invalid_argument);
}

TEST(OneDimensional, WriteCsvThrowsOnPrecedence)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.add_bin_type(100);
    packingsolver::ItemTypeId item_type_id_1 = instance_builder.add_item_type(10);
    packingsolver::ItemTypeId item_type_id_2 = instance_builder.add_item_type(20);
    instance_builder.add_item_type_precedence(item_type_id_1, item_type_id_2);
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    EXPECT_THROW(
            instance.write(path.string(), packingsolver::InstanceFormat::Csv),
            std::invalid_argument);
}

struct OneDimensionalOptimizeTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;

    /**
     * Maximum size of the dynamic programming of the subset sum problems; if
     * negative, the default one.
     */
    packingsolver::Counter dynamic_programming_subset_sum_maximum_size = -1;
};

inline std::ostream& operator<<(std::ostream& os, const OneDimensionalOptimizeTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class OneDimensionalOptimizeTest: public testing::TestWithParam<OneDimensionalOptimizeTestParams> { };

TEST_P(OneDimensionalOptimizeTest, OneDimensionalOptimize)
{
    OneDimensionalOptimizeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    if (test_params.dynamic_programming_subset_sum_maximum_size >= 0) {
        optimize_parameters.dynamic_programming_subset_sum_maximum_size
            = test_params.dynamic_programming_subset_sum_maximum_size;
    }
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
        OneDimensional,
        OneDimensionalOptimizeTest,
        testing::ValuesIn(std::vector<OneDimensionalOptimizeTestParams>{
            {
                // The lower bound of 'VariableSizedBinPacking' (the
                // one-dimensional trivial bound, which every problem type
                // uses through its one-dimensional relaxation) used to
                // require the optional bins alone to cover the items,
                // ignoring that the mandatory bins of a bin type with a
                // positive 'copies_min' hold items too. No two items fit in
                // the same bin: the optimum uses three bins of type 1
                // (cost 3), but the bound was 4.
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "solution.csv",
            }, {
                // Single bin and the profit of each item equal to its
                // length: the knapsack problem is a subset sum problem,
                // solved with the word RAM dynamic programming.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "solution.csv",
            }, {
                // Same instance, the word RAM dynamic programming being too
                // large: it is solved with the primal-dual dynamic
                // programming.
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "items.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "bins.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "parameters.csv",
                fs::path("data") / "onedimensional" / "tests" / "knapsack_subset_sum" / "solution.csv",
                8,
            }}));

TEST(OneDimensional, ItemTypeLengthMustBePositive)
{
    InstanceBuilder instance_builder;
    EXPECT_THROW(instance_builder.add_item_type(0), std::invalid_argument);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(10);
    EXPECT_THROW(instance_builder.set_item_type_length(item_type_id, 0), std::invalid_argument);
}

TEST(OneDimensional, UnlimitedItemCopies)
{
    // Item copies -1: as many copies as the total length of the bins allows,
    // ceil((2 * 100 + 50) / 30) = 9.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.add_bin_type(50);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(30);
    instance_builder.set_item_type_copies(item_type_id, -1);
    const Instance instance = instance_builder.build();
    EXPECT_EQ(instance.item_type(item_type_id).copies, 9);
    EXPECT_EQ(instance.item_type(item_type_id).copies_min, 0);

    // 3 items in each bin of length 100, 1 in the bin of length 50.
    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.verbosity_level = 0;
    Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_items(), 7);
}

TEST(OneDimensional, UnlimitedItemCopiesInvalid)
{
    // Only with the knapsack objective.
    {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(packingsolver::Objective::BinPacking);
        instance_builder.add_bin_type(100);
        packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(30);
        instance_builder.set_item_type_copies(item_type_id, -1);
        EXPECT_THROW(instance_builder.build(), std::invalid_argument);
    }
    // Not with bins with unlimited copies.
    {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(packingsolver::Objective::Knapsack);
        packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
        instance_builder.set_bin_type_copies(bin_type_id, -1);
        packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(30);
        instance_builder.set_item_type_copies(item_type_id, -1);
        EXPECT_THROW(instance_builder.build(), std::invalid_argument);
    }
}

TEST(OneDimensional, ItemNestedLength)
{
    // 3 items of length 70 with a nesting length of 10, in a bin of length
    // 500: they end at 70 + 60 + 60 = 190, which is the length they occupy.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    instance_builder.add_bin_type(500);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(70);
    instance_builder.set_item_type_nesting_length(item_type_id, 10);
    instance_builder.set_item_type_copies(item_type_id, 3);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 1);
    for (int i = 0; i < 3; ++i)
        solution_builder.add_item(0, item_type_id);
    const Solution solution = solution_builder.build();
    EXPECT_EQ(solution.item_length(), 210);
    EXPECT_EQ(solution.item_nested_length(), 190);
    EXPECT_EQ(solution.length(), 190);
    // The waste: the length used, or of the bin, minus the nested length of
    // the items.
    EXPECT_EQ(solution.waste(), 0);
    EXPECT_EQ(solution.full_waste(), 310);
}

TEST(Onedimensional, InstanceBuilderCropBinType)
{
    InstanceBuilder instance_builder;
    instance_builder.add_item_type(10);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100);
    instance_builder.crop_bin_type(bin_type_id, 60);
    Instance instance = instance_builder.build();

    EXPECT_EQ(instance.bin_type(bin_type_id).length, 60);
}

TEST(OneDimensional, BinPackingGoal)
{
    // 6 items of length 1 in bins of length 10, at most 2 items per bin (a
    // resource): the optimal number of bins is 3, while the length bound is
    // 1. With a goal of 3, the MILP starts its search at 3 bins, and stops
    // with a solution reaching the goal.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10);
    instance_builder.set_bin_type_copies(bin_type_id, 6);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(1);
    instance_builder.set_item_type_copies(item_type_id, 6);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, {1.0});
    instance_builder.set_bin_packing_goal(3);
    const Instance instance = instance_builder.build();
    EXPECT_EQ(instance.bin_packing_goal(), 3);

    OptimizeParameters optimize_parameters;
    optimize_parameters.verbosity_level = 0;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_milp_assignment = true;
    Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 3);
    EXPECT_TRUE(output.is_proven_optimal());
}

TEST(OneDimensional, BinPackingGoalUnreachable)
{
    // Same instance with a goal of 2, which can't be reached: the MILP proves
    // that 2 bins are infeasible, and the search continues to the optimal
    // solution with 3 bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10);
    instance_builder.set_bin_type_copies(bin_type_id, 6);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(1);
    instance_builder.set_item_type_copies(item_type_id, 6);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, {1.0});
    instance_builder.set_bin_packing_goal(2);
    const Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.verbosity_level = 0;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_milp_assignment = true;
    Output output = optimize(instance, optimize_parameters);
    EXPECT_EQ(output.bin_packing_bound, 3);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 3);
    EXPECT_TRUE(output.is_proven_optimal());
}

namespace
{

/**
 * Knapsack instance: a bin of length 10, items of lengths 6, 5 and 5 (profit
 * equal to the length). The optimal profit is 10 (the two items of length
 * 5).
 */
Instance knapsack_goal_instance(packingsolver::Profit knapsack_goal)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.add_bin_type(10);
    instance_builder.add_item_type(6);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(5);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.set_knapsack_goal(knapsack_goal);
    return instance_builder.build();
}

/**
 * Variable-sized bin packing instance: items of lengths 6 and 4, a bin type
 * of length 10 and cost 10, and a bin type of length 6 and cost 7. The
 * optimal cost is 10 (both items in a bin of length 10).
 */
Instance variable_sized_bin_packing_goal_instance(packingsolver::Profit goal)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    packingsolver::BinTypeId bin_type_id_1 = instance_builder.add_bin_type(10);
    instance_builder.set_bin_type_cost(bin_type_id_1, 10);
    instance_builder.set_bin_type_copies(bin_type_id_1, 2);
    packingsolver::BinTypeId bin_type_id_2 = instance_builder.add_bin_type(6);
    instance_builder.set_bin_type_cost(bin_type_id_2, 7);
    instance_builder.set_bin_type_copies(bin_type_id_2, 2);
    instance_builder.add_item_type(6);
    instance_builder.add_item_type(4);
    instance_builder.set_variable_sized_bin_packing_goal(goal);
    return instance_builder.build();
}

OptimizeParameters goal_optimize_parameters()
{
    OptimizeParameters optimize_parameters;
    optimize_parameters.verbosity_level = 0;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_milp_assignment = true;
    return optimize_parameters;
}

}

TEST(OneDimensional, KnapsackGoal)
{
    // The goal is reached: the search stops with a solution reaching it.
    const Instance instance = knapsack_goal_instance(10);
    EXPECT_EQ(instance.knapsack_goal(), 10);
    Output output = optimize(instance, goal_optimize_parameters());
    EXPECT_EQ(output.solution_pool.best().profit(), 10);
    EXPECT_TRUE(output.is_proven_optimal());
}

TEST(OneDimensional, KnapsackGoalUnreachable)
{
    // The goal can't be reached: the search continues to the optimal
    // solution.
    const Instance instance = knapsack_goal_instance(11);
    Output output = optimize(instance, goal_optimize_parameters());
    EXPECT_EQ(output.solution_pool.best().profit(), 10);
    EXPECT_EQ(output.knapsack_bound, 10);
    EXPECT_TRUE(output.is_proven_optimal());
}

TEST(OneDimensional, VariableSizedBinPackingGoal)
{
    // The goal is reached: the search stops with a solution reaching it.
    const Instance instance = variable_sized_bin_packing_goal_instance(10);
    EXPECT_EQ(instance.variable_sized_bin_packing_goal(), 10);
    Output output = optimize(instance, goal_optimize_parameters());
    EXPECT_EQ(output.solution_pool.best().cost(), 10);
    EXPECT_TRUE(output.is_proven_optimal());
}

TEST(OneDimensional, VariableSizedBinPackingGoalUnreachable)
{
    // The goal can't be reached: the search continues to the optimal
    // solution.
    const Instance instance = variable_sized_bin_packing_goal_instance(9);
    Output output = optimize(instance, goal_optimize_parameters());
    EXPECT_EQ(output.solution_pool.best().cost(), 10);
    EXPECT_EQ(output.variable_sized_bin_packing_bound, 10);
    EXPECT_TRUE(output.is_proven_optimal());
}

TEST(OneDimensional, DynamicProgrammingKnapsackBoundWithPenalties)
{
    // Items A and B (length 5, profit 10) together exceed a 'penalize'
    // resource of penalty 15, item C (length 5, profit 9) doesn't consume
    // it. The dynamic programming ignores the resources and selects A and B,
    // whose profit with the penalty is 5, while A and C reach 19: the bound
    // must be the profit of the relaxation, 20, not 5.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10);
    packingsolver::ResourceId resource_id = instance_builder.add_bin_type_resource(
            bin_type_id, 1, true, 15);
    packingsolver::ItemTypeId item_type_id_a = instance_builder.add_item_type(5);
    instance_builder.set_item_type_profit(item_type_id_a, 10);
    packingsolver::ItemTypeId item_type_id_b = instance_builder.add_item_type(5);
    instance_builder.set_item_type_profit(item_type_id_b, 10);
    packingsolver::ItemTypeId item_type_id_c = instance_builder.add_item_type(5);
    instance_builder.set_item_type_profit(item_type_id_c, 9);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_a, {1.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_b, {1.0});
    const Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.verbosity_level = 0;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);
    EXPECT_DOUBLE_EQ(output.solution_pool.best().profit(), 19);
    EXPECT_GE(output.knapsack_bound, 19);
}
