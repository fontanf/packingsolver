#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/solution_builder.hpp"
#include "irregular/tree_search_periodic_packing.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

struct IrregularTreeSearchPeriodicPackingTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularTreeSearchPeriodicPackingTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularTreeSearchPeriodicPackingTest: public testing::TestWithParam<IrregularTreeSearchPeriodicPackingTestParams> { };

// Calls tree_search_periodic_packing() directly instead of optimize():
// optimize() unconditionally dispatches any 1-item/1-bin Knapsack instance
// to the separate trivial_single_item fast path (irregular/trivial.cpp),
// regardless of use_tree_search_periodic_packing -- bypassing this
// algorithm (and its spacing handling) entirely. Some of the instances
// below are deliberately single-item/single-copy (to test an exact-fit
// boundary), so going through optimize() would silently test
// trivial_single_item instead. Confirmed to produce identical results to
// optimize(..., use_tree_search_periodic_packing = true) on the
// multi-item instances too.
TEST_P(IrregularTreeSearchPeriodicPackingTest, IrregularTreeSearchPeriodicPacking)
{
    IrregularTreeSearchPeriodicPackingTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    std::cout << "Certificate path: " << test_params.certificate_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    TreeSearchPeriodicPackingParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    TreeSearchPeriodicPackingOutput output = tree_search_periodic_packing(instance, parameters);

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
        Irregular,
        IrregularTreeSearchPeriodicPackingTest,
        testing::ValuesIn(std::vector<IrregularTreeSearchPeriodicPackingTestParams>{
            {
                fs::path("data") / "irregular" / "users" / "2025-12-08.json",
                fs::path("data") / "irregular" / "users" / "2025-12-08_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_2.json",
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_3.json",
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_4.json",
                fs::path("data") / "irregular" / "users" / "2026-04-08_sub_4_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub.json",
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub_solution.json"
            }, {
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub_2.json",
                fs::path("data") / "irregular" / "users" / "2026-04-23_sub_2_solution.json"
            }, {
                // Right triangle, 40 copies, bin exactly 50x40 (a 5x4 grid
                // of 10x10 triangle bounding boxes, both rotations used,
                // zero gap). item_item_minimum_spacing = 0.5 shrinks that to
                // a 4x3 grid, so only 24 of the 40 copies fit.
                fs::path("data") / "irregular" / "tests" / "periodic_packing_item_item_spacing.json",
                fs::path("data") / "irregular" / "tests" / "periodic_packing_item_item_spacing_solution.json"
            }, {
                // Same triangle and copies, bin 52x42 (a little extra slack
                // on both axes beyond the tight 50x40, so bin spacing --
                // not a lack of room -- is what breaks the fit).
                // item_bin_minimum_spacing = 1.2 shrinks the 5x4 grid to
                // 4x3, so only 24 of the 40 copies fit.
                fs::path("data") / "irregular" / "tests" / "periodic_packing_item_bin_spacing.json",
                fs::path("data") / "irregular" / "tests" / "periodic_packing_item_bin_spacing_solution.json"
            }, {
                // Single rectangular item (10x6), single copy,
                // item_bin_minimum_spacing = 1, bin exactly 12x8: the exact
                // boundary at which the item still fits.
                fs::path("data") / "irregular" / "tests" / "periodic_packing_single_item_exact_fit_rectangle.json",
                fs::path("data") / "irregular" / "tests" / "periodic_packing_single_item_exact_fit_rectangle_solution.json"
            }, {
                // Same exact-fit boundary as above, but with a
                // non-rectangular (right triangle) item, to exercise the
                // NFP-based (not just AABB) bin-wall spacing check.
                fs::path("data") / "irregular" / "tests" / "periodic_packing_single_item_exact_fit_triangle.json",
                fs::path("data") / "irregular" / "tests" / "periodic_packing_single_item_exact_fit_triangle_solution.json"
            }}));
