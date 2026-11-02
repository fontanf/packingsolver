#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "irregular/tree_search_relative_positions.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

namespace
{

Instance read_instance(const std::string& name)
{
    InstanceBuilder instance_builder;
    instance_builder.read((fs::path("data") / "irregular" / "tests" / (name + ".json")).string());
    return instance_builder.build();
}

/**
 * Bound of an output, and open dimension of its best solution, for the open
 * dimension objectives.
 */
std::pair<LengthDbl, LengthDbl> open_dimension_bound_and_value(
        const Instance& instance,
        const irregular::Output& output)
{
    const Solution& solution = output.solution_pool.best();
    if (instance.objective() == Objective::OpenDimensionX)
        return {output.open_dimension_x_bound, solution.x_max()};
    return {output.open_dimension_y_bound, solution.y_max()};
}

}

struct IrregularTreeSearchRelativePositionsTestParams
{
    /** Name of the instance, in 'data/irregular/tests'. */
    std::string name;

    /** 'true' iff the instance is expected to be proven infeasible. */
    bool expected_proven_infeasible;

    /** For the open dimension objectives, expected bound. */
    LengthDbl expected_open_dimension_bound = 0;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularTreeSearchRelativePositionsTestParams& test_params)
{
    os << test_params.name;
    return os;
}

class IrregularTreeSearchRelativePositionsTest: public testing::TestWithParam<IrregularTreeSearchRelativePositionsTestParams> { };

