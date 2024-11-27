#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"

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
