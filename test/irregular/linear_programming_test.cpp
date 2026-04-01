#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/linear_programming.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

struct TestParams
{
    fs::path instance_path;
    fs::path initial_solution_path;
    double x_weight;
    double y_weight;
    fs::path expected_solution_path;
};

inline std::ostream& operator<<(std::ostream& os, const TestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class LinearProgrammingTest: public testing::TestWithParam<TestParams> { };

TEST_P(LinearProgrammingTest, LinearProgramming)
{
    TestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    std::cout << "Initial solution path: " << test_params.initial_solution_path << std::endl;
    Solution initial_solution(instance, test_params.initial_solution_path.string());

    std::cout << "Expected solution path: " << test_params.expected_solution_path << std::endl;
    Solution expected_solution(instance, test_params.expected_solution_path.string());

    LinearProgrammingAnchorParameters lp_parameters;
    LinearProgrammingAnchorOutput lp_output = linear_programming_anchor(
            initial_solution,
            test_params.x_weight,
            test_params.y_weight,
            lp_parameters);
    const Solution& solution = lp_output.solution;

    std::cout << std::endl
        << "Expected solution" << std::endl
        << "-----------------" << std::endl;
    expected_solution.format(std::cout);

    std::cout << std::endl
        << "LP solution" << std::endl
        << "-----------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(solution.number_of_different_bins(), expected_solution.number_of_different_bins());
    for (BinPos bin_pos = 0;
            bin_pos < (BinPos)expected_solution.number_of_different_bins();
            ++bin_pos) {
        const auto& sol_bin = solution.bin(bin_pos);
        const auto& exp_bin = expected_solution.bin(bin_pos);
        EXPECT_EQ(sol_bin.items.size(), exp_bin.items.size());
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)exp_bin.items.size();
                ++item_pos) {
            const SolutionItem& sol_item = sol_bin.items[item_pos];
            const SolutionItem& exp_item = exp_bin.items[item_pos];
            EXPECT_EQ(sol_item.item_type_id, exp_item.item_type_id);
            EXPECT_NEAR(sol_item.bl_corner.x, exp_item.bl_corner.x, 1e-3);
            EXPECT_NEAR(sol_item.bl_corner.y, exp_item.bl_corner.y, 1e-3);
            EXPECT_EQ(sol_item.angle, exp_item.angle);
            EXPECT_EQ(sol_item.mirror, exp_item.mirror);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// FindBestEdgeSeparator tests
////////////////////////////////////////////////////////////////////////////////

struct FindBestEdgeSeparatorTestParams
{
    Shape shape_1;
    Point shift_1;
    double scale_1;
    Shape shape_2;
    Point shift_2;
    double scale_2;

    ShapePos expected_edge_shape_pos;
    ElementPos expected_element_pos;
    Point expected_point;
    LengthDbl expected_distance;
};

inline std::ostream& operator<<(std::ostream& os, const FindBestEdgeSeparatorTestParams&)
{
    return os;
}

class FindBestEdgeSeparatorTest: public testing::TestWithParam<FindBestEdgeSeparatorTestParams> { };

TEST_P(FindBestEdgeSeparatorTest, FindBestEdgeSeparator)
{
    const FindBestEdgeSeparatorTestParams& p = GetParam();
    EdgeSeparationConstraintParameters result = find_best_edge_separator(
            p.shape_1, p.shift_1, p.scale_1,
            p.shape_2, p.shift_2, p.scale_2);

    EXPECT_EQ(result.edge_shape_pos, p.expected_edge_shape_pos);
    EXPECT_EQ(result.edge_element_pos, p.expected_element_pos);
    EXPECT_NEAR(result.point.x, p.expected_point.x, 1e-9);
    EXPECT_NEAR(result.point.y, p.expected_point.y, 1e-9);
    EXPECT_NEAR(result.distance, p.expected_distance, 1e-9);
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        FindBestEdgeSeparatorTest,
        testing::ValuesIn(std::vector<FindBestEdgeSeparatorTestParams>{
            {
                // Two unit squares side by side (gap=1 in x).
                // Best edge: right edge of shape_1 (element 1: (1,0)->(1,1)).
                // Closest vertex of shape_2: (2,0), distance=1.
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {0, 0}, 1.0,
                build_shape({{2, 0}, {3, 0}, {3, 1}, {2, 1}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 0,
                /* expected_element_pos */ 1,
                /* expected_point */ {2, 0},
                /* expected_distance */ 1.0,
            }, {
                // Two unit squares stacked vertically (gap=1 in y).
                // Best edge: top edge of shape_1 (element 2: (1,1)->(0,1)).
                // Closest vertex of shape_2: (0,2), distance=1.
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {0, 0}, 1.0,
                build_shape({{0, 2}, {1, 2}, {1, 3}, {0, 3}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 0,
                /* expected_element_pos */ 2,
                /* expected_point */ {0, 2},
                /* expected_distance */ 1.0,
            }, {
                // Two unit squares touching (gap=0).
                // Best edge: right edge of shape_1 (element 1: (1,0)->(1,1)).
                // Closest vertex of shape_2: (1,0), distance=0.
                build_shape({{0, 0}, {1, 0}, {1, 1}, {0, 1}}),
                {0, 0}, 1.0,
                build_shape({{1, 0}, {2, 0}, {2, 1}, {1, 1}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 0,
                /* expected_element_pos */ 1,
                /* expected_point */ {1, 0},
                /* expected_distance */ 0.0,
            }, {
                // Right triangle shape_1 vs unit square shape_2 placed to the right.
                // shape_1: (0,0)->(4,0)->(0,4), shape_2: (5,0)->(6,0)->(6,1)->(5,1).
                // The left edge of shape_2 (element 3: (5,1)->(5,0)) separates best
                // because shape_1 is entirely to its left with min distance=1
                // (vertex (4,0) of shape_1 is the closest), whereas the hypotenuse
                // of shape_1 gives only min distance=1/sqrt(2) < 1.
                // Expected: edge from shape_2 (edge_shape_pos=1), element 3,
                // closest point (4,0) from shape_1, distance=1.
                build_shape({{0, 0}, {4, 0}, {0, 4}}),
                {0, 0}, 1.0,
                build_shape({{5, 0}, {6, 0}, {6, 1}, {5, 1}}),
                {0, 0}, 1.0,
                /* expected_edge_shape_pos */ 1,
                /* expected_element_pos */ 3,
                /* expected_point */ {4, 0},
                /* expected_distance */ 1.0,
            }}));

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        LinearProgrammingTest,
        testing::ValuesIn(std::vector<TestParams>{
            {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_single_item.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_single_item_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_single_item_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_items.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_items_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_items_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_rotation_90.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_rotation_90_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_rotation_90_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_anchor_top_right.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_anchor_top_right_initial.json",
                -1.0, -1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_anchor_top_right_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_test.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_test_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_test_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_bins.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_bins_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_bins_expected.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_bins.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_bins_top_right_initial.json",
                -1.0, -1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_bins_top_right_expected.json"
            }, {
                // 4 rectangles in a row with gaps; LP closes all gaps to bottom-left.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_four_rects_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_four_rects_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_four_rects_bl_expected.json"
            }, {
                // Single right-triangle item pushed to bottom-left corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_triangle_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_triangle_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_triangle_bl_expected.json"
            }, {
                // Same triangle pushed to top-right corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_triangle_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_triangle_tr_initial.json",
                -1.0, -1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_triangle_tr_expected.json"
            }, {
                // Single L-shaped (non-convex) item pushed to bottom-left corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_l_shape_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_l_shape_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_l_shape_bl_expected.json"
            }, {
                // Single isosceles trapezoid pushed to bottom-left corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_trapezoid_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_trapezoid_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_trapezoid_bl_expected.json"
            }, {
                // 3 non-convex L-shapes already touching, slide as a group to bottom-left.
                // Bin height equals item height so only horizontal movement occurs.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_three_l_shapes_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_three_l_shapes_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_three_l_shapes_bl_expected.json"
            }, {
                // 4 squares in a 2x2 grid with gaps: tests 2D compaction in both x and y.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_four_squares_2d_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_four_squares_2d_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_four_squares_2d_bl_expected.json"
            }, {
                // Mixed item types: non-convex L-shape and rectangle, already touching,
                // slide as a group to bottom-left corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_mixed_types_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_mixed_types_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_mixed_types_bl_expected.json"
            }, {
                // T-shape (8 vertices, 2 concavities) pushed to bottom-left corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_t_shape_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_t_shape_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_t_shape_bl_expected.json"
            }, {
                // Same T-shape pushed to top-right corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_t_shape_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_t_shape_tr_initial.json",
                -1.0, -1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_t_shape_tr_expected.json"
            }, {
                // Cross/plus shape (12 vertices, 4 concavities) pushed to bottom-left corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_cross_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_cross_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_cross_bl_expected.json"
            }, {
                // Same cross/plus shape pushed to top-right corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_cross_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_cross_tr_initial.json",
                -1.0, -1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_cross_tr_expected.json"
            }, {
                // Two staircase shapes (8 vertices, 1 concavity each) pushed to bottom-left corner.
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_staircases_bl.json",
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_staircases_bl_initial.json",
                1.0, 1.0,
                fs::path("data") / "irregular" / "tests" / "linear_programming_anchor/lp_two_staircases_bl_expected.json"
            }}));
