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
     *      0    3
     *      |    |
     *      1    4
     *      |    |
     *      2    5
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
    BranchingScheme::Node node(branching_scheme);

    EXPECT_EQ(node.bin_number(), 0);
    EXPECT_EQ(node.item_number(), 0);
    EXPECT_EQ(node.waste(), 0);

    // Add item 0
    node.apply_insertion({.j1 = 0, .j2 = -1, .df = -1, .x1 = 300, .y2 = 200, .x3 = 300, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0}, info);
    std::vector<BranchingScheme::SolutionNode> nodes {
        {.f = -1, .p = 300},
        {.f =  0, .p = 200},
        {.f =  1, .p = 300},
    };
    std::vector<BranchingScheme::NodeItem> items {
        {.j = 0, .node = 2},
    };
    EXPECT_EQ(node.nodes(), nodes);
    EXPECT_EQ(node.items(), items);
    EXPECT_EQ(node.z1(), 0);
    EXPECT_EQ(node.z2(), 0);
    EXPECT_EQ(node.x1_max(), 3500);
    EXPECT_EQ(node.y2_max(), 3210);
    EXPECT_EQ(node.bin_number(), 1);
    EXPECT_EQ(node.item_number(), 1);
    EXPECT_EQ(node.waste(), 0);

    // Add item 1;
    node.apply_insertion({.j1 = 1, .j2 = -1, .df = 0, .x1 = 700, .y2 = 300, .x3 = 700, .x1_max = 3800, .y2_max = 3210, .z1 = 0, .z2 = 0}, info);
    std::vector<BranchingScheme::SolutionNode> nodes2 {
        {.f = -1, .p = 300},
        {.f =  0, .p = 200},
        {.f =  1, .p = 300},
        {.f = -1, .p = 700},
        {.f =  3, .p = 300},
        {.f =  4, .p = 700},
    };
    std::vector<BranchingScheme::NodeItem> items2 {
        {.j = 0, .node = 2},
        {.j = 1, .node = 5},
    };
    EXPECT_EQ(node.nodes(), nodes2);
    EXPECT_EQ(node.items(), items2);
    EXPECT_EQ(node.z1(), 0);
    EXPECT_EQ(node.z2(), 0);
    EXPECT_EQ(node.x1_max(), 3800);
    EXPECT_EQ(node.y2_max(), 3210);
    EXPECT_EQ(node.bin_number(), 1);
    EXPECT_EQ(node.item_number(), 2);
    EXPECT_EQ(node.waste(), 3210 * 700 - 200 * 300 - 300 * 400);
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
     *       0
     *      / \
     *     1   4
     *    / \  |
     *   2   3 5
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

    BranchingScheme::Node node(branching_scheme);

    // Add item 1
    node.apply_insertion({.j1 = 1, .j2 = -1, .df = -1, .x1 = 300, .y2 = 200, .x3 = 300, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0}, info);
    std::vector<BranchingScheme::SolutionNode> nodes1 {
        {.f = -1, .p = 300},
        {.f =  0, .p = 200},
        {.f =  1, .p = 300},
    };
    std::vector<BranchingScheme::NodeItem> items1 {
        {.j = 1, .node = 2},
    };
    EXPECT_EQ(node.nodes(), nodes1);
    EXPECT_EQ(node.items(), items1);

    // Add item 2
    node.apply_insertion({.j1 = 2, .j2 = -1, .df = 2, .x1 = 700, .y2 = 200, .x3 = 700, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0}, info);
    std::vector<BranchingScheme::SolutionNode> nodes2 {
        {.f = -1, .p = 700},
        {.f =  0, .p = 200},
        {.f =  1, .p = 300},
        {.f =  1, .p = 700},
    };
    std::vector<BranchingScheme::NodeItem> items2 {
        {.j = 1, .node = 2},
        {.j = 2, .node = 3},
    };
    EXPECT_EQ(node.nodes(), nodes2);
    EXPECT_EQ(node.items(), items2);
    EXPECT_EQ(node.z1(), 0);
    EXPECT_EQ(node.z2(), 0);
    EXPECT_EQ(node.x1_max(), 3500);
    EXPECT_EQ(node.y2_max(), 3210);
    EXPECT_EQ(node.bin_number(), 1);
    EXPECT_EQ(node.item_number(), 2);
    EXPECT_EQ(node.waste(), 100 * 400);

    // Add item 0
    node.apply_insertion({.j1 = 0, .j2 = -1, .df = 1, .x1 = 700, .y2 = 800, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0}, info);
    std::vector<BranchingScheme::SolutionNode> nodes0 {
        {.f = -1, .p = 700},
        {.f =  0, .p = 200},
        {.f =  1, .p = 300},
        {.f =  1, .p = 700},
        {.f =  0, .p = 800},
        {.f =  4, .p = 500},
    };
    std::vector<BranchingScheme::NodeItem> items0 {
        {.j = 1, .node = 2},
        {.j = 2, .node = 3},
        {.j = 0, .node = 5},
    };
    EXPECT_EQ(node.nodes(), nodes0);
    EXPECT_EQ(node.items(), items0);
    EXPECT_EQ(node.z1(), 0);
    EXPECT_EQ(node.z2(), 0);
    EXPECT_EQ(node.x1_max(), 3500);
    EXPECT_EQ(node.y2_max(), 3210);
    EXPECT_EQ(node.bin_number(), 1);
    EXPECT_EQ(node.item_number(), 3);
    EXPECT_EQ(node.x1_curr(), 700);
    EXPECT_EQ(node.area(), 700 * 3210);
    EXPECT_EQ(node.item_area(), 300 * 200 + 100 * 400 + 500 * 600);
    EXPECT_EQ(node.waste(), 700 * 3210 - 300 * 200 - 100 * 400 - 500 * 600);
}

