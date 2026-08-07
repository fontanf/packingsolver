#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

struct RectangleTreeSearchTestParams
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

inline std::ostream& operator<<(std::ostream& os, const RectangleTreeSearchTestParams& test_params)
{
    os << (!test_params.instance_path.empty()? test_params.instance_path: test_params.items_path);
    return os;
}

class RectangleTreeSearchTest: public testing::TestWithParam<RectangleTreeSearchTestParams> { };

TEST_P(RectangleTreeSearchTest, RectangleTreeSearch)
{
    RectangleTreeSearchTestParams test_params = GetParam();
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
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = true;
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
        RectangleTreeSearchTest,
        testing::ValuesIn(std::vector<RectangleTreeSearchTestParams>{
            {
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_area" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_area" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_area" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_area" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "solution.csv",
            }, {
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_y" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_y" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_y" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_y" / "solution.csv",
            }, {
                // BinPacking with two bin types where the smaller one
                // (tried first, since bin types must be used in increasing
                // size order) can never fit the item: the optimal solution
                // must leave that bin empty rather than skip it, since bin
                // usage order is fixed for this objective. Same instance as
                // 'sequential_value_correction_test.cpp', to check both
                // algorithms agree on it.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_empty_bin" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_empty_bin" / "bins.csv",
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_empty_bin" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_empty_bin" / "solution.csv",
            }, {
                // Resources have no CSV representation, so this fixture is
                // read from JSON instead (see 'instance_path' above). Same
                // fixture as 'benders_decomposition_test.cpp': a capacity-1
                // resource with each item consuming 1 caps every bin to a
                // single item, even though the bin is geometrically large
                // enough to fit several - forcing 3 bins for 3 items.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "bin_packing_resource_capacity" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_resource_capacity" / "instance.json",
            }, {
                // Knapsack objective, item profit 10, resource capacity 1
                // with penalty 100, uniform (single-entry, non-
                // 'threshold_schedule(N)') consumption schedule - unlike
                // 'benders_decomposition_contiguity_test.cpp''s own
                // 'knapsack_penalize_resource' fixture, whose MILP-encoded
                // 'penalize' resource requires the 'threshold_schedule(N)'
                // shape instead, this uniform shape is exactly what
                // 'tree_search'/'Solution::update_indicators' expect.
                // Packing a 2nd item in the single available bin crosses
                // the resource once (-100), so the solver should prefer
                // packing just 1 item (profit 10) over 2 (profit
                // 20 - 100 = -80).
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_penalize_resource_uniform" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_penalize_resource_uniform" / "instance.json",
            }}));
