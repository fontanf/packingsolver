#include "packingsolver/box/instance_builder.hpp"
#include "box/sequential_feasibility.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::box;

namespace
{

/** What the mock solver received for one sub-problem. */
struct SolverCall
{
    /** Number of bins of the sub-instance. */
    BinPos number_of_bins = 0;

    /** Dimensions of the last bin of the sub-instance. */
    Box last_bin_box = {0, 0, 0};
};

/**
 * Run the sequential feasibility with a solver which finds no solution: the
 * result is the column packing, if it succeeds.
 */
SequentialFeasibilityOutput run(
        const Instance& instance,
        std::vector<SolverCall>& solver_calls)
{
    SequentialFeasibilitySolver solver = [&solver_calls](
            const Instance& sub_instance)
    {
        SolverCall solver_call;
        solver_call.number_of_bins = sub_instance.number_of_bins();
        solver_call.last_bin_box = sub_instance.bin_type(
                sub_instance.bin_type_id(sub_instance.number_of_bins() - 1)).box;
        solver_calls.push_back(solver_call);
        return SolutionPool<Instance, Solution>(sub_instance, 1);
    };
    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    return sequential_feasibility(instance, solver, parameters);
}

/**
 * Check that a solution contains all the items, and that no item intersects
 * another item or goes out of its bin.
 */
void expect_full_and_no_overlaps(const Solution& solution)
{
    const Instance& instance = solution.instance();
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.feasible());
    struct Space { Point start; Point end; };
    auto intersect = [](const Space& space_1, const Space& space_2)
    {
        return space_1.end.x > space_2.start.x && space_2.end.x > space_1.start.x
            && space_1.end.y > space_2.start.y && space_2.end.y > space_1.start.y
            && space_1.end.z > space_2.start.z && space_2.end.z > space_1.start.z;
    };
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        const BinType& bin_type = instance.bin_type(solution_bin.bin_type_id);
        std::vector<Space> spaces;
        for (const SolutionItem& item: solution_bin.items) {
            Box box = instance.item_type(item.item_type_id).box.rotate(item.rotation);
            Space space = {
                item.bl_corner,
                {item.bl_corner.x + box.x, item.bl_corner.y + box.y, item.bl_corner.z + box.z}};
            EXPECT_GE(space.start.x, 0);
            EXPECT_GE(space.start.y, 0);
            EXPECT_GE(space.start.z, 0);
            EXPECT_LE(space.end.x, bin_type.box.x);
            EXPECT_LE(space.end.y, bin_type.box.y);
            EXPECT_LE(space.end.z, bin_type.box.z);
            for (const Space& other: spaces)
                EXPECT_FALSE(intersect(space, other));
            spaces.push_back(space);
        }
    }
}

}

TEST(BoxSequentialFeasibility, ColumnPackingBinPackingWithLeftovers)
{
    // Bin 300x100x100, twenty 10x10x10 items. The column packing packs them
    // in two columns of ten items, in a single row: the first sub-problem
    // starts from its length, 10, shrunk, 9, instead of the length of the
    // bin, 300. The mock solver finds no solution for it: the result is the
    // column packing.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.add_bin_type(300, 100, 100);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10, 10);
    instance_builder.set_item_type_copies(item_type_id, 20);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].last_bin_box.x, 9);
    EXPECT_EQ(output.solution_pool.best().x_max(), 10);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(BoxSequentialFeasibility, ColumnPackingOpenDimensionZ)
{
    // Bin 10x10x100, three 10x10x10 items: the rows follow each other along
    // z.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionZ);
    instance_builder.add_bin_type(10, 10, 100);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10, 10);
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].last_bin_box.z, 29);
    EXPECT_EQ(output.solution_pool.best().z_max(), 30);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(BoxSequentialFeasibility, ColumnPackingRotation)
{
    // Bin 100x50x50, three 40x10x10 items with all the rotations: they are
    // packed with their length 10 along x, so the length used is 10 instead
    // of 40.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(100, 50, 50);
    ItemTypeId item_type_id = instance_builder.add_item_type(40, 10, 10);
    for (Rotation rotation: {
            Rotation::XYZ, Rotation::YXZ, Rotation::ZYX,
            Rotation::YZX, Rotation::XZY, Rotation::ZXY}) {
        instance_builder.add_item_type_rotation(item_type_id, rotation);
    }
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    EXPECT_EQ(output.solution_pool.best().x_max(), 10);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(BoxSequentialFeasibility, ColumnPackingMaximumWeight)
{
    // Two 100x100x100 bins of maximum weight 10, two 10x10x10 items of weight
    // 6: the column packing packs them in two bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.set_bin_type_maximum_weight(bin_type_id, 10);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10, 10);
    instance_builder.set_item_type_copies(item_type_id, 2);
    instance_builder.set_item_type_weight(item_type_id, 6);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 2);
    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].number_of_bins, 1);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(BoxSequentialFeasibility, ColumnPackingUnsupportedUsesAllBins)
{
    // Five bins with a resource, which the column packing doesn't handle: the
    // first sub-problem uses all the bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 5);
    instance_builder.add_bin_type_resource(bin_type_id, 10);
    instance_builder.add_item_type(10, 10, 10);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].number_of_bins, 5);
}
