#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/dual_feasible_functions.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangle;

TEST(RectangleDualFeasibleFunctions, InfeasibleNonSquareBinRotation)
{
    // Bin 200x100 (non-square). Four copies of an *unoriented* 55x70 item:
    // both dimensions exceed half the bin's height (50) regardless of
    // rotation, so this is provably infeasible for a single bin - but only
    // via the rotation-aware breakpoint sweep, not via the pre-existing
    // Clautiaux-doubling fallback, which pays a "squaring tax" on this
    // non-square bin (it inflates the height capacity from 100 to 200,
    // which is enough to hide this particular violation).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(200, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(55, 70, false);
    instance_builder.set_item_type_copies(item_type_id, 4);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.timer.set_time_limit(0.0);
    rectangle::Output output = optimize(instance, optimize_parameters);

    EXPECT_TRUE(output.is_proven_infeasible);
}

TEST(RectangleDualFeasibleFunctions, FeasibleNonSquareBinRotation)
{
    // Same bin and item as above, but only 2 copies: they fit side by side
    // along the width axis (2 * 70 = 140 <= 200, height 55 <= 100) -
    // genuinely feasible, must not be flagged.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(200, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(55, 70, false);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    rectangle::Output output = optimize(instance, optimize_parameters);

    EXPECT_FALSE(output.is_proven_infeasible);
    EXPECT_EQ(output.solution_pool.best().number_of_items(), 2);
}

TEST(RectangleDualFeasibleFunctions, CutCheckerFindsViolationOnNonSquareBin)
{
    // Same setup as InfeasibleNonSquareBinRotation, but exercising
    // find_most_violated_dual_feasible_function_cut() directly (the
    // checker meant to run before building a Benders subproblem): the
    // returned cut must be valid for the whole instance's item universe,
    // not just the exact candidate that revealed it.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(200, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(55, 70, false);
    instance_builder.set_item_type_copies(item_type_id, 4);
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = {{item_type_id, 4}};
    DualFeasibleFunctionsCut cut = find_most_violated_dual_feasible_function_cut(
            instance, selected_items);

    EXPECT_TRUE(cut.found);
    EXPECT_GT(cut.violation, 0.0);
    ASSERT_EQ((ItemTypeId)cut.coefficients.size(), instance.number_of_item_types());
    EXPECT_GT(cut.coefficients[item_type_id], 0.0);
}

TEST(RectangleDualFeasibleFunctions, CutCheckerNoViolationForFeasibleSelection)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(200, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(55, 70, false);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = {{item_type_id, 2}};
    DualFeasibleFunctionsCut cut = find_most_violated_dual_feasible_function_cut(
            instance, selected_items);

    EXPECT_FALSE(cut.found);
}

TEST(RectangleDualFeasibleFunctions, InfeasibleOrientedSquareBinUnchanged)
{
    // Regression check: the original (pre-rotation-aware) oriented-only
    // case still works. Bin 100x100, two oriented 60x60 items: both
    // dimensions exceed half the bin in both axes, so at most one fits.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(60, 60, true);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.timer.set_time_limit(0.0);
    rectangle::Output output = optimize(instance, optimize_parameters);

    EXPECT_TRUE(output.is_proven_infeasible);
}
