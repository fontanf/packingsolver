#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/sequential_feasibility.hpp"
#include "irregular/local_search.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

Shape rectangle_shape(LengthDbl width, LengthDbl height)
{
    return shape::build_rectangle(0, width, 0, height);
}

std::vector<ItemShape> rectangle_item_shape(LengthDbl width, LengthDbl height)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = rectangle_shape(width, height);
    return {item_shape};
}

/** What the mock solver received for one sub-problem. */
struct SolverCall
{
    /** Number of bins of the sub-instance. */
    BinPos number_of_bins = 0;

    /** x_max of the first bin type of the sub-instance. */
    LengthDbl bin_x_max = 0;

    /** Whether a warm start was passed. */
    bool has_initial_solution = false;

    /** Whether the warm start is a solution of the original instance. */
    bool initial_solution_of_original_instance = false;

    /** Number of bins of the warm start. */
    BinPos initial_solution_number_of_bins = 0;

    /** x of the bottom-left corner of the items of the warm start. */
    std::vector<LengthDbl> initial_solution_item_xs;
};

SolverCall record_call(
        const Instance& instance,
        const Instance& sub_instance,
        const Solution* initial_solution)
{
    SolverCall solver_call;
    solver_call.number_of_bins = sub_instance.number_of_bins();
    solver_call.bin_x_max = sub_instance.bin_type(0).aabb_orig.x_max;
    solver_call.has_initial_solution = (initial_solution != nullptr);
    if (initial_solution != nullptr) {
        solver_call.initial_solution_of_original_instance
            = (&initial_solution->instance() == &instance);
        solver_call.initial_solution_number_of_bins = initial_solution->number_of_bins();
        for (BinPos bin_pos = 0;
                bin_pos < initial_solution->number_of_different_bins();
                ++bin_pos) {
            for (const SolutionItem& item: initial_solution->bin(bin_pos).items)
                solver_call.initial_solution_item_xs.push_back(item.bl_corner.x);
        }
    }
    return solver_call;
}

/** Strip 100x10 with three 10x10 items. */
Instance build_open_dimension_x_instance()
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(rectangle_shape(100, 10));
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 3);
    return instance_builder.build();
}

/**
 * Run the sequential feasibility with a solver which finds no solution: the
 * result is the column packing.
 */
Solution column_packing(const Instance& instance)
{
    SequentialFeasibilitySolver solver = [](
            const Instance& sub_instance,
            const Solution*)
    {
        return SolutionPool<Instance, Solution>(sub_instance, 1);
    };
    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    SequentialFeasibilityOutput output = sequential_feasibility(
            instance,
            solver,
            parameters);
    return output.solution_pool.best();
}

/**
 * Check that a solution contains all the items, and that no item intersects a
 * border, a defect or another item.
 */
void expect_full_and_no_overlaps(const Solution& solution)
{
    EXPECT_TRUE(solution.full());
    for (BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        Solution::OverlappingItems overlapping_items = solution.compute_overlapping_items(bin_pos);
        EXPECT_TRUE(overlapping_items.item_border_pairs.empty());
        EXPECT_TRUE(overlapping_items.item_defect_pairs.empty());
        EXPECT_TRUE(overlapping_items.item_item_pairs.empty());
        EXPECT_TRUE(overlapping_items.items_outside_bin.empty());
    }
}

}

TEST(IrregularSequentialFeasibility, WarmStartFromPreviousSolution)
{
    // The column packing places the three items side by side (length 30): the
    // first sub-problem starts from a shorter strip (29.7), without warm
    // start. The mock solver returns a solution of length 29. The second
    // sub-problem must receive this solution, as a solution of the original
    // instance, as warm start, unchanged, although its strip is shorter
    // (28.71), so that the last item sticks out of it.
    Instance instance = build_open_dimension_x_instance();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilitySolver solver = [&instance, &solver_calls](
            const Instance& sub_instance,
            const Solution* initial_solution)
    {
        solver_calls.push_back(record_call(instance, sub_instance, initial_solution));
        SolutionPool<Instance, Solution> solution_pool(sub_instance, 1);
        if (solver_calls.size() == 1) {
            SolutionBuilder solution_builder(sub_instance);
            BinPos bin_pos = solution_builder.add_bin(0, 1);
            for (LengthDbl x: {0.0, 10.0, 19.0})
                solution_builder.add_item(bin_pos, 0, {x, 0}, 0, false);
            solution_pool.add(solution_builder.build(), "mock");
        }
        return solution_pool;
    };

    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    SequentialFeasibilityOutput output = sequential_feasibility(
            instance,
            solver,
            parameters);

    ASSERT_EQ(solver_calls.size(), 2);
    EXPECT_FALSE(solver_calls[0].has_initial_solution);
    EXPECT_NEAR(solver_calls[0].bin_x_max, 29.7, 1e-6);
    ASSERT_TRUE(solver_calls[1].has_initial_solution);
    EXPECT_TRUE(solver_calls[1].initial_solution_of_original_instance);
    EXPECT_NEAR(solver_calls[1].bin_x_max, 28.71, 1e-6);
    EXPECT_EQ(solver_calls[1].initial_solution_number_of_bins, 1);
    EXPECT_EQ(
            solver_calls[1].initial_solution_item_xs,
            std::vector<LengthDbl>({0.0, 10.0, 19.0}));
    EXPECT_NEAR(output.solution_pool.best().x_max(), 29, 1e-6);
}

