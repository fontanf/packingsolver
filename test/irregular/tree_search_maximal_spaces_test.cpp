#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/solution_builder.hpp"
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

struct IrregularTreeSearchMaximalSpacesSolutionTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularTreeSearchMaximalSpacesSolutionTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularTreeSearchMaximalSpacesSolutionTest: public testing::TestWithParam<IrregularTreeSearchMaximalSpacesSolutionTestParams> { };

TEST_P(IrregularTreeSearchMaximalSpacesSolutionTest, IrregularTreeSearchMaximalSpaces)
{
    IrregularTreeSearchMaximalSpacesSolutionTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    std::cout << "Certificate path: " << test_params.certificate_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    TreeSearchMaximalSpacesParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    parameters.not_anytime_tree_search_queue_size = 4;
    TreeSearchMaximalSpacesOutput output = tree_search_maximal_spaces(instance, parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularTreeSearchMaximalSpacesSolutionTest,
        testing::ValuesIn(std::vector<IrregularTreeSearchMaximalSpacesSolutionTestParams>{
            {
                // A resource of capacity 1 consumed by item types 0 and 1: they
                // can't be packed together, item types 0 and 2 are.
                fs::path("data") / "irregular" / "tests" / "knapsack_hard_resource.json",
                fs::path("data") / "irregular" / "tests" / "knapsack_hard_resource_solution.json"
            }, {
                // A resource of capacity 2 consumed by the 5 copies of item type
                // 0: 2 copies are packed with 3 copies of item type 1.
                fs::path("data") / "irregular" / "tests" / "knapsack_hard_resource_copies.json",
                fs::path("data") / "irregular" / "tests" / "knapsack_hard_resource_copies_solution.json"
            }, {
                // The resource of 'knapsack_hard_resource', penalized with a
                // penalty of 60: packing item types 0 and 1 together (190 - 60) is
                // worse than packing item types 0 and 2 (150).
                fs::path("data") / "irregular" / "tests" / "knapsack_penalized_resource.json",
                fs::path("data") / "irregular" / "tests" / "knapsack_penalized_resource_solution.json"
            }}));
