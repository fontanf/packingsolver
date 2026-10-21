#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/local_search.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

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

}

TEST(IrregularLocalSearch, ConvertInitialSolutionDropsBins)
{
    // Original instance: three 10x10 bins, three 10x10 items, one per bin.
    // Sub-instance: the same bin type with only two copies. The converted
    // solution keeps the first two bins with their items, and drops the last
    // one with its item.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(10, 10));
    instance_builder.set_bin_type_copies(bin_type_id, 3);
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    SolutionBuilder solution_builder(instance);
    for (BinPos copy = 0; copy < 3; ++copy) {
        BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);
        solution_builder.add_item(bin_pos, item_type_id, {0, 0}, 0, false);
    }
    Solution solution = solution_builder.build();

    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Feasibility);
    sub_instance_builder.set_parameters(instance.parameters());
    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type(instance, bin_type_id);
    sub_instance_builder.set_bin_type_copies(sub_bin_type_id, 2);
    sub_instance_builder.add_item_type(instance, item_type_id);
    Instance sub_instance = sub_instance_builder.build();

    Solution converted_solution = convert_initial_solution(sub_instance, solution);

    EXPECT_EQ(&converted_solution.instance(), &sub_instance);
    EXPECT_EQ(converted_solution.number_of_bins(), 2);
    EXPECT_EQ(converted_solution.number_of_items(), 2);
}

TEST(IrregularLocalSearch, ConvertInitialSolutionKeepsPositions)
{
    // Original instance: strip 100x10, three 10x10 items side by side
    // (length 30). Sub-instance: strip 29.7x10. The items keep their
    // positions, although the last one sticks out of the strip.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::OpenDimensionX);
    instance_builder.add_bin_type(rectangle_shape(100, 10));
    ItemTypeId item_type_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id, 3);
    Instance instance = instance_builder.build();

    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(0, 1);
    for (LengthDbl x: {0.0, 10.0, 20.0})
        solution_builder.add_item(bin_pos, item_type_id, {x, 0}, 0, false);
    Solution solution = solution_builder.build();

    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Feasibility);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.add_bin_type(rectangle_shape(29.7, 10));
    sub_instance_builder.add_item_type(instance, item_type_id);
    Instance sub_instance = sub_instance_builder.build();

    Solution converted_solution = convert_initial_solution(sub_instance, solution);

    ASSERT_EQ(converted_solution.number_of_different_bins(), 1);
    std::vector<LengthDbl> item_xs;
    for (const SolutionItem& item: converted_solution.bin(0).items)
        item_xs.push_back(item.bl_corner.x);
    EXPECT_EQ(item_xs, std::vector<LengthDbl>({0.0, 10.0, 20.0}));
}

TEST(IrregularLocalSearch, SeparatorDistance)
{
    // Bin 100x100; two 10x10 items and one 5x5 item.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id_large = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(item_type_id_large, 2);
    instance_builder.add_item_type(rectangle_item_shape(5, 5));
    Instance instance = instance_builder.build();
    LengthDbl scale_value = instance.parameters().scale_value;

    std::mt19937_64 generator(0);
    Separator separator(instance, SeparatorParameters(), generator);
    auto item_state = [scale_value](LengthDbl position_x, LengthDbl position_y)
    {
        Separator::ItemState item_state;
        item_state.bin_pos = 0;
        item_state.orientation_id = 0;
        item_state.position = {scale_value * position_x, scale_value * position_y};
        return item_state;
    };
    // Items 0 and 1 are the copies of the 10x10 item type; item 2 is the 5x5
    // item.
    std::vector<Separator::ItemState> layout = {item_state(0, 0), item_state(20, 0), item_state(50, 50)};
    std::vector<Separator::ItemState> layout_swapped = {item_state(20, 0), item_state(0, 0), item_state(50, 50)};
    std::vector<Separator::ItemState> layout_moved = {item_state(0, 0), item_state(20, 0), item_state(80, 80)};

    EXPECT_EQ(separator.distance(layout, layout), 0);
    EXPECT_EQ(separator.distance(layout, layout_swapped), 0);
    EXPECT_EQ(separator.distance(layout, layout_moved), 1);
}

