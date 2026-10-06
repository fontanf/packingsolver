#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

TEST(RectangleGuillotine, BinPackingWithLeftoversA1)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "roadef2018";
    instance_builder.read_item_types((directory / "A1_items.csv").string());
    instance_builder.read_bin_types((directory / "A1_bins.csv").string());
    instance_builder.read_defects((directory / "A1_defects.csv").string());
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.solution_pool.best().waste(), 425486);
}

TEST(RectangleGuillotine, BinPackingWithLeftoversA17)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "roadef2018";
    instance_builder.read_item_types((directory / "A17_items.csv").string());
    instance_builder.read_bin_types((directory / "A17_bins.csv").string());
    instance_builder.read_defects((directory / "A17_defects.csv").string());
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.solution_pool.best().waste(), 3617251);
}

TEST(RectangleGuillotine, BinPackingWithLeftoversA20)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "roadef2018";
    instance_builder.read_item_types((directory / "A20_items.csv").string());
    instance_builder.read_bin_types((directory / "A20_bins.csv").string());
    instance_builder.read_defects((directory / "A20_defects.csv").string());
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    optimize_parameters.not_anytime_tree_search_queue_size = 1e4;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.solution_pool.best().waste(), 1467925);
}

TEST(RectangleGuillotine, BinPackingWithLeftoversB5)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "rectangle" / "roadef2018";
    instance_builder.read_item_types((directory / "B5_items.csv").string());
    instance_builder.read_bin_types((directory / "B5_bins.csv").string());
    instance_builder.read_defects((directory / "B5_defects.csv").string());
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.solution_pool.best().waste(), 72155615);
}

TEST(RectangleGuillotine, BinPackingWithLeftoversEmptyBinTreeSearch)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    instance_builder.add_bin_type(6000, 3000);
    instance_builder.add_defect(0, 0, 0, 6000, 3000);
    instance_builder.add_bin_type(6000, 3000);
    instance_builder.add_item_type(6000, 3000);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.solution_pool.best().number_of_items(), instance.number_of_items());
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 2);
}

struct RectangleGuillotineOptimizeTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleGuillotineOptimizeTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineOptimizeTest: public testing::TestWithParam<RectangleGuillotineOptimizeTestParams> { };

