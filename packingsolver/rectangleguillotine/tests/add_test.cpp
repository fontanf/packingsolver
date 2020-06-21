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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(200, 300);
    instance.add_item(300, 400);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    EXPECT_EQ(root->bin_number(), 0);
    EXPECT_EQ(root->item_number(), 0);
    EXPECT_EQ(root->waste(), 0);

    // Add item 0
    auto node_1 = branching_scheme.child(root, {.j1 = 0, .j2 = -1, .df = -1, .x1 = 300, .y2 = 200, .x3 = 300, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0});
    EXPECT_EQ(node_1->z1(), 0);
    EXPECT_EQ(node_1->z2(), 0);
    EXPECT_EQ(node_1->x1_max(), 3500);
    EXPECT_EQ(node_1->y2_max(), 3210);
    EXPECT_EQ(node_1->bin_number(), 1);
    EXPECT_EQ(node_1->item_number(), 1);
    EXPECT_EQ(node_1->waste(), 0);

    // Add item 1;
    auto node_2 = branching_scheme.child(node_1, {.j1 = 1, .j2 = -1, .df = 0, .x1 = 700, .y2 = 300, .x3 = 700, .x1_max = 3800, .y2_max = 3210, .z1 = 0, .z2 = 0});
    EXPECT_EQ(node_2->z1(), 0);
    EXPECT_EQ(node_2->z2(), 0);
    EXPECT_EQ(node_2->x1_max(), 3800);
    EXPECT_EQ(node_2->y2_max(), 3210);
    EXPECT_EQ(node_2->bin_number(), 1);
    EXPECT_EQ(node_2->item_number(), 2);
    EXPECT_EQ(node_2->waste(), 3210 * 700 - 200 * 300 - 300 * 400);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 600);
    instance.add_item(200, 300);
    instance.add_item(100, 400);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);

    auto root = branching_scheme.root();

    // Add item 1
    auto node_1 = branching_scheme.child(root, {.j1 = 1, .j2 = -1, .df = -1, .x1 = 300, .y2 = 200, .x3 = 300, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0});

    // Add item 2
    auto node_2 = branching_scheme.child(node_1, {.j1 = 2, .j2 = -1, .df = 2, .x1 = 700, .y2 = 200, .x3 = 700, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0});
    EXPECT_EQ(node_2->z1(), 0);
    EXPECT_EQ(node_2->z2(), 0);
    EXPECT_EQ(node_2->x1_max(), 3500);
    EXPECT_EQ(node_2->y2_max(), 3210);
    EXPECT_EQ(node_2->bin_number(), 1);
    EXPECT_EQ(node_2->item_number(), 2);
    EXPECT_EQ(node_2->waste(), 100 * 400);

    // Add item 0
    auto node_3 = branching_scheme.child(node_2, {.j1 = 0, .j2 = -1, .df = 1, .x1 = 700, .y2 = 800, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0});
    EXPECT_EQ(node_3->z1(), 0);
    EXPECT_EQ(node_3->z2(), 0);
    EXPECT_EQ(node_3->x1_max(), 3500);
    EXPECT_EQ(node_3->y2_max(), 3210);
    EXPECT_EQ(node_3->bin_number(), 1);
    EXPECT_EQ(node_3->item_number(), 3);
    EXPECT_EQ(node_3->x1_curr(), 700);
    EXPECT_EQ(node_3->area(), 700 * 3210);
    EXPECT_EQ(node_3->item_area(), 300 * 200 + 100 * 400 + 500 * 600);
    EXPECT_EQ(node_3->waste(), 700 * 3210 - 300 * 200 - 100 * 400 - 500 * 600);
}

