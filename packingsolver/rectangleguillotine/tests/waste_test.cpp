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

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(500, 500, -1, 1, false, true);
    instance.add_item_type(300, 300, -1, 1, false, false);
    instance.add_item_type(400, 400, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {0, -1, -1, 500, 500, 500, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).waste, 0);

    BranchingScheme::Insertion i1 {1, -1,  2, 800, 500, 800, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 200 * 300);
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

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(500, 500, -1, 1, false, true);
    instance.add_item_type(300, 300, -1, 1, false, false);
    instance.add_item_type(100, 500, -1, 1, false, false);
    instance.add_item_type(100, 500, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {0, -1, -1, 500, 500, 500, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).item_area, 500 * 500);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).current_area, 500 * 500);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).waste, 0);

    BranchingScheme::Insertion i1 {1, -1, 1, 500, 800, 300, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).item_area, 500 * 500 + 300 * 300);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).current_area, 500 * 500 + 300 * 300);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 0);

    BranchingScheme::Insertion i2 {2, -1, 1, 500, 900, 500, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).waste, 300 * 200);
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

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(200, 500, -1, 1, false, true);
    instance.add_item_type(200, 400, -1, 1, false, false);
    instance.add_item_type(200, 500, -1, 1, false, false);
    instance.add_item_type(600, 100, -1, 1, false, false);
    instance.add_item_type(600, 100, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {0, -1, -1, 200, 500, 200, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).waste, 0);

    BranchingScheme::Insertion i1 {1, -1, 2, 400, 500, 400, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 20000);

    BranchingScheme::Insertion i2 {2, -1, 2, 600, 500, 600, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).waste, 20000);

    BranchingScheme::Insertion i3 {3, -1, 1, 600, 600, 600, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is3 = branching_scheme.insertions(node_3, info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    auto node_4 = branching_scheme.child(node_3, i3);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_4).waste, 20000);
}

TEST(RectangleGuillotineBranchingScheme, Waste4)
{
    Info info = Info()
        //.set_log2stderr(true)
        ;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(1578, 758, -1, 1, false, true);
    instance.add_item_type(738, 1550, -1, 1, false, false);
    instance.add_item_type(581, 276, -1, 1, false, false);
    instance.add_item_type(781, 276, -1, 1, false, false);
    instance.add_item_type(1426, 648, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {0, -1, -1, 758, 1578, 758, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).waste, 0);

    BranchingScheme::Insertion i1 {1, -1, 0, 2308, 738, 2308, 4258, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    //EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 758 * (3210 - 1578));
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

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(1578, 758, -1, 1, false, true);
    instance.add_item_type(738, 1550, -1, 1, false, false);
    instance.add_item_type(581, 276, -1, 1, false, false);
    instance.add_item_type(781, 1396, -1, 1, false, false);
    instance.add_item_type(1426, 648, -1, 1, false, false);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {0, -1, -1, 758, 1578, 758, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).waste, 0);

    BranchingScheme::Insertion i1 {1, -1, 1, 758, 3128, 738, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 0);

    BranchingScheme::Insertion i2 {2, -1, 0, 1339, 276, 1339, 4258, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_3).waste, 20 * 1550 + (3210 - 3128) * 758);

    BranchingScheme::Insertion i3 {3, -1, 1, 1539, 1672, 1539, 4258, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is3 = branching_scheme.insertions(node_3, info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    auto node_4 = branching_scheme.child(node_3, i3);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_4).waste, 20 * 1550 + (3210 - 3128) * 758 + (781 - 581) * 276);

    BranchingScheme::Insertion i4 = {4, -1, 1, 1539, 3098, 1406, 4258, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is4 = branching_scheme.insertions(node_4, info);
    EXPECT_NE(std::find(is4.begin(), is4.end(), i4), is4.end());
    auto node_5 = branching_scheme.child(node_4, i4);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_5).waste, 425486);
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

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    instance.add_item_type(1500, 2000);
    instance.add_item_type(1710, 1995);
    instance.add_bin_type(6000, 3210);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 {0, -1, -1, 2000, 1500, 2000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 {1, -1, 1, 2020, 3210, 1995, 3500, 3210, 1, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).waste, 20 * 1500 + 25 * 1710);
}

