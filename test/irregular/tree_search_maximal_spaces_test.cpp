#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/tree_search_maximal_spaces.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

class IrregularTreeSearchMaximalSpacesTest: public testing::TestWithParam<fs::path> { };

// Calls tree_search_maximal_spaces() directly instead of optimize(), which
// dispatches some instances to other algorithms (e.g. the single item ones
// to 'trivial_single_item').
TEST_P(IrregularTreeSearchMaximalSpacesTest, IrregularTreeSearchMaximalSpaces)
{
    fs::path instance_path = GetParam();
    std::cout << "Instance path: " << instance_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(instance_path.string());
    Instance instance = instance_builder.build();

    TreeSearchMaximalSpacesParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    parameters.not_anytime_tree_search_queue_size = 4;
    TreeSearchMaximalSpacesOutput output = tree_search_maximal_spaces(instance, parameters);

    const Solution& solution = output.solution_pool.best();
    solution.format(std::cout);
    EXPECT_TRUE(solution.feasible());
    for (packingsolver::BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        Solution::OverlappingItems overlapping_items = solution.compute_overlapping_items(bin_pos);
        EXPECT_TRUE(overlapping_items.item_item_pairs.empty());
        EXPECT_TRUE(overlapping_items.item_border_pairs.empty());
        EXPECT_TRUE(overlapping_items.item_defect_pairs.empty());
        EXPECT_TRUE(overlapping_items.items_outside_bin.empty());
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularTreeSearchMaximalSpacesTest,
        testing::ValuesIn(std::vector<fs::path>{
            fs::path("data") / "irregular" / "tests" / "circle.json",
            fs::path("data") / "irregular" / "tests" / "circular_bin.json",
            fs::path("data") / "irregular" / "tests" / "direction_y.json",
            fs::path("data") / "irregular" / "tests" / "feasibility_squares_triangles.json",
            fs::path("data") / "irregular" / "tests" / "fixed_item.json",
            fs::path("data") / "irregular" / "tests" / "free_rotation.json",
            fs::path("data") / "irregular" / "tests" / "item_defect_minimum_spacing.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_bin_border.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_defect_blocks_middle.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_fixed_item_with_hole.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_fixed_item_with_hole_spacing.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_fixed_items_overlapping_aabbs.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_item_bin_minimum_spacing.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_item_defect_minimum_spacing.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_item_item_minimum_spacing.json",
            fs::path("data") / "irregular" / "tests" / "knapsack_l_shapes_triangles.json",
            fs::path("data") / "irregular" / "tests" / "mirror.json",
            fs::path("data") / "irregular" / "tests" / "mirror_only_rotation.json",
            fs::path("data") / "irregular" / "tests" / "non_rectangular_bin.json",
            fs::path("data") / "irregular" / "tests" / "open_dimension_x_fixed_items_defects_spacing.json",
            fs::path("data") / "irregular" / "tests" / "open_dimension_x_mixed_shapes.json",
            fs::path("data") / "irregular" / "tests" / "open_dimension_xy_1.json",
            fs::path("data") / "irregular" / "tests" / "open_dimension_xy_mixed_shapes.json",
            fs::path("data") / "irregular" / "tests" / "open_dimension_xy_small.json",
            fs::path("data") / "irregular" / "tests" / "periodic_packing_item_bin_spacing.json",
            fs::path("data") / "irregular" / "tests" / "periodic_packing_item_item_spacing.json",
            fs::path("data") / "irregular" / "tests" / "periodic_packing_single_item_exact_fit_rectangle.json",
            fs::path("data") / "irregular" / "tests" / "periodic_packing_single_item_exact_fit_triangle.json",
            fs::path("data") / "irregular" / "tests" / "polygon_with_hole.json",
            fs::path("data") / "irregular" / "tests" / "rectangles_non_guillotine.json",
            fs::path("data") / "irregular" / "tests" / "rotations.json",
            fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions.json",
            fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_2.json",
            fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_3.json",
            fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_4.json",
            }));
