#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "irregular/milp_nfp.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

struct MilpNfpTestItemType
{
    Shape shape;
    std::vector<Angle> angles;
};

Instance build_instance(
        LengthDbl bin_width,
        LengthDbl bin_height,
        const std::vector<MilpNfpTestItemType>& item_types)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(shape::build_rectangle(0, bin_width, 0, bin_height));
    for (const MilpNfpTestItemType& item_type: item_types) {
        ItemShape item_shape;
        item_shape.shape_orig.shape = item_type.shape;
        ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
        for (Angle angle: item_type.angles)
            instance_builder.add_item_type_allowed_rotation(item_type_id, angle, angle, false);
    }
    return instance_builder.build();
}

MilpNfpStatus status(
        const Instance& instance,
        const std::vector<ItemTypeId>& item_type_ids)
{
    MilpNfpParameters parameters;
    parameters.verbosity_level = 0;
    return milp_nfp(instance, 0, item_type_ids, parameters).status;
}

/**
 * Step of size 20 x 20: two copies, one of them rotated by 180 degrees, tile
 * a 20 x 30 rectangle, at a single relative position.
 */
Shape build_step()
{
    return shape::build_shape({{0, 0}, {20, 0}, {20, 20}, {10, 20}, {10, 10}, {0, 10}});
}

}

TEST(IrregularMilpNfp, SquaresTooLarge)
{
    // Two 55 x 55 squares don't fit in a 100 x 100 bin.
    Instance instance = build_instance(100, 100, {{shape::build_square(55), {0}}});
    EXPECT_EQ(status(instance, {0, 0}), MilpNfpStatus::Infeasible);
}

TEST(IrregularMilpNfp, SquaresExactFit)
{
    // Four 50 x 50 squares exactly fit in a 100 x 100 bin, five don't.
    Instance instance = build_instance(100, 100, {{shape::build_square(50), {0}}});
    EXPECT_EQ(status(instance, {0, 0}), MilpNfpStatus::Feasible);
    EXPECT_EQ(status(instance, {0, 0, 0, 0}), MilpNfpStatus::Feasible);
    EXPECT_EQ(status(instance, {0, 0, 0, 0, 0}), MilpNfpStatus::Infeasible);
}

TEST(IrregularMilpNfp, StepsInterlocked)
{
    // With the rotation by 180 degrees, two steps exactly fit in a 20 x 30
    // bin, interlocked.
    Instance instance = build_instance(20, 30, {{build_step(), {0, 180}}});
    EXPECT_EQ(status(instance, {0, 0}), MilpNfpStatus::Feasible);
}

TEST(IrregularMilpNfp, StepsWithoutRotation)
{
    // Without the rotation, two steps don't fit in a 20 x 30 bin.
    Instance instance = build_instance(20, 30, {{build_step(), {0}}});
    EXPECT_EQ(status(instance, {0, 0}), MilpNfpStatus::Infeasible);
}

TEST(IrregularMilpNfp, Crosses)
{
    // Every placement of a cross of span 70 in a 100 x 100 bin covers the
    // center of the bin, so two crosses don't fit, whereas one cross and a
    // 25 x 25 square in a corner fit.
    Shape cross = shape::build_shape({
            {20, 0}, {50, 0}, {50, 20}, {70, 20}, {70, 50}, {50, 50},
            {50, 70}, {20, 70}, {20, 50}, {0, 50}, {0, 20}, {20, 20}});
    Instance instance = build_instance(100, 100, {
            {cross, {0}},
            {shape::build_square(25), {0}}});
    EXPECT_EQ(status(instance, {0, 0}), MilpNfpStatus::Infeasible);
    EXPECT_EQ(status(instance, {0, 1}), MilpNfpStatus::Feasible);
}

TEST(IrregularMilpNfp, CoreWidening)
{
    // Two 60 x 40 rectangles can't be side by side in a 100 x 100 bin, so
    // each one is widened to 100 x 40; three of them don't fit, which the core
    // widening proves without a MILP. Two 60 x 50 rectangles fit exactly.
    Instance instance = build_instance(100, 100, {
            {shape::build_rectangle(0, 60, 0, 40), {0}},
            {shape::build_rectangle(0, 60, 0, 50), {0}}});
    MilpNfpParameters parameters;
    parameters.verbosity_level = 0;
    MilpNfpOutput output = milp_nfp(instance, 0, {0, 0, 0}, parameters);
    EXPECT_EQ(output.status, MilpNfpStatus::Infeasible);
    EXPECT_EQ(output.number_of_milps, 0);
    EXPECT_EQ(status(instance, {0, 0}), MilpNfpStatus::Feasible);
    EXPECT_EQ(status(instance, {1, 1}), MilpNfpStatus::Feasible);
}

TEST(IrregularMilpNfp, Symmetries)
{
    // The rotations by 0 and 180 degrees of a square are the same, and the
    // ones of the (single) step are symmetric through the bin: a single
    // rotation combination is left. The square fits in the notch of the
    // step.
    Instance instance = build_instance(20, 30, {
            {build_step(), {0, 180}},
            {shape::build_square(10), {0, 180}}});
    MilpNfpParameters parameters;
    parameters.verbosity_level = 0;
    MilpNfpOutput output = milp_nfp(instance, 0, {0, 1}, parameters);
    EXPECT_EQ(output.status, MilpNfpStatus::Feasible);
    EXPECT_EQ(output.number_of_rotation_combinations, 1);
}

TEST(IrregularMilpNfp, Packing)
{
    // When the relaxation is feasible, the positions of its solution are
    // returned; here, without exact fits, they are a packing.
    Instance instance = build_instance(100, 100, {
            {build_step(), {0, 180}},
            {shape::build_square(30), {0}},
            {shape::build_rectangle(0, 60, 0, 25), {0}}});
    MilpNfpParameters parameters;
    parameters.verbosity_level = 0;
    MilpNfpOutput output = milp_nfp(instance, 0, {0, 0, 1, 2}, parameters);
    ASSERT_EQ(output.status, MilpNfpStatus::Feasible);
    ASSERT_EQ(output.items.size(), 4);
    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(0, 1);
    for (const SolutionItem& item: output.items) {
        solution_builder.add_item(
                bin_pos,
                item.item_type_id,
                item.bl_corner,
                item.angle,
                item.mirror);
    }
    Solution solution = solution_builder.build();
    Solution::OverlappingItems overlapping_items = solution.compute_overlapping_items(bin_pos);
    EXPECT_TRUE(overlapping_items.item_item_pairs.empty());
    EXPECT_TRUE(overlapping_items.item_border_pairs.empty());
    EXPECT_TRUE(overlapping_items.items_outside_bin.empty());
}

TEST(IrregularMilpNfp, OptimizeExactInfeasible)
{
    // In 'Exact' mode, the automatic selection runs the MILP NFP algorithm for
    // a single bin feasibility problem, which proves that two 55 x 55 squares
    // don't fit in a 100 x 100 bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    ItemShape item_shape;
    item_shape.shape_orig.shape = shape::build_square(55);
    ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    instance_builder.set_item_type_copies(item_type_id, 2);
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = OptimizationMode::Exact;
    irregular::Output output = optimize(instance, parameters);
    EXPECT_TRUE(output.is_proven_infeasible);
}