TEST(IrregularSequentialFeasibility, WarmStartBinPacking)
{
    // Three 10x10 bins, three 10x10 items. The column packing puts one item
    // per bin: the first sub-problem has two bins, without warm start. The
    // mock solver returns a solution with two bins. The second sub-problem has
    // one bin, but its warm start is this solution unchanged, with two bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(10, 10));
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilitySolver solver = [&instance, &solver_calls](
            const Instance& sub_instance,
            const Solution* initial_solution)
    {
        solver_calls.push_back(record_call(instance, sub_instance, initial_solution));
        SolutionPool<Instance, Solution> solution_pool(sub_instance, 1);
        if (solver_calls.size() == 1) {
            SolutionBuilder solution_builder(sub_instance);
            for (BinPos copy = 0; copy < 2; ++copy) {
                BinPos bin_pos = solution_builder.add_bin(0, 1);
                solution_builder.add_item(bin_pos, 0, {0, 0}, 0, false);
                if (copy == 0)
                    solution_builder.add_item(bin_pos, 0, {0, 0}, 0, false);
            }
            solution_pool.add(solution_builder.build(), "mock");
        }
        return solution_pool;
    };

    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    sequential_feasibility(
            instance,
            solver,
            parameters);

    ASSERT_EQ(solver_calls.size(), 2);
    EXPECT_EQ(solver_calls[0].number_of_bins, 2);
    EXPECT_FALSE(solver_calls[0].has_initial_solution);
    EXPECT_EQ(solver_calls[1].number_of_bins, 1);
    ASSERT_TRUE(solver_calls[1].has_initial_solution);
    EXPECT_TRUE(solver_calls[1].initial_solution_of_original_instance);
    EXPECT_EQ(solver_calls[1].initial_solution_number_of_bins, 2);
    EXPECT_EQ(solver_calls[1].initial_solution_item_xs.size(), 3);
}

TEST(IrregularSequentialFeasibility, BinPackingWithLeftoversRestrictsLastBin)
{
    // Three 30x10 bins, four 10x10 items. The mock solver packs three items in
    // a first bin and one item in a second bin. Only the last bin of the next
    // sub-problems is restricted: it is a bin type of its own, after the
    // full-size bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(30, 10));
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 4);
    Instance instance = instance_builder.build();

    struct LeftoversCall
    {
        std::vector<LengthDbl> bin_types_x_max;
        std::vector<BinPos> bin_types_copies;
        std::vector<BinTypeId> initial_solution_bin_type_ids;
    };
    std::vector<LeftoversCall> calls;
    SequentialFeasibilitySolver solver = [&calls](
            const Instance& sub_instance,
            const Solution* initial_solution)
    {
        LeftoversCall call;
        for (BinTypeId sub_bin_type_id = 0;
                sub_bin_type_id < sub_instance.number_of_bin_types();
                ++sub_bin_type_id) {
            call.bin_types_x_max.push_back(sub_instance.bin_type(sub_bin_type_id).aabb_orig.x_max);
            call.bin_types_copies.push_back(sub_instance.bin_type(sub_bin_type_id).copies);
        }
        if (initial_solution != nullptr) {
            Solution converted_solution = convert_initial_solution(sub_instance, *initial_solution);
            for (BinPos bin_pos = 0;
                    bin_pos < converted_solution.number_of_different_bins();
                    ++bin_pos) {
                call.initial_solution_bin_type_ids.push_back(
                        converted_solution.bin(bin_pos).bin_type_id);
            }
        }
        calls.push_back(call);

        SolutionPool<Instance, Solution> solution_pool(sub_instance, 1);
        if (calls.size() <= 2) {
            // Three items in a full-size bin, one in the last bin.
            SolutionBuilder solution_builder(sub_instance);
            BinPos bin_pos_1 = solution_builder.add_bin(0, 1);
            for (LengthDbl x: {0.0, 10.0, 20.0})
                solution_builder.add_item(bin_pos_1, 0, {x, 0}, 0, false);
            BinPos bin_pos_2 = solution_builder.add_bin(sub_instance.number_of_bin_types() - 1, 1);
            solution_builder.add_item(bin_pos_2, 0, {0, 0}, 0, false);
            solution_pool.add(solution_builder.build(), "mock");
        }
        return solution_pool;
    };

    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    sequential_feasibility(instance, solver, parameters);

    ASSERT_GE(calls.size(), 3);
    const LeftoversCall& call = calls[2];
    ASSERT_EQ(call.bin_types_x_max.size(), 2);
    EXPECT_NEAR(call.bin_types_x_max[0], 30, 1e-6);
    EXPECT_EQ(call.bin_types_copies[0], 1);
    EXPECT_NEAR(call.bin_types_x_max[1], 0.99 * 10, 1e-6);
    EXPECT_EQ(call.bin_types_copies[1], 1);
    // The last bin of the previous solution goes to the restricted bin.
    EXPECT_EQ(call.initial_solution_bin_type_ids, std::vector<BinTypeId>({0, 1}));
}

