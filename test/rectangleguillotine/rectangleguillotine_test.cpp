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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
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
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
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
}

INSTANTIATE_TEST_SUITE_P(
        RectangleGuillotine,
        RectangleGuillotineOptimizeTest,
        testing::ValuesIn(std::vector<RectangleGuillotineOptimizeTestParams>{
            {
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
            }}));
