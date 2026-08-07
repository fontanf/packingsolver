#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/benders_decomposition_contiguity.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

TEST(RectangleBendersDecompositionContiguity, UnsupportedObjectiveThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, SeveralBinTypesThrows)
{
    // The master (a P|cont|Cmax-style MILP over a single bin's columns) is
    // only defined for one bin used once - see the "Scope" paragraph in
    // 'benders_decomposition_contiguity.hpp'.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_bin_type(6, 6);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, BinTypeCopiesNotOneThrows)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.add_item_type(5, 5, true);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, UnsupportedPenalizeResourceCapacityThrows)
{
    // 'onedimentional_contiguity::add_resource_constraints' (used by
    // 'milp', not 'tree_search' - see
    // 'BendersDecompositionContiguityParameters::use_tree_search') only
    // supports a 'penalize' resource shaped like a Jepsen et al.
    // (2008)-style "at least 2 of a set of item-type units" cut: capacity
    // == 1 (see the 'knapsack_penalize_resource' fixture in this file's own
    // parametrized test below for a supported one) - any other capacity
    // throws rather than silently ignoring or mishandling it.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 2.0, true, 100.0);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 1, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 2, 0.0);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    parameters.use_tree_search = false;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

TEST(RectangleBendersDecompositionContiguity, UnsupportedPenalizeResourceScheduleShapeThrows)
{
    // A uniform (non-'threshold_schedule(N)') consumption schedule - see the
    // 'knapsack_penalize_resource' fixture in this file's own parametrized
    // test below for why this shape is required for 'milp' (not
    // 'tree_search' - see
    // 'BendersDecompositionContiguityParameters::use_tree_search').
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0, true, 100.0);
    ItemTypeId item_type_id = instance_builder.add_item_type(5, 5, true);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id, 0, 1.0);
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    parameters.use_tree_search = false;
    EXPECT_THROW(benders_decomposition_contiguity(instance, parameters), std::invalid_argument);
}

struct RectangleBendersDecompositionContiguityTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;

    // Path to a reference solution certificate. Empty for an instance
    // expected to be proven infeasible ('Feasibility' only).
    fs::path certificate_path;

    /**
     * Path to a JSON instance file; if non-empty, used instead of
     * 'items_path'/'bins_path'/'parameters_path' (needed for features with
     * no CSV representation, e.g. resources).
     */
    fs::path instance_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleBendersDecompositionContiguityTestParams& test_params)
{
    os << (!test_params.instance_path.empty()? test_params.instance_path: test_params.items_path);
    return os;
}

class RectangleBendersDecompositionContiguityTest: public testing::TestWithParam<RectangleBendersDecompositionContiguityTestParams> { };

