#define PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG

#include "irregular/periodic_packing.hpp"
#include "irregular/rotations.hpp"

#include "packingsolver/irregular/instance_builder.hpp"

#include "shape/shapes_intersections.hpp"

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
#include "shape/writer.hpp"
#endif

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::irregular;


struct ComputePeriodicPackingsSingleShapeTestParams
{
    std::string name;
    ShapeWithHoles shape;
    std::vector<PeriodicPacking> expected_output;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const ComputePeriodicPackingsSingleShapeTestParams& p)
{
    os << p.name;
    return os;
}

class ComputePeriodicPackingsSingleShapeTest:
    public testing::TestWithParam<ComputePeriodicPackingsSingleShapeTestParams> { };

TEST_P(ComputePeriodicPackingsSingleShapeTest, OutputMatchesExpected)
{
    const ComputePeriodicPackingsSingleShapeTestParams& params = GetParam();

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    Writer writer;
    writer.add_shape_with_holes(params.shape, "Input");
    writer.write_json("periodic_packing_input.json");
#endif

    std::vector<PeriodicPacking> output = compute_periodic_packings(params.shape);

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    for (shape::ShapePos pos = 0;
            pos < (shape::ShapePos)output.size();
            ++pos) {
        const PeriodicPacking& periodic_packing = output[pos];
        Writer writer;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                ShapeWithHoles shape = params.shape;
                shape.shift(periodic_packing.positions[0]);
                shape.shift(i * periodic_packing.vector_1);
                shape.shift(j * periodic_packing.vector_2);
                writer.add_shape_with_holes(shape, "Output");
            }
        }
        writer.write_json("periodic_packing_output_" + std::to_string(pos) + ".json");
    }
#endif

    ASSERT_EQ((int)output.size(), (int)params.expected_output.size())
        << "Size mismatch for " << params.name
        << ": expected " << params.expected_output.size()
        << ", got " << output.size();

    for (int idx = 0; idx < (int)output.size(); ++idx) {
        EXPECT_TRUE(equal(output[idx], params.expected_output[idx]))
            << "Packing " << idx << " mismatch for " << params.name
            << ": v1=(" << output[idx].vector_1.x << "," << output[idx].vector_1.y << ")"
            << " v2=(" << output[idx].vector_2.x << "," << output[idx].vector_2.y << ")";
    }
}

INSTANTIATE_TEST_SUITE_P(
        PeriodicPacking,
        ComputePeriodicPackingsSingleShapeTest,
        testing::Values(
            ComputePeriodicPackingsSingleShapeTestParams{
                "Rectangle",
                {shape::build_rectangle(2.0, 3.0)},
                {
                    {{{0, 0}}, {2.0, 0}, {0, 3.0}},
                }
            },
            ComputePeriodicPackingsSingleShapeTestParams{
                "ArrowRight",
                {shape::build_shape({{1, 0}, {3, 0}, {2, 1}, {3, 2}, {1, 2}, {0, 1}})},
                {
                    {{{0, 0}}, {2, 0}, {0, 2}},
                }
            },
            ComputePeriodicPackingsSingleShapeTestParams{
                "ArrowTop",
                {shape::build_shape({{0, 0}, {1, 1}, {2, 0}, {2, 3}, {1, 4}, {0, 3}})},
                {
                    {{{0, 0}}, {2, 0}, {0, 3}},
                }
            }
        ),
        testing::PrintToStringParamName()
);


struct ComputePeriodicPackingsTwoShapesTestParams
{
    std::string name;
    ShapeWithHoles shape_0;
    ShapeWithHoles shape_r;
    std::vector<PeriodicPacking> expected_output;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const ComputePeriodicPackingsTwoShapesTestParams& p)
{
    os << p.name;
    return os;
}

class ComputePeriodicPackingsTwoShapesTest:
    public testing::TestWithParam<ComputePeriodicPackingsTwoShapesTestParams> { };

