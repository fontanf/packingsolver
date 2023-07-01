#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, InsertionNoDefect)
{
    /**
     * The defect insertion is dominated by one of the item insertions.
     *
     * |--------------------------------------------------| 3210
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                            x                     |
     * |                                                  |
     * |-------------------|                              | 500
     * |         0         | 500                          |
     * |-------------------|------------------------------|
     *                   1000                           6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 2000, 1500, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0},
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect1)
{
    /**
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |-------|---|                      | 1000
     * |   0   |   |                      |
     * |-------|   |                      | 500
     * |       | 1 |                      |
     * |   x   |   |                      | 250
     * |       |   |                      |
     * |-------|---|----------------------|
     *   500 1000 1700
     *
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(1700, 2000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 500, 248, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 750, 1000, 3500, 3210, 0, 1},
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
        {-1, -1, -1, 502, 250, 502, 3500, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(root, info), is);

    BranchingScheme::Insertion i0 = {-1, 0, -1, 1000, 750, 1000, 3500, 3210, 0, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is_2 {
        {1, -1, 2, 2000, 750, 2000, 3500, 3210, 0, 1},
        {1, -1, 2, 1700, 1000, 1700, 3500, 3210, 0, 0},
        {1, -1, 1, 1000, 1450, 1000, 3500, 3210, 0, 0},
        {1, -1, 1, 1000, 1750, 700, 3500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is_2);

    BranchingScheme::Insertion i1 = {1, -1, 2, 1700, 1000, 1700, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 1700, 3000, 1700, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect2)
{
    /**
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |-------|                          | 1400
     * |       |                          |
     * |   0   |---|                      | 1000
     * |-------|   |                      |
     * |   x   |   |                      | 900
     * |       |   |                      |
     * |       | 1 |                      |
     * |   x   |   |                      | 300
     * |       |   |                      |
     * |-------|---|----------------------|
     *   500 1000 1700
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 500, 298, 2, 2);
    instance_builder.add_defect(0, 500, 898, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 800, 1000, 3500, 3210, 0, 1},
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
        {-1, -1, -1, 502, 300, 502, 3500, 3210, 1, 1},
        {-1, -1, -1, 502, 900, 502, 3500, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(root, info), is);

    BranchingScheme::Insertion i0 = {-1, 0, -1, 1000, 800, 1000, 3500, 3210, 0, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is_2 {
        {1, -1, 2, 2000, 800, 2000, 3500, 3210, 0, 1},
        {1, -1, 2, 1700, 1400, 1700, 3500, 3210, 0, 1},
        {-1, 1, 1, 1000, 1600, 1000, 3500, 3210, 0, 1},
        {-1, 1, 1, 1000, 1900, 700, 3500, 3210, 0, 1},
        {-1, -1, 1, 1000, 900, 502, 3500, 3210, 0, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is_2);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect3)
{
    /**
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |-------|                          | 1800
     * |       |                          |
     * |   0   |                          |
     * |-------|                          |
     * |   x   |                          | 1300
     * |       |---|                      | 1000
     * |       |   |                      |
     * |   x   |   |                      | 900
     * |       |   |                      |
     * |       | 1 |                      |
     * |   x   |   |                      | 300
     * |       |   |                      |
     * |-------|---|----------------------|
     *   500 1000 1700
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 500, 298, 2, 2);
    instance_builder.add_defect(0, 500, 898, 2, 2);
    instance_builder.add_defect(0, 500, 1298, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 800, 1000, 3500, 3210, 0, 1},
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
        {-1, -1, -1, 502, 300, 502, 3500, 3210, 1, 1},
        {-1, -1, -1, 502, 900, 502, 3500, 3210, 1, 1},
        //{-1, -1, -1, 502, 1300, 502, 3500, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(root, info), is);

    BranchingScheme::Insertion i0 = {-1, 0, -1, 1000, 800, 1000, 3500, 3210, 0, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is_2 {
        {1, -1, 2, 2000, 800, 2000, 3500, 3210, 0, 1},
        {1, -1, 2, 1700, 1800, 1700, 3500, 3210, 0, 1},
        {-1, 1, 1, 1000, 2000, 1000, 3500, 3210, 0, 1},
        {-1, 1, 1, 1000, 2300, 700, 3500, 3210, 0, 1},
        {-1, -1, 1, 1000, 900, 502, 3500, 3210, 0, 1},
        {-1, -1, 1, 1000, 1300, 502, 3500, 3210, 0, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is_2);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect4)
{
    /**
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |---|-------|                      | 1000
     * |   |   1   |                      |
     * |   |-------|                      | 500
     * | 0 |       |                      |
     * |   |   x   |                      | 250
     * |   |       |                      |
     * |---|-------|----------------------|
     *    500    2000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 1000, 248, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {-1, 1, 2, 2000, 1000, 2000, 3500, 3210, 0, 0},
        {1, -1, 2, 1000, 1500, 1000, 3500, 3210, 0, 0},
        {-1, -1, 2, 1002, 1000, 1002, 3500, 3210, 1, 0},
        {1, -1, 1, 500, 2500, 500, 3500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect5)
{
    /**
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |   |-------|                      | 1300
     * |---|   1   |                      | 1000
     * |   |-------|                      |
     * |   |   x   |                      | 800
     * |   |       |                      |
     * | 0 |       |                      |
     * |   |   x   |                      | 250
     * |   |       |                      |
     * |---|-------|----------------------|
     *    500    2000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 1000, 248, 2, 2);
    instance_builder.add_defect(0, 1000, 798, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {-1, 1, 2, 2000, 1300, 2000, 3500, 3210, 0, 1},
        {1, -1, 2, 1000, 1500, 1000, 3500, 3210, 0, 0},
        {-1, -1, 2, 1002, 1000, 1002, 3500, 3210, 1, 0},
        {1, -1, 1, 500, 2500, 500, 3500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect6)
{
    /**
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |-------|                          | 1000
     * |   1   |   x                      | 750
     * |-------|                          | 500
     * |   0   |                          |
     * |-------|--------------------------|
     *       1000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 1250, 748, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, 1, -1, 1000, 1000, 1000, 3500, 3210, 0, 2};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 2, 2500, 1000, 2500, 3500, 3210, 0, 2},
        {-1, -1, 2, 1252, 1000, 1252, 3500, 3210, 1, 2},
        {2, -1, 1, 1500, 1500, 1500, 3500, 3210, 0, 0},
        {2, -1, 1, 1000, 2500, 500, 3500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect7)
{
    /**
     * The defect is very close to the side, therefore yj = minwaste.
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |-------|                          |
     * |   0   |                          |
     * |-------|                          |
     * |   x                              | 10
     * |----------------------------------|
     *   500
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 248, 8, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 520, 1000, 3500, 3210, 0, 1},
        {-1, 0, -1, 500, 1020, 500, 3500, 3210, 0, 1},
        {-1, -1, -1, 250, 20, 250, 3500, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(root, info), is);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDefect8)
{
    /**
     * If we add item 2, then yk = 3000 => 3210
     * However, in this case, item 0 contains a defect.
     * Therefore, it should not be possible to insert item 1.
     *
     * |----------------------------------| 3210
     * |   x                              | 3205
     * |           |---|                  | 3180
     * |       |---)   |                  | 3170
     * |       |   |   |                  |
     * |       |   |   |                  |
     * |       |   |   |                  |
     * |       |   |   |                  |
     * |       |   |   |                  |
     * |       | 1 | 2 |                  |
     * |       |   |   |                  |
     * |-------|   |   |                  |
     * |   0   |   |   |                  |
     * |-------|   |   |                  |
     * |       |   |   |                  |
     * |   x   |   |   |                  | 250
     * |       |   |   |                  |
     * |-------|---|---|------------------|
     *       1000 1200 1400
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(200, 3170, -1, 1, false, 0);
    instance_builder.add_item_type(200, 3180, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 498, 248, 2, 2);
    instance_builder.add_defect(0, 498, 3205, 2, 2);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {-1, 0, -1, 1000, 750, 1000, 3500, 3210, 0, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 2, 1200, 3170, 1200, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {-1, -1, 1, 1200, 3210, 500, 3500, 3210, 0, 0},
        {2, -1, 0, 4380, 200, 4380, 4700, 3210, 0, 0},
        {2, -1, 0, 1400, 3180, 1400, 4700, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDefect)
{
    /**
     *
     * |------------------------|-------------------------| 3210
     * |                        |                         |
     * |           2            |                         |
     * |                        |                         |
     * |------------------------|                         | 2210
     * |                        |                         |
     * |           1            |           3             |
     * |                        |                         |
     * |------------------------|                         | 1210
     * |                     x  |                         | 1200
     * |-------------------|    |                         | 1000
     * |                   |    |                         |
     * |         0         |    |                         |
     * |                   |    |                         |
     * |-------------------|----|-------------------------|
     *                   2000  3000                     6000
     *                     2500
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(2000, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(3000, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(3000, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(3000, 3210, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 2490, 1200, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 2000, 1000, 2000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {-1, -1, 2, 2500, 1210, 2500, 3500, 3210, 1, 1},
        {-1, 1, 1, 3000, 2210, 3000, 3500, 3210, 0, 1},
        {1, -1, 0, 5000, 1000, 5000, 5500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is);
}

