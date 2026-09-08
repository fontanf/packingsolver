#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/item_subset_incompatibility.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangle;

TEST(RectangleTripletIncompatibility, ThreeEqualSquaresTooBigForTheBin)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.add_item_type(6, 6, true);
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = {{0, 1}, {1, 1}, {2, 1}};
    std::vector<IncompatibleTriplet> triplets = find_incompatible_triplets(instance, 0, selected_items);

    ASSERT_EQ(triplets.size(), 1);
    EXPECT_EQ(triplets[0].item_type_ids[0], 0);
    EXPECT_EQ(triplets[0].item_type_ids[1], 1);
    EXPECT_EQ(triplets[0].item_type_ids[2], 2);
}

TEST(RectangleTripletIncompatibility, ThreeEqualSquaresFitViaPairSplit)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(4, 4, true);
    instance_builder.add_item_type(4, 4, true);
    instance_builder.add_item_type(4, 4, true);
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = {{0, 1}, {1, 1}, {2, 1}};
    std::vector<IncompatibleTriplet> triplets = find_incompatible_triplets(instance, 0, selected_items);

    EXPECT_EQ(triplets.size(), 0);
}

TEST(RectangleTripletIncompatibility, PairSplitShapesAllFailDespiteFittingArea)
{
    // Area 36 + 24 + 24 = 84 <= 100 (the bin's own area), so this
    // specifically exercises the pair-split shapes failing, not just a
    // trivial area/row/column argument.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.add_item_type(6, 4, true);
    instance_builder.add_item_type(6, 4, true);
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = {{0, 1}, {1, 1}, {2, 1}};
    std::vector<IncompatibleTriplet> triplets = find_incompatible_triplets(instance, 0, selected_items);

    ASSERT_EQ(triplets.size(), 1);
    EXPECT_EQ(triplets[0].item_type_ids[0], 0);
    EXPECT_EQ(triplets[0].item_type_ids[1], 1);
    EXPECT_EQ(triplets[0].item_type_ids[2], 2);
}

TEST(RectangleTripletIncompatibility, ExtraSmallItemDoesNotHideTheViolationOrGetFlaggedItself)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.add_item_type(6, 4, true);
    instance_builder.add_item_type(6, 4, true);
    instance_builder.add_item_type(1, 1, true);
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = {{0, 1}, {1, 1}, {2, 1}, {3, 1}};
    std::vector<IncompatibleTriplet> triplets = find_incompatible_triplets(instance, 0, selected_items);

    bool found_expected_triplet = false;
    for (const IncompatibleTriplet& triplet: triplets) {
        if (triplet.item_type_ids[0] == 0
                && triplet.item_type_ids[1] == 1
                && triplet.item_type_ids[2] == 2) {
            found_expected_triplet = true;
        }
        EXPECT_NE(triplet.item_type_ids[0], 3);
        EXPECT_NE(triplet.item_type_ids[1], 3);
        EXPECT_NE(triplet.item_type_ids[2], 3);
    }
    EXPECT_TRUE(found_expected_triplet);
}

TEST(RectangleTripletIncompatibility, RepeatedItemType)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Feasibility);
    instance_builder.add_bin_type(10, 10);
    instance_builder.add_item_type(6, 6, true);
    instance_builder.set_item_type_copies(0, 2);
    instance_builder.add_item_type(6, 4, true);
    Instance instance = instance_builder.build();

    std::vector<std::pair<ItemTypeId, ItemPos>> selected_items = {{0, 2}, {1, 1}};
    std::vector<IncompatibleTriplet> triplets = find_incompatible_triplets(instance, 0, selected_items);

    ASSERT_EQ(triplets.size(), 1);
    EXPECT_EQ(triplets[0].item_type_ids[0], 0);
    EXPECT_EQ(triplets[0].item_type_ids[1], 0);
    EXPECT_EQ(triplets[0].item_type_ids[2], 1);
}
