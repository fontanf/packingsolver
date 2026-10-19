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

}

TEST(IrregularSequentialFeasibility, WarmStartFromPreviousSolution)
{
    // The first sub-problem has no warm start. The mock solver packs the
    // three items side by side (length 30). The second sub-problem must
    // receive this solution, as a solution of the original instance, as warm
    // start, unchanged, although its strip is shorter (29.7), so that the last
    // item sticks out of it.
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
            for (LengthDbl x: {0.0, 10.0, 20.0})
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
    ASSERT_TRUE(solver_calls[1].has_initial_solution);
    EXPECT_TRUE(solver_calls[1].initial_solution_of_original_instance);
    EXPECT_NEAR(solver_calls[1].bin_x_max, 29.7, 1e-6);
    EXPECT_EQ(solver_calls[1].initial_solution_number_of_bins, 1);
    EXPECT_EQ(
            solver_calls[1].initial_solution_item_xs,
            std::vector<LengthDbl>({0.0, 10.0, 20.0}));
    EXPECT_NEAR(output.solution_pool.best().x_max(), 30, 1e-6);
}

TEST(IrregularSequentialFeasibility, WarmStartBinPacking)
{
    // Three 10x10 bins, three 10x10 items. The mock solver puts one item
    // per bin. The second sub-problem has two bins, but its warm start is the
    // previous solution unchanged, with three bins.
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
            for (BinPos copy = 0; copy < 3; ++copy) {
                BinPos bin_pos = solution_builder.add_bin(0, 1);
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
    EXPECT_EQ(solver_calls[0].number_of_bins, 3);
    EXPECT_FALSE(solver_calls[0].has_initial_solution);
    EXPECT_EQ(solver_calls[1].number_of_bins, 2);
    ASSERT_TRUE(solver_calls[1].has_initial_solution);
    EXPECT_TRUE(solver_calls[1].initial_solution_of_original_instance);
    EXPECT_EQ(solver_calls[1].initial_solution_number_of_bins, 3);
    EXPECT_EQ(solver_calls[1].initial_solution_item_xs.size(), 3);
}

TEST(IrregularSequentialFeasibility, Compression)
{
    // The mock solver packs the three items side by side (length 30) in the
    // first sub-problem, then fails. With the compression, the next
    // sub-problems shrink the best solution by smaller and smaller ratios
    // (half of the shrink ratio, then multiplied by 0.9), warm-started from
    // the best solution.
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
            for (LengthDbl x: {0.0, 10.0, 20.0})
                solution_builder.add_item(bin_pos, 0, {x, 0}, 0, false);
            solution_pool.add(solution_builder.build(), "mock");
        }
        return solution_pool;
    };

    SequentialFeasibilityParameters parameters;
    parameters.verbosity_level = 0;
    parameters.compression = true;
    SequentialFeasibilityOutput output = sequential_feasibility(
            instance,
            solver,
            parameters);

    ASSERT_GE(solver_calls.size(), 4);
    EXPECT_NEAR(solver_calls[1].bin_x_max, 30 * (1 - 0.01), 1e-6);
    EXPECT_NEAR(solver_calls[2].bin_x_max, 30 * (1 - 0.005), 1e-6);
    EXPECT_NEAR(solver_calls[3].bin_x_max, 30 * (1 - 0.0045), 1e-6);
    for (Counter call_pos = 1; call_pos < (Counter)solver_calls.size(); ++call_pos) {
        ASSERT_TRUE(solver_calls[call_pos].has_initial_solution);
        EXPECT_EQ(
                solver_calls[call_pos].initial_solution_item_xs,
                std::vector<LengthDbl>({0.0, 10.0, 20.0}));
    }
    // The compression stops once the ratio gets below the minimum.
    EXPECT_LT(solver_calls.size(), 100);
    EXPECT_NEAR(output.solution_pool.best().x_max(), 30, 1e-6);
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
