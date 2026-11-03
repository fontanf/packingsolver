#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/sequential_feasibility.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangle;

namespace
{

/** What the mock solver received for one sub-problem. */
struct SolverCall
{
    /** Number of bins of the sub-instance. */
    BinPos number_of_bins = 0;

    /** Dimensions of the last bin of the sub-instance. */
    Rectangle last_bin_rect = {0, 0};
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
        solver_call.last_bin_rect = sub_instance.bin_type(
                sub_instance.bin_type_id(sub_instance.number_of_bins() - 1)).rect;
        solver_calls.push_back(solver_call);
        return SolutionPool<Instance, Solution>(sub_instance, 1);
    };
    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    return sequential_feasibility(instance, solver, parameters);
}

/**
 * Check that a solution contains all the items, and that no item intersects
 * another item or a defect, or goes out of its bin.
 */
void expect_full_and_no_overlaps(const Solution& solution)
{
    const Instance& instance = solution.instance();
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.feasible());
    struct Box { Length x_start, x_end, y_start, y_end; };
    auto intersect = [](const Box& box_1, const Box& box_2)
    {
        return box_1.x_end > box_2.x_start && box_2.x_end > box_1.x_start
            && box_1.y_end > box_2.y_start && box_2.y_end > box_1.y_start;
    };
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        const BinType& bin_type = instance.bin_type(solution_bin.bin_type_id);
        std::vector<Box> boxes;
        for (const SolutionItem& item: solution_bin.items) {
            const ItemType& item_type = instance.item_type(item.item_type_id);
            Box box = {
                item.bl_corner.x, item.bl_corner.x + item_type.x(item.rotate),
                item.bl_corner.y, item.bl_corner.y + item_type.y(item.rotate)};
            EXPECT_GE(box.x_start, 0);
            EXPECT_GE(box.y_start, 0);
            EXPECT_LE(box.x_end, bin_type.rect.x);
            EXPECT_LE(box.y_end, bin_type.rect.y);
            for (const Box& other: boxes)
                EXPECT_FALSE(intersect(box, other));
            for (const Defect& defect: bin_type.defects) {
                Box defect_box = {
                    defect.pos.x, defect.pos.x + defect.rect.x,
                    defect.pos.y, defect.pos.y + defect.rect.y};
                EXPECT_FALSE(intersect(box, defect_box));
            }
            boxes.push_back(box);
        }
    }
}

}

TEST(RectangleSequentialFeasibility, ColumnPackingBinPackingWithLeftovers)
{
    // Bin 300x100, twenty 10x10 items. The column packing packs them in two
    // columns: the first sub-problem starts from its length, 20, shrunk, 19,
    // instead of the width of the bin. The mock solver finds no solution for
    // it: the result is the column packing.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.add_bin_type(300, 100);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10);
    instance_builder.set_item_type_copies(item_type_id, 20);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].last_bin_rect.x, 19);
    EXPECT_EQ(output.solution_pool.best().x_max(), 20);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(RectangleSequentialFeasibility, ColumnPackingOpenDimensionY)
{
    // Strip of width 10, three 10x10 items: the items are packed in rows.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionY);
    instance_builder.add_bin_type(10, 100);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10);
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].last_bin_rect.y, 29);
    EXPECT_EQ(output.solution_pool.best().y_max(), 30);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(RectangleSequentialFeasibility, ColumnPackingRotation)
{
    // Strip of height 50, three 40x10 items which can be rotated: they are
    // packed standing, one per column (length 30), instead of lying (40).
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(100, 50);
    ItemTypeId item_type_id = instance_builder.add_item_type(40, 10);
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    EXPECT_EQ(output.solution_pool.best().x_max(), 30);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(RectangleSequentialFeasibility, ColumnPackingDefect)
{
    // Bin 100x100 with a defect from y = 45 to y = 55, twenty 10x40 oriented
    // items: in each column, the second item is packed above the defect.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.add_defect(bin_type_id, 0, 45, 100, 10);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 40, true);
    instance_builder.set_item_type_copies(item_type_id, 20);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 1);
    EXPECT_EQ(solver_calls.size(), 0);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(RectangleSequentialFeasibility, ColumnPackingMaximumWeight)
{
    // Two 100x100 bins of maximum weight 10, two 10x10 items of weight 6: the
    // column packing packs them in two bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.set_bin_type_maximum_weight(bin_type_id, 10);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10);
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

TEST(RectangleSequentialFeasibility, ColumnPackingUnsupportedUsesAllBins)
{
    // Five bins and an item with an eligibility, which the column packing
    // doesn't handle: the first sub-problem uses all the bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 5);
    instance_builder.add_bin_type_eligibility(bin_type_id, 0);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10);
    instance_builder.set_item_type_eligibility(item_type_id, 0);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].number_of_bins, 5);
}
