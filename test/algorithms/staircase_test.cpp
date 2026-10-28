#include "algorithms/staircase.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;

struct StaircaseTestParams
{
    /** Extents (x, y, z) of the boxes [0, x] x [0, y] x [0, z]. */
    std::vector<std::array<Length, 3>> extents;
};

void PrintTo(const StaircaseTestParams& params, std::ostream* os)
{
    for (const auto& extent: params.extents)
        *os << "(" << extent[0] << ", " << extent[1] << ", " << extent[2] << ") ";
    *os << "\n";
}

class StaircaseTest: public testing::TestWithParam<StaircaseTestParams> { };

TEST_P(StaircaseTest, Staircase)
{
    StaircaseTestParams test_params = GetParam();
    PrintTo(test_params, &std::cout);

    // Compare to the number of unit cells covered by the union.
    Length x_max = 0;
    Length y_max = 0;
    Length z_max = 0;
    for (const auto& extent: test_params.extents) {
        x_max = (std::max)(x_max, extent[0]);
        y_max = (std::max)(y_max, extent[1]);
        z_max = (std::max)(z_max, extent[2]);
    }
    Area expected_area = 0;
    Volume expected_volume = 0;
    for (Length x = 0; x < x_max; ++x) {
        for (Length y = 0; y < y_max; ++y) {
            bool covered_2d = false;
            for (const auto& extent: test_params.extents)
                if (x < extent[0] && y < extent[1])
                    covered_2d = true;
            if (covered_2d)
                expected_area++;
            for (Length z = 0; z < z_max; ++z) {
                bool covered_3d = false;
                for (const auto& extent: test_params.extents)
                    if (x < extent[0] && y < extent[1] && z < extent[2])
                        covered_3d = true;
                if (covered_3d)
                    expected_volume++;
            }
        }
    }

    Staircase2D staircase_2d;
    Staircase3D staircase_3d;
    for (const auto& extent: test_params.extents) {
        staircase_2d.add(extent[0], extent[1]);
        staircase_3d.add(extent[0], extent[1], extent[2]);
    }
    EXPECT_EQ(staircase_2d.area(), expected_area);
    EXPECT_EQ(staircase_3d.volume(), expected_volume);
}

INSTANTIATE_TEST_SUITE_P(
        Staircase,
        StaircaseTest,
        testing::ValuesIn(std::vector<StaircaseTestParams>{
            {{}},
            {{{3, 2, 1}}},
            // Dominated extent, before and after the dominating one.
            {{{2, 2, 2}, {3, 3, 3}, {1, 3, 2}}},
            // Same x, increasing y.
            {{{3, 1, 1}, {3, 2, 1}}},
            // Staircase.
            {{{5, 1, 1}, {1, 5, 1}, {3, 3, 1}, {2, 4, 1}}},
            // Extents in all the directions.
            {{{4, 1, 1}, {1, 4, 1}, {1, 1, 4}, {2, 2, 2}, {3, 2, 1}, {1, 2, 3}}},
            // An extent dominating some of the previous ones.
            {{{1, 4, 1}, {4, 1, 2}, {2, 2, 1}, {3, 3, 2}, {1, 1, 5}}},
            }));
