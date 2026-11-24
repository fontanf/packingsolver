#include "packingsolver/rectangle/algorithm_formatter.hpp"
#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "packingsolver/rectangle/post_process.hpp"
#include "rectangle/solution_builder.hpp"

#include "algorithms/leftover.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

TEST(Rectangle, ReoptimizeNonLastBinsLeftovers)
{
    // Two 10 x 10 bins and three 5 x 5 items. In the first bin of the
    // solution, the two items are in opposite corners: the free space is
    // split into two regions, and the leftover value of the bin is 0.
    // Re-optimized, they are next to each other.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(5, 5);
    instance_builder.set_item_type_copies(item_type_id, 3);
    const Instance instance = instance_builder.build();

    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(bin_type_id, 1);
    solution_builder.add_item(0, item_type_id, {0, 0}, false);
    solution_builder.add_item(0, item_type_id, {5, 5}, false);
    solution_builder.add_bin(bin_type_id, 1);
    solution_builder.add_item(1, item_type_id, {0, 0}, false);
    const Solution solution = solution_builder.build();
    ASSERT_TRUE(solution.feasible());
    EXPECT_EQ(solution.bin(0).leftover_value, 0);
    EXPECT_EQ(solution.bin(0).staircase_leftover_value, 0);

    OptimizeParameters parameters;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    const Solution new_solution = reoptimize_non_last_bins_leftovers(solution, parameters);
    ASSERT_TRUE(new_solution.feasible());
    EXPECT_EQ(new_solution.number_of_bins(), 2);
    EXPECT_EQ(new_solution.bin(0).leftover_value, 50 * 50);
    EXPECT_EQ(new_solution.bin(0).staircase_leftover_value, 50);
    EXPECT_EQ(new_solution.leftover_value(), solution.leftover_value());
    EXPECT_TRUE(solution < new_solution);
}

namespace
{

/**
 * Two 10 x 10 bins and three 5 x 5 items, for the 'BinPacking' objective: in
 * each bin of the solution, the items are in opposite corners, so the free
 * space is split into two regions.
 */
Solution scattered_solution(const Instance& instance)
{
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 1);
    solution_builder.add_item(0, 0, {0, 0}, false);
    solution_builder.add_item(0, 0, {5, 5}, false);
    solution_builder.add_bin(0, 1);
    solution_builder.add_item(1, 0, {5, 5}, false);
    return solution_builder.build();
}

Instance scattered_instance()
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(5, 5);
    instance_builder.set_item_type_copies(item_type_id, 3);
    return instance_builder.build();
}

}

TEST(Rectangle, ReoptimizeAllBinsLeftovers)
{
    // Re-optimized, the items of the first bin are next to each other, and
    // the item of the last bin is in the corner. The number of bins, i.e. the
    // value of the solution, is the same, but the leftovers are better.
    const Instance instance = scattered_instance();
    const Solution solution = scattered_solution(instance);
    ASSERT_TRUE(solution.feasible());
    EXPECT_EQ(solution.bin(0).leftover_value, 0);
    EXPECT_EQ(solution.bin(1).leftover_value, 0);

    OptimizeParameters parameters;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    const Solution new_solution = reoptimize_all_bins_leftovers(solution, parameters);
    ASSERT_TRUE(new_solution.feasible());
    EXPECT_EQ(new_solution.number_of_bins(), 2);
    EXPECT_EQ(new_solution.bin(0).leftover_value, 50 * 50);
    EXPECT_EQ(new_solution.bin(1).leftover_value, 75 * 75);
    EXPECT_FALSE(solution < new_solution);
    EXPECT_FALSE(new_solution < solution);
    EXPECT_TRUE(new_solution.leftovers_better(solution));
    EXPECT_FALSE(solution.leftovers_better(new_solution));
}

TEST(Rectangle, UpdateSolutionLeftovers)
{
    // A solution with the same value as the best one but better leftovers
    // replaces it; a solution with worse leftovers doesn't.
    const Instance instance = scattered_instance();
    const Solution solution = scattered_solution(instance);
    OptimizeParameters parameters;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    parameters.verbosity_level = 0;
    const Solution new_solution = reoptimize_all_bins_leftovers(solution, parameters);
    ASSERT_TRUE(new_solution.leftovers_better(solution));

    Output output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.update_solution(solution, "scattered");
    EXPECT_EQ(output.solution_pool.best_label(), "scattered");

    // 'update_solution' only keeps strictly better solutions.
    algorithm_formatter.update_solution(new_solution, "compact");
    EXPECT_EQ(output.solution_pool.best_label(), "scattered");

    algorithm_formatter.update_solution_leftovers(new_solution, "compact");
    EXPECT_EQ(output.solution_pool.best_label(), "compact");
    EXPECT_EQ(output.solution_pool.best().total_leftover_value(), new_solution.total_leftover_value());

    algorithm_formatter.update_solution_leftovers(solution, "scattered");
    EXPECT_EQ(output.solution_pool.best_label(), "compact");
}

