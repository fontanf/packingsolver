#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "irregular/benders_decomposition.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Add an item type 'width' x 'height', which can't be rotated.
 */
ItemTypeId add_rectangle_item_type(
        InstanceBuilder& instance_builder,
        LengthDbl width,
        LengthDbl height,
        Profit profit,
        ItemPos copies)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = shape::build_rectangle(0, width, 0, height);
    ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    instance_builder.set_item_type_profit(item_type_id, profit);
    instance_builder.set_item_type_copies(item_type_id, copies);
    return item_type_id;
}

OptimizeParameters benders_decomposition_parameters(bool use_tree_search_relative_positions)
{
    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = OptimizationMode::NotAnytimeSequential;
    parameters.use_benders_decomposition = true;
    parameters.benders_decomposition_use_tree_search_relative_positions = use_tree_search_relative_positions;
    return parameters;
}

}

/*
 * Three 60 x 40 items in 100 x 100 bins: by area, they fit in a single bin,
 * but two of them can't be side by side, so they need two bins.
 */

TEST(IrregularBendersDecomposition, BinPacking)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    add_rectangle_item_type(instance_builder, 60, 40, 1, 3);
    Instance instance = instance_builder.build();

    irregular::Output output = optimize(instance, benders_decomposition_parameters(true));
    EXPECT_EQ(output.solution_pool.best().feasible(), true);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 2);
    EXPECT_EQ(output.bin_packing_bound, 2);
}

TEST(IrregularBendersDecomposition, BinPackingWithoutTreeSearchRelativePositions)
{
    // Without the tree search on relative positions, the subproblems don't
    // prove that the bins are infeasible, but the solution is still found.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    add_rectangle_item_type(instance_builder, 60, 40, 1, 3);
    Instance instance = instance_builder.build();

    irregular::Output output = optimize(instance, benders_decomposition_parameters(false));
    EXPECT_EQ(output.solution_pool.best().feasible(), true);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 2);
}

TEST(IrregularBendersDecomposition, Knapsack)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    add_rectangle_item_type(instance_builder, 60, 40, 10, 3);
    Instance instance = instance_builder.build();

    irregular::Output output = optimize(instance, benders_decomposition_parameters(true));
    EXPECT_EQ(output.solution_pool.best().profit(), 20);
    EXPECT_EQ(output.knapsack_bound, 20);
}

TEST(IrregularBendersDecomposition, VariableSizedBinPacking)
{
    // A 100 x 50 bin (cost 4) holds two 60 x 40 items by area, but only one
    // geometrically; a 100 x 100 bin (cost 10) holds two. The optimal
    // solution uses three 100 x 50 bins.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::VariableSizedBinPacking);
    BinTypeId large_bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    instance_builder.set_bin_type_cost(large_bin_type_id, 10);
    instance_builder.set_bin_type_copies(large_bin_type_id, 3);
    BinTypeId small_bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 50));
    instance_builder.set_bin_type_cost(small_bin_type_id, 4);
    instance_builder.set_bin_type_copies(small_bin_type_id, 3);
    add_rectangle_item_type(instance_builder, 60, 40, 1, 3);
    Instance instance = instance_builder.build();

    irregular::Output output = optimize(instance, benders_decomposition_parameters(true));
    EXPECT_EQ(output.solution_pool.best().feasible(), true);
    EXPECT_EQ(output.solution_pool.best().cost(), 12);
    EXPECT_EQ(output.variable_sized_bin_packing_bound, 12);
}

TEST(IrregularBendersDecomposition, KnapsackExact)
{
    // In 'Exact' mode, the automatic selection runs the Benders
    // decomposition for a single bin knapsack, which proves the bound.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    add_rectangle_item_type(instance_builder, 60, 40, 10, 3);
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = OptimizationMode::Exact;
    irregular::Output output = optimize(instance, parameters);
    EXPECT_EQ(output.solution_pool.best().profit(), 20);
    EXPECT_EQ(output.knapsack_bound, 20);
}

TEST(IrregularColumnGeneration, BinPackingDualPricing)
{
    // Five 60 x 40 items in 100 x 100 bins: by area, they fit in two bins,
    // but a bin holds at most two of them. The 'Dual' pricing (a Benders
    // decomposition with the tree search on relative positions) proves it,
    // so the bound of the column generation is 3.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    instance_builder.set_bin_type_copies(bin_type_id, 5);
    add_rectangle_item_type(instance_builder, 60, 40, 1, 5);
    Instance instance = instance_builder.build();

    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.use_column_generation = true;
    irregular::Output output = optimize(instance, parameters);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 3);
    EXPECT_EQ(output.bin_packing_bound, 3);
}

TEST(IrregularBendersDecomposition, KnapsackGoal)
{
    // With a knapsack goal of 25, the bound 20 proves that it can't be
    // reached; with a knapsack goal of 15, a solution of profit 20 reaches
    // it.
    for (Profit knapsack_goal: {25.0, 15.0}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::Knapsack);
        instance_builder.set_knapsack_goal(knapsack_goal);
        instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
        add_rectangle_item_type(instance_builder, 60, 40, 10, 3);
        Instance instance = instance_builder.build();

        irregular::Output output(instance);
        output.knapsack_bound = 30;
        EXPECT_FALSE(output.is_proven_optimal());
        if (knapsack_goal > 20) {
            output.knapsack_bound = 20;
            EXPECT_TRUE(output.is_proven_optimal());
        } else {
            output = optimize(instance, benders_decomposition_parameters(true));
            EXPECT_GE(output.solution_pool.best().profit(), 15);
            EXPECT_TRUE(output.is_proven_optimal());
        }
    }
}

TEST(IrregularBendersDecomposition, IncompatiblePairsAndTriples)
{
    // In a 100 x 100 bin, two 60 x 60 items don't fit together; a 60 x 60
    // item and a 60 x 40 item fit, but not with another 60 x 40 item; two
    // 60 x 40 items fit, but not three: one pair and two triples are
    // incompatible, and three bins are needed.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(shape::build_rectangle(0, 100, 0, 100));
    instance_builder.set_bin_type_copies(bin_type_id, 4);
    add_rectangle_item_type(instance_builder, 60, 60, 1, 2);
    add_rectangle_item_type(instance_builder, 60, 40, 1, 3);
    Instance instance = instance_builder.build();

    BendersDecompositionParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = OptimizationMode::NotAnytimeSequential;
    parameters.subproblem_use_tree_search_relative_positions = true;
    parameters.compute_incompatible_pairs = true;
    parameters.compute_incompatible_triples = true;
    BendersDecompositionOutput output = benders_decomposition(instance, parameters);
    EXPECT_EQ(output.number_of_incompatible_pairs, 1);
    EXPECT_EQ(output.number_of_incompatible_triples, 2);
    EXPECT_EQ(output.solution_pool.best().number_of_bins(), 3);
    EXPECT_EQ(output.bin_packing_bound, 3);
}
