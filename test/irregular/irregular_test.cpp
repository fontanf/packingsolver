#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/optimize.hpp"
#include "packingsolver/irregular/solution.hpp"
#include "irregular/solution_builder.hpp"
#include "irregular/shape_simplification.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

TEST(Irregular, BinCopies)
{
    Shape shape;
    ShapeElement shape_element;
    shape_element.type = ShapeElementType::LineSegment;
    shape_element.start = {0, 0};
    shape_element.end = {10, 0};
    shape.elements.push_back(shape_element);
    shape_element.start = {10, 0};
    shape_element.end = {10, 10};
    shape.elements.push_back(shape_element);
    shape_element.start = {10, 10};
    shape_element.end = {0, 10};
    shape.elements.push_back(shape_element);
    shape_element.start = {0, 10};
    shape_element.end = {0, 0};
    shape.elements.push_back(shape_element);

    ItemShape item_shape;
    item_shape.shape_orig.shape = shape;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::VariableSizedBinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    instance_builder.set_item_type_copies(item_type_id, 10);
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(shape);
    instance_builder.set_bin_type_copies(bin_type_id, 10);
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.add_bin(0, 2);
    Solution solution = solution_builder.build();
    EXPECT_EQ(solution.number_of_bins(), 2);
    EXPECT_EQ(solution.bin_copies(0), 2);
}

struct IrregularOptimizeTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularOptimizeTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularOptimizeTest: public testing::TestWithParam<IrregularOptimizeTestParams> { };

TEST_P(IrregularOptimizeTest, IrregularOptimize)
{
    IrregularOptimizeTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);

    // The bound must not exceed the value of the reference solution, which
    // is feasible.
    switch (instance.objective()) {
    case packingsolver::Objective::BinPacking:
        EXPECT_LE(output.bin_packing_bound, solution.number_of_bins());
        break;
    case packingsolver::Objective::VariableSizedBinPacking:
        EXPECT_FALSE(packingsolver::strictly_lesser_cost(
                    solution.cost(),
                    output.variable_sized_bin_packing_bound));
        break;
    case packingsolver::Objective::Knapsack:
        EXPECT_FALSE(packingsolver::strictly_greater_profit(
                    solution.profit(),
                    output.knapsack_bound));
        break;
    default:
        break;
    }
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularOptimizeTest,
        testing::ValuesIn(std::vector<IrregularOptimizeTestParams>{
            {
                // The lower bound of 'VariableSizedBinPacking' (the
                // one-dimensional trivial bound, which every problem type
                // uses through its one-dimensional relaxation) used to
                // require the optional bins alone to cover the items,
                // ignoring that the mandatory bins of a bin type with a
                // positive 'copies_min' hold items too. No two items fit in
                // the same bin: the optimum uses three bins of type 1
                // (cost 3), but the bound was 4.
                fs::path("data") / "irregular" / "tests" / "variable_sized_bin_packing_mandatory_bins.json",
                fs::path("data") / "irregular" / "tests" / "variable_sized_bin_packing_mandatory_bins_solution.json",
            }, {
                // Fixed items with several bins: the sub-instances of the
                // algorithms for several bins, with fewer item types, used to
                // check the ids of the item types of the fixed items before
                // remapping them.
                fs::path("data") / "irregular" / "tests" / "fixed_items_several_bins.json",
                fs::path("data") / "irregular" / "tests" / "fixed_items_several_bins_solution.json",
            }, {
                // The endpoints of the circular arcs of this item, from a DXF
                // file, are not exactly equidistant from their centers. The
                // scaled shape used to be found self-intersecting.
                fs::path("data") / "irregular" / "tests" / "inconsistent_arc_centers.json",
                fs::path("data") / "irregular" / "tests" / "inconsistent_arc_centers_solution.json",
            }}));

TEST(Irregular, QualityRules)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = build_rectangle(0, 5, 0, 5);
    item_shape.quality_rule = 1;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    // Quality rule 0 allows defects of type 1; quality rule 1, of type 0.
    EXPECT_EQ(instance_builder.add_quality_rule({0, 1}), 0);
    EXPECT_EQ(instance_builder.add_quality_rule({1, 0}), 1);
    instance_builder.add_item_type({item_shape});
    instance_builder.add_bin_type(build_rectangle(0, 10, 0, 10));
    const Instance instance = instance_builder.build();

    EXPECT_FALSE(instance.can_contain(0, 0));
    EXPECT_TRUE(instance.can_contain(0, 1));
    EXPECT_TRUE(instance.can_contain(1, 0));
    EXPECT_FALSE(instance.can_contain(1, 1));
    // Out of range defect types and quality rules.
    EXPECT_FALSE(instance.can_contain(0, 2));
    EXPECT_FALSE(instance.can_contain(0, -1));
    EXPECT_FALSE(instance.can_contain(2, 0));
    EXPECT_FALSE(instance.can_contain(-1, 0));
}

