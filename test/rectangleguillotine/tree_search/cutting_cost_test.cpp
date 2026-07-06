#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/tree_search.hpp"

#include "treesearchsolver/iterative_beam_search_2.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, CuttingCostSingleItem)
{
    /**
     * A single item exactly filling the bin: no cuts needed, only the bin
     * cost applies.
     */
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingCuttingCost);
    instance_builder.set_number_of_stages(3);
    instance_builder.set_cut_type(CutType::NonExact);
    instance_builder.add_item_type(100, 100, -1, 1, false, 0);
    instance_builder.add_bin_type(100, 100, 1, -1, 0);
    instance_builder.set_fixed_cutting_cost(0, 10);
    instance_builder.set_variable_cutting_cost(0, 1);
    for (Counter stage_id = 1; stage_id <= 4; ++stage_id) {
        instance_builder.set_fixed_cutting_cost(stage_id, 5);
        instance_builder.set_variable_cutting_cost(stage_id, 0.01);
    }
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);
    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    Solution solution = branching_scheme.to_solution(output.solution_pool.best());

    EXPECT_TRUE(solution.full());
    // Bin cost only: fixed + variable * area = 10 + 1 * (100 * 100).
    EXPECT_EQ(solution.cutting_cost(), 10 + 1 * 100 * 100);
}

TEST(RectangleGuillotineBranchingScheme, CuttingCostTwoColumns)
{
    /**
     * Two items exactly filling one column each: one real 1-cut splits the
     * bin into the two columns, each item exactly fills its column (no
     * further 2-cut/3-cut needed).
     *
     * |-----------------|-----------------|
     * |                 |                 |
     * |        0        |        1        | 100
     * |                 |                 |
     * |-----------------|-----------------|
     *                 100               200
     */
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingCuttingCost);
    instance_builder.set_number_of_stages(3);
    instance_builder.set_cut_type(CutType::NonExact);
    instance_builder.add_item_type(100, 100, -1, 2, false, 0);
    instance_builder.add_bin_type(200, 100, 1, -1, 0);
    instance_builder.set_fixed_cutting_cost(0, 10);
    instance_builder.set_variable_cutting_cost(0, 1);
    instance_builder.set_fixed_cutting_cost(1, 5);
    instance_builder.set_variable_cutting_cost(1, 1);
    for (Counter stage_id = 2; stage_id <= 4; ++stage_id) {
        instance_builder.set_fixed_cutting_cost(stage_id, 5);
        instance_builder.set_variable_cutting_cost(stage_id, 1);
    }
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters branching_scheme_parameters;
    BranchingScheme branching_scheme(instance, branching_scheme_parameters);
    auto output = treesearchsolver::iterative_beam_search_2(branching_scheme);
    Solution solution = branching_scheme.to_solution(output.solution_pool.best());

    EXPECT_TRUE(solution.full());
    // Bin cost: 10 + 1 * (200 * 100) = 20010.
    // One real 1-cut: 5 + 1 * 100 (bin height) = 105.
    // No 2-cut/3-cut: each item exactly fills its column.
    EXPECT_EQ(solution.cutting_cost(), 20010 + 105);
}