TEST_P(ComputePeriodicPackingsTwoShapesTest, OutputMatchesExpected)
{
    const ComputePeriodicPackingsTwoShapesTestParams& test_params = GetParam();

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    Writer writer;
    writer.add_shape_with_holes(test_params.shape_0, "Input 1");
    writer.add_shape_with_holes(test_params.shape_r, "Input 2");
    writer.write_json("periodic_packing_input.json");
#endif

    std::vector<PeriodicPacking> output = compute_periodic_packings(test_params.shape_0, test_params.shape_r);

    std::cout << "output" << std::endl;
    for (const PeriodicPacking& periodic_packing: output)
        std::cout << "- " << periodic_packing << std::endl;

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
    for (shape::ShapePos pos = 0;
            pos < (shape::ShapePos)output.size();
            ++pos) {
        const PeriodicPacking& periodic_packing = output[pos];
        Writer writer;
        for (int i = 0; i < 2; ++i) {
            for (int j = 0; j < 2; ++j) {
                ShapeWithHoles shape_0 = test_params.shape_0;
                shape_0.shift(periodic_packing.positions[0]);
                shape_0.shift(i * periodic_packing.vector_1);
                shape_0.shift(j * periodic_packing.vector_2);
                writer.add_shape_with_holes(shape_0, "Output 1");
                ShapeWithHoles shape_r = test_params.shape_r;
                shape_r.shift(periodic_packing.positions[1]);
                shape_r.shift(i * periodic_packing.vector_1);
                shape_r.shift(j * periodic_packing.vector_2);
                writer.add_shape_with_holes(shape_r, "Output 2");
            }
        }
        writer.write_json("periodic_packing_output_" + std::to_string(pos) + ".json");
    }
#endif

    ASSERT_EQ(output.size(), test_params.expected_output.size());
    for (const PeriodicPacking& expected_periodic_packing: test_params.expected_output) {
        EXPECT_NE(std::find_if(
                      output.begin(),
                      output.end(),
                      [&expected_periodic_packing](const PeriodicPacking& periodic_packing) { return equal(periodic_packing, expected_periodic_packing); }),
                  output.end());
    }
}

INSTANTIATE_TEST_SUITE_P(
        PeriodicPacking,
        ComputePeriodicPackingsTwoShapesTest,
        testing::Values(
            ComputePeriodicPackingsTwoShapesTestParams{
                "RightTriangle_0and180",
                {shape::build_triangle({0, 0}, {1.0, 0}, {0, 1.0})},
                {shape::build_triangle({1.0, 1.0}, {0.0, 1.0}, {1.0, 0.0})},
                {
                    {{{0, 0}, {0, 0}}, {1.0, 0}, {0, 1.0}},
                }
            },
            ComputePeriodicPackingsTwoShapesTestParams{
                "Triangle",
                {shape::build_triangle({0, 0}, {2, 0}, {1, 1})},
                {shape::build_triangle({1, 0}, {2, 1}, {0, 1})},
                {
                    {{{0, 0}, {1, 0}}, {2, 0}, {0, 1}},
                    {{{0, 0}, {0, 1}}, {2, 0}, {0, 2}},
                    {{{0, 0}, {0, 1}}, {0, 2}, {1, 1}},
                }
            }
        ),
        testing::PrintToStringParamName()
);


////////////////////////////////////////////////////////////////////////////////
////////////////////////// Real-world shapes (regression) ////////////////////////
////////////////////////////////////////////////////////////////////////////////

/**
 * These shapes are copied verbatim (vertex coordinates as-is) from real
 * production instances in data_periodic_packing/, rather than being
 * hand-crafted "nice" polygons. They exist to catch bugs that only show up on
 * irregular, near-degenerate, or many-vertex real-world geometry, whether in
 * compute_periodic_packings itself or in the shape library routines it calls
 * (no_fit_polygon, boolean_operations, shapes_intersections).
 *
 * Since there is no independently-known "correct" answer for these shapes,
 * the tests below check invariants that must hold for *any* valid periodic
 * packing, rather than comparing against exact expected output.
 */

