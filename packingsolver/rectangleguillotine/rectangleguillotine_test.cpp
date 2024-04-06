#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/optimize.hpp"

#include <gtest/gtest.h>

using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotine, BinPackingWithLeftoversA1)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    instance_builder.read_item_types("data/rectangle/roadef2018/A1_items.csv");
    instance_builder.read_bin_types("data/rectangle/roadef2018/A1_bins.csv");
    instance_builder.read_defects("data/rectangle/roadef2018/A1_defects.csv");
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    std::string certificate_path = "data/rectangle_solutions/roadef2018/A1_solution.csv";

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
    instance_builder.read_item_types("data/rectangle/roadef2018/A17_items.csv");
    instance_builder.read_bin_types("data/rectangle/roadef2018/A17_bins.csv");
    instance_builder.read_defects("data/rectangle/roadef2018/A17_defects.csv");
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    std::string certificate_path = "data/rectangle_solutions/roadef2018/A17_solution.csv";

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
    instance_builder.read_item_types("data/rectangle/roadef2018/A20_items.csv");
    instance_builder.read_bin_types("data/rectangle/roadef2018/A20_bins.csv");
    instance_builder.read_defects("data/rectangle/roadef2018/A20_defects.csv");
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    std::string certificate_path = "data/rectangle_solutions/roadef2018/A20_solution.csv";

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
    instance_builder.read_item_types("data/rectangle/roadef2018/B5_items.csv");
    instance_builder.read_bin_types("data/rectangle/roadef2018/B5_bins.csv");
    instance_builder.read_defects("data/rectangle/roadef2018/B5_defects.csv");
    instance_builder.set_roadef2018();
    Instance instance = instance_builder.build();

    std::string certificate_path = "data/rectangle_solutions/roadef2018/B5_solution.csv";

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_tree_search = true;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.solution_pool.best().waste(), 72155615);
}