TEST(IrregularLocalSearch, DirectTrigonometryFeasibility)
{
    // Bin 45x25; four 10x10 squares and four 10x10 right triangles. The bin
    // leaves some slack: the overlap minimization can't make items touch
    // exactly. The initial solution has all the items at the same position.
    // The local search must separate them, with one worker and with three
    // workers.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(rectangle_shape(45, 25));
    ItemTypeId square_id = instance_builder.add_item_type(rectangle_item_shape(10, 10));
    instance_builder.set_item_type_copies(square_id, 4);
    ItemShape triangle_shape;
    triangle_shape.shape_orig.shape = shape::build_triangle({0, 0}, {10, 0}, {0, 10});
    ItemTypeId triangle_id = instance_builder.add_item_type({triangle_shape});
    instance_builder.add_item_type_allowed_rotation(triangle_id, 0, 0, false);
    instance_builder.add_item_type_allowed_rotation(triangle_id, 180, 180, false);
    instance_builder.set_item_type_copies(triangle_id, 4);
    Instance instance = instance_builder.build();

    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(0, 1);
    for (ItemPos copy = 0; copy < 4; ++copy) {
        solution_builder.add_item(bin_pos, square_id, {0, 0}, 0, false);
        solution_builder.add_item(bin_pos, triangle_id, {0, 0}, 0, false);
    }
    Solution initial_solution = solution_builder.build();

    for (Counter number_of_workers: {1, 3}) {
        LocalSearchParameters parameters;
        parameters.verbosity_level = 0;
        parameters.timer.set_time_limit(30);
        parameters.separator_parameters.number_of_workers = number_of_workers;
        std::mt19937_64 generator(0);
        LocalSearchOutput output = local_search(instance, generator, &initial_solution, parameters);
        const Solution& solution = output.solution_pool.best();
        EXPECT_TRUE(solution.full()) << "number_of_workers " << number_of_workers;
        EXPECT_TRUE(solution.feasible()) << "number_of_workers " << number_of_workers;
        for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
            Solution::OverlappingItems overlapping_items = solution.compute_overlapping_items(bin_pos);
            EXPECT_TRUE(overlapping_items.item_item_pairs.empty());
            EXPECT_TRUE(overlapping_items.items_outside_bin.empty());
        }
    }
}

namespace
{

/**
 * Return true iff 'solution' is not worse than 'reference_solution', the
 * continuous part of the objective (length, area, leftover value) being
 * allowed to be worse by a relative 'tolerance'.
 */
bool not_worse(
        const Solution& solution,
        const Solution& reference_solution,
        double tolerance)
{
    if (!solution.feasible())
        return false;
    switch (solution.instance().objective()) {
    case Objective::Feasibility: {
        return solution.full();
    } case Objective::BinPacking: {
        return solution.number_of_bins() <= reference_solution.number_of_bins();
    } case Objective::BinPackingWithLeftovers: {
        if (solution.number_of_bins() != reference_solution.number_of_bins())
            return solution.number_of_bins() < reference_solution.number_of_bins();
        return solution.leftover_value_scaled()
            >= (1 - tolerance) * reference_solution.leftover_value_scaled();
    } case Objective::OpenDimensionX: {
        return solution.x_max() <= (1 + tolerance) * reference_solution.x_max();
    } case Objective::OpenDimensionY: {
        return solution.y_max() <= (1 + tolerance) * reference_solution.y_max();
    } case Objective::OpenDimensionXY: {
        return solution.open_dimension_xy_area()
            <= (1 + tolerance) * reference_solution.open_dimension_xy_area();
    } default: {
        return false;
    }
    }
}

}

