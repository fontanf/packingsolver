#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectanlgeGuillotineBranchingSchemeTestParams
{
    fs::path bins_path;
    fs::path defects_path;
    fs::path items_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectanlgeGuillotineBranchingSchemeTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineBranchingSchemeTest: public testing::TestWithParam<RectanlgeGuillotineBranchingSchemeTestParams> { };

TEST_P(RectangleGuillotineBranchingSchemeTest, RectangleGuillotineBranchingScheme)
{
    RectanlgeGuillotineBranchingSchemeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    auto output = optimize(instance, optimize_parameters);

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
        RectangleGuillotine,
        RectangleGuillotineBranchingSchemeTest,
        testing::ValuesIn(std::vector<RectanlgeGuillotineBranchingSchemeTestParams>{
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_1" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_1" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_1" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_1" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_1" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_2" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_2" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_2" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_2" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_2" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_3" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_3" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_3" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_3" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_3" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_4" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_4" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_4" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_4" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_4" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_5" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_5" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_5" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_5" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_5" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_6" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_6" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_6" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_6" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_6" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_7" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_7" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_7" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_7" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_defects_7" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-26" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-26" / "defects.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-26" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-26" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2024-11-26" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_2nvo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_2nvo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_2nvo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_2nvo" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_3evo" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_3evo" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_3evo" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_soft_trims_3evo" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "users" / "2025-03-04" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "users" / "2025-03-04" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2025-03-04" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "users" / "2025-03-04" / "solution.csv",
            }}));



class RectangleGuillotineBranchingSchemeRoadef2018FixedStackTest: public testing::TestWithParam<std::string> { };

TEST_P(RectangleGuillotineBranchingSchemeRoadef2018FixedStackTest, RectangleGuillotineBranchingSchemeRoadef2018FixedStack)
{
    std::string name = GetParam();
    InstanceBuilder instance_0_builder;
    auto dir = fs::path("data") / "rectangle" / "roadef2018";
    instance_0_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    instance_0_builder.read_bin_types((dir / (name + "_bins.csv")).string());
    instance_0_builder.read_defects((dir / (name + "_defects.csv")).string());
    instance_0_builder.read_item_types((dir / (name + "_items.csv")).string());
    instance_0_builder.set_roadef2018();
    Instance instance_0 = instance_0_builder.build();

    std::cout << "Read solution..." << std::endl;
    SolutionBuilder solution_builder(instance_0);
    auto solution_dir = fs::path("data") / "rectangle_solutions" / "roadef2018";
    solution_builder.read((solution_dir / (name + "_solution.csv")).string());
    Solution solution = solution_builder.build();
    std::vector<ItemPos> item_type_id_to_pos(instance_0.number_of_item_types(), -1);
    ItemPos pos = 0;
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy) {
            for (const SolutionNode& solution_node: solution_bin.nodes) {
                if (solution_node.f == -1
                        || solution_node.item_type_id < 0) {
                    continue;
                }
                if (item_type_id_to_pos[solution_node.item_type_id] == -1) {
                    item_type_id_to_pos[solution_node.item_type_id] = pos;
                    pos++;
                }
            }
        }
    }
    std::vector<ItemTypeId> sorted_item_type_ids(instance_0.number_of_item_types(), -1);
    std::iota(sorted_item_type_ids.begin(), sorted_item_type_ids.end(), 0);
    sort(
            sorted_item_type_ids.begin(),
            sorted_item_type_ids.end(),
            [&item_type_id_to_pos](
                ItemTypeId item_type_id_1,
                ItemTypeId item_type_id_2) -> bool
            {
                return item_type_id_to_pos[item_type_id_1]
                    < item_type_id_to_pos[item_type_id_2];
            });

    std::cout << "Build new instance..." << std::endl;
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_parameters(instance_0.parameters());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_0.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_0.bin_type(bin_type_id);
        instance_builder.add_bin_type(
                bin_type,
                bin_type.copies,
                bin_type.copies_min);
    }
    for (ItemTypeId item_type_id: sorted_item_type_ids) {
        const ItemType& item_type = instance_0.item_type(item_type_id);
        instance_builder.add_item_type(
                item_type.rect.w,
                item_type.rect.h,
                item_type.profit,
                item_type.copies,
                item_type.oriented,
                0);
    }
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
    optimize_parameters.not_anytime_tree_search_queue_size = 1e7;
    //optimize_parameters.not_anytime_tree_search_queue_size = 1;
    auto output = optimize(instance, optimize_parameters);

    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        RectangleGuillotine,
        RectangleGuillotineBranchingSchemeRoadef2018FixedStackTest,
        testing::ValuesIn(std::vector<std::string>{
            "A1",
            "A2",
            "A3",
            "A4",
            "A5",
            "A6",
            "A7",
            "A8",
            "A9",
            "A10",
            "A11",
            "A12",
            "A13",
            "A14",
            "A15",
            "A16",
            "A17",
            "A18",
            "A19",
            "A20",
            "B1",
            "B2",
            "B3",
            "B4",
            "B5",
            "B6",
            "B7",
            "B8",
            "B9",
            "B10",
            "B11",
            "B12",
            "B13",
            "B14",
            "B15",
            "X1",
            "X2",
            "X3",
            "X4",
            "X5",
            "X6",
            "X7",
            "X8",
            "X9",
            "X10",
            "X11",
            "X12",
            "X13",
            "X14",
            "X15",
            }));
