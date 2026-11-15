#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/optimize.hpp"
#include "box/solution_builder.hpp"
#include "box/static_stability.hpp"

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
            }, {
                // The center of gravity of the plank is beyond its support.
                "plank",
                {20, 10, 30},
                {{{10, 10, 10}, {0, 0, 0}}, {{18, 10, 5}, {2, 0, 10}}},
                false,
            }, {
                // Same plank with an item on its end above the support: the
                // center of gravity of the plank is still beyond its support,
                // but the item balances it.
                "counterweight",
                {20, 10, 30},
                {
                    {{10, 10, 10}, {0, 0, 0}},
                    {{18, 10, 5}, {2, 0, 10}},
                    {{8, 10, 10}, {2, 0, 15}},
                },
                true,
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

struct BoxDropItemsItem
{
    Box box;
    Point bl_corner;

    /** Expected z coordinate of the item after the drop. */
    Length expected_z;

    /** Weight; the volume if negative. */
    Weight weight = -1;
};

struct BoxDropItemsTestParams
{
    std::string name;
    Box bin;
    std::vector<BoxDropItemsItem> items;
    bool expected_stable;
};

inline std::ostream& operator<<(std::ostream& os, const BoxDropItemsTestParams& test_params)
{
    os << test_params.name;
    return os;
}

class BoxDropItemsTest: public testing::TestWithParam<BoxDropItemsTestParams> { };

TEST_P(BoxDropItemsTest, BoxDropItems)
{
    BoxDropItemsTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.set_static_stability(true);
    instance_builder.add_bin_type(test_params.bin.x, test_params.bin.y, test_params.bin.z);
    for (const BoxDropItemsItem& item: test_params.items) {
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

    Solution new_solution = drop_items(solution);
    EXPECT_EQ(new_solution.stable(), test_params.expected_stable);
    EXPECT_EQ(new_solution.profit(), solution.profit());
    ASSERT_EQ(new_solution.number_of_different_bins(), 1);
    const SolutionBin& bin = new_solution.bin(0);
    ASSERT_EQ(bin.items.size(), test_params.items.size());
    for (const SolutionItem& item: bin.items) {
        const BoxDropItemsItem& expected_item = test_params.items[item.item_type_id];
        EXPECT_EQ(item.bl_corner.x, expected_item.bl_corner.x);
        EXPECT_EQ(item.bl_corner.y, expected_item.bl_corner.y);
        EXPECT_EQ(item.bl_corner.z, expected_item.expected_z);
    }
}

INSTANTIATE_TEST_SUITE_P(
        Box,
        BoxDropItemsTest,
        testing::ValuesIn(std::vector<BoxDropItemsTestParams>{
            {
                "floating_item",
                {10, 10, 20},
                {{{10, 10, 5}, {0, 0, 10}, 0}},
                true,
            }, {
                // The items fall by more than their heights and keep their
                // order.
                "floating_tower",
                {10, 10, 100},
                {{{10, 10, 10}, {0, 0, 30}, 0}, {{10, 10, 10}, {0, 0, 60}, 10}},
                true,
            }, {
                // The item above the base lands on it, the other one on the
                // floor.
                "lower_supports",
                {20, 10, 50},
                {
                    {{10, 10, 10}, {0, 0, 0}, 0},
                    {{10, 10, 5}, {0, 0, 30}, 10},
                    {{10, 10, 5}, {10, 0, 30}, 0},
                },
                true,
            }, {
                // The item lands on the highest of its supports.
                "highest_support",
                {20, 10, 50},
                {
                    {{10, 10, 10}, {0, 0, 0}, 0},
                    {{10, 10, 20}, {10, 0, 0}, 0},
                    {{20, 10, 5}, {0, 0, 40}, 20},
                },
                true,
            }, {
                // The bin is stable: the item without weight stays where it
                // is.
                "stable_bin",
                {10, 10, 30},
                {{{10, 10, 10}, {0, 0, 0}, 0}, {{10, 10, 5}, {0, 0, 20}, 20, 0}},
                true,
            }, {
                // No item floats, but the top item tips.
                "overhang_too_large",
                {20, 10, 20},
                {{{10, 10, 10}, {0, 0, 0}, 0}, {{10, 10, 10}, {6, 0, 10}, 10}},
                false,
            }}));

struct BoxRemoveUnbalancedItemsItem
{
    Box box;
    Point bl_corner;

    /** True iff the item is expected to remain in the bin. */
    bool expected_kept;

    /** Weight; the volume if negative. */
    Weight weight = -1;
};

struct BoxRemoveUnbalancedItemsTestParams
{
    std::string name;
    Box bin;
    std::vector<BoxRemoveUnbalancedItemsItem> items;
};

inline std::ostream& operator<<(std::ostream& os, const BoxRemoveUnbalancedItemsTestParams& test_params)
{
    os << test_params.name;
    return os;
}

class BoxRemoveUnbalancedItemsTest: public testing::TestWithParam<BoxRemoveUnbalancedItemsTestParams> { };

TEST_P(BoxRemoveUnbalancedItemsTest, BoxRemoveUnbalancedItems)
{
    BoxRemoveUnbalancedItemsTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.set_static_stability(true);
    instance_builder.add_bin_type(test_params.bin.x, test_params.bin.y, test_params.bin.z);
    for (const BoxRemoveUnbalancedItemsItem& item: test_params.items) {
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

    Solution new_solution = remove_unbalanced_items(solution);
    EXPECT_TRUE(new_solution.stable());
    ASSERT_EQ(new_solution.number_of_different_bins(), 1);
    std::vector<bool> kept(test_params.items.size(), false);
    for (const SolutionItem& item: new_solution.bin(0).items) {
        kept[item.item_type_id] = true;
        // The remaining items keep their positions.
        EXPECT_EQ(item.bl_corner, test_params.items[item.item_type_id].bl_corner);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < (ItemTypeId)test_params.items.size();
            ++item_type_id) {
        EXPECT_EQ(kept[item_type_id], test_params.items[item_type_id].expected_kept)
            << "item type " << item_type_id;
    }
}

INSTANTIATE_TEST_SUITE_P(
        Box,
        BoxRemoveUnbalancedItemsTest,
        testing::ValuesIn(std::vector<BoxRemoveUnbalancedItemsTestParams>{
            {
                "stable_bin",
                {20, 10, 20},
                {{{10, 10, 10}, {0, 0, 0}, true}, {{10, 10, 10}, {4, 0, 10}, true}},
            }, {
                "floating_item",
                {10, 10, 20},
                {{{10, 10, 5}, {0, 0, 10}, false}},
            }, {
                "overhang_too_large",
                {20, 10, 20},
                {{{10, 10, 10}, {0, 0, 0}, true}, {{10, 10, 10}, {6, 0, 10}, false}},
            }, {
                // The item resting on the item which tips is removed with it.
                "item_on_an_overhang_too_large",
                {20, 10, 30},
                {
                    {{10, 10, 10}, {0, 0, 0}, true},
                    {{10, 10, 10}, {6, 0, 10}, false},
                    {{10, 10, 10}, {6, 0, 20}, false},
                },
            }, {
                // The stable stack next to the one which tips is kept.
                "other_stack_kept",
                {30, 10, 20},
                {
                    {{10, 10, 10}, {0, 0, 0}, true},
                    {{10, 10, 10}, {6, 0, 10}, false},
                    {{10, 10, 10}, {20, 0, 0}, true},
                    {{10, 10, 10}, {20, 0, 10}, true},
                },
            }}));
