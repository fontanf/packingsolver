#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::box;

TEST(BoxDualFeasibleFunctions, InfeasibleRotationTwoAxisSwap)
{
    // Bin 200x100x100. Four copies of an item 55x70x100, allowed to rotate
    // by swapping its x/y sides (only) - its z side always matches the
    // bin's z exactly, so this reduces to the same 2D case validated for
    // rectangle (55x70 unoriented in a 200x100 bin): both x/y sides exceed
    // half the bin's y-height (50) regardless of rotation, so it is
    // provably infeasible for a single bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(200, 100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(55, 70, 100);
    instance_builder.add_item_type_rotation(item_type_id, Rotation::YXZ);
    instance_builder.set_item_type_copies(item_type_id, 4);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.timer.set_time_limit(0.0);
    box::Output output = optimize(instance, optimize_parameters);

    EXPECT_TRUE(output.is_proven_infeasible);
}

TEST(BoxDualFeasibleFunctions, FeasibleRotationTwoAxisSwap)
{
    // Same bin and item as above, but only 2 copies: they fit side by side
    // along the bin's x-axis (2 * 70 = 140 <= 200, 55 <= 100) - genuinely
    // feasible, must not be flagged.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(200, 100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(55, 70, 100);
    instance_builder.add_item_type_rotation(item_type_id, Rotation::YXZ);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    box::Output output = optimize(instance, optimize_parameters);

    EXPECT_FALSE(output.is_proven_infeasible);
    EXPECT_EQ(output.solution_pool.best().number_of_items(), 2);
}

TEST(BoxDualFeasibleFunctions, InfeasibleFullRotationFreedom)
{
    // Bin 100x100x100 (cube). Two copies of an oriented-looking 60x60x60
    // cube item with *all* 6 rotations allowed (rotation is a no-op for a
    // cube, but exercises the full rotation-enumeration code path): both
    // dimensions exceed half the bin in every axis, so at most one fits,
    // regardless of the (here, irrelevant) rotation freedom.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(60, 60, 60);
    for (Rotation rotation: {Rotation::YXZ, Rotation::ZYX, Rotation::YZX, Rotation::XZY, Rotation::ZXY})
        instance_builder.add_item_type_rotation(item_type_id, rotation);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.timer.set_time_limit(0.0);
    box::Output output = optimize(instance, optimize_parameters);

    EXPECT_TRUE(output.is_proven_infeasible);
}

TEST(BoxDualFeasibleFunctions, InfeasibleSingleRotationUnchanged)
{
    // Regression check: the original (pre-rotation-aware) fully-oriented
    // case still works. Bin 100x100x100, two oriented 60x60x60 items.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Feasibility);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 1);
    ItemTypeId item_type_id = instance_builder.add_item_type(60, 60, 60);
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.timer.set_time_limit(0.0);
    box::Output output = optimize(instance, optimize_parameters);

    EXPECT_TRUE(output.is_proven_infeasible);
}
