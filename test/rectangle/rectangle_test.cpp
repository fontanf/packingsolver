#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

TEST(Rectangle, BinCopies)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    instance_builder.add_item_type(1, 1, -1, 10);
    instance_builder.add_bin_type(10, 10, -1, 10);
    const Instance instance = instance_builder.build();
    Solution solution(instance);
    solution.add_bin(0, 2);
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

struct TestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const TestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class AlgorithmTest: public testing::TestWithParam<TestParams> { };

TEST_P(AlgorithmTest, Algorithm)
{
    TestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);

    Solution solution(instance, test_params.certificate_path.string());
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        AlgorithmTest,
        testing::ValuesIn(std::vector<TestParams>{
            {
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_items.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_solution.csv",
            }}));