TEST(IrregularSequentialFeasibility, StopsWhenAFixedItemWouldBeOutOfTheBin)
{
    // Strip 100x10 with three 10x10 items and a 10x10 fixed item at x = 40.
    // The column packing packs the three items on the left: the length of the
    // solution is the end of the fixed item, 50, and the next sub-problem
    // would cut the fixed item. The algorithm must stop.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(100, 10));
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 3);
    ItemTypeId fixed_item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.add_fixed_item(bin_type_id, fixed_item_type_id, {40, 0}, 0, false);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilitySolver solver = [&instance, &solver_calls](
            const Instance& sub_instance,
            const Solution* initial_solution)
    {
        solver_calls.push_back(record_call(instance, sub_instance, initial_solution));
        SolutionPool<Instance, Solution> solution_pool(sub_instance, 1);
        // Safeguard against an infinite loop.
        if (solver_calls.size() > 100)
            return solution_pool;
        SolutionBuilder solution_builder(sub_instance);
        BinPos bin_pos = solution_builder.add_bin(0, 1);
        solution_builder.add_item(bin_pos, 1, {40, 0}, 0, false, true);
        for (LengthDbl x: {0.0, 10.0, 20.0})
            solution_builder.add_item(bin_pos, 0, {x, 0}, 0, false);
        solution_pool.add(solution_builder.build(), "mock");
        return solution_pool;
    };

    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    SequentialFeasibilityOutput output = sequential_feasibility(
            instance,
            solver,
            parameters);

    EXPECT_EQ(solver_calls.size(), 0);
    EXPECT_NEAR(output.solution_pool.best().x_max(), 50, 1e-6);
}

TEST(IrregularSequentialFeasibility, ColumnPackingBinPackingWithLeftovers)
{
    // Bin 300x100, twenty 10x10 items. The column packing packs them in two
    // columns: the first sub-problem starts from its length, 20, instead of
    // the width of the bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.add_bin_type(rectangle_shape(300, 100));
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 20);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilitySolver solver = [&instance, &solver_calls](
            const Instance& sub_instance,
            const Solution* initial_solution)
    {
        solver_calls.push_back(record_call(instance, sub_instance, initial_solution));
        return SolutionPool<Instance, Solution>(sub_instance, 1);
    };
    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    SequentialFeasibilityOutput output = sequential_feasibility(
            instance,
            solver,
            parameters);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_NEAR(solver_calls[0].bin_x_max, 19.8, 1e-6);
    EXPECT_NEAR(output.solution_pool.best().x_max(), 20, 1e-6);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(IrregularSequentialFeasibility, ColumnPackingOpenDimensionY)
{
    // Strip of width 10, three 10x10 items: the items are packed in rows.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionY);
    instance_builder.add_bin_type(rectangle_shape(10, 100));
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    Solution solution = column_packing(instance);
    EXPECT_NEAR(solution.y_max(), 30, 1e-6);
    expect_full_and_no_overlaps(solution);
}

TEST(IrregularSequentialFeasibility, ColumnPackingDefect)
{
    // Bin 100x100 with a defect from y = 45 to y = 55, twenty 10x40 items:
    // in each column, the second item is packed above the defect.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(100, 100));
    instance_builder.set_bin_type_copies(bin_type_id, 2);
    instance_builder.add_defect(bin_type_id, -1, {shape::build_rectangle(0, 100, 45, 55)});
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 40));
    instance_builder.set_item_type_copies(item_type_id, 20);
    Instance instance = instance_builder.build();

    Solution solution = column_packing(instance);
    EXPECT_EQ(solution.number_of_bins(), 1);
    expect_full_and_no_overlaps(solution);
}