TEST(Irregular, AddFixedItemInvalidIds)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = build_rectangle(0, 5, 0, 5);

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(build_rectangle(0, 10, 0, 10));
    EXPECT_THROW(
            instance_builder.add_fixed_item(bin_type_id + 1, item_type_id, {0, 0}, 0, false),
            std::invalid_argument);
    EXPECT_THROW(
            instance_builder.add_fixed_item(-1, item_type_id, {0, 0}, 0, false),
            std::invalid_argument);
    EXPECT_THROW(
            instance_builder.add_fixed_item(bin_type_id, -1, {0, 0}, 0, false),
            std::invalid_argument);
    // The item type of a fixed item may be added after it, but must exist
    // when the instance is built.
    instance_builder.add_fixed_item(bin_type_id, item_type_id + 1, {0, 0}, 0, false);
    EXPECT_THROW(instance_builder.build(), std::invalid_argument);
}

TEST(Irregular, SolutionBuilderAddFixedItem)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = build_rectangle(0, 5, 0, 5);

    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(build_rectangle(0, 10, 0, 10));
    instance_builder.add_fixed_item(bin_type_id, item_type_id, {5, 5}, 90, false);
    Instance instance = instance_builder.build();

    SolutionBuilder solution_builder(instance);
    packingsolver::BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);
    EXPECT_THROW(solution_builder.add_fixed_item(bin_pos, 1), std::invalid_argument);
    EXPECT_THROW(solution_builder.add_fixed_item(bin_pos, -1), std::invalid_argument);
    solution_builder.add_fixed_item(bin_pos, 0);
    // A fixed item is added at most once to a bin.
    EXPECT_THROW(solution_builder.add_fixed_item(bin_pos, 0), std::invalid_argument);
    Solution solution = solution_builder.build();

    // The position, angle and mirror come from the fixed item of the bin
    // type.
    const SolutionItem& solution_item = solution.bin(0).items[0];
    EXPECT_EQ(solution_item.item_type_id, item_type_id);
    EXPECT_EQ(solution_item.bl_corner.x, 5);
    EXPECT_EQ(solution_item.bl_corner.y, 5);
    EXPECT_EQ(solution_item.angle, 90);
    EXPECT_EQ(solution_item.fixed_item_pos, 0);
    EXPECT_TRUE(solution_item.is_fixed());
}

TEST(Irregular, SolutionWriteReadFixedItems)
{
    // The fixed items of a certificate are still fixed once it is written
    // and read again.
    InstanceBuilder instance_builder;
    instance_builder.read((fs::path("data") / "irregular" / "tests" / "fixed_items_several_bins.json").string());
    const Instance instance = instance_builder.build();
    SolutionBuilder solution_builder(instance);
    solution_builder.read((fs::path("data") / "irregular" / "tests" / "fixed_items_several_bins_solution.json").string());
    Solution solution = solution_builder.build();

    fs::path certificate_path = fs::temp_directory_path() / fs::unique_path("%%%%-%%%%-%%%%.json");
    solution.write(certificate_path.string());
    SolutionBuilder solution_builder_2(instance);
    solution_builder_2.read(certificate_path.string());
    Solution solution_2 = solution_builder_2.build();
    fs::remove(certificate_path);

    packingsolver::ItemPos number_of_fixed_items = 0;
    ASSERT_EQ(solution_2.number_of_different_bins(), solution.number_of_different_bins());
    for (packingsolver::BinPos bin_pos = 0;
            bin_pos < solution.number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        const SolutionBin& solution_bin_2 = solution_2.bin(bin_pos);
        ASSERT_EQ(solution_bin_2.items.size(), solution_bin.items.size());
        for (packingsolver::ItemPos item_pos = 0;
                item_pos < (packingsolver::ItemPos)solution_bin.items.size();
                ++item_pos) {
            EXPECT_EQ(
                    solution_bin_2.items[item_pos].fixed_item_pos,
                    solution_bin.items[item_pos].fixed_item_pos);
            if (solution_bin.items[item_pos].is_fixed())
                number_of_fixed_items += solution_bin.copies;
        }
    }
    EXPECT_EQ(number_of_fixed_items, instance.number_of_fixed_items());
}