TEST(Rectangle, LeftoverPostProcessBinPacking)
{
    // With the 'BinPacking' objective, the leftover post-process re-packs the
    // bins of the best solution to concentrate their free space: two 5 x 5
    // items in a 10 x 10 bin, next to each other, leave a 10 x 5 leftover.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(5, 5);
    instance_builder.set_item_type_copies(item_type_id, 2);
    const Instance instance = instance_builder.build();
    OptimizeParameters parameters;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    parameters.verbosity_level = 0;
    const Output output = optimize(instance, parameters);
    const Solution& solution = output.solution_pool.best();
    ASSERT_TRUE(solution.feasible());
    EXPECT_EQ(solution.number_of_bins(), 1);
    EXPECT_EQ(solution.bin(0).leftover_value, 50.0 * 50);
    EXPECT_EQ(solution.total_leftover_value(), 100.0 * 100 + 50.0 * 50);
}

TEST(Rectangle, LeftoverModeStaircase)
{
    // A 12 x 10 bin with a 6 x 4 item at the origin and a 3 x 4 item above
    // it: the staircase is the union of the rectangles 6 x 4 and 3 x 8, so the
    // leftover area is 120 - 24 - 12 = 84, against 120 - 6 x 8 = 72 for the
    // L-shape of the bounding box of the items.
    for (packingsolver::LeftoverValueType leftover_value_type: {
            packingsolver::LeftoverValueType::SquaredArea,
            packingsolver::LeftoverValueType::Depth}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
        instance_builder.set_leftover_mode(LeftoverMode::Staircase);
        instance_builder.set_leftover_value_type(leftover_value_type);
        packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(12, 10);
        packingsolver::ItemTypeId item_type_id_1 = instance_builder.add_item_type(6, 4);
        packingsolver::ItemTypeId item_type_id_2 = instance_builder.add_item_type(3, 4);
        const Instance instance = instance_builder.build();

        SolutionBuilder solution_builder(instance);
        solution_builder.add_bin(bin_type_id, 1);
        solution_builder.add_item(0, item_type_id_1, {0, 0}, false);
        solution_builder.add_item(0, item_type_id_2, {0, 4}, false);
        const Solution solution = solution_builder.build();
        ASSERT_TRUE(solution.feasible());
        double expected_leftover_value = (leftover_value_type == packingsolver::LeftoverValueType::SquaredArea)?
            84.0 * 84.0:
            packingsolver::staircase_leftover_value<2>(leftover_value_type, {12, 10}, {{6, 4}, {3, 8}});
        EXPECT_NEAR(solution.leftover_value(), expected_leftover_value, 1e-9 * expected_leftover_value)
            << leftover_value_type;
        EXPECT_GT(
                solution.leftover_value(),
                packingsolver::rectangle::leftover_value(leftover_value_type, LeftoverMode::Area, 12, 10, 6, 8));
        // The waste is measured in the staircase.
        EXPECT_EQ(solution.waste(), 0);
    }
}

