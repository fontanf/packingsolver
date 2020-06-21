#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, Waste1)
{
    /*
     * |-------------------------------------------|
     * |                                           |
     * |                                           |
     * |                                           |
     * |-------|                                   | 500
     * |       |                                   |
     * |       |----|                              | 300
     * |   0   |    |                              |
     * |       | 1  |                              |
     * +-------|----|------------------------------|
     *        500  800
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 500, -1, 1, false, true);
    instance.add_item(300, 300, -1, 1, false, false);
    instance.add_item(400, 400, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 500, .y2 = 500, .x3 = 500,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.children(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(node_1->waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df =  2,
            .x1 = 800, .y2 = 500, .x3 = 800,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.children(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(node_2->waste(), 200 * 300);
}

TEST(RectangleGuillotineBranchingScheme, Waste2)
{
    /*
     * +------------------------------------------------+
     * |                                                |
     * |                                                |
     * +--------+                                       |
     * |  500   |                                       |
     * | x100   |                                       |
     * +-----+--+                                       |
     * |     |                                          |
     * | 300 |                                          |
     * |x300 |                                          |
     * +-----+--+                                       |
     * |        |                                       |
     * |   500  |                                       |
     * |  x500  |                                       |
     * |        |                                       |
     * +--------+---------------------------------------+
     *
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 500, -1, 1, false, true);
    instance.add_item(300, 300, -1, 1, false, false);
    instance.add_item(100, 500, -1, 1, false, false);
    instance.add_item(100, 500, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 500, .y2 = 500, .x3 = 500,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.children(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(node_1->waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 1,
            .x1 = 500, .y2 = 800, .x3 = 300,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.children(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(node_2->waste(), 0);

    BranchingScheme::Insertion i2 {
            .j1 = 2, .j2 = -1, .df = 1,
            .x1 = 500, .y2 = 900, .x3 = 500,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.children(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
    EXPECT_EQ(node_3->waste(), 300 * 200);
}

TEST(RectangleGuillotineBranchingScheme, Waste3)
{
    /*
     * +-----------------------------------------------------------+
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * |                                                           |
     * +-----------------+                                         |
     * |       1x6       |                                         |
     * +-----------------+                                         |
     * |     |     |     |                                         |
     * |     +-----+     |                                         |
     * |     |     |     |                                         |
     * | 2x5 |     | 2x5 |                                         |
     * |     | 2x4 |     |                                         |
     * |     |     |     |                                         |
     * +-----+-----+-----+-----------------------------------------+
     */

    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(200, 500, -1, 1, false, true);
    instance.add_item(200, 400, -1, 1, false, false);
    instance.add_item(200, 500, -1, 1, false, false);
    instance.add_item(600, 100, -1, 1, false, false);
    instance.add_item(600, 100, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 200, .y2 = 500, .x3 = 200,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.children(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(node_1->waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 2,
            .x1 = 400, .y2 = 500, .x3 = 400,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.children(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(node_2->waste(), 20000);

    BranchingScheme::Insertion i2 {
            .j1 = 2, .j2 = -1, .df = 2,
            .x1 = 600, .y2 = 500, .x3 = 600,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.children(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
    EXPECT_EQ(node_3->waste(), 20000);

    BranchingScheme::Insertion i3 {
            .j1 = 3, .j2 = -1, .df = 1,
            .x1 = 600, .y2 = 600, .x3 = 600,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is3 = branching_scheme.children(node_3, info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    auto node_4 = branching_scheme.child(node_3, i3);
    EXPECT_EQ(node_4->waste(), 20000);
}

TEST(RectangleGuillotineBranchingScheme, Waste4)
{
    Info info = Info()
        //.set_log2stderr(true)
        ;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1578, 758, -1, 1, false, true);
    instance.add_item(738, 1550, -1, 1, false, false);
    instance.add_item(581, 276, -1, 1, false, false);
    instance.add_item(781, 276, -1, 1, false, false);
    instance.add_item(1426, 648, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 758, .y2 = 1578, .x3 = 758,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.children(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(node_1->waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 0,
            .x1 = 2308, .y2 = 738, .x3 = 2308,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.children(node_1, info);
    //EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(node_2->waste(), 758 * (3210 - 1578));
}

TEST(RectangleGuillotineBranchingScheme, Waste5)
{
    /**
     * +------+-------+----------------------------+
     * |      |       |                            |
     * |      |       |                            |
     * |      |   4   |                            |
     * |   1  |       |                            |
     * |      +-------++                           |
     * |      |        |                           |
     * +------+        |                           |
     * |      |        |                           |
     * |      |    3   |                           |
     * |      |        |                           |
     * |  0   |        |                           |
     * |      +------+-+                           |
     * |      |   2  |                             |
     * +-------------+-----------------------------+
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1578, 758, -1, 1, false, true);
    instance.add_item(738, 1550, -1, 1, false, false);
    instance.add_item(581, 276, -1, 1, false, false);
    instance.add_item(781, 1396, -1, 1, false, false);
    instance.add_item(1426, 648, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 758, .y2 = 1578, .x3 = 758,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.children(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(node_1->waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 1,
            .x1 = 758, .y2 = 3128, .x3 = 738,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.children(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(node_2->waste(), 0);

    BranchingScheme::Insertion i2 {
            .j1 = 2, .j2 = -1, .df = 0,
            .x1 = 1339, .y2 = 276, .x3 = 1339,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.children(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
    EXPECT_EQ(node_3->waste(), 20 * 1550 + (3210 - 3128) * 758);

    BranchingScheme::Insertion i3 {
            .j1 = 3, .j2 = -1, .df = 1,
            .x1 = 1539, .y2 = 1672, .x3 = 1539,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is3 = branching_scheme.children(node_3, info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    auto node_4 = branching_scheme.child(node_3, i3);
    EXPECT_EQ(node_4->waste(), 20 * 1550 + (3210 - 3128) * 758 + (781 - 581) * 276);

    BranchingScheme::Insertion i4 = {
            .j1 = 4, .j2 = -1, .df = 1,
            .x1 = 1539, .y2 = 3098, .x3 = 1406,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is4 = branching_scheme.children(node_4, info);
    EXPECT_NE(std::find(is4.begin(), is4.end(), i4), is4.end());
    auto node_5 = branching_scheme.child(node_4, i4);
    EXPECT_EQ(node_5->waste(), 425486);
}

TEST(RectangleGuillotineBranchingScheme, Waste6)
{
    /**
     * Waste = 20 * 1500 + 25 * 1710
     *
     * |-----------|----------------------|
     * |           |                      |
     * |           |                      |
     * |           |                      |
     * |           |                      |
     * |     1     |                      |
     * |           |                      |
     * |           |                      |
     * |           |                      |
     * |-----------||                     | 1500
     * |            |                     |
     * |            |                     |
     * |            |                     |
     * |     0      |                     |
     * |            |                     |
     * |            |                     |
     * |            |                     |
     * |------------|---------------------|
     *           1995
     *            2000
     *
     */

    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1500, 2000);
    instance.add_item(1710, 1995);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 2000, .y2 = 1500, .x3 = 2000,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.children(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 1,
            .x1 = 2020, .y2 = 3210, .x3 = 1995,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.children(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(node_2->waste(), 20 * 1500 + 25 * 1710);
}