TEST_P(RectangleGuillotineOptimizeTest, RectangleGuillotineOptimize)
{
    RectangleGuillotineOptimizeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    // An instance in the JSON format is given as the items path.
    if (test_params.items_path.extension() == ".json") {
        instance_builder.read(test_params.items_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_defects(test_params.defects_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
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
        RectangleGuillotine,
        RectangleGuillotineOptimizeTest,
        testing::ValuesIn(std::vector<RectangleGuillotineOptimizeTestParams>{
            {
                // The lower bound of 'VariableSizedBinPacking' (the
                // one-dimensional trivial bound, which every problem type
                // uses through its one-dimensional relaxation) used to
                // require the optional bins alone to cover the items,
                // ignoring that the mandatory bins of a bin type with a
                // positive 'copies_min' hold items too. No two items fit in
                // the same bin: the optimum uses three bins of type 1
                // (cost 3), but the bound was 4.
                fs::path("data") / "rectangleguillotine" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "variable_sized_bin_packing_mandatory_bins" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-24" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-24" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-24" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-24" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "users" / "2025-01-29" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2025-01-29" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "users" / "2025-01-29" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2025-01-29" / "solution.csv",
            }, {
                // The soft trims of the solution of the tree search were
                // those of the flipped instance when the first stage is
                // horizontal, and on the wrong depths with two stages.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_horizontal" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_horizontal" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_horizontal" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_horizontal" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_two_stages" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_two_stages" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_two_stages" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_two_stages" / "solution.csv",
            }, {
                // The tree search didn't charge the cuts separating the
                // waste of the soft trims in its solutions, and with two
                // stages, the 2-cuts didn't go through the soft left trim.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_cutting_cost" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_cutting_cost" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_cutting_cost_two_stages" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trims_cutting_cost_two_stages" / "solution.csv",
            }, {
                // The residual of the solution included the trims in its area,
                // which doesn't include them.
                fs::path("data") / "rectangleguillotine" / "tests" / "waste_cost_top_trim" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "waste_cost_top_trim" / "solution.csv",
            }, {
                // The tree search charged the cut of a 3-level sub-plate
                // without item, which the solution merges with the waste.
                fs::path("data") / "rectangleguillotine" / "tests" / "cutting_cost_defect_waste" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "cutting_cost_defect_waste" / "solution.csv",
            }, {
                // The waste cost of the tree search didn't include the cut
                // thickness of the last 1-cut.
                fs::path("data") / "rectangleguillotine" / "tests" / "waste_cost_cut_thickness" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "waste_cost_cut_thickness" / "solution.csv",
            }, {
                // Only one of the two 3-cuts was charged when the 1-level
                // sub-plate is widened beyond the new 3-level sub-plate
                // (because of a defect).
                fs::path("data") / "rectangleguillotine" / "tests" / "cutting_cost_defect_3_cuts" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "cutting_cost_defect_3_cuts" / "solution.csv",
            }, {
                // The cut of the waste of a soft left trim wasn't counted in
                // the maximum number of 1-cuts.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_maximum_number_1_cuts" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_maximum_number_1_cuts" / "solution.csv",
            }, {
                // The cut of the waste of a soft bottom trim wasn't counted in
                // the maximum number of 2-cuts.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_maximum_number_2_cuts" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_maximum_number_2_cuts" / "solution.csv",
            }, {
                // The waste of a soft trim must satisfy the minimum waste
                // length: the items were placed right after a soft left trim
                // narrower than it.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_minimum_waste_left" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_minimum_waste_left" / "solution.csv",
            }, {
                // Same with a soft bottom trim.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_minimum_waste_bottom" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_minimum_waste_bottom" / "solution.csv",
            }, {
                // The waste of a soft trim not larger than the cut thickness
                // couldn't be cut.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_cut_thickness" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_cut_thickness" / "solution.csv",
            }, {
                // Soft left trim narrower than the minimum waste length, and
                // last sub-plate up to the border of a soft right trim, with a
                // waste cost.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_minimum_waste_waste_cost" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_minimum_waste_waste_cost" / "solution.csv",
            }, {
                // When a 2-level sub-plate is heightened, the waste above its
                // previous items must satisfy the minimum waste length after
                // the cut thickness.
                fs::path("data") / "rectangleguillotine" / "tests" / "minimum_waste_cut_thickness" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "minimum_waste_cut_thickness" / "solution.csv",
            }, {
                // An item on top of its 3-level sub-plate (above a defect)
                // moved into the soft top trim with the top of its 2-level
                // sub-plate.
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_item_above_defect" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "soft_trim_item_above_defect" / "solution.csv",
            }}));

TEST(RectangleGuillotine, SolutionItemInSoftTrim)
{
    // Bin 100x100 with a soft left trim of 10: an item may not be placed in
    // the trim, even though the trim isn't cut.
    InstanceBuilder instance_builder;
    instance_builder.add_item_type(40, 50);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.add_trims(
            bin_type_id,
            10, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft);
    Instance instance = instance_builder.build();

    // Item at x = 0, in the trim.
    SolutionBuilder solution_builder_1(instance);
    solution_builder_1.add_bin(0, 1, CutOrientation::Vertical);
    solution_builder_1.add_node(1, 40);
    solution_builder_1.add_node(2, 50);
    solution_builder_1.set_last_node_item(0);
    Solution solution_1 = solution_builder_1.build();
    EXPECT_FALSE(solution_1.trims_feasible());
    EXPECT_FALSE(solution_1.feasible());

    // Item at x = 10, after the trim.
    SolutionBuilder solution_builder_2(instance);
    solution_builder_2.add_bin(0, 1, CutOrientation::Vertical);
    solution_builder_2.add_node(1, 10);
    solution_builder_2.add_node(1, 50);
    solution_builder_2.add_node(2, 50);
    solution_builder_2.set_last_node_item(0);
    Solution solution_2 = solution_builder_2.build();
    EXPECT_TRUE(solution_2.trims_feasible());
    EXPECT_TRUE(solution_2.feasible());
}

TEST(RectangleGuillotine, AddTrimsAcceptsValidRightTrim)
{
    // Bin 50x100 (width < height, so a bug comparing right_trim against the
    // bin's height instead of its width would go unnoticed here too if it
    // wrongly *rejected* something valid - this trim leaves 20 of usable
    // width (50 - 10 - 20 = 20 > 0), well within either dimension.
    InstanceBuilder instance_builder;
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(50, 100);
    EXPECT_NO_THROW(instance_builder.add_trims(
            bin_type_id,
            10, TrimType::Hard,
            20, TrimType::Hard,
            0, TrimType::Hard,
            0, TrimType::Hard));
}

TEST(RectangleGuillotine, AddTrimsRejectsRightTrimExceedingWidthNotHeight)
{
    // Bin 50x100. left_trim=0, right_trim=60: the right trim alone already
    // exceeds the bin's *width* (50), which must be rejected - but a bug
    // comparing against the bin's *height* (100) instead would wrongly
    // accept it (60 <= 100), since width and height differ here.
    InstanceBuilder instance_builder;
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(50, 100);
    EXPECT_THROW(
            instance_builder.add_trims(
                    bin_type_id,
                    0, TrimType::Hard,
                    60, TrimType::Hard,
                    0, TrimType::Hard,
                    0, TrimType::Hard),
            std::invalid_argument);
}

TEST(RectangleGuillotine, AddTrimsRejectsTrimsConsumingWholeWidth)
{
    // Bin 50x100. left_trim=0, right_trim=50: consumes the entire width,
    // leaving zero usable space - must be rejected, matching how the other
    // three trims each reject consuming their entire dimension.
    InstanceBuilder instance_builder;
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(50, 100);
    EXPECT_THROW(
            instance_builder.add_trims(
                    bin_type_id,
                    0, TrimType::Hard,
                    50, TrimType::Hard,
                    0, TrimType::Hard,
                    0, TrimType::Hard),
            std::invalid_argument);
}

namespace
{

Instance read_json(const std::string& json)
{
    std::stringstream ss(json);
    InstanceBuilder instance_builder;
    instance_builder.read(ss);
    return instance_builder.build();
}

const std::string BIN_AND_ITEM_TYPES = R"(
        "bin_types": [{"width": 100, "height": 50}],
        "item_types": [{"width": 10, "height": 10}]
)";

}

TEST(RectangleGuillotine, ReadUnknownEnumValues)
{
    // Unrecognized values are rejected, instead of being ignored or leaving
    // the value uninitialized.
    for (const std::string& parameter: {
            R"("cut_type": "inexact")",
            R"("first_stage_orientation": "diagonal")"}) {
        EXPECT_THROW(
                read_json("{\"objective\": \"bin-packing\", " + parameter + ", " + BIN_AND_ITEM_TYPES + "}"),
                std::invalid_argument);
    }
    EXPECT_THROW(
            read_json(R"({
                "objective": "bin-packing",
                "bin_types": [{"width": 100, "height": 50, "left_trim": 5, "left_trim_type": "medium"}],
                "item_types": [{"width": 10, "height": 10}]})"),
            std::invalid_argument);
    Instance instance = read_json(R"({
            "objective": "bin-packing",
            "cut_type": "exact",
            "first_stage_orientation": "any",
            "bin_types": [{"width": 100, "height": 50, "left_trim": 5, "left_trim_type": "soft"}],
            "item_types": [{"width": 10, "height": 10}]})");
    EXPECT_EQ(instance.parameters().cut_type, CutType::Exact);
    EXPECT_EQ(instance.parameters().first_stage_orientation, CutOrientation::Any);
    EXPECT_EQ(instance.bin_type(0).left_trim_type, TrimType::Soft);
}

