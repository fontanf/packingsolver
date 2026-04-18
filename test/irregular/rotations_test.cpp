#include "irregular/rotations.hpp"

#include "shape/shape.hpp"
#include "shape/writer.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

// Return true if `angles` contains a value within 0.01° of `target_angle`
// (wrapping at 360°).  0.01° is well above floating-point noise from
// trigonometric calculations while tight enough to catch wrong-angle bugs.
bool contains_angle(
        const std::vector<Angle>& angles,
        Angle target_angle)
{
    constexpr Angle tolerance = 0.01;
    for (Angle angle: angles) {
        Angle diff = std::abs(angle - target_angle);
        diff = std::min(diff, 360.0 - diff);
        if (diff <= tolerance)
            return true;
    }
    return false;
}

}  // namespace

// ─── test parameters ──────────────────────────────────────────────────────────

struct BinFitRotationsTestParams
{
    std::string description;
    Shape shape;
    LengthDbl bin_width;
    LengthDbl bin_height;

    // Every angle listed here must appear in the output (within 0.01°).
    std::vector<Angle> expected_angles;
};

std::ostream& operator<<(std::ostream& os, const BinFitRotationsTestParams& params)
{
    os << params.description;
    return os;
}

// ─── test body ────────────────────────────────────────────────────────────────

class BinFitRotationsTest: public testing::TestWithParam<BinFitRotationsTestParams> {};

TEST_P(BinFitRotationsTest, BinFitRotations)
{
    const BinFitRotationsTestParams& params = GetParam();
    std::cout << "expected_angles:" << std::endl;
    for (Angle angle: params.expected_angles)
        std::cout << "- " << angle << std::endl;

    const std::vector<Angle> output = compute_bin_fit_rotations(
            params.shape,
            params.bin_width,
            params.bin_height);

    std::cout << "output:" << std::endl;
    for (Angle angle: output)
        std::cout << "- " << angle << std::endl;

    for (Angle expected_angle: params.expected_angles) {
        EXPECT_TRUE(contains_angle(output, expected_angle))
            << "Expected angle=" << expected_angle << " not found in output";
    }

    for (Angle angle: output) {
        EXPECT_TRUE(contains_angle(params.expected_angles, angle))
            << "Angle=" << angle << " not found in expected angles";
    }

    //shape::Writer writer;
    //Shape bin_outline = shape::build_rectangle(params.bin_width, params.bin_height);
    //writer.add_shape(bin_outline, "bin");
    //for (Angle angle: output) {
    //    Shape shape = params.shape.rotate(angle);
    //    AxisAlignedBoundingBox aabb = shape.compute_min_max();
    //    shape.shift(-aabb.x_min, -aabb.y_min);
    //    std::string label = "Angle " + std::to_string(angle);
    //    writer.add_shape(shape, label);
    //}
    //writer.write_json("item_type_rotations_output.json");
}

// ─── test cases ───────────────────────────────────────────────────────────────

INSTANTIATE_TEST_SUITE_P(
    BinFitRotations,
    BinFitRotationsTest,
    testing::ValuesIn(std::vector<BinFitRotationsTestParams>{

        // ── exact fits at discrete angles ──────────────────────────────────

        // 4×3 rectangle in a 3×4 bin: at 90° the width becomes 3 = bin_width
        // and the height becomes 4 = bin_height.
        {
            "4x3_in_3x4_exact_fit_at_90deg",
            shape::build_rectangle(4, 3),
            3, 4,
            {90.0, 270.0},
        },

        // ── full-rotation rectangle, only axis-aligned fits ────────────────

        // 4×3 rectangle in a 4×3 bin.  At 73.74° the width touches 4 but the
        // height is ≈4.68 > 3, so that angle is rejected.  The only valid
        // placements are at 0° and 180°.
        {
            "4x3_in_4x3_only_axis_aligned",
            shape::build_rectangle(4, 3),
            4, 3,
            {0.0, 180.0},
        },

        // ── non-axis-aligned critical angles ──────────────────────────────

        // 101×1 item in a 100×50 bin: no axis-aligned rotation fits.
        //
        // Width = 100 is achieved at θ ≈ 8.656° (θ ∈ (0°,90°)):
        //   101·cos θ + 1·sin θ = 100
        //   → θ = atan2(1, 101) + acos(100/√10202) ≈ 8.656°
        //
        // Height = 50 is achieved at θ ≈ 29.104° (θ ∈ (0°,90°)):
        //   1·cos θ + 101·sin θ = 50
        //   → θ = atan2(101, 1) − acos(50/√10202) ≈ 29.104°
        //
        // The function returns all 8 critical angles (the two base angles plus
        // their reflections through 180° and 360°):
        //   width=100:  8.656°, 171.344°, 188.656°, 351.344°
        //   height=50: 29.104°, 150.896°, 209.104°, 330.896°
        {
            "101x1_in_100x50_non_axis_aligned",
            shape::build_rectangle(101, 1),
            100, 50,
            {8.656, 29.104, 150.896, 171.344, 188.656, 209.104, 330.896, 351.344},
        },

        // ── item that never fits ───────────────────────────────────────────

        // 101×101 square in a 100×100 bin.  The minimum bounding box is
        // 101×101 at 0° and grows at all other angles, so no rotation fits.
        {
            "101x101_in_100x100_never_fits",
            shape::build_rectangle(101, 101),
            100, 100,
            {},
        },

    }),
    [](const testing::TestParamInfo<BinFitRotationsTestParams>& info) {
        return info.param.description;
    });