TEST(Rectangle, VariableSizedBinPackingWithLeftovers)
{
    // A 100 x 100 bin, a 50 x 50 bin and a 50 x 50 item. In the first
    // solution, the item is in the small bin, which is used entirely: the
    // large bin is kept whole. In the second one, the item is in the large
    // bin, which leaves an L-shape of area 7500, and the small bin is kept.
    // The leftover value factor is 1 / 10000: the L-shape is refunded
    // 7500^2 / 10000 = 5625 units of area, at the price of the large bin.
    for (bool costs: {false, true}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPackingWithLeftovers);
        packingsolver::BinTypeId large_bin_type_id = instance_builder.add_bin_type(100, 100);
        packingsolver::BinTypeId small_bin_type_id = instance_builder.add_bin_type(50, 50);
        if (costs) {
            instance_builder.set_bin_type_cost(large_bin_type_id, 1);
            instance_builder.set_bin_type_cost(small_bin_type_id, 10);
        }
        packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type(50, 50);
        const Instance instance = instance_builder.build();
        EXPECT_DOUBLE_EQ(instance.leftover_value_factor(), 1.0 / 10000);

        SolutionBuilder small_solution_builder(instance);
        small_solution_builder.add_bin(small_bin_type_id, 1);
        small_solution_builder.add_item(0, item_type_id, {0, 0}, false);
        const Solution small_solution = small_solution_builder.build();
        SolutionBuilder large_solution_builder(instance);
        large_solution_builder.add_bin(large_bin_type_id, 1);
        large_solution_builder.add_item(0, item_type_id, {0, 0}, false);
        const Solution large_solution = large_solution_builder.build();

        // The stock left: 10000^2 against 7500^2 + 2500^2.
        EXPECT_EQ(small_solution.total_leftover_value(), 10000.0 * 10000.0);
        EXPECT_EQ(large_solution.total_leftover_value(), 7500.0 * 7500.0 + 2500.0 * 2500.0);
        if (costs) {
            // The large bin is much cheaper: 1 - 5625 / 10000 against 10.
            EXPECT_DOUBLE_EQ(small_solution.net_cost(), 10);
            EXPECT_DOUBLE_EQ(large_solution.net_cost(), 1 - 0.5625);
            EXPECT_TRUE(small_solution < large_solution);
            EXPECT_FALSE(large_solution < small_solution);
        } else {
            // 10000 - 5625 against 2500: the large bin is kept whole.
            EXPECT_DOUBLE_EQ(small_solution.net_cost(), 2500);
            EXPECT_DOUBLE_EQ(large_solution.net_cost(), 10000 - 5625);
            EXPECT_TRUE(large_solution < small_solution);
            EXPECT_FALSE(small_solution < large_solution);
        }

        OptimizeParameters parameters;
        parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
        const Output output = optimize(instance, parameters);
        ASSERT_TRUE(output.solution_pool.best().feasible());
        EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
        EXPECT_EQ(output.solution_pool.best().bin(0).bin_type_id,
                (costs)? large_bin_type_id: small_bin_type_id);
    }
}

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
            }, {
                // Large dimensions: the reduction used to need memory
                // proportional to the dimensions of the bin (multiple-choice
                // subset sum dynamic programming in
                // 'max_achievable_dimension_sum'), ~1 GB here.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_large_dimensions" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_large_dimensions" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_large_dimensions" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_large_dimensions" / "solution.csv",
            }, {
                // 'BinPackingWithLeftovers' in a not-anytime mode: the
                // post-process re-optimizes the last bin with guides
                // {2, 3}. It used to rebuild the solution from all the bins
                // but the last two (instead of the last one), so with
                // several bins the rebuilt solution missed the items of the
                // second-to-last bin and was never kept. Here, the
                // re-optimized last bin increases the leftover value from
                // 4083 to 4240.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_last_bin_post_process" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_last_bin_post_process" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_last_bin_post_process" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_last_bin_post_process" / "solution.csv",
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

TEST(Rectangle, InstanceBuilderCropBinType)
{
    // Bin 100x100 with three defects, cropped to 60x70.
    InstanceBuilder instance_builder;
    instance_builder.add_item_type(10, 10);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    // Inside the part kept.
    instance_builder.add_defect(bin_type_id, 20, 20, 10, 10);
    // Across the right and top sides of the part kept.
    instance_builder.add_defect(bin_type_id, 50, 30, 20, 50);
    // Outside the part kept.
    instance_builder.add_defect(bin_type_id, 80, 20, 10, 10);
    instance_builder.crop_bin_type(bin_type_id, 60, 70);
    Instance instance = instance_builder.build();

    const BinType& bin_type = instance.bin_type(bin_type_id);
    EXPECT_EQ(bin_type.rect.x, 60);
    EXPECT_EQ(bin_type.rect.y, 70);
    ASSERT_EQ(bin_type.defects.size(), 2);
    EXPECT_EQ(bin_type.defects[0].pos.x, 20);
    EXPECT_EQ(bin_type.defects[0].rect.x, 10);
    EXPECT_EQ(bin_type.defects[0].rect.y, 10);
    EXPECT_EQ(bin_type.defects[1].pos.x, 50);
    EXPECT_EQ(bin_type.defects[1].pos.y, 30);
    EXPECT_EQ(bin_type.defects[1].rect.x, 10);
    EXPECT_EQ(bin_type.defects[1].rect.y, 40);
}

TEST(Rectangle, WriteJsonRoundTripGoals)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.set_knapsack_goal(1.0445144262560912);
    instance_builder.set_knapsack_cutoff(0.98765432101234567);
    instance_builder.add_bin_type(100, 100);
    instance_builder.add_item_type(10, 10);
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    std::string json_path = path.string() + ".json";
    instance.write(json_path, packingsolver::InstanceFormat::Json);

    InstanceBuilder read_instance_builder;
    read_instance_builder.read(json_path);
    const Instance read_instance = read_instance_builder.build();

    fs::remove(json_path);

    EXPECT_EQ(read_instance.objective(), packingsolver::Objective::Knapsack);
    EXPECT_EQ(read_instance.knapsack_goal(), 1.0445144262560912);
    EXPECT_EQ(read_instance.knapsack_cutoff(), 0.98765432101234567);
}

TEST(Rectangle, WriteCsvRoundTripGoals)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.set_knapsack_goal(1.0445144262560912);
    instance_builder.set_knapsack_cutoff(0.98765432101234567);
    instance_builder.add_bin_type(100, 100);
    instance_builder.add_item_type(10, 10);
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    instance.write(path.string(), packingsolver::InstanceFormat::Csv);

    InstanceBuilder read_instance_builder;
    read_instance_builder.read_item_types(path.string() + "_items.csv");
    read_instance_builder.read_bin_types(path.string() + "_bins.csv");
    read_instance_builder.read_parameters(path.string() + "_parameters.csv");
    const Instance read_instance = read_instance_builder.build();

    for (const std::string& suffix: {"_items.csv", "_bins.csv", "_defects.csv", "_parameters.csv"})
        fs::remove(path.string() + suffix);

    EXPECT_EQ(read_instance.objective(), packingsolver::Objective::Knapsack);
    EXPECT_EQ(read_instance.knapsack_goal(), 1.0445144262560912);
    EXPECT_EQ(read_instance.knapsack_cutoff(), 0.98765432101234567);
}
