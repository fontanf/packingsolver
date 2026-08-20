#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

struct RectangleColumnGenerationTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleColumnGenerationTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleColumnGenerationTest: public testing::TestWithParam<RectangleColumnGenerationTestParams> { };

TEST_P(RectangleColumnGenerationTest, RectangleColumnGeneration)
{
    RectangleColumnGenerationTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search = false;
    optimize_parameters.use_tree_search_maximal_spaces = false;
    optimize_parameters.use_sequential_value_correction = false;
    optimize_parameters.use_column_generation = true;
    optimize_parameters.use_dichotomic_search = false;
    optimize_parameters.use_sequential_single_knapsack = false;
    optimize_parameters.use_benders_decomposition = false;
    optimize_parameters.use_bar_relaxation = false;
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
        RectangleColumnGenerationTest,
        testing::ValuesIn(std::vector<RectangleColumnGenerationTestParams>{
            {
                // Two bin types of different costs (50 and 80): column
                // generation's usual bound conversion is unsound here, so
                // this can only be solved to optimality via the sequential
                // feasibility scheme (see 'ColumnGenerationParameters::
                // use_sequential_feasibility'). Optimal solution uses 3
                // bins (2 of the cheaper type, 1 of the costlier one).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_multiple_bin_types_heterogeneous_cost" / "solution.csv",
            }, {
                // Reused from 'benders_decomposition_test.cpp': two bin
                // types (same cost here, but still more than one, so this
                // also goes through the sequential feasibility scheme by
                // default), 3 copies of a single item type. Reference
                // solution uses 1 bin of the first type and 2 of the
                // second, 3 bins total.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_two_bin_types" / "solution.csv",
            }, {
                // Reused from 'benders_decomposition_test.cpp': a single bin
                // type but two different item sizes that must be mixed
                // within a bin to reach the optimum of 2 bins - exercises
                // column generation's regular item-mix pricing rather than
                // the sequential feasibility scheme (only one bin type
                // here).
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_mixed_items_two_bins" / "solution.csv",
            }, {
                // Regression check that the 'VariableSizedBinPacking'
                // objective's own bound conversion (which multiplies rather
                // than divides by a bin cost - see 'column_generation''s
                // 'new_bound_callback') is unaffected by the
                // 'BinPacking'-only sequential feasibility scheme added
                // alongside it. Two bin types, one copy of each: a cheap
                // 15x15 bin too small to fit both 10x10 items together, and
                // a costlier 20x30 bin that fits both at once. Using the
                // cheap bin still requires the costly one for the second
                // item (total cost 1 + 3 = 4), so the optimum instead uses
                // only the costly bin alone (cost 3), for 1 bin total.
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "variable_sized_bin_packing" / "solution.csv",
            }, {
                // Exercises 'column_generation''s 'BinPackingWithLeftovers'
                // sequential feasibility scheme: repeatedly shrinks the
                // last bin (via 'next_leftover_bin_type') until infeasible,
                // keeping the previous, still-feasible candidate as the
                // answer. A single bin, so this also exercises 'optimize''s
                // own 'number_of_bins() <= 1' dispatch exemption for this
                // objective (every other objective there disables column
                // generation instead). Single 12x10 bin, nine 3x2 items,
                // 'leftover_mode,X': the nine items tile exactly into a
                // 6x10 region, leaving the remaining 6 units of width
                // (12 - 6) as leftover.
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "bin_packing_with_leftovers_x" / "solution.csv",
            }}));