struct IrregularLocalSearchTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularLocalSearchTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularLocalSearchTest: public testing::TestWithParam<IrregularLocalSearchTestParams> { };

TEST_P(IrregularLocalSearchTest, IrregularLocalSearch)
{
    // The local search is stopped by its time limit, so its solution is only
    // required to be not worse than the reference solution, up to a relative
    // tolerance on the continuous part of the objective.
    IrregularLocalSearchTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    std::cout << "Certificate path: " << test_params.certificate_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    LocalSearchParameters parameters;
    parameters.verbosity_level = 0;
    parameters.timer.set_time_limit(5);
    std::mt19937_64 generator(0);
    LocalSearchOutput output = local_search(instance, generator, nullptr, parameters);
    const Solution& solution = output.solution_pool.best();

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution reference_solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    reference_solution.format(std::cout);

    EXPECT_TRUE(not_worse(solution, reference_solution, 0.05));
    // The fixed items aren't packed as free items.
    std::vector<ItemPos> item_types_free_copies(instance.number_of_item_types(), 0);
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        for (const SolutionItem& item: solution_bin.items)
            if (!item.is_fixed)
                item_types_free_copies[item.item_type_id] += solution_bin.copies;
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        EXPECT_LE(item_types_free_copies[item_type_id], item_type.copies - item_type.copies_fixed);
    }
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        Solution::OverlappingItems overlapping_items = solution.compute_overlapping_items(bin_pos);
        EXPECT_TRUE(overlapping_items.item_border_pairs.empty());
        EXPECT_TRUE(overlapping_items.item_defect_pairs.empty());
        EXPECT_TRUE(overlapping_items.item_item_pairs.empty());
        EXPECT_TRUE(overlapping_items.items_outside_bin.empty());
    }
}

// Not included: 'polygon_with_hole' (the items are never placed in the holes of
// other items) and 'trapezoidal_insertions_4' (the items fit only with exact
// contacts between them, which the overlap minimization doesn't reach).
INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularLocalSearchTest,
        testing::ValuesIn(std::vector<IrregularLocalSearchTestParams>{
            {
                fs::path("data") / "irregular" / "tests" / "feasibility_squares_triangles.json",
                fs::path("data") / "irregular" / "tests" / "feasibility_squares_triangles_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "bin_packing_squares.json",
                fs::path("data") / "irregular" / "tests" / "bin_packing_squares_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "bin_packing_with_leftovers_squares.json",
                fs::path("data") / "irregular" / "tests" / "bin_packing_with_leftovers_squares_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "open_dimension_x_mixed_shapes.json",
                fs::path("data") / "irregular" / "tests" / "open_dimension_x_mixed_shapes_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "open_dimension_x_fixed_items_defects_spacing.json",
                fs::path("data") / "irregular" / "tests" / "open_dimension_x_fixed_items_defects_spacing_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "circle.json",
                fs::path("data") / "irregular" / "tests" / "circle_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "mirror_only_rotation.json",
                fs::path("data") / "irregular" / "tests" / "mirror_only_rotation_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "multiple_bins.json",
                fs::path("data") / "irregular" / "tests" / "multiple_bins_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "item_defect_minimum_spacing.json",
                fs::path("data") / "irregular" / "tests" / "item_defect_minimum_spacing_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "fixed_items_several_bins.json",
                fs::path("data") / "irregular" / "tests" / "fixed_items_several_bins_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "fixed_item.json",
                fs::path("data") / "irregular" / "tests" / "fixed_item_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_2.json",
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_2_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_3.json",
                fs::path("data") / "irregular" / "tests" / "trapezoidal_insertions_3_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "direction_y.json",
                fs::path("data") / "irregular" / "tests" / "direction_y_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_small.json",
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_small_solution.json"
            }, {
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_1.json",
                fs::path("data") / "irregular" / "tests" / "open_dimension_xy_1_solution.json"
            }}));
