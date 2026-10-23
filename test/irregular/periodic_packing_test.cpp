#define PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG

#include "packingsolver/irregular/optimize.hpp"
#include "irregular/periodic_packing.hpp"
#include "irregular/rotations.hpp"
#include "irregular/tree_search_periodic_packing.hpp"

#include "packingsolver/irregular/instance_builder.hpp"

#include "shape/shapes_intersections.hpp"
#include "shape/offset.hpp"

#ifdef PACKINGSOLVER_PERIODIC_PACKING_TEST_ENABLE_DEBUG
#include "shape/writer.hpp"
#endif

#include <gtest/gtest.h>

#include <map>

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
 *
 * When `spacing` > 0, checks the stronger requirement that every pair of
 * copies stays at least `spacing` apart, rather than merely not overlapping:
 * each shape is independently inflated by spacing / 2 (an arbitrary but
 * irrelevant split -- dist(A, B) >= spacing iff inflate(A, spacing / 2) and
 * inflate(B, spacing / 2) don't overlap, regardless of how the spacing is
 * divided between the two sides) before the same intersection checks, using
 * shape::inflate directly rather than going through compute_periodic_packings'
 * own NFP-based spacing logic, so a bug there can't hide from this either.
 */
bool no_overlap_in_tiling(
        const std::vector<ShapeWithHoles>& shapes,
        const PeriodicPacking& pp,
        LengthDbl spacing = 0.0,
        int check_range = 2)
{
    std::vector<ShapeWithHoles> base;
    for (int i = 0; i < (int)shapes.size(); ++i) {
        ShapeWithHoles s = shapes[i];
        s.shift(pp.positions[i].x, pp.positions[i].y);
        if (spacing > 0.0)
            s = shape::inflate(s, spacing / 2.0);
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
    LengthDbl spacing = 0.0;
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

    std::vector<PeriodicPacking> output = compute_periodic_packings(params.shape, params.spacing);

    for (const PeriodicPacking& pp: output) {
        // The two lattice vectors must span a non-degenerate cell.
        EXPECT_GT(lattice_cell_area(pp), 0.0)
            << params.name << ": " << pp;
        // The cell must be at least as large as the item it contains.
        EXPECT_GE(lattice_cell_area(pp) + 1e-6, params.shape.compute_area())
            << params.name << ": " << pp;
        // No two copies of the shape may come closer than params.spacing
        // when tiled periodically.
        EXPECT_TRUE(no_overlap_in_tiling({params.shape}, pp, params.spacing))
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
                },
                RealShapeSingleTestParams{
                    "Quad_20260408_item3_spacing10",
                    {shape::build_shape({{0, 20}, {20, 0}, {207, 0}, {0, 205}})},
                    10.0
                },
                RealShapeSingleTestParams{
                    "Polygon12_20260423_1_item20_spacing5",
                    {shape::build_shape({
                        {0, 71.05}, {142.98, 0}, {142.98, 123.59}, {152.98, 123.59},
                        {152.98, 22.59}, {172.98, 2.59}, {612.98, 2.59}, {632.98, 22.59},
                        {632.98, 123.59}, {197.64, 218.88}, {194.2, 220.21}, {97.91, 268.07}})},
                    5.0
                }
        ),
        testing::PrintToStringParamName()
);


struct RealShapeTwoRotationsTestParams
{
    std::string name;
    ShapeWithHoles shape;
    Angle rotation_angle;
    LengthDbl spacing = 0.0;
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

    std::vector<PeriodicPacking> output = compute_periodic_packings(params.shape, shape_r, params.spacing);

