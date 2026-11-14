#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"
#include "box/solution_builder.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::box;

struct BoxStaticStabilityItem
{
    Box box;
    Point bl_corner;

    /** Weight; the volume if negative. */
    Weight weight = -1;
};

struct BoxStaticStabilityTestParams
{
    std::string name;
    Box bin;
    std::vector<BoxStaticStabilityItem> items;
    bool expected_stable;
};

inline std::ostream& operator<<(std::ostream& os, const BoxStaticStabilityTestParams& test_params)
{
    os << test_params.name;
    return os;
}

class BoxStaticStabilityTest: public testing::TestWithParam<BoxStaticStabilityTestParams> { };

TEST_P(BoxStaticStabilityTest, BoxStaticStability)
{
    BoxStaticStabilityTestParams test_params = GetParam();
    for (bool static_stability: {false, true}) {
        InstanceBuilder instance_builder;
        instance_builder.set_objective(packingsolver::Objective::Knapsack);
        instance_builder.set_static_stability(static_stability);
        instance_builder.add_bin_type(test_params.bin.x, test_params.bin.y, test_params.bin.z);
        for (const BoxStaticStabilityItem& item: test_params.items) {
            ItemTypeId item_type_id = instance_builder.add_item_type(item.box.x, item.box.y, item.box.z);
            if (item.weight >= 0)
                instance_builder.set_item_type_weight(item_type_id, item.weight);
        }
        Instance instance = instance_builder.build();

        SolutionBuilder solution_builder(instance);
        BinPos bin_pos = solution_builder.add_bin(0, 1);
        for (ItemTypeId item_type_id = 0;
                item_type_id < (ItemTypeId)test_params.items.size();
                ++item_type_id) {
            solution_builder.add_item(
                    bin_pos,
                    item_type_id,
                    test_params.items[item_type_id].bl_corner,
                    Rotation::XYZ);
        }
        Solution solution = solution_builder.build();

        // The static stability is only checked if it is required.
        bool expected_stable = !static_stability || test_params.expected_stable;
        EXPECT_EQ(solution.stable(), expected_stable);
        EXPECT_EQ(solution.bin(0).stable, expected_stable);
        EXPECT_EQ(solution.feasible(), expected_stable);
    }
}

INSTANTIATE_TEST_SUITE_P(
        Box,
        BoxStaticStabilityTest,
        testing::ValuesIn(std::vector<BoxStaticStabilityTestParams>{
            {
                "items_on_the_floor",
                {20, 10, 10},
                {{{10, 10, 10}, {0, 0, 0}}, {{10, 10, 10}, {10, 0, 0}}},
                true,
            }, {
                "tower",
                {10, 10, 20},
                {{{10, 10, 10}, {0, 0, 0}}, {{10, 10, 10}, {0, 0, 10}}},
                true,
            }, {
                "floating_item",
                {10, 10, 20},
                {{{10, 10, 5}, {0, 0, 10}}},
                false,
            }, {
                // The center of gravity of the top item is above its support.
                "overhang",
                {20, 10, 20},
                {{{10, 10, 10}, {0, 0, 0}}, {{10, 10, 10}, {4, 0, 10}}},
                true,
            }, {
                // The center of gravity of the top item is beyond its
                // support.
                "overhang_too_large",
                {20, 10, 20},
                {{{10, 10, 10}, {0, 0, 0}}, {{10, 10, 10}, {6, 0, 10}}},
                false,
            }, {
                // A plank on two pillars.
                "bridge",
                {30, 10, 20},
                {
                    {{5, 10, 10}, {0, 0, 0}},
                    {{5, 10, 10}, {25, 0, 0}},
                    {{30, 10, 5}, {0, 0, 10}},
                },
                true,
            }, {
                // The middle item is stable on its own, and so is the top
                // item on the middle item, and the top item is light enough
                // for the middle item not to tip.
                "light_item_on_an_overhang",
                {20, 10, 30},
                {
                    {{10, 10, 10}, {0, 0, 0}},
                    {{10, 10, 10}, {4, 0, 10}},
                    {{4, 10, 10}, {10, 0, 20}},
                },
                true,
            }, {
                // Same layout with a heavy top item: it tips the middle item,
                // although each item is stable on its own support.
                "heavy_item_on_an_overhang",
                {20, 10, 30},
                {
                    {{10, 10, 10}, {0, 0, 0}},
                    {{10, 10, 10}, {4, 0, 10}},
                    {{4, 10, 10}, {10, 0, 20}, 10000},
                },
                false,
            }}));

TEST(Box, ItemTypeWeightDefaultsToTheVolume)
{
    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(10, 10, 10);
    instance_builder.add_item_type(2, 3, 4);
    ItemTypeId item_type_id = instance_builder.add_item_type(2, 3, 4);
    instance_builder.set_item_type_weight(item_type_id, 7);
    Instance instance = instance_builder.build();
    EXPECT_EQ(instance.item_type(0).weight, 24);
    EXPECT_EQ(instance.item_type(1).weight, 7);
}