namespace
{

/** Unsigned area of the parallelogram spanned by the two lattice vectors. */
double lattice_cell_area(const PeriodicPacking& pp)
{
    return std::abs(
            pp.vector_1.x * pp.vector_2.y
            - pp.vector_1.y * pp.vector_2.x);
}

/**
 * Check that no two shapes overlap in the periodic tiling induced by `pp`:
 * shapes[i] shifted by pp.positions[i], repeated at every offset
 * n * vector_1 + m * vector_2 for |n|, |m| <= check_range.
 *
 * This intentionally duplicates (rather than reuses) the equivalent internal
 * check in periodic_packing.cpp, so that a bug in that internal check can't
 * also hide it from the tests.
 */
bool no_overlap_in_tiling(
        const std::vector<ShapeWithHoles>& shapes,
        const PeriodicPacking& pp,
        int check_range = 2)
{
    std::vector<ShapeWithHoles> base;
    for (int i = 0; i < (int)shapes.size(); ++i) {
        ShapeWithHoles s = shapes[i];
        s.shift(pp.positions[i].x, pp.positions[i].y);
        base.push_back(s);
    }

    for (int i = 0; i < (int)base.size(); ++i)
        for (int j = i + 1; j < (int)base.size(); ++j)
            if (shape::intersect(base[i], base[j], true))
                return false;

    for (int n = -check_range; n <= check_range; ++n) {
        for (int m = -check_range; m <= check_range; ++m) {
            if (n == 0 && m == 0)
                continue;
            Point offset = {
                n * pp.vector_1.x + m * pp.vector_2.x,
                n * pp.vector_1.y + m * pp.vector_2.y,
            };
            for (int i = 0; i < (int)base.size(); ++i) {
                ShapeWithHoles copy = base[i];
                copy.shift(offset.x, offset.y);
                for (int j = 0; j < (int)base.size(); ++j)
                    if (shape::intersect(base[j], copy, true))
                        return false;
            }
        }
    }
    return true;
}

}  // namespace

struct RealShapeSingleTestParams
{
    std::string name;
    ShapeWithHoles shape;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RealShapeSingleTestParams& p)
{
    os << p.name;
    return os;
}

class ComputePeriodicPackingsRealShapeSingleTest:
    public testing::TestWithParam<RealShapeSingleTestParams> { };

TEST_P(ComputePeriodicPackingsRealShapeSingleTest, OutputIsValid)
{
    const RealShapeSingleTestParams& params = GetParam();

    std::vector<PeriodicPacking> output = compute_periodic_packings(params.shape);

    for (const PeriodicPacking& pp: output) {
        // The two lattice vectors must span a non-degenerate cell.
        EXPECT_GT(lattice_cell_area(pp), 0.0)
            << params.name << ": " << pp;
        // The cell must be at least as large as the item it contains.
        EXPECT_GE(lattice_cell_area(pp) + 1e-6, params.shape.compute_area())
            << params.name << ": " << pp;
        // No two copies of the shape may overlap when tiled periodically.
        EXPECT_TRUE(no_overlap_in_tiling({params.shape}, pp))
            << params.name << ": " << pp;
    }
}