TEST_P(RectangleBendersDecompositionContiguityTest, RectangleBendersDecompositionContiguity)
{
    RectangleBendersDecompositionContiguityTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    if (!test_params.instance_path.empty()) {
        instance_builder.read(test_params.instance_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
    Instance instance = instance_builder.build();

    BendersDecompositionContiguityParameters parameters;
    parameters.verbosity_level = 0;
    BendersDecompositionContiguityOutput output = benders_decomposition_contiguity(instance, parameters);

    if (test_params.certificate_path.empty()) {
        EXPECT_EQ(output.is_proven_infeasible, true);
    } else {
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
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleBendersDecompositionContiguityTest,
        testing::ValuesIn(std::vector<RectangleBendersDecompositionContiguityTestParams>{
            {
                // Same fixture (and expected profit) as
                // 'benders_decomposition_test.cpp': item1 (profit 190) is
                // pairwise-incompatible with the others, so the optimum
                // uses only the 4 copies of item0 (profit 50 each).
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_incompatible_item_pair" / "solution.csv",
            }, {
                // Same fixture as 'benders_decomposition_test.cpp' and
                // 'bar_relaxation_test.cpp' ('SquareGridBoundIsTight'-style
                // exact tiling): the 5 items tile the 20x10 bin exactly, so
                // the optimum is the sum of their (area-default) profits.
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_all_items_fit_exactly" / "solution.csv",
            }, {
                // Same fixture as 'benders_decomposition_test.cpp': all 9
                // copies of the lower-profit item0 (50 each) tile the bin
                // exactly (3x3 grid), strictly beating the single high-
                // profit item1 (199) alone.
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_high_profit_item_unpacked" / "solution.csv",
            }, {
                // Same fixture as 'benders_decomposition_test.cpp': the 5
                // copies of the thin, high-total-profit item0 (5 each) beat
                // the single larger-area item1 (profit 10) once geometry -
                // not just area - is accounted for.
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_area_domination_mismatch" / "solution.csv",
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': both copies of
                // the rotatable item fit side by side in the non-square
                // bin - the bar relaxation bound (2) is already tight here.
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_rotatable_items_fit_side_by_side" / "solution.csv",
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': only 3 of the
                // 4 copies actually fit once placed (a fourth would
                // overlap) - the bar relaxation bound (40/11) is loose
                // here, exercising that this algorithm finds the true,
                // strictly lower, exact optimum instead of the bound.
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_non_square_bin_too_many_rotatable_items" / "solution.csv",
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': 4 copies of a
                // 5x5 item tile a 10x10 bin exactly - genuinely feasible.
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_exact_tiling_feasible" / "solution.csv",
            }, {
                // Same fixture as 'bar_relaxation_test.cpp': the item
                // (20x20) does not fit the bin (10x10) in either
                // orientation, so no candidate position exists for it at
                // all - the BMP itself is infeasible, without ever needing
                // a cut.
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_item_too_big_infeasible" / "parameters.csv",
                fs::path(""),
            }, {
                // Regression test: 21 mandatory items filling 76% of a
                // 100x100 bin - genuinely feasible, but the plain
                // niche/skyline enumeration tree for 'y_check' (no
                // fathoming rules, no gap-closing symmetry-kill, no
                // preprocessing) used to blow up combinatorially on it and
                // never return. Exercises the full combinatorial
                // branch-and-bound of Côté, Dell'Amico & Iori (2014, §3.2)
                // / Wang et al. (2025, Appendix G.2) now backing 'y_check'.
                fs::path("data") / "rectangle" / "tests" / "feasibility_near_tight_21_items" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_near_tight_21_items" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_near_tight_21_items" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_near_tight_21_items" / "solution.csv",
            }, {
                // Regression test: a 2x3 bin exactly tiled by two 1x1 items
                // and two 1x2 items, one of each per column (each column's
                // own pair summing to exactly the bin's own height) -
                // genuinely feasible. 'ycheck::search''s own fathoming 4
                // used to compare only width and x (not height) between a
                // candidate and an already-placed, higher-indexed block:
                // once one column's own pair got placed, it wrongly treated
                // the still-unplaced 1x2 item of the *other* column as
                // redundant merely for sharing a width and x with the
                // already-placed 1x1 item there, and skipped placing it -
                // wrongly proving this instance infeasible.
                fs::path("data") / "rectangle" / "tests" / "feasibility_two_tight_column_stacks" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_two_tight_column_stacks" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_two_tight_column_stacks" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_two_tight_column_stacks" / "solution.csv",
            }, {
                // Regression test found by fuzzing the solver's CLI with
                // guaranteed-feasible instances (via a random skyline
                // placer) and looking for a wrong 'IsProvenInfeasible': a
                // 2x6 bin exactly tiled by five 1x2 items and two 1x1 items
                // - genuinely feasible (e.g. three 1x2 items in one column,
                // the other two 1x2's plus the two 1x1's in the other) but,
                // like 'feasibility_two_tight_column_stacks' above, wrongly
                // proven infeasible by the same missing-height-check bug in
                // 'ycheck::search''s own fathoming 4.
                fs::path("data") / "rectangle" / "tests" / "feasibility_fuzzer_found_column_partition" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_fuzzer_found_column_partition" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_fuzzer_found_column_partition" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "feasibility_fuzzer_found_column_partition" / "solution.csv",
            }, {
                // Resources have no CSV representation (see 'instance_path'
                // above), so this and the next two fixtures are read from
                // JSON instead. Same fixture as
                // 'benders_decomposition_test.cpp''s own
                // 'knapsack_resource_capacity_one_item' entry (single-bin
                // 'Knapsack', matching 'benders_decomposition_contiguity''s
                // own single-bin requirement): a capacity-1 resource with
                // each item consuming 1 caps the (single) bin to just 1
                // item, even though it is geometrically large enough to fit
                // all 3 - exercising 'add_resource_constraints' (see
                // 'benders_decomposition_contiguity.cpp') directly, rather
                // than through the domain's own algorithm-selection
                // dispatcher.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_one_item" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_one_item" / "instance.json",
            }, {
                // A capacity-1 resource with each item consuming 1 makes
                // packing both items impossible, even though the bin is
                // geometrically large enough to fit them side by side - the
                // 'Feasibility' objective requires every item packed, so the
                // instance as a whole is infeasible.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(""),
                fs::path("data") / "rectangle" / "tests" / "feasibility_resource_capacity_infeasible" / "instance.json",
            }, {
                // Item profit 10, resource capacity 1 with penalty 100.
                // Unlike 'tree_search_test.cpp''s own
                // 'knapsack_penalize_resource_uniform' fixture (whose
                // uniform, length-1 consumption schedule only
                // 'tree_search'/'Solution::update_indicators' need),
                // 'benders_decomposition_contiguity''s own MILP
                // encoding of a 'penalize' resource is restricted to the
                // same 'threshold_schedule(N)' shape as
                // 'onedimensional::add_penalize_resource_constraints' (see
                // 'add_resource_constraints' in
                // 'benders_decomposition_contiguity.cpp') - N ones (here
                // N = 2, one per copy) followed by an explicit trailing zero
                // - so both copies are set explicitly rather than relying on
                // a single-entry schedule's implicit repeat. Packing a 2nd
                // item in the single available bin crosses the resource
                // once (-100), so the solver should prefer packing just 1
                // item (profit 10) over 2 (profit 20 - 100 = -80).
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_penalize_resource" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_penalize_resource" / "instance.json",
            }}));
