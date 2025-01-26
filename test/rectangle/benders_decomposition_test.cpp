#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

struct RectangleBendersDecompositionTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleBendersDecompositionTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleBendersDecompositionTest: public testing::TestWithParam<RectangleBendersDecompositionTestParams> { };

TEST_P(RectangleBendersDecompositionTest, RectangleBendersDecomposition)
{
    RectangleBendersDecompositionTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_defects(test_params.defects_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.use_benders_decomposition = true;
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
        RectangleBendersDecompositionTest,
        testing::ValuesIn(std::vector<RectangleBendersDecompositionTestParams>{
            {
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_incompatible_pairs" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_incompatible_pairs" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_incompatible_pairs" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_incompatible_pairs" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_all_fit" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_all_fit" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_all_fit" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_all_fit" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_unpacked_high_profit" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_unpacked_high_profit" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_unpacked_high_profit" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_benders_unpacked_high_profit" / "solution.csv",
            }}));