INSTANTIATE_TEST_SUITE_P(
        PeriodicPackingRealShapes,
        ComputePeriodicPackingsRealShapeSingleTest,
        testing::Values(
                RealShapeSingleTestParams{
                    "Quad_20260408_item3",
                    {shape::build_shape({{0, 20}, {20, 0}, {207, 0}, {0, 205}})}
                },
                RealShapeSingleTestParams{
                    "Hexagon_20260408_item0",
                    {shape::build_shape({{0, 0}, {380, 0}, {400, 20}, {400, 355}, {310, 355}, {0, 125}})}
                },
                RealShapeSingleTestParams{
                    "Polygon12_20260423_1_item20",
                    {shape::build_shape({
                        {0, 71.05}, {142.98, 0}, {142.98, 123.59}, {152.98, 123.59},
                        {152.98, 22.59}, {172.98, 2.59}, {612.98, 2.59}, {632.98, 22.59},
                        {632.98, 123.59}, {197.64, 218.88}, {194.2, 220.21}, {97.91, 268.07}})}
                },
                RealShapeSingleTestParams{
                    "ArchPolygon127_20251208_item0",
                    {shape::build_shape({
                        {-1.42109e-14, -230.244}, {9.97228, -230.119}, {19.9384, -229.746}, {29.892, -229.125}, {39.8271, -228.256}, {49.7375, -227.139}, {59.6169, -225.776}, {69.4593, -224.167}, {79.2585, -222.313}, {89.0084, -220.215}, {98.703, -217.874}, {108.336, -215.293}, {117.902, -212.473}, {127.395, -209.414}, {136.808, -206.121}, {146.136, -202.593}, {155.374, -198.834}, {164.515, -194.846}, {173.553, -190.631}, {182.484, -186.192}, {191.302, -181.532}, {200, -176.654}, {208.574, -171.56}, {217.019, -166.254}, {225.328, -160.739}, {233.497, -155.019}, {241.522, -149.097}, {249.396, -142.976}, {257.115, -136.661}, {264.674, -130.156}, {272.069, -123.464}, {279.295, -116.59}, {286.347, -109.538}, {293.221, -102.313}, {299.912, -94.9179}, {306.418, -87.3586}, {312.733, -79.6394}, {318.853, -71.7653}, {324.775, -63.741}, {330.496, -55.5715}, {336.01, -47.262}, {341.316, -38.8176}, {346.41, -30.2435}, {351.289, -21.5451}, {355.949, -12.7278}, {360.388, -3.79702}, {364.602, 5.24164}, {368.59, 14.3826}, {372.349, 23.6201}, {375.877, 32.9484}, {379.171, 42.3618}, {382.229, 51.8544}, {385.05, 61.4203}, {387.631, 71.0535}, {389.971, 80.7481}, {392.069, 90.498}, {393.923, 100.297}, {395.532, 110.14}, {396.896, 120.019}, {398.012, 129.929}, {398.882, 139.864}, {399.503, 149.818}, {399.876, 159.784}, {400, 169.756}, {-400, 169.756}, {-399.876, 159.784}, {-399.503, 149.818}, {-398.882, 139.864}, {-398.012, 129.929}, {-396.896, 120.019}, {-395.532, 110.14}, {-393.923, 100.297}, {-392.069, 90.498}, {-389.971, 80.7481}, {-387.631, 71.0535}, {-385.05, 61.4203}, {-382.229, 51.8544}, {-379.171, 42.3618}, {-375.877, 32.9484}, {-372.349, 23.6201}, {-368.59, 14.3826}, {-364.602, 5.24164}, {-360.388, -3.79702}, {-355.949, -12.7278}, {-351.289, -21.5451}, {-346.41, -30.2435}, {-341.316, -38.8176}, {-336.01, -47.262}, {-330.496, -55.5715}, {-324.775, -63.741}, {-318.853, -71.7653}, {-312.733, -79.6394}, {-306.418, -87.3586}, {-299.912, -94.9179}, {-293.221, -102.313}, {-286.347, -109.538}, {-279.295, -116.59}, {-272.069, -123.464}, {-264.674, -130.156}, {-257.115, -136.661}, {-249.396, -142.976}, {-241.522, -149.097}, {-233.497, -155.019}, {-225.328, -160.739}, {-217.019, -166.254}, {-208.574, -171.56}, {-200, -176.654}, {-191.302, -181.532}, {-182.484, -186.192}, {-173.553, -190.631}, {-164.515, -194.846}, {-155.374, -198.834}, {-146.136, -202.593}, {-136.808, -206.121}, {-127.395, -209.414}, {-117.902, -212.473}, {-108.336, -215.293}, {-98.703, -217.874}, {-89.0084, -220.215}, {-79.2585, -222.313}, {-69.4593, -224.167}, {-59.6169, -225.776}, {-49.7375, -227.139}, {-39.8271, -228.256}, {-29.892, -229.125}, {-19.9384, -229.746}, {-9.97228, -230.119}})}
                }
        ),
        testing::PrintToStringParamName()
);


struct RealShapeTwoRotationsTestParams
{
    std::string name;
    ShapeWithHoles shape;
    Angle rotation_angle;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RealShapeTwoRotationsTestParams& p)
{
    os << p.name;
    return os;
}

class ComputePeriodicPackingsRealShapeTwoRotationsTest:
    public testing::TestWithParam<RealShapeTwoRotationsTestParams> { };

