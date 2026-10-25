#include "packingsolver/irregular/instance_builder.hpp"
#include "irregular/milp_raster.hpp"
#include "irregular/solution_builder.hpp"

#include "shape/offset.hpp"
#include "shape/shapes_intersections.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

using namespace packingsolver;
using namespace packingsolver::irregular;
namespace fs = boost::filesystem;

namespace
{

/**
 * Check that the items of a solution satisfy the minimum spacings: item-item,
 * item-bin (with the bounding box of the bin) and item-defect.
 */
void check_spacings(const Solution& solution)
{
    const Instance& instance = solution.instance();
    LengthDbl item_spacing = instance.item_spacing_scaled();
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        const BinType& bin_type = instance.bin_type(solution_bin.bin_type_id);
        LengthDbl bin_spacing = instance.bin_spacing_scaled(solution_bin.bin_type_id);
        std::vector<ShapeWithHoles> shapes;
        for (ItemPos item_pos = 0; item_pos < (ItemPos)solution_bin.items.size(); ++item_pos) {
            const ItemType& item_type = instance.item_type(solution_bin.items[item_pos].item_type_id);
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                ShapeWithHoles item_shape = solution.shape_scaled(bin_pos, item_pos, item_shape_pos);
                // Bin.
                AxisAlignedBoundingBox aabb = item_shape.compute_min_max();
                EXPECT_FALSE(shape::strictly_lesser(aabb.x_min, bin_type.aabb_scaled.x_min + bin_spacing));
                EXPECT_FALSE(shape::strictly_greater(aabb.x_max, bin_type.aabb_scaled.x_max - bin_spacing));
                EXPECT_FALSE(shape::strictly_lesser(aabb.y_min, bin_type.aabb_scaled.y_min + bin_spacing));
                EXPECT_FALSE(shape::strictly_greater(aabb.y_max, bin_type.aabb_scaled.y_max - bin_spacing));
                // Defects.
                for (const Defect& defect: bin_type.defects)
                    EXPECT_FALSE(shape::intersect(defect.shape_inflated, item_shape, true));
                shapes.push_back(item_shape);
            }
        }
        // Items.
        for (Counter shape_pos_1 = 0; shape_pos_1 < (Counter)shapes.size(); ++shape_pos_1) {
            ShapeWithHoles shape_inflated = shape::inflate(shapes[shape_pos_1], item_spacing);
            for (Counter shape_pos_2 = shape_pos_1 + 1; shape_pos_2 < (Counter)shapes.size(); ++shape_pos_2)
                EXPECT_FALSE(shape::intersect(shape_inflated, shapes[shape_pos_2], true));
        }
    }
}

}

struct IrregularMilpRasterTestParams
{
    fs::path instance_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(std::ostream& os, const IrregularMilpRasterTestParams& test_params)
{
    os << test_params.instance_path;
    return os;
}

class IrregularMilpRasterTest: public testing::TestWithParam<IrregularMilpRasterTestParams> { };

TEST_P(IrregularMilpRasterTest, IrregularMilpRaster)
{
    IrregularMilpRasterTestParams test_params = GetParam();
    std::cout << "Instance path: " << test_params.instance_path << std::endl;
    std::cout << "Certificate path: " << test_params.certificate_path << std::endl;

    InstanceBuilder instance_builder;
    instance_builder.read(test_params.instance_path.string());
    Instance instance = instance_builder.build();

    MilpRasterParameters milp_raster_parameters;
    milp_raster_parameters.verbosity_level = 0;
    milp_raster_parameters.maximum_number_of_cell_sizes = 3;
    milp_raster_parameters.maximum_number_of_nodes = 1000;
    MilpRasterOutput output = milp_raster(instance, milp_raster_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
    check_spacings(output.solution_pool.best());
}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        IrregularMilpRasterTest,
        testing::ValuesIn(std::vector<IrregularMilpRasterTestParams>{
            {
                // 10x10 bin, 3x3 items, item-item spacing 1: a 3x3 grid of
                // items would need 3 * 3 + 2 * 1 = 11 > 10, so only a 2x2
                // grid fits.
                fs::path("data") / "irregular" / "tests" / "knapsack_item_item_minimum_spacing.json",
                fs::path("data") / "irregular" / "tests" / "knapsack_item_item_minimum_spacing_solution.json"
            }, {
                // 10x10 bin, 5x5 items, item-bin spacing 1: the items must be
                // in the 8x8 square at the center of the bin, so only one
                // fits.
                fs::path("data") / "irregular" / "tests" / "knapsack_item_bin_minimum_spacing.json",
                fs::path("data") / "irregular" / "tests" / "knapsack_item_bin_minimum_spacing_solution.json"
            }, {
                // 10x4 bin, 4x4 items, a 2x4 defect in the middle with an
                // item-defect spacing of 1: the space left on each side of
                // the defect is 4 wide without the spacing, 3 wide with it,
                // so no item fits.
                fs::path("data") / "irregular" / "tests" / "knapsack_item_defect_minimum_spacing.json",
                fs::path("data") / "irregular" / "tests" / "knapsack_item_defect_minimum_spacing_solution.json"
            }}));