TEST(RectangleGuillotine, ReadUnlimitedNumberOfStages)
{
    // The number of stages is read before the bin types: the bound is
    // computed when the instance is built.
    Instance instance = read_json(
            "{\"objective\": \"bin-packing\", \"number_of_stages\": \"unlimited\", "
            + BIN_AND_ITEM_TYPES + "}");
    EXPECT_EQ(instance.parameters().number_of_stages, 150);
    EXPECT_TRUE(instance.number_of_stages_unlimited());
    EXPECT_EQ(instance.parameters().cut_type, CutType::Exact);
    EXPECT_EQ(instance.parameters().first_stage_orientation, CutOrientation::Any);
    // The unlimited number of stages overrides the cut type and the first
    // stage orientation.
    instance = read_json(
            "{\"objective\": \"bin-packing\", \"number_of_stages\": \"unlimited\", "
            "\"cut_type\": \"non-exact\", \"first_stage_orientation\": \"vertical\", "
            + BIN_AND_ITEM_TYPES + "}");
    EXPECT_EQ(instance.parameters().cut_type, CutType::Exact);
    EXPECT_EQ(instance.parameters().first_stage_orientation, CutOrientation::Any);
    EXPECT_EQ(instance.parameters().number_of_stages, 150);
}

TEST(RectangleGuillotine, ReadCuttingCosts)
{
    // Without 'number_of_stages' and 'cut_type': the defaults, 3 stages and
    // non-exact cuts, so 5 cutting costs.
    Instance instance = read_json(
            "{\"objective\": \"bin-packing-cutting-cost\", "
            "\"cutting_costs\": [{\"fixed\": 10, \"variable\": 1}, {\"fixed\": 5, \"variable\": 2}], "
            + BIN_AND_ITEM_TYPES + "}");
    ASSERT_EQ(instance.parameters().cutting_costs.size(), 5);
    EXPECT_EQ(instance.parameters().cutting_costs[0].fixed, 10);
    EXPECT_EQ(instance.parameters().cutting_costs[1].variable, 2);
    EXPECT_EQ(instance.parameters().cutting_costs[4].fixed, 0);
    // 2 stages and exact cuts: at most 3 cutting costs.
    EXPECT_THROW(
            read_json(
                "{\"objective\": \"bin-packing-cutting-cost\", "
                "\"number_of_stages\": 2, \"cut_type\": \"exact\", "
                "\"cutting_costs\": [{\"fixed\": 1, \"variable\": 1}, {\"fixed\": 1, \"variable\": 1}, "
                "{\"fixed\": 1, \"variable\": 1}, {\"fixed\": 1, \"variable\": 1}], "
                + BIN_AND_ITEM_TYPES + "}"),
            std::invalid_argument);
    // Without cutting costs, all are 0.
    instance = read_json("{\"objective\": \"bin-packing\", " + BIN_AND_ITEM_TYPES + "}");
    ASSERT_EQ(instance.parameters().cutting_costs.size(), 5);
    EXPECT_EQ(instance.parameters().cutting_costs[0].fixed, 0);
}