TEST_P(ComputePeriodicPackingsRealShapeTwoRotationsTest, OutputIsValid)
{
    const RealShapeTwoRotationsTestParams& params = GetParam();
    ShapeWithHoles shape_r = params.shape.rotate(params.rotation_angle);

    std::vector<PeriodicPacking> output = compute_periodic_packings(params.shape, shape_r);

    for (const PeriodicPacking& pp: output) {
        EXPECT_GT(lattice_cell_area(pp), 0.0)
            << params.name << ": " << pp;
        EXPECT_GE(
                lattice_cell_area(pp) + 1e-6,
                params.shape.compute_area() + shape_r.compute_area())
            << params.name << ": " << pp;
        EXPECT_TRUE(no_overlap_in_tiling({params.shape, shape_r}, pp))
            << params.name << ": " << pp;
    }
}

INSTANTIATE_TEST_SUITE_P(
        PeriodicPackingRealShapes,
        ComputePeriodicPackingsRealShapeTwoRotationsTest,
        testing::Values(
                RealShapeTwoRotationsTestParams{
                    "Quad_20260408_item3_0and180",
                    {shape::build_shape({{0, 20}, {20, 0}, {207, 0}, {0, 205}})},
                    180.0
                },
                RealShapeTwoRotationsTestParams{
                    "Hexagon_20260408_item0_0and180",
                    {shape::build_shape({{0, 0}, {380, 0}, {400, 20}, {400, 355}, {310, 355}, {0, 125}})},
                    180.0
                },
                RealShapeTwoRotationsTestParams{
                    "Polygon12_20260423_1_item20_0and90",
                    {shape::build_shape({
                        {0, 71.05}, {142.98, 0}, {142.98, 123.59}, {152.98, 123.59},
                        {152.98, 22.59}, {172.98, 2.59}, {612.98, 2.59}, {632.98, 22.59},
                        {632.98, 123.59}, {197.64, 218.88}, {194.2, 220.21}, {97.91, 268.07}})},
                    90.0
                }
        ),
        testing::PrintToStringParamName()
);

// compute_periodic_packings(instance, item_type_rotations) attaches an
// aabb_scaled to each PeriodicItemPacking, computed as the union of every
// placed item's *actual* bounding box (i.e. the item's own shape bounding
// box shifted by its placement position). A regression had the first
// item's contribution merged in unshifted, which silently inflated
// aabb_scaled whenever that item's placement position wasn't the origin
// (e.g. the second shape of a two-rotation packing) -- this in turn made
// compute_periodic_blocks() think tiled copies needed more room than they
// really do, wrongly rejecting achievable block sizes.
TEST(ComputePeriodicPackingsInstanceTest, AabbScaledMatchesPlacedItems)
{
    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(shape::build_rectangle(2000, 2000));
    ItemTypeId item_type_id = instance_builder.add_item_type(
            {{shape::build_shape({{0, 20}, {20, 0}, {207, 0}, {0, 205}})}});
    instance_builder.add_item_type_allowed_rotation(item_type_id, 0.0, 0.0, false);
    instance_builder.add_item_type_allowed_rotation(item_type_id, 180.0, 180.0, false);
    instance_builder.set_item_type_copies(item_type_id, 8);
    Instance instance = instance_builder.build();

    auto all_rotations = compute_item_type_rotations(instance);
    std::vector<PeriodicItemPacking> output = compute_periodic_packings(
            instance, all_rotations[instance.bin_type_id(0)]);
    ASSERT_FALSE(output.empty());

    for (const PeriodicItemPacking& packing: output) {
        AxisAlignedBoundingBox expected_aabb;
        for (const SolutionItem& item: packing.items) {
            ShapeWithHoles item_shape = instance.item_shape_scaled(
                    item.item_type_id, 0, item.angle, item.mirror);
            AxisAlignedBoundingBox item_aabb = item_shape.compute_min_max();
            item_aabb.shift(item.bl_corner);
            expected_aabb = shape::merge(expected_aabb, item_aabb);
        }
        EXPECT_TRUE(shape::equal(expected_aabb.x_min, packing.aabb_scaled.x_min));
        EXPECT_TRUE(shape::equal(expected_aabb.x_max, packing.aabb_scaled.x_max));
        EXPECT_TRUE(shape::equal(expected_aabb.y_min, packing.aabb_scaled.y_min));
        EXPECT_TRUE(shape::equal(expected_aabb.y_max, packing.aabb_scaled.y_max));
    }
}