TEST(Irregular, ReadUnknownLeftoverMode)
{
    // An unrecognized value is rejected instead of being ignored.
    std::stringstream ss(R"({
            "objective": "bin-packing-with-leftovers",
            "parameters": {"leftover_mode": "middle"},
            "bin_types": [{"type": "rectangle", "width": 100, "height": 50}],
            "item_types": [{"type": "rectangle", "width": 10, "height": 10}]})");
    InstanceBuilder instance_builder;
    EXPECT_THROW(instance_builder.read(ss), std::invalid_argument);
}

TEST(Irregular, UnlimitedItemCopies)
{
    // Item copies -1: as many copies as the total area of the bins allows,
    // ceil(2 * 100 * 50 / (30 * 30)) = 12.
    std::stringstream ss(R"({
            "objective": "knapsack",
            "bin_types": [{"type": "rectangle", "width": 100, "height": 50, "copies": 2}],
            "item_types": [{"type": "rectangle", "width": 30, "height": 30, "copies": -1}]})");
    InstanceBuilder instance_builder;
    instance_builder.read(ss);
    const Instance instance = instance_builder.build();
    EXPECT_EQ(instance.item_type(0).copies, 12);
}

TEST(Irregular, InfeasibleSubsetItemTypeDoesntFit)
{
    // The 120 x 10 item doesn't fit in the 100 x 100 bins: a single copy of
    // it is the infeasible subset.
    std::stringstream ss(R"({
            "objective": "bin-packing",
            "bin_types": [{"type": "rectangle", "width": 100, "height": 100, "copies": 2}],
            "item_types": [
                {"type": "rectangle", "width": 10, "height": 10, "copies": 3},
                {"type": "rectangle", "width": 120, "height": 10, "copies": 2,
                 "allowed_rotations": [{"start": 0, "end": 0}]}]})");
    InstanceBuilder instance_builder;
    instance_builder.read(ss);
    const Instance instance = instance_builder.build();
    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, parameters);
    EXPECT_TRUE(output.is_proven_infeasible);
    EXPECT_EQ(output.infeasible_subset, (std::vector<std::pair<packingsolver::ItemTypeId, packingsolver::ItemPos>>{{1, 1}}));
}

TEST(Irregular, InfeasibleSubsetArea)
{
    // The area of 5 of the 50 x 50 squares exceeds the one of the 100 x 100
    // bin. The two item types of 50 x 50 squares are merged by the
    // reduction, so the 5 copies of the infeasible subset are drawn from
    // both of them; the 10 x 10 square isn't in it.
    std::stringstream ss(R"({
            "objective": "feasibility",
            "bin_types": [{"type": "rectangle", "width": 100, "height": 100, "copies": 1}],
            "item_types": [
                {"type": "rectangle", "width": 50, "height": 50, "copies": 3},
                {"type": "rectangle", "width": 50, "height": 50, "copies": 3},
                {"type": "rectangle", "width": 10, "height": 10, "copies": 1}]})");
    InstanceBuilder instance_builder;
    instance_builder.read(ss);
    const Instance instance = instance_builder.build();
    OptimizeParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, parameters);
    EXPECT_TRUE(output.is_proven_infeasible);
    EXPECT_EQ(output.infeasible_subset_number_of_items(), 5);
    for (const std::pair<packingsolver::ItemTypeId, packingsolver::ItemPos>& p: output.infeasible_subset) {
        EXPECT_NE(p.first, 2);
        EXPECT_LE(p.second, 3);
    }
}

TEST(Irregular, OpenDimensionXYWithoutAspectRatio)
{
    // Not supported yet.
    std::stringstream ss(R"({
            "objective": "open-dimension-xy",
            "bin_types": [{"type": "rectangle", "width": 1000, "height": 1000}],
            "item_types": [{"type": "rectangle", "width": 20, "height": 10, "copies": 10}]})");
    InstanceBuilder instance_builder;
    instance_builder.read(ss);
    EXPECT_THROW(instance_builder.build(), std::invalid_argument);
}

