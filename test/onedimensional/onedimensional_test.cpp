#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

TEST(OneDimensional, BinCopies)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    instance_builder.add_item_type(1, -1, 10);
    instance_builder.add_bin_type(10, -1, 10);
    const Instance instance = instance_builder.build();
    Solution solution(instance);
    solution.add_bin(0, 2);
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

TEST(OneDimensional, Users_2023_08_01)
{
    InstanceBuilder instance_builder;
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2023-08-01_items.csv").string());
    instance_builder.read_bin_types((directory / "2023-08-01_bins.csv").string());
    instance_builder.read_parameters((directory / "2023-08-01_parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, (directory / "2023-08-01_solution.csv").string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}

TEST(OneDimensional, Users_2024_04_06_t1)
{
    InstanceBuilder instance_builder;
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2024-04-06_t1_items.csv").string());
    instance_builder.read_bin_types((directory / "2024-04-06_t1_bins.csv").string());
    instance_builder.read_parameters((directory / "2024-04-06_t1_parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, (directory / "2024-04-06_t1_solution.csv").string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}

TEST(OneDimensional, Users_2024_04_06_t2)
{
    InstanceBuilder instance_builder;
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2024-04-06_t2_items.csv").string());
    instance_builder.read_bin_types((directory / "2024-04-06_t2_bins.csv").string());
    instance_builder.read_parameters((directory / "2024-04-06_t2_parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, (directory / "2024-04-06_t2_solution.csv").string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}

TEST(OneDimensional, Users_2024_04_06_t3)
{
    InstanceBuilder instance_builder;
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2024-04-06_t3_items.csv").string());
    instance_builder.read_bin_types((directory / "2024-04-06_t3_bins.csv").string());
    instance_builder.read_parameters((directory / "2024-04-06_t3_parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_tree_search = 1;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, (directory / "2024-04-06_t3_solution.csv").string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}

TEST(OneDimensional, Users_2024_04_07)
{
    InstanceBuilder instance_builder;
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2024-04-07_items.csv").string());
    instance_builder.read_bin_types((directory / "2024-04-07_bins.csv").string());
    instance_builder.read_parameters((directory / "2024-04-07_parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_sequential_value_correction = 1;
    optimize_parameters.verbosity_level = 2;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, (directory / "2024-04-07_solution.csv").string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}

TEST(OneDimensional, Users_2024_04_09)
{
    InstanceBuilder instance_builder;
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2024-04-09_items.csv").string());
    instance_builder.read_bin_types((directory / "2024-04-09_bins.csv").string());
    instance_builder.read_parameters((directory / "2024-04-09_parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_sequential_value_correction = 1;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, (directory / "2024-04-09_solution.csv").string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}

TEST(OneDimensional, Users_2024_04_21)
{
    InstanceBuilder instance_builder;
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2024-04-21_items.csv").string());
    instance_builder.read_bin_types((directory / "2024-04-21_bins.csv").string());
    instance_builder.read_parameters((directory / "2024-04-21_parameters.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, (directory / "2024-04-21_solution.csv").string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}
