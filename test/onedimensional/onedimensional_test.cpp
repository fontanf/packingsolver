#include "packingsolver/onedimensional/instance_builder.hpp"
#include "packingsolver/onedimensional/optimize.hpp"
#include "onedimensional/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::onedimensional;
namespace fs = boost::filesystem;

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
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 5);
    const Instance instance = instance_builder.build();

    InstanceBuilder sub_instance_builder;
    packingsolver::BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
    packingsolver::ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type(instance, item_type_id);
    const Instance sub_instance = sub_instance_builder.build();

    EXPECT_EQ(sub_instance.bin_type(sub_bin_type_id).number_of_resources(), 1);
    EXPECT_EQ(
            sub_instance.bin_type(sub_bin_type_id).resource(0).item_consumption(sub_item_type_id, 0),
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
    instance.write(path.string(), InstanceFormat::Csv);

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
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 5);
    const Instance instance = instance_builder.build();

    fs::path path = fs::temp_directory_path() / fs::unique_path();
    std::string json_path = path.string() + ".json";
    instance.write(json_path, InstanceFormat::Json);

    InstanceBuilder read_instance_builder;
    read_instance_builder.read(json_path);
    const Instance read_instance = read_instance_builder.build();

    fs::remove(json_path);

    EXPECT_EQ(read_instance.objective(), packingsolver::Objective::BinPacking);
    ASSERT_EQ(read_instance.number_of_bin_types(), 1);
    EXPECT_EQ(read_instance.bin_type(0).number_of_resources(), 1);
    EXPECT_EQ(read_instance.bin_type(0).resource(0).capacity, 10.0);
    EXPECT_EQ(read_instance.bin_type(0).resource(0).item_consumption(0, 0), 5.0);
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
            instance.write(path.string(), InstanceFormat::Csv),
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
            instance.write(path.string(), InstanceFormat::Csv),
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
            instance.write(path.string(), InstanceFormat::Csv),
            std::invalid_argument);
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read((directory / "2023-08-01_solution.csv").string());
    Solution solution = solution_builder.build();
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read((directory / "2024-04-06_t1_solution.csv").string());
    Solution solution = solution_builder.build();
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read((directory / "2024-04-06_t2_solution.csv").string());
    Solution solution = solution_builder.build();
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = 1;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read((directory / "2024-04-06_t3_solution.csv").string());
    Solution solution = solution_builder.build();
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_sequential_value_correction = 1;
    optimize_parameters.verbosity_level = 2;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read((directory / "2024-04-07_solution.csv").string());
    Solution solution = solution_builder.build();
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_sequential_value_correction = 1;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read((directory / "2024-04-09_solution.csv").string());
    Solution solution = solution_builder.build();
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_column_generation = 1;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read((directory / "2024-04-21_solution.csv").string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);
    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
}