TEST(Irregular, OpenDimensionXYMixedShapes)
{
    // The initial bin of the sequential feasibility, from the area of the
    // items, used to be too small for these items, which fill their bounding
    // boxes well: no solution was found. (The value of the solution found
    // depends on the platform.)
    InstanceBuilder instance_builder;
    instance_builder.read((fs::path("data") / "irregular" / "tests" / "open_dimension_xy_mixed_shapes.json").string());
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    Output output = optimize(instance, optimize_parameters);
    const Solution& solution = output.solution_pool.best();
    EXPECT_TRUE(solution.feasible());
    EXPECT_EQ(solution.number_of_items(), instance.number_of_items());
}

TEST(Irregular, InstanceBuilderCropBinType)
{
    // Bin 100x100 with three defects, cropped to 60x70.
    InstanceBuilder instance_builder;
    ItemShape item_shape;
    item_shape.shape_orig.shape = shape::build_rectangle(10, 10);
    instance_builder.add_item_type({item_shape});
    packingsolver::BinTypeId bin_type_id = instance_builder.add_bin_type(
            shape::build_rectangle(100, 100));
    // Inside the part kept.
    instance_builder.add_defect(bin_type_id, -1, {shape::build_rectangle(20, 30, 20, 30)});
    // Across the right and top sides of the part kept.
    instance_builder.add_defect(bin_type_id, -1, {shape::build_rectangle(50, 70, 30, 80)});
    // Outside the part kept.
    instance_builder.add_defect(bin_type_id, -1, {shape::build_rectangle(80, 90, 20, 30)});
    instance_builder.crop_bin_type(bin_type_id, 60, 70);
    Instance instance = instance_builder.build();

    const BinType& bin_type = instance.bin_type(bin_type_id);
    EXPECT_NEAR(bin_type.area_orig, 60 * 70, 1e-6);
    EXPECT_NEAR(bin_type.aabb_orig.x_max, 60, 1e-6);
    EXPECT_NEAR(bin_type.aabb_orig.y_max, 70, 1e-6);
    ASSERT_EQ(bin_type.defects.size(), 2);
    AxisAlignedBoundingBox aabb = bin_type.defects[1].shape_orig.shape.compute_min_max();
    EXPECT_NEAR(aabb.x_min, 50, 1e-6);
    EXPECT_NEAR(aabb.x_max, 60, 1e-6);
    EXPECT_NEAR(aabb.y_min, 30, 1e-6);
    EXPECT_NEAR(aabb.y_max, 70, 1e-6);
}

