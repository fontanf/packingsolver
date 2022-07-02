#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, ApplyInsertion1)
{
    /**
     * |--------------------------|
     * |                          |
     * |                          |
     * |                          |
     * |                          |
     * |                          |
     * |                          |
     * |   |---|                  | 300
     * |---|   |                  | 200
     * | 0 | 1 |                  |
     * |---|---|------------------|
     * 0  300 700
     *
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(200, 300);
    instance.add_item_type(300, 400);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*root).number_of_bins, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*root).number_of_items, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*root).waste, 0);

    // Add item 0
    auto node_1 = branching_scheme.child(root, {0, -1, -1, 300, 200, 300, 3500, 3210, 0, 0});
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).z1, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).z2, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).x1_max, 3500);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).y2_max, 3210);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).number_of_bins, 1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).number_of_items, 1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).waste, 0);

    // Add item 1;
    auto node_2 = branching_scheme.child(node_1, {1, -1, 0, 700, 300, 700, 3800, 3210, 0, 0});
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).z1, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).z2, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).x1_max, 3800);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).y2_max, 3210);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).number_of_bins, 1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).number_of_items, 2);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 3210 * 700 - 200 * 300 - 300 * 400);
}

TEST(RectangleGuillotineBranchingScheme, ApplyInsertion2)
{
    /**
     *
     * |--------------------------|
     * |                          |
     * |                          |
     * |                          |
     * |                          |
     * |                          |
     * |                          |
     * |------|                   | 800
     * |      |                   |
     * |      |                   |
     * |  0   |                   |
     * |      |                   |
     * |      |                   |
     * |---|--|                   | 200
     * |   |                      |
     * |   |                      |
     * | 1 |---|                  | 100
     * |   | 2 |                  |
     * |---|---|------------------|
     * 0  300 700
     *       500
     *
     */

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(500, 600);
    instance.add_item_type(200, 300);
    instance.add_item_type(100, 400);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);

    auto root = branching_scheme.root();

    // Add item 1
    auto node_1 = branching_scheme.child(root, {1, -1, -1, 300, 200, 300, 3500, 3210, 0, 0});

    // Add item 2
    auto node_2 = branching_scheme.child(node_1, {2, -1, 2, 700, 200, 700, 3500, 3210, 0, 0});
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).z1, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).z2, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).x1_max, 3500);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).y2_max, 3210);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).number_of_bins, 1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).number_of_items, 2);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 100 * 400);

    // Add item 0
    auto node_3 = branching_scheme.child(node_2, {0, -1, 1, 700, 800, 500, 3500, 3210, 0, 0});
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).z1, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).z2, 0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).x1_max, 3500);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).y2_max, 3210);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).number_of_bins, 1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).number_of_items, 3);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).x1_curr, 700);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).current_area, 700 * 3210);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).item_area, 300 * 200 + 100 * 400 + 500 * 600);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).waste, 700 * 3210 - 300 * 200 - 100 * 400 - 500 * 600);
}

