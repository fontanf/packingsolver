#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/benders_decomposition.hpp"
#include "rectangle/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
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
    rectangle::Output output = optimize(instance, optimize_parameters);

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
            }, {
                // Same fixture as 'benders_decomposition_contiguity_test.cpp'
                // ('knapsack_resource_capacity_one_item'): a capacity-1
                // resource with each item consuming 1 caps the (single) bin
                // to just 1 item, even though it is geometrically large
                // enough to fit all 3 - unlike
                // 'bin_packing_resource_capacity' above (multiple bins,
                // 'BinPacking'), this is a single-bin 'Knapsack' instance,
                // checking that the master's own resource handling (see
                // 'build_master_instance' in 'benders_decomposition.cpp')
                // agrees with 'benders_decomposition_contiguity''s own
                // bespoke resource constraints on the same instance.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_one_item" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_one_item" / "instance.json",
            }}));

TEST(RectangleThresholdSchedule, Basic)
{
    // 'threshold_schedule(3)' should be exactly '{1, 1, 1, 0}': three
    // per-copy ones (the schedule is still growing) followed by the
    // trailing zero that caps the contribution at 'threshold' regardless
    // of how many further copies get packed (see 'threshold_schedule''s
    // own doc comment in 'benders_decomposition.hpp').
    std::vector<double> schedule = threshold_schedule(3);
    ASSERT_EQ(schedule.size(), 4);
    EXPECT_EQ(schedule[0], 1.0);
    EXPECT_EQ(schedule[1], 1.0);
    EXPECT_EQ(schedule[2], 1.0);
    EXPECT_EQ(schedule[3], 0.0);
}

namespace
{

/** The number of copies 'item_type_id' is capped to by 'lifted_cut'. */
ItemPos lifted_threshold(
        const ResourceCut& lifted_cut,
        ItemTypeId item_type_id)
{
    for (const std::pair<ItemTypeId, std::vector<double>>& entry: lifted_cut.consumption)
        if (entry.first == item_type_id)
            return (ItemPos)entry.second.size() - 1;
    return -1;
}

}

TEST(RectangleLiftNoGoodCut, LiftCoefficientsStayBoundedByCoverSize)
{
    // Regression test for a bug where 'lift_no_good_cut' re-derived its
    // '|C| - 1' constant (the function's own doc comment in
    // 'benders_decomposition.hpp' calls it 'cover_size') from a running
    // total that grew with every previously lifted item's own
    // coefficient, instead of keeping it fixed for the whole procedure.
    // That made each lift coefficient inflate the next one, without
    // bound: on a real 100-item instance this grew a single coefficient
    // past 2.6e8 within about 80 lifts, and the resulting
    // 'threshold_schedule' allocation (a vector of that many doubles) was
    // enough to exhaust memory and crash the solver.
    //
    // Bin and every item type here are exactly 10x10 (oriented, so a
    // single copy already fills the whole bin): forcing any one candidate
    // into the feasibility subproblem built for each lift leaves no bar
    // capacity for anything else, so the bar relaxation bound alongside
    // it is always exactly 0, regardless of what has already been lifted
    // into 'S'. With 'cover_size' correctly held fixed at 2 (the original
    // cover {A, B}'s thresholds sum to 2), every candidate should
    // therefore get the same coefficient ('cover_size - 1 - 0 == 1'); with
    // the bug, each one keeps compounding on the last instead ('m' growing
    // 2 -> 3 -> 5, coefficients 1 -> 2 -> 4).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10);
    ItemTypeId item_type_a = instance_builder.add_item_type(10, 10, true);
    ItemTypeId item_type_b = instance_builder.add_item_type(10, 10, true);
    ItemTypeId candidate_0 = instance_builder.add_item_type(10, 10, true);
    ItemTypeId candidate_1 = instance_builder.add_item_type(10, 10, true);
    ItemTypeId candidate_2 = instance_builder.add_item_type(10, 10, true);
    Instance instance = instance_builder.build();

    // The original cut: 'A' and 'B' cannot both be packed. This cover is
    // fabricated, not derived from an actual geometric proof -
    // 'lift_no_good_cut' takes the cut's soundness for granted and only
    // ever tightens it further, so nothing here needs to actually be
    // infeasible for the lifting logic itself to be exercised.
    ResourceCut original_cut;
    original_cut.capacity = 1.0;
    original_cut.consumption.push_back({item_type_a, threshold_schedule(1)});
    original_cut.consumption.push_back({item_type_b, threshold_schedule(1)});

    BendersDecompositionParameters parameters;
    ResourceCut lifted_cut = lift_no_good_cut(
            original_cut,
            instance,
            bin_type_id,
            parameters);

    EXPECT_EQ(lifted_threshold(lifted_cut, candidate_0), 1);
    EXPECT_EQ(lifted_threshold(lifted_cut, candidate_1), 1);
    EXPECT_EQ(lifted_threshold(lifted_cut, candidate_2), 1);
}