TEST_P(IrregularTreeSearchRelativePositionsTest, IrregularTreeSearchRelativePositions)
{
    IrregularTreeSearchRelativePositionsTestParams test_params = GetParam();
    std::cout << "Instance: " << test_params.name << std::endl;
    Instance instance = read_instance(test_params.name);
    bool open_dimension = (instance.objective() == Objective::OpenDimensionX
            || instance.objective() == Objective::OpenDimensionY);

    // The same result is expected with a single thread and with several
    // ones.
    for (Counter number_of_threads: {1, 4}) {
        TreeSearchRelativePositionsParameters parameters;
        parameters.verbosity_level = 0;
        parameters.number_of_threads = number_of_threads;
        TreeSearchRelativePositionsOutput output = tree_search_relative_positions(instance, parameters);
        EXPECT_EQ(output.is_proven_infeasible, test_params.expected_proven_infeasible);
        if (!open_dimension)
            continue;
        std::pair<LengthDbl, LengthDbl> bound_and_value
            = open_dimension_bound_and_value(instance, output);
        EXPECT_NEAR(bound_and_value.first, test_params.expected_open_dimension_bound, 1e-4);
        // If the positions of the solution of the relaxation are a packing,
        // its open dimension is the bound.
        if (output.solution_pool.best().feasible())
            EXPECT_NEAR(bound_and_value.second, bound_and_value.first, 1e-4);
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularTreeSearchRelativePositionsTest,
        testing::ValuesIn(std::vector<IrregularTreeSearchRelativePositionsTestParams>{
            {
                // Two 55 x 55 squares don't fit in a 100 x 100 bin.
                "feasibility_squares_too_large",
                true
            }, {
                // Four 50 x 50 squares exactly fit in a 100 x 100 bin.
                "feasibility_squares_exact_fit",
                false
            }, {
                // Five 50 x 50 squares don't fit in a 100 x 100 bin.
                "feasibility_squares_one_too_many",
                true
            }, {
                // Step of size 20 x 20: with the rotation by 180 degrees, two
                // steps exactly fit in a 20 x 30 bin, interlocked.
                "feasibility_steps_interlocked",
                false
            }, {
                // Without the rotation, two steps don't fit in a 20 x 30 bin.
                "feasibility_steps_without_rotation",
                true
            }, {
                // Every placement of a cross of span 70 in a 100 x 100 bin
                // covers the center of the bin, so two crosses don't fit.
                "feasibility_crosses",
                true
            }, {
                // One cross and a 25 x 25 square in a corner fit.
                "feasibility_cross_and_square",
                false
            }, {
                // Two 60 x 40 rectangles can't be side by side in a 100 x 100
                // bin, so three of them don't fit.
                "feasibility_rectangles_core_bound",
                true
            }, {
                // Two 60 x 50 rectangles fit exactly in a 100 x 100 bin.
                "feasibility_rectangles_exact_fit",
                false
            }, {
                // The rotations by 0 and 180 degrees of a square are the
                // same, and the ones of the (single) step are symmetric
                // through the bin: a single rotation of each item is left.
                // The square fits in the notch of the step.
                "feasibility_step_and_square_symmetries",
                false
            }, {
                // Two steps, a 30 x 30 square and a 60 x 25 rectangle fit in a
                // 100 x 100 bin.
                "feasibility_steps_square_rectangle",
                false
            }, {
                // Five items of poly2a, with four rotations each: the
                // infeasible set needs many nodes.
                "feasibility_poly2a_five_items_infeasible",
                true
            }, {
                "feasibility_poly2a_five_items_feasible",
                false
            }, {
                // In a strip of height 30, two steps take a length of 20 with
                // the rotation by 180 degrees (interlocked).
                "open_dimension_x_steps_interlocked",
                false,
                20
            }, {
                // Without the rotation, they take a length of 30 (one in the
                // notch of the other).
                "open_dimension_x_steps_without_rotation",
                false,
                30
            }, {
                // Same as above, in a strip of width 30.
                "open_dimension_y_steps_interlocked",
                false,
                20
            }, {
                "open_dimension_y_steps_without_rotation",
                false,
                30
            }, {
                // The five first items of fu, without rotation, in a strip of
                // height 38.
                "open_dimension_x_fu_five_items",
                false,
                17.8889
            }}));

TEST(IrregularTreeSearchRelativePositions, CoreBound)
{
    // Two 60 x 40 rectangles can't be side by side in a 100 x 100 bin, so
    // each one is widened to 100 x 40; three of them don't fit, which the core
    // bound proves at the root. The subsets of one, two and three items are
    // each solved at the root.
    Instance instance = read_instance("feasibility_rectangles_core_bound");
    TreeSearchRelativePositionsParameters parameters;
    parameters.verbosity_level = 0;
    TreeSearchRelativePositionsOutput output = tree_search_relative_positions(instance, parameters);
    EXPECT_TRUE(output.is_proven_infeasible);
    EXPECT_EQ(output.number_of_nodes, 3);
}

TEST(IrregularTreeSearchRelativePositions, InfeasibleSubset)
{
    // The two 55 x 55 squares don't fit in the 100 x 100 bin: the search
    // stops at this subset of the largest items, without the 10 x 10 square.
    Instance instance = read_instance("feasibility_squares_too_large_and_small_square");
    TreeSearchRelativePositionsParameters parameters;
    parameters.verbosity_level = 0;
    TreeSearchRelativePositionsOutput output = tree_search_relative_positions(instance, parameters);
    EXPECT_TRUE(output.is_proven_infeasible);
    EXPECT_EQ(output.infeasible_subset, (std::vector<std::pair<ItemTypeId, ItemPos>>{{1, 2}}));
}

TEST(IrregularTreeSearchRelativePositions, Packing)
{
    // When the relaxation is feasible, the positions of its solution are
    // used; here, without exact fits, they are a packing.
    Instance instance = read_instance("feasibility_steps_square_rectangle");
    TreeSearchRelativePositionsParameters parameters;
    parameters.verbosity_level = 0;
    TreeSearchRelativePositionsOutput output = tree_search_relative_positions(instance, parameters);
    EXPECT_TRUE(output.solution_pool.best().feasible());
}

TEST(IrregularTreeSearchRelativePositions, OpenDimensionUpperBound)
{
    // With an upper bound of 20, there is no better solution, so the bound
    // is 20; with an upper bound of 25, the solution of length 20 is found.
    Instance instance = read_instance("open_dimension_x_steps_interlocked");
    TreeSearchRelativePositionsParameters parameters;
    parameters.verbosity_level = 0;
    parameters.open_dimension_upper_bound = 20;
    TreeSearchRelativePositionsOutput output = tree_search_relative_positions(instance, parameters);
    EXPECT_FALSE(output.is_proven_infeasible);
    EXPECT_FALSE(output.solution_pool.best().feasible());
    EXPECT_NEAR(output.open_dimension_x_bound, 20, 1e-6);
    parameters.open_dimension_upper_bound = 25;
    TreeSearchRelativePositionsOutput output_2 = tree_search_relative_positions(instance, parameters);
    EXPECT_TRUE(output_2.solution_pool.best().feasible());
    EXPECT_NEAR(output_2.solution_pool.best().x_max(), 20, 1e-6);
    EXPECT_NEAR(output_2.open_dimension_x_bound, 20, 1e-6);
}

TEST(IrregularTreeSearchRelativePositions, OptimizeExactOpenDimension)
{
    // In 'Exact' mode, the automatic selection runs the tree search on
    // relative positions for the open dimension objectives, which proves
    // that two steps without rotation take a length of 30, whereas the area
    // bound is 20.
    Instance instance = read_instance("open_dimension_x_steps_without_rotation");
    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = OptimizationMode::Exact;
    irregular::Output output = optimize(instance, parameters);
    EXPECT_TRUE(output.is_proven_optimal());
    EXPECT_NEAR(output.solution_pool.best().x_max(), 30, 1e-6);
    EXPECT_NEAR(output.open_dimension_x_bound, 30, 1e-6);
}

TEST(IrregularTreeSearchRelativePositions, OptimizeExactInfeasible)
{
    // In 'Exact' mode, the automatic selection runs the tree search on
    // relative positions for a single bin feasibility problem, which proves
    // that two 55 x 55 squares don't fit in a 100 x 100 bin, with a 10 x 10
    // square which isn't in the infeasible subset.
    Instance instance = read_instance("feasibility_squares_too_large_and_small_square");
    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = OptimizationMode::Exact;
    irregular::Output output = optimize(instance, parameters);
    EXPECT_TRUE(output.is_proven_infeasible);
    EXPECT_EQ(output.infeasible_subset, (std::vector<std::pair<ItemTypeId, ItemPos>>{{1, 2}}));
}
