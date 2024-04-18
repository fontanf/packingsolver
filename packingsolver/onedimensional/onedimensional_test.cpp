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

TEST(OneDimensional, Users20240409)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPackingWithLeftovers);
    fs::path directory = fs::path("data") / "onedimensional" / "users";
    instance_builder.read_item_types((directory / "2024-04-09_items.csv").string());
    instance_builder.read_bin_types((directory / "2024-04-09_bins.csv").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytime;
    Output output = optimize(instance, optimize_parameters);

    EXPECT_EQ(output.solution_pool.best().cost(), 27800);
}
