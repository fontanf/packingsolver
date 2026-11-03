#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/sequential_feasibility.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

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
 * result is the strip packing, if it succeeds.
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

}

TEST(RectangleGuillotineSequentialFeasibility, StripPackingBinPackingWithLeftovers)
{
    // Bin 300x100, twenty 10x10 items. The strip packing packs them in two
    // strips: the first sub-problem starts from its width, 20, shrunk, 19,
    // instead of the width of the bin. The mock solver finds no solution for
    // it: the result is the strip packing.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.add_bin_type(300, 100);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10);
    instance_builder.set_item_type_copies(item_type_id, 20);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].last_bin_rect.w, 19);
    const Solution& solution = output.solution_pool.best();
    EXPECT_EQ(solution.width(), 20);
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.feasible());
}

TEST(RectangleGuillotineSequentialFeasibility, StripPackingOpenDimensionY)
{
    // Bin 10x100, three 10x10 items: the first-stage cuts are horizontal.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionY);
    instance_builder.set_first_stage_orientation(CutOrientation::Any);
    instance_builder.add_bin_type(10, 100);
    ItemTypeId item_type_id = instance_builder.add_item_type(10, 10);
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].last_bin_rect.h, 29);
    const Solution& solution = output.solution_pool.best();
    EXPECT_EQ(solution.height(), 30);
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.feasible());
}

TEST(RectangleGuillotineSequentialFeasibility, StripPackingThirdStage)
{
    // Bin 100x40, a 10x20 item and an 8x20 item, oriented: they are packed
    // in the same strip, the second one with a third-stage cut.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(100, 40);
    instance_builder.add_item_type(10, 20, true);
    instance_builder.add_item_type(8, 20, true);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    const Solution& solution = output.solution_pool.best();
    EXPECT_EQ(solution.width(), 10);
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.feasible());
}

TEST(RectangleGuillotineSequentialFeasibility, StripPackingHardTrimsCutThickness)
{
    // Bin 100x100 with hard trims of 5 and cut thickness 2, four 40x30
    // items: two strips of two rotated items, in one bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.set_number_of_stages_unlimited();
    instance_builder.set_cut_thickness(2);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.add_trims(
            bin_type_id,
            5, TrimType::Hard,
            5, TrimType::Hard,
            5, TrimType::Hard,
            5, TrimType::Hard);
    ItemTypeId item_type_id = instance_builder.add_item_type(40, 30);
    instance_builder.set_item_type_copies(item_type_id, 4);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    const Solution& solution = output.solution_pool.best();
    EXPECT_EQ(solution.number_of_bins(), 1);
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.feasible());
    EXPECT_EQ(solver_calls.size(), 0);
}

TEST(RectangleGuillotineSequentialFeasibility, StripPackingSoftTrimsCutThickness)
{
    // Bin 100x100 with soft trims of 5 and cut thickness 2, four 44x44
    // items: they only fit inside the trims, after the waste of the left and
    // bottom trims and the cut separating it.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.set_number_of_stages_unlimited();
    instance_builder.set_cut_thickness(2);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.add_trims(
            bin_type_id,
            5, TrimType::Soft,
            5, TrimType::Soft,
            5, TrimType::Soft,
            5, TrimType::Soft);
    ItemTypeId item_type_id = instance_builder.add_item_type(44, 44);
    instance_builder.set_item_type_copies(item_type_id, 4);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilityOutput output = run(instance, solver_calls);

    const Solution& solution = output.solution_pool.best();
    EXPECT_EQ(solution.number_of_bins(), 1);
    EXPECT_TRUE(solution.full());
    EXPECT_TRUE(solution.feasible());
}

TEST(RectangleGuillotineSequentialFeasibility, StripPackingUnsupportedUsesAllBins)
{
    // Five bins with a defect, which the strip packing doesn't handle: the
    // first sub-problem uses all the bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(100, 100);
    instance_builder.set_bin_type_copies(bin_type_id, 5);
    instance_builder.add_defect(bin_type_id, 50, 50, 10, 10);
    instance_builder.add_item_type(10, 10);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    run(instance, solver_calls);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_EQ(solver_calls[0].number_of_bins, 5);
}