    for (const PeriodicPacking& pp: output) {
        EXPECT_GT(lattice_cell_area(pp), 0.0)
            << params.name << ": " << pp;
        EXPECT_GE(
                lattice_cell_area(pp) + 1e-6,
                params.shape.compute_area() + shape_r.compute_area())
            << params.name << ": " << pp;
        EXPECT_TRUE(no_overlap_in_tiling({params.shape, shape_r}, pp, params.spacing))
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
                },
                RealShapeTwoRotationsTestParams{
                    "Hexagon_20260408_item0_0and180_spacing8",
                    {shape::build_shape({{0, 0}, {380, 0}, {400, 20}, {400, 355}, {310, 355}, {0, 125}})},
                    180.0,
                    8.0
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

// The periodic packings computed by InstanceBuilder::build() are stored in
// the instance, and carried over, with their item type ids remapped, to the
// sub-instances built with InstanceBuilder::add_item_type(instance, id),
// instead of being recomputed.
TEST(ComputePeriodicPackingsInstanceTest, PeriodicPackingsCarriedOverToSubInstances)
{
    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(shape::build_rectangle(2000, 2000));
    // Below the copies threshold: no periodic packing.
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(
            {{shape::build_rectangle(100, 50)}});
    instance_builder.set_item_type_copies(item_type_id_0, 2);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(
            {{shape::build_shape({{0, 20}, {20, 0}, {207, 0}, {0, 205}})}});
    instance_builder.add_item_type_allowed_rotation(item_type_id_1, 0.0, 0.0, false);
    instance_builder.add_item_type_allowed_rotation(item_type_id_1, 180.0, 180.0, false);
    instance_builder.set_item_type_copies(item_type_id_1, 20);
    Instance instance = instance_builder.build();

    ASSERT_FALSE(instance.periodic_packings().empty());
    for (const PeriodicItemPacking& packing: instance.periodic_packings())
        for (const SolutionItem& item: packing.items)
            EXPECT_EQ(item.item_type_id, item_type_id_1);
    EXPECT_TRUE(instance.item_type(item_type_id_0).periodic_packings_computed);
    EXPECT_TRUE(instance.item_type(item_type_id_1).periodic_packings_computed);

    // Sub-instance with the item types in the reverse order.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.add_bin_type(shape::build_rectangle(2000, 2000));
    ItemTypeId sub_item_type_id_1 = sub_instance_builder.add_item_type(instance, item_type_id_1);
    sub_instance_builder.add_item_type(instance, item_type_id_0);
    Instance sub_instance = sub_instance_builder.build();

    ASSERT_EQ(sub_instance.periodic_packings().size(), instance.periodic_packings().size());
    for (std::size_t packing_pos = 0;
            packing_pos < sub_instance.periodic_packings().size();
            ++packing_pos) {
        const PeriodicItemPacking& packing = instance.periodic_packings()[packing_pos];
        const PeriodicItemPacking& sub_packing = sub_instance.periodic_packings()[packing_pos];
        ASSERT_EQ(sub_packing.items.size(), packing.items.size());
        for (std::size_t item_pos = 0; item_pos < packing.items.size(); ++item_pos) {
            EXPECT_EQ(sub_packing.items[item_pos].item_type_id, sub_item_type_id_1);
            EXPECT_EQ(sub_packing.items[item_pos].angle, packing.items[item_pos].angle);
            EXPECT_EQ(sub_packing.items[item_pos].mirror, packing.items[item_pos].mirror);
            EXPECT_TRUE(shape::equal(sub_packing.items[item_pos].bl_corner.x, packing.items[item_pos].bl_corner.x));
            EXPECT_TRUE(shape::equal(sub_packing.items[item_pos].bl_corner.y, packing.items[item_pos].bl_corner.y));
        }
        EXPECT_TRUE(shape::equal(sub_packing.vector_1.x, packing.vector_1.x));
        EXPECT_TRUE(shape::equal(sub_packing.vector_1.y, packing.vector_1.y));
        EXPECT_TRUE(shape::equal(sub_packing.vector_2.x, packing.vector_2.x));
        EXPECT_TRUE(shape::equal(sub_packing.vector_2.y, packing.vector_2.y));
    }

    // Sub-instance without that item type: no periodic packing.
    InstanceBuilder sub_instance_builder_2;
    sub_instance_builder_2.add_bin_type(shape::build_rectangle(2000, 2000));
    sub_instance_builder_2.add_item_type(instance, item_type_id_0);
    Instance sub_instance_2 = sub_instance_builder_2.build();
    EXPECT_TRUE(sub_instance_2.periodic_packings().empty());
}

struct ComputeLargestEmptySideRectangleTestParams
{
    std::string name;

    Shape shape;

    BoundingBoxSide side;

    /** Expected rectangle; empty if no rectangle is expected. */
    AxisAlignedBoundingBox expected_rectangle;

    /**
     * If true, only check that the rectangle is non-empty and empty of the
     * shape, not its exact position (circular arcs are approximated).
     */
    bool approximate = false;

    friend std::ostream& operator<<(
            std::ostream& os,
            const ComputeLargestEmptySideRectangleTestParams& params)
    {
        return os << params.name;
    }
};

class ComputeLargestEmptySideRectangleTest: public testing::TestWithParam<ComputeLargestEmptySideRectangleTestParams> { };

TEST_P(ComputeLargestEmptySideRectangleTest, ComputeLargestEmptySideRectangle)
{
    ComputeLargestEmptySideRectangleTestParams params = GetParam();
    ShapeWithHoles shape_with_holes;
    shape_with_holes.shape = params.shape;
    AxisAlignedBoundingBox rectangle = compute_largest_empty_side_rectangle(
            shape_with_holes, params.side, 1.0);
    std::cout << "rectangle " << rectangle.x_min << " " << rectangle.x_max
        << " " << rectangle.y_min << " " << rectangle.y_max << std::endl;

    bool expected_empty = params.expected_rectangle.x_min > params.expected_rectangle.x_max;
    if (expected_empty) {
        EXPECT_GT(rectangle.x_min, rectangle.x_max);
        return;
    }
    ASSERT_LT(rectangle.x_min, rectangle.x_max);
    ASSERT_LT(rectangle.y_min, rectangle.y_max);
    // The rectangle is empty of the shape.
    EXPECT_FALSE(shape::intersect(params.shape, shape::build_rectangle(rectangle), true));
    if (params.approximate) {
        EXPECT_FALSE(shape::strictly_lesser(rectangle.x_min, params.expected_rectangle.x_min));
        EXPECT_FALSE(shape::strictly_greater(rectangle.x_max, params.expected_rectangle.x_max));
        EXPECT_FALSE(shape::strictly_lesser(rectangle.y_min, params.expected_rectangle.y_min));
        EXPECT_FALSE(shape::strictly_greater(rectangle.y_max, params.expected_rectangle.y_max));
    } else {
        EXPECT_TRUE(shape::equal(rectangle.x_min, params.expected_rectangle.x_min));
        EXPECT_TRUE(shape::equal(rectangle.x_max, params.expected_rectangle.x_max));
        EXPECT_TRUE(shape::equal(rectangle.y_min, params.expected_rectangle.y_min));
        EXPECT_TRUE(shape::equal(rectangle.y_max, params.expected_rectangle.y_max));
    }
}

namespace
{

// L-shape with arms of width 50 along the bottom and the left sides.
Shape l_shape()
{
    return shape::build_shape({{0, 0}, {700, 0}, {700, 50}, {50, 50}, {50, 550}, {0, 550}});
}

// U-shape with arms of width 50, opening upwards.
Shape u_shape()
{
    return shape::build_shape({{0, 0}, {300, 0}, {300, 200}, {250, 200}, {250, 50}, {50, 50}, {50, 200}, {0, 200}});
}

// Square with a quarter disk of radius 50 removed at its top-right corner.
Shape notched_square()
{
    return shape::build_shape({{0, 0}, {100, 0}, {100, 50}, {100, 100, -1}, {50, 100}, {0, 100}});
}

AxisAlignedBoundingBox no_rectangle()
{
    return AxisAlignedBoundingBox();
}

}

INSTANTIATE_TEST_SUITE_P(
        Irregular,
        ComputeLargestEmptySideRectangleTest,
        testing::ValuesIn(std::vector<ComputeLargestEmptySideRectangleTestParams>{
            {"LShapeTop", l_shape(), BoundingBoxSide::Top, {50, 700, 50, 550}},
            {"LShapeRight", l_shape(), BoundingBoxSide::Right, {50, 700, 50, 550}},
            {"LShapeBottom", l_shape(), BoundingBoxSide::Bottom, no_rectangle()},
            {"LShapeLeft", l_shape(), BoundingBoxSide::Left, no_rectangle()},
            {"UShapeTop", u_shape(), BoundingBoxSide::Top, {50, 250, 50, 200}},
            {"UShapeBottom", u_shape(), BoundingBoxSide::Bottom, no_rectangle()},
            {"UShapeLeft", u_shape(), BoundingBoxSide::Left, no_rectangle()},
            {"UShapeRight", u_shape(), BoundingBoxSide::Right, no_rectangle()},
            {"RectangleTop", shape::build_rectangle(300, 200), BoundingBoxSide::Top, no_rectangle()},
            {"RectangleLeft", shape::build_rectangle(300, 200), BoundingBoxSide::Left, no_rectangle()},
            {"TriangleTop", shape::build_shape({{0, 0}, {100, 0}, {0, 100}}), BoundingBoxSide::Top, no_rectangle()},
            {"NotchedSquareTop", notched_square(), BoundingBoxSide::Top, {50, 100, 50, 100}, true},
            {"NotchedSquareRight", notched_square(), BoundingBoxSide::Right, {50, 100, 50, 100}, true},
            {"NotchedSquareBottom", notched_square(), BoundingBoxSide::Bottom, no_rectangle()},
        }),
        testing::PrintToStringParamName());

namespace
{

/**
 * Check that no two items of a 3 x 3 tiling of a periodic packing are closer
 * than the item-item minimum spacing.
 */
void expect_no_overlap_in_tiling(
        const Instance& instance,
        const PeriodicItemPacking& packing)
{
    std::vector<ShapeWithHoles> tiling;
    for (int n = -1; n <= 1; ++n) {
        for (int m = -1; m <= 1; ++m) {
            for (const SolutionItem& item: packing.items) {
                ShapeWithHoles item_shape = instance.item_shape_scaled(
                        item.item_type_id, 0, item.angle, item.mirror);
                item_shape.shift(
                        item.bl_corner.x + n * packing.vector_1.x + m * packing.vector_2.x,
                        item.bl_corner.y + n * packing.vector_1.y + m * packing.vector_2.y);
                tiling.push_back(item_shape);
            }
        }
    }
    LengthDbl spacing_scaled = instance.item_spacing_scaled();
    std::vector<ShapeWithHoles> inflated_tiling;
    std::vector<AxisAlignedBoundingBox> aabbs;
    for (const ShapeWithHoles& item_shape: tiling) {
        inflated_tiling.push_back(shape::inflate(item_shape, spacing_scaled / 2 * 0.999));
        aabbs.push_back(inflated_tiling.back().compute_min_max());
    }
    for (std::size_t pos_1 = 0; pos_1 < tiling.size(); ++pos_1) {
        for (std::size_t pos_2 = pos_1 + 1; pos_2 < tiling.size(); ++pos_2) {
            if (aabbs[pos_1].x_max <= aabbs[pos_2].x_min
                    || aabbs[pos_2].x_max <= aabbs[pos_1].x_min
                    || aabbs[pos_1].y_max <= aabbs[pos_2].y_min
                    || aabbs[pos_2].y_max <= aabbs[pos_1].y_min) {
                continue;
            }
            EXPECT_FALSE(shape::intersect(inflated_tiling[pos_1], inflated_tiling[pos_2], true));
        }
    }
}

double periodic_packing_density(
        const Instance& instance,
        const PeriodicItemPacking& packing)
{
    AreaDbl item_area = 0.0;
    for (const SolutionItem& item: packing.items)
        item_area += instance.item_type(item.item_type_id).area_scaled;
    return item_area / std::abs(
            packing.vector_1.x * packing.vector_2.y
            - packing.vector_1.y * packing.vector_2.x);
}

}

// Two L-shapes wrap around a square: InstanceBuilder::build() generates
// periodic packings of the two item types, made of a square and two
// L-shapes in rotations r and r + 180, without overlap.
TEST(ComputeWrappingPeriodicPackingsTest, LShapesAroundSquare)
{
    LengthDbl spacing = 10;
    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(shape::build_rectangle(3000, 1500));
    instance_builder.set_item_item_minimum_spacing(spacing);
    ItemTypeId l_shape_id = instance_builder.add_item_type(
            {{shape::build_shape({{0, 0}, {700, 0}, {700, 50}, {50, 50}, {50, 550}, {0, 550}})}});
    instance_builder.add_item_type_allowed_rotation(l_shape_id, 0.0, 0.0, false);
    instance_builder.add_item_type_allowed_rotation(l_shape_id, 180.0, 180.0, false);
    instance_builder.set_item_type_copies(l_shape_id, 20);
    ItemTypeId square_id = instance_builder.add_item_type(
            {{shape::build_rectangle(400, 400)}});
    instance_builder.set_item_type_copies(square_id, 20);
    Instance instance = instance_builder.build();

    std::vector<PeriodicItemPacking> wrapping_packings;
    for (const PeriodicItemPacking& packing: instance.periodic_packings())
        if (!periodic_packing_has_single_item_type(packing))
            wrapping_packings.push_back(packing);
    ASSERT_FALSE(wrapping_packings.empty());

    double best_density = 0.0;
    for (const PeriodicItemPacking& packing: wrapping_packings) {
        ASSERT_EQ(packing.items.size(), 3);
        ItemPos number_of_l_shapes = 0;
        ItemPos number_of_squares = 0;
        for (const SolutionItem& item: packing.items) {
            if (item.item_type_id == l_shape_id)
                number_of_l_shapes++;
            if (item.item_type_id == square_id)
                number_of_squares++;
        }
        EXPECT_EQ(number_of_l_shapes, 2);
        EXPECT_EQ(number_of_squares, 1);
        expect_no_overlap_in_tiling(instance, packing);
        best_density = (std::max)(best_density, periodic_packing_density(instance, packing));
    }
    std::cout << "best density " << best_density << std::endl;
    EXPECT_GT(best_density, 0.5);
}


// alma_02 (fontanf/packingsolver data/irregular/alma2025): two L-shapes
// wrap around the interlocked pair of the other item type (two copies in
// rotations r and r + 180). The best periodic packing of a cell of the pair
// and two L-shapes has a density of about 0.705, and the two L-shapes form
// an exact 760 x 550 rectangle: neither of them sticks out of the other one.
TEST(ComputeWrappingPeriodicPackingsTest, LShapesAroundPairs)
{
    InstanceBuilder instance_builder;
    instance_builder.read("data/irregular/tests/wrapping_l_shapes_around_pairs.json");
    Instance instance = instance_builder.build();

    const PeriodicItemPacking* best_packing = nullptr;
    double best_density = 0.0;
    for (const PeriodicItemPacking& packing: instance.periodic_packings()) {
        if (periodic_packing_has_single_item_type(packing))
            continue;
        ItemPos number_of_l_shapes = 0;
        for (const SolutionItem& item: packing.items)
            if (item.item_type_id == 0)
                number_of_l_shapes++;
        EXPECT_EQ(number_of_l_shapes, 2);
        double density = periodic_packing_density(instance, packing);
        if (best_packing == nullptr
                || shape::strictly_greater(density, best_density)
                || (shape::equal(density, best_density)
                    && shape::strictly_lesser(
                        (packing.aabb_scaled.x_max - packing.aabb_scaled.x_min)
                        * (packing.aabb_scaled.y_max - packing.aabb_scaled.y_min),
                        (best_packing->aabb_scaled.x_max - best_packing->aabb_scaled.x_min)
                        * (best_packing->aabb_scaled.y_max - best_packing->aabb_scaled.y_min)))) {
            best_density = density;
            best_packing = &packing;
        }
    }
    ASSERT_NE(best_packing, nullptr);
    std::cout << "best density " << best_density << std::endl;
    EXPECT_EQ(best_packing->items.size(), 4);
    EXPECT_GT(best_density, 0.70);
    double scale_value = instance.parameters().scale_value;
    EXPECT_NEAR((best_packing->aabb_scaled.x_max - best_packing->aabb_scaled.x_min) / scale_value, 760, 1e-3);
    EXPECT_NEAR((best_packing->aabb_scaled.y_max - best_packing->aabb_scaled.y_min) / scale_value, 550, 1e-3);
    expect_no_overlap_in_tiling(instance, *best_packing);
}

// The items of the periodic blocks are inside the blocks, including for the
// periodic packings of compute_wrapping_periodic_packings, whose unit cell
// doesn't start at (0, 0).
TEST(ComputeWrappingPeriodicPackingsTest, BlockItemsInsideBlocks)
{
    InstanceBuilder instance_builder;
    instance_builder.read("data/irregular/tests/wrapping_l_shapes_around_pairs.json");
    Instance instance = instance_builder.build();
    auto all_item_type_rotations = compute_item_type_rotations(instance);
    std::vector<PeriodicBlock> blocks = compute_periodic_blocks(
            instance,
            instance.periodic_packings(),
            all_item_type_rotations[instance.bin_type_id(0)]);

    ItemPos number_of_wrapping_blocks = 0;
    for (const PeriodicBlock& block: blocks) {
        if (block.item_copies.size() > 1)
            number_of_wrapping_blocks++;
        for (const SolutionItem& item: block.items) {
            ShapeWithHoles item_shape = instance.item_shape_scaled(
                    item.item_type_id, 0, item.angle, item.mirror);
            AxisAlignedBoundingBox aabb = item_shape.compute_min_max();
            aabb.shift(item.bl_corner);
            EXPECT_FALSE(shape::strictly_lesser(aabb.x_min, 0.0));
            EXPECT_FALSE(shape::strictly_lesser(aabb.y_min, 0.0));
            EXPECT_FALSE(shape::strictly_greater(aabb.x_max, block.bx));
            EXPECT_FALSE(shape::strictly_greater(aabb.y_max, block.by));
        }
    }
    EXPECT_GT(number_of_wrapping_blocks, 0);
}

// The items of the periodic packings of compute_wrapping_periodic_packings
// keep the rotations as given in the allowed rotations of their item types:
// allowed rotations given with negative angles used to be normalized to
// [0, 360), giving rotations which are not allowed.
TEST(ComputeWrappingPeriodicPackingsTest, AllowedRotationsWithNegativeAngles)
{
    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(shape::build_rectangle(3000, 1500));
    instance_builder.set_item_item_minimum_spacing(10);
    ItemTypeId l_shape_id = instance_builder.add_item_type(
            {{shape::build_shape({{0, 0}, {700, 0}, {700, 50}, {50, 50}, {50, 550}, {0, 550}})}});
    instance_builder.add_item_type_allowed_rotation(l_shape_id, -90.0, -90.0, false);
    instance_builder.add_item_type_allowed_rotation(l_shape_id, 90.0, 90.0, false);
    instance_builder.set_item_type_copies(l_shape_id, 20);
    ItemTypeId square_id = instance_builder.add_item_type(
            {{shape::build_rectangle(400, 400)}});
    instance_builder.set_item_type_copies(square_id, 20);
    Instance instance = instance_builder.build();

    ItemPos number_of_wrapping_packings = 0;
    for (const PeriodicItemPacking& packing: instance.periodic_packings()) {
        if (!periodic_packing_has_single_item_type(packing))
            number_of_wrapping_packings++;
        for (const SolutionItem& item: packing.items) {
            EXPECT_TRUE(instance.item_type(item.item_type_id).is_rotation_allowed(item.angle, item.mirror))
                << "item type " << item.item_type_id << " angle " << item.angle << " mirror " << item.mirror;
        }
    }
    EXPECT_GT(number_of_wrapping_packings, 0);
}

// alma_02 (fontanf/packingsolver data/irregular/alma2025), item type 4: its
// periodic packings have a vector_2 which is not perpendicular to vector_1.
// The blocks are built from the periodic packings clipped to the bin (see
// 'add_clipped_lattice_blocks'), so they contain partial unit cells: the
// items of the blocks must still be inside the blocks, and must not be
// closer than the item-item minimum spacing.
TEST(ComputePeriodicBlocksTest, ClippedLatticeBlocks)
{
    InstanceBuilder instance_builder;
    instance_builder.read("data/irregular/tests/periodic_blocks_lowered_items.json");
    Instance instance = instance_builder.build();
    auto all_item_type_rotations = compute_item_type_rotations(instance);
    std::vector<PeriodicBlock> blocks = compute_periodic_blocks(
            instance,
            instance.periodic_packings(),
            all_item_type_rotations[instance.bin_type_id(0)]);
    ASSERT_FALSE(blocks.empty());

    // Largest block of each periodic packing.
    std::map<int, const PeriodicBlock*> largest_blocks;
    for (const PeriodicBlock& block: blocks) {
        if (block.packing_id < 0)
            continue;
        auto it = largest_blocks.find(block.packing_id);
        if (it == largest_blocks.end() || block.number_of_items > it->second->number_of_items)
            largest_blocks[block.packing_id] = &block;
    }
    ASSERT_FALSE(largest_blocks.empty());

    // The largest blocks of some periodic packings only, to keep the test
    // fast.
    const int maximum_number_of_checked_blocks = 8;
    int number_of_checked_blocks = 0;
    LengthDbl spacing_scaled = instance.item_spacing_scaled();
    for (const auto& p: largest_blocks) {
        if (number_of_checked_blocks >= maximum_number_of_checked_blocks)
            break;
        number_of_checked_blocks++;
        const PeriodicBlock& block = *p.second;
        std::vector<ShapeWithHoles> inflated_shapes;
        std::vector<AxisAlignedBoundingBox> aabbs;
        for (const SolutionItem& item: block.items) {
            ShapeWithHoles item_shape = instance.item_shape_scaled(
                    item.item_type_id, 0, item.angle, item.mirror);
            item_shape.shift(item.bl_corner.x, item.bl_corner.y);
            AxisAlignedBoundingBox aabb = item_shape.compute_min_max();
            EXPECT_FALSE(shape::strictly_lesser(aabb.x_min, 0.0));
            EXPECT_FALSE(shape::strictly_lesser(aabb.y_min, 0.0));
            EXPECT_FALSE(shape::strictly_greater(aabb.x_max, block.bx));
            EXPECT_FALSE(shape::strictly_greater(aabb.y_max, block.by));
            inflated_shapes.push_back(shape::inflate(item_shape, spacing_scaled / 2 * 0.999));
            aabbs.push_back(inflated_shapes.back().compute_min_max());
        }
        for (std::size_t pos_1 = 0; pos_1 < inflated_shapes.size(); ++pos_1) {
            for (std::size_t pos_2 = pos_1 + 1; pos_2 < inflated_shapes.size(); ++pos_2) {
                if (aabbs[pos_1].x_max <= aabbs[pos_2].x_min
                        || aabbs[pos_2].x_max <= aabbs[pos_1].x_min
                        || aabbs[pos_1].y_max <= aabbs[pos_2].y_min
                        || aabbs[pos_2].y_max <= aabbs[pos_1].y_min) {
                    continue;
                }
                EXPECT_FALSE(shape::intersect(inflated_shapes[pos_1], inflated_shapes[pos_2], true));
            }
        }
    }
}

// alma_02 (fontanf/packingsolver data/irregular/alma2025), item types 0
// (L-shapes) and 3: two L-shapes framing a pair of items of type 3 form an
// exact 820 x 550 rectangle, the height of an L-shape. The blocks built from
// the periodic packing clipped to the bin must contain this one: a block
// whose items have the same right as an item above it used to be skipped.
TEST(ComputePeriodicBlocksTest, WrappingBlockExactRectangle)
{
    InstanceBuilder instance_builder;
    instance_builder.read("data/irregular/tests/wrapping_l_shapes_around_type_3_pairs.json");
    Instance instance = instance_builder.build();
    auto all_item_type_rotations = compute_item_type_rotations(instance);
    std::vector<PeriodicBlock> blocks = compute_periodic_blocks(
            instance,
            instance.periodic_packings(),
            all_item_type_rotations[instance.bin_type_id(0)]);

    double scale_value = instance.parameters().scale_value;
    const PeriodicBlock* smallest_block = nullptr;
    for (const PeriodicBlock& block: blocks) {
        ItemPos number_of_l_shapes = 0;
        ItemPos number_of_others = 0;
        for (const auto& kv: block.item_copies) {
            if (kv.first == 0)
                number_of_l_shapes += kv.second;
            else
                number_of_others += kv.second;
        }
        if (number_of_l_shapes == 2 && number_of_others == 2
                && (smallest_block == nullptr
                    || block.bx * block.by < smallest_block->bx * smallest_block->by)) {
            smallest_block = &block;
        }
    }
    ASSERT_NE(smallest_block, nullptr);
    // Dimensions of the rectangle plus the trailing item-item spacing of a
    // block.
    EXPECT_NEAR(smallest_block->bx / scale_value, 820 + 10, 1e-3);
    EXPECT_NEAR(smallest_block->by / scale_value, 550 + 10, 1e-3);
}

// Blocks framed by L-shapes (alma_02, item types 0 and 3): the framed blocks
// are built from the bounding boxes of the blocks and the empty rectangles
// of the L-shapes only, so their items must be checked: they must be inside
// the blocks, and must not be closer than the item-item minimum spacing.
TEST(ComputePeriodicBlocksTest, FramedBlocks)
{
    InstanceBuilder instance_builder;
    instance_builder.read("data/irregular/tests/wrapping_l_shapes_around_type_3_pairs.json");
    Instance instance = instance_builder.build();
    auto all_item_type_rotations = compute_item_type_rotations(instance);
    std::vector<PeriodicBlock> blocks = compute_periodic_blocks(
            instance,
            instance.periodic_packings(),
            all_item_type_rotations[instance.bin_type_id(0)]);

    LengthDbl spacing_scaled = instance.item_spacing_scaled();
    ItemPos number_of_framed_blocks = 0;
    for (const PeriodicBlock& block: blocks) {
        if (block.packing_id != -1 || block.item_copies.size() < 2)
            continue;
        number_of_framed_blocks++;
        if (block.number_of_items > 12)
            continue;
        std::vector<ShapeWithHoles> inflated_shapes;
        std::vector<AxisAlignedBoundingBox> aabbs;
        for (const SolutionItem& item: block.items) {
            ShapeWithHoles item_shape = instance.item_shape_scaled(
                    item.item_type_id, 0, item.angle, item.mirror);
            item_shape.shift(item.bl_corner.x, item.bl_corner.y);
            AxisAlignedBoundingBox aabb = item_shape.compute_min_max();
            EXPECT_FALSE(shape::strictly_lesser(aabb.x_min, 0.0));
            EXPECT_FALSE(shape::strictly_lesser(aabb.y_min, 0.0));
            EXPECT_FALSE(shape::strictly_greater(aabb.x_max, block.bx));
            EXPECT_FALSE(shape::strictly_greater(aabb.y_max, block.by));
            inflated_shapes.push_back(shape::inflate(item_shape, spacing_scaled / 2 * 0.999));
            aabbs.push_back(inflated_shapes.back().compute_min_max());
        }
        for (std::size_t pos_1 = 0; pos_1 < inflated_shapes.size(); ++pos_1) {
            for (std::size_t pos_2 = pos_1 + 1; pos_2 < inflated_shapes.size(); ++pos_2) {
                if (aabbs[pos_1].x_max <= aabbs[pos_2].x_min
                        || aabbs[pos_2].x_max <= aabbs[pos_1].x_min
                        || aabbs[pos_1].y_max <= aabbs[pos_2].y_min
                        || aabbs[pos_2].y_max <= aabbs[pos_1].y_min) {
                    continue;
                }
                EXPECT_FALSE(shape::intersect(inflated_shapes[pos_1], inflated_shapes[pos_2], true));
            }
        }
    }
    std::cout << "framed blocks " << number_of_framed_blocks << " / " << blocks.size() << std::endl;
    EXPECT_GT(number_of_framed_blocks, 0);
}
