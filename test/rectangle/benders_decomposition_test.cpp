#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/solution_builder.hpp"

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

    /**
     * Path to a JSON instance file; if non-empty, used instead of
     * 'items_path'/'bins_path'/'defects_path'/'parameters_path' (needed for
     * features with no CSV representation, e.g. resources).
     */
    fs::path instance_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleBendersDecompositionTestParams& test_params)
{
    os << (!test_params.instance_path.empty()? test_params.instance_path: test_params.items_path);
    return os;
}

class RectangleBendersDecompositionTest: public testing::TestWithParam<RectangleBendersDecompositionTestParams> { };

TEST_P(RectangleBendersDecompositionTest, RectangleBendersDecomposition)
{
    RectangleBendersDecompositionTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    if (!test_params.instance_path.empty()) {
        instance_builder.read(test_params.instance_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_defects(test_params.defects_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.use_benders_decomposition = true;
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
        Rectangle,
        RectangleBendersDecompositionTest,
        testing::ValuesIn(std::vector<RectangleBendersDecompositionTestParams>{
            {
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_dimension_mismatch" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_dimension_mismatch" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_dimension_mismatch" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing_dimension_mismatch" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "solution.csv",
            }, {
                // A capacity-1 resource with each item consuming 1 caps
                // every bin to a single item, even though the bin is
                // geometrically large enough to fit several - forcing 3
                // bins for 3 items. Enforced entirely by the master's own
                // MILP (see 'build_master_instance' in
                // 'benders_decomposition.cpp'), so the geometric slave
                // subproblem never even needs to consider the resource.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_resource_capacity" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_resource_capacity" / "instance.json",
            }}));
