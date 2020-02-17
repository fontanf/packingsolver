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
     * |-------|                                   |
     * | 50x50 |                                   |
     * |       |----|                              |
     * |       | 30 |                              |
     * |       |x30 |                              |
     * +-------|----|------------------------------|
     */

    Info info;
    Instance instance(Objective::BinPackingLeftovers);
    instance.add_item(50, 50, -1, 1, false, true);
    instance.add_item(30, 30, -1, 1, false, false);
    instance.add_item(40, 40, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 50, .y2 = 50, .x3 = 50,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);
    EXPECT_EQ(node.waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df =  2,
            .x1 = 80, .y2 = 50, .x3 = 80,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    node.apply_insertion(i1, info);
    EXPECT_EQ(node.waste(), 20 * 30);
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

    Instance instance(Objective::BinPackingLeftovers);
    instance.add_item(500, 500, -1, 1, false, true);
    instance.add_item(300, 300, -1, 1, false, false);
    instance.add_item(100, 500, -1, 1, false, false);
    instance.add_item(100, 500, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 500, .y2 = 500, .x3 = 500,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);
    EXPECT_EQ(node.waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 1,
            .x1 = 500, .y2 = 800, .x3 = 300,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    node.apply_insertion(i1, info);
    EXPECT_EQ(node.waste(), 0);

    BranchingScheme::Insertion i2 {
            .j1 = 2, .j2 = -1, .df = 1,
            .x1 = 500, .y2 = 900, .x3 = 500,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);
    EXPECT_EQ(node.waste(), 300 * 200);
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

    Instance instance(Objective::BinPackingLeftovers);
    instance.add_item(200, 500, -1, 1, false, true);
    instance.add_item(200, 400, -1, 1, false, false);
    instance.add_item(200, 500, -1, 1, false, false);
    instance.add_item(600, 100, -1, 1, false, false);
    instance.add_item(600, 100, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 200, .y2 = 500, .x3 = 200,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);
    EXPECT_EQ(node.waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 2,
            .x1 = 400, .y2 = 500, .x3 = 400,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    node.apply_insertion(i1, info);
    EXPECT_EQ(node.waste(), 20000);

    BranchingScheme::Insertion i2 {
            .j1 = 2, .j2 = -1, .df = 2,
            .x1 = 600, .y2 = 500, .x3 = 600,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);
    EXPECT_EQ(node.waste(), 20000);

    BranchingScheme::Insertion i3 {
            .j1 = 3, .j2 = -1, .df = 1,
            .x1 = 600, .y2 = 600, .x3 = 600,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is3 = node.children(info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    node.apply_insertion(i3, info);
    EXPECT_EQ(node.waste(), 20000);
}

TEST(RectangleGuillotineBranchingScheme, Waste4)
{
    /* test A1 with two objects (waste positive)

     0
     |
     1      Plate
     | \
     2  3   1
        |
        4   2
        |
        5   3

    */

    Info info;

    Instance instance(Objective::BinPackingLeftovers);
    instance.add_item(1578, 758, -1, 1, false, true);
    instance.add_item(738, 1550, -1, 1, false, false);
    instance.add_item(581, 276, -1, 1, false, false);
    instance.add_item(781, 276, -1, 1, false, false);
    instance.add_item(1426, 648, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 758, .y2 = 1578, .x3 = 758,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);
    EXPECT_EQ(node.waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 0,
            .x1 = 2308, .y2 = 738, .x3 = 2308,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    node.apply_insertion(i1, info);
    EXPECT_EQ(node.waste(), 758 * (3210 - 1578));
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

    Instance instance(Objective::BinPackingLeftovers);
    instance.add_item(1578, 758, -1, 1, false, true);
    instance.add_item(738, 1550, -1, 1, false, false);
    instance.add_item(581, 276, -1, 1, false, false);
    instance.add_item(781, 1396, -1, 1, false, false);
    instance.add_item(1426, 648, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 758, .y2 = 1578, .x3 = 758,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);
    EXPECT_EQ(node.waste(), 0);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 1,
            .x1 = 758, .y2 = 3128, .x3 = 738,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    node.apply_insertion(i1, info);
    EXPECT_EQ(node.waste(), 0);

    BranchingScheme::Insertion i2 {
            .j1 = 2, .j2 = -1, .df = 0,
            .x1 = 1339, .y2 = 276, .x3 = 1339,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);
    EXPECT_EQ(node.waste(), 20 * 1550 + (3210 - 3128) * 758);

    BranchingScheme::Insertion i3 {
            .j1 = 3, .j2 = -1, .df = 1,
            .x1 = 1539, .y2 = 1672, .x3 = 1539,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is3 = node.children(info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    node.apply_insertion(i3, info);
    EXPECT_EQ(node.waste(), 20 * 1550 + (3210 - 3128) * 758 + (781 - 581) * 276);

    BranchingScheme::Insertion i4 = {
            .j1 = 4, .j2 = -1, .df = 1,
            .x1 = 1539, .y2 = 3098, .x3 = 1406,
            .x1_max = 4258, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is4 = node.children(info);
    EXPECT_NE(std::find(is4.begin(), is4.end(), i4), is4.end());
    node.apply_insertion(i4, info);
    EXPECT_EQ(node.waste(), 425486);
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

    Instance instance(Objective::BinPackingLeftovers);
    instance.add_item(1500, 2000);
    instance.add_item(1710, 1995);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 {
            .j1 = 0, .j2 = -1, .df = -1,
            .x1 = 2000, .y2 = 1500, .x3 = 2000,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 {
            .j1 = 1, .j2 = -1, .df = 1,
            .x1 = 2020, .y2 = 3210, .x3 = 1995,
            .x1_max = 3500, .y2_max = 3210,
            .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    node.apply_insertion(i1, info);
    EXPECT_EQ(node.waste(), 20 * 1500 + 25 * 1710);
}