TEST(Irregular, InstanceCacheSubInstance)
{
    // Original instance and its simplified shapes.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::BinPacking);
    instance_builder.add_bin_type(build_rectangle(100, 100));
    packingsolver::ItemTypeId item_type_id_0 = instance_builder.add_item_type(
            {{build_rectangle(10, 20)}});
    packingsolver::ItemTypeId item_type_id_1 = instance_builder.add_item_type(
            {{build_shape({{0, 0}, {30, 0}, {30, 10}, {15, 12}, {0, 10}})}});
    Instance instance = instance_builder.build();
    // With 'end' true, 'get_simplified_instance' only returns the simplified
    // shapes already computed.
    bool end_true = true;
    bool end_false = false;
    EXPECT_EQ(instance.get_simplified_instance(0.2, end_true), nullptr);
    ASSERT_NE(instance.get_simplified_instance(0.2, end_false), nullptr);
    const SimplifiedInstance& simplified_instance = *instance.get_simplified_instance(0.2, end_true);

    // Sub-instance with the item types in the reverse order: the simplified
    // shapes are carried over with the ids of the sub-instance.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    sub_instance_builder.add_item_type_from_original_instance(item_type_id_1);
    sub_instance_builder.add_item_type_from_original_instance(item_type_id_0);
    Instance sub_instance = sub_instance_builder.build();
    ASSERT_NE(sub_instance.get_simplified_instance(0.2, end_true), nullptr);
    const SimplifiedInstance& sub_simplified_instance = *sub_instance.get_simplified_instance(0.2, end_true);
    ASSERT_EQ(sub_simplified_instance.item_types.size(), 2);
    ASSERT_EQ(sub_simplified_instance.bin_types.size(), 1);
    EXPECT_EQ(
            sub_simplified_instance.item_types[0].shapes[0].shape.shape.to_string(0),
            simplified_instance.item_types[item_type_id_1].shapes[0].shape.shape.to_string(0));
    EXPECT_EQ(
            sub_simplified_instance.item_types[1].shapes[0].shape.shape.to_string(0),
            simplified_instance.item_types[item_type_id_0].shapes[0].shape.shape.to_string(0));

    // Sub-instance with another item-item spacing: the inflated shapes would
    // be different, so the simplified shapes are not carried over.
    InstanceBuilder sub_instance_builder_2;
    sub_instance_builder_2.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder_2.set_parameters(instance.parameters());
    sub_instance_builder_2.set_item_item_minimum_spacing(1.0);
    sub_instance_builder_2.set_original_instance(instance);
    sub_instance_builder_2.add_bin_type_from_original_instance(0);
    sub_instance_builder_2.add_item_type_from_original_instance(item_type_id_0);
    Instance sub_instance_2 = sub_instance_builder_2.build();
    EXPECT_EQ(sub_instance_2.get_simplified_instance(0.2, end_true), nullptr);

    // Sub-instance with an item type which doesn't come from the original
    // instance: the simplified shapes are not carried over.
    InstanceBuilder sub_instance_builder_3;
    sub_instance_builder_3.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder_3.set_parameters(instance.parameters());
    sub_instance_builder_3.set_original_instance(instance);
    sub_instance_builder_3.add_bin_type_from_original_instance(0);
    sub_instance_builder_3.add_item_type_from_original_instance(item_type_id_0);
    sub_instance_builder_3.add_item_type({{build_rectangle(5, 5)}});
    Instance sub_instance_3 = sub_instance_builder_3.build();
    EXPECT_EQ(sub_instance_3.get_simplified_instance(0.2, end_true), nullptr);

    // Sub-instance with a cropped bin type: the simplified shapes are not
    // carried over.
    InstanceBuilder sub_instance_builder_4;
    sub_instance_builder_4.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder_4.set_parameters(instance.parameters());
    sub_instance_builder_4.set_original_instance(instance);
    packingsolver::BinTypeId sub_bin_type_id_4
        = sub_instance_builder_4.add_bin_type_from_original_instance(0);
    sub_instance_builder_4.crop_bin_type(sub_bin_type_id_4, 50, 50);
    sub_instance_builder_4.add_item_type_from_original_instance(item_type_id_0);
    Instance sub_instance_4 = sub_instance_builder_4.build();
    EXPECT_EQ(sub_instance_4.get_simplified_instance(0.2, end_true), nullptr);
}

TEST(Irregular, InitialSolution)
{
    // Bin 110x110; four 50x50 squares. The initial solution packs two of
    // them. The local search starts from it, so the solution found is not
    // worse. An initial solution with overlapping items is rejected.
    ItemShape item_shape;
    item_shape.shape_orig.shape = shape::build_rectangle(0, 50, 0, 50);
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.add_bin_type(shape::build_rectangle(0, 110, 0, 110));
    packingsolver::ItemTypeId item_type_id = instance_builder.add_item_type({item_shape});
    instance_builder.set_item_type_copies(item_type_id, 4);
    const Instance instance = instance_builder.build();

    SolutionBuilder solution_builder(instance);
    packingsolver::BinPos bin_pos = solution_builder.add_bin(0, 1);
    solution_builder.add_item(bin_pos, item_type_id, {0, 0}, 0, false);
    solution_builder.add_item(bin_pos, item_type_id, {55, 0}, 0, false);
    Solution initial_solution = solution_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.verbosity_level = 0;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_local_search_direct_trigonometry = true;
    optimize_parameters.initial_solution = &initial_solution;
    Output output = optimize(instance, optimize_parameters);
    EXPECT_TRUE(output.solution_pool.best().feasible());
    EXPECT_GE(output.solution_pool.best().profit(), initial_solution.profit());

    SolutionBuilder overlapping_solution_builder(instance);
    bin_pos = overlapping_solution_builder.add_bin(0, 1);
    overlapping_solution_builder.add_item(bin_pos, item_type_id, {0, 0}, 0, false);
    overlapping_solution_builder.add_item(bin_pos, item_type_id, {25, 0}, 0, false);
    Solution overlapping_solution = overlapping_solution_builder.build();
    optimize_parameters.initial_solution = &overlapping_solution;
    EXPECT_THROW(optimize(instance, optimize_parameters), std::invalid_argument);
}