TEST(IrregularSequentialFeasibility, ColumnPackingBorders)
{
    // Triangular and circular bins: the items are packed inside.
    for (const Shape& bin_shape: {
            shape::build_triangle({0, 0}, {100, 0}, {0, 100}),
            shape::build_circle(50)}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::BinPacking);
        BinTypeId bin_type_id = instance_builder.add_bin_type(bin_shape);
        instance_builder.set_bin_type_copies(bin_type_id, 2);
        instance_builder.set_item_bin_minimum_spacing(bin_type_id, 1);
        ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
        instance_builder.set_item_type_copies(item_type_id, 15);
        Instance instance = instance_builder.build();

        Solution solution = column_packing(instance);
        EXPECT_EQ(solution.number_of_bins(), 1);
        expect_full_and_no_overlaps(solution);
    }
}

TEST(IrregularSequentialFeasibility, ColumnPackingFixedItems)
{
    // Bin 100x100 with a 50x50 fixed item in its bottom-left corner, twenty
    // 10x10 items: they are packed around the fixed item.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.set_item_item_minimum_spacing(1);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 20);
    ItemTypeId fixed_item_type_id = instance_builder.add_item_type(rectangle_item_shape(50, 50));
    instance_builder.add_fixed_item(bin_type_id, fixed_item_type_id, {0, 0}, 0, false);
    Instance instance = instance_builder.build();

    Solution solution = column_packing(instance);
    EXPECT_EQ(solution.number_of_bins(), 1);
    expect_full_and_no_overlaps(solution);
}

TEST(IrregularSequentialFeasibility, ColumnPackingFirstSubProblemFails)
{
    // Strip 100x10 with a defect from x = 0 to x = 40, three 10x10 items. The
    // column packing packs them after the defect (length 70): the first
    // sub-problem starts from 69.3, without warm start. The mock solver finds
    // no solution for it: the algorithm stops with the column packing.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(100, 10));
    instance_builder.add_defect(bin_type_id, -1, {shape::build_rectangle(0, 40, 0, 10)});
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    std::vector<SolverCall> solver_calls;
    SequentialFeasibilitySolver solver = [&instance, &solver_calls](
            const Instance& sub_instance,
            const Solution* initial_solution)
    {
        solver_calls.push_back(record_call(instance, sub_instance, initial_solution));
        return SolutionPool<Instance, Solution>(sub_instance, 1);
    };
    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    SequentialFeasibilityOutput output = sequential_feasibility(
            instance,
            solver,
            parameters);

    ASSERT_EQ(solver_calls.size(), 1);
    EXPECT_NEAR(solver_calls[0].bin_x_max, 69.3, 1e-6);
    EXPECT_FALSE(solver_calls[0].has_initial_solution);
    EXPECT_NEAR(output.solution_pool.best().x_max(), 70, 1e-6);
    expect_full_and_no_overlaps(output.solution_pool.best());
}

TEST(IrregularSequentialFeasibility, ColumnPackingInfeasibleUsesAllBins)
{
    // Ten triangular bins, two items of the same triangle: their bounding
    // boxes don't fit in the bins, so the column packing fails, and the first
    // sub-problem uses all the bins.
    for (Objective objective: {Objective::BinPacking, Objective::BinPackingWithLeftovers}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(objective);
        Shape triangle = shape::build_triangle({0, 0}, {100, 0}, {0, 100});
        BinTypeId bin_type_id = instance_builder.add_bin_type(triangle);
        instance_builder.set_bin_type_copies(bin_type_id, 10);
        ItemShape item_shape;
        item_shape.shape_orig.shape = triangle;
        ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
        instance_builder.set_item_type_copies(item_type_id, 2);
        Instance instance = instance_builder.build();

        std::vector<SolverCall> solver_calls;
        SequentialFeasibilitySolver solver = [&instance, &solver_calls](
                const Instance& sub_instance,
                const Solution* initial_solution)
        {
            solver_calls.push_back(record_call(instance, sub_instance, initial_solution));
            return SolutionPool<Instance, Solution>(sub_instance, 1);
        };
        SequentialFeasibilityParameters parameters;
        parameters.verbosity_level = 0;
        sequential_feasibility(instance, solver, parameters);

        ASSERT_EQ(solver_calls.size(), 1);
        EXPECT_EQ(solver_calls[0].number_of_bins, 10);
        EXPECT_FALSE(solver_calls[0].has_initial_solution);
        EXPECT_NEAR(solver_calls[0].bin_x_max, 100, 1e-6);
    }
}
