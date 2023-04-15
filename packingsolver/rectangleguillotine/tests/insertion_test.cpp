#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, InsertionDFm1I)
{
    /**
     *
     * |--------------------------------------------------| 3210
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
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
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0},
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDfm1II)
{
    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(3000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(3000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(3000, 3210, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 3210, 3000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 6000, 3210, 6000, 6000, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, -1, 3000, 3210, 3000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf0)
{
    /**
     *
     * |--------|---------------------------------------| 3210
     * |        |                                       |
     * |        |                                       |
     * |        |                                       |
     * |        |                                       |
     * |    0   |                                       |
     * |        |------------------|                    | 1000
     * |        |                  |                    |
     * |        |        1         |                    |
     * |        |                  |                    |
     * |--------|------------------|--------------------|
     *        1000               2500                 6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 1500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        //{1, -1, 2, 2500, 3210, 2500, 3500, 3210, 0, 0},
        //{1, -1, 2, 2000, 3210, 2000, 3500, 3210, 0, 0},
        {1, -1, 0, 2500, 1000, 2500, 4500, 3210, 0, 0},
        {1, -1, 0, 2000, 1500, 2000, 4500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf1I)
{
    /**
     *
     * |--------|----------------------------------------|
     * |        |                                        |
     * |        |                                        |
     * |        |                                        |
     * |        |                                        |
     * |        |------------------|                     | 2100
     * |        |                  |                     |
     * |   0    |                  |                     |
     * |        |        2         |                     |
     * |        |                  |                     |
     * |        |------------------|----------|          | 1000
     * |        |                             |          |
     * |        |             1               |          |
     * |        |                             |          |
     * |--------|-----------------------------|----------|
     * 0      1000               3000       4000       6000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 3000, -1, 1, false, 0);
    instance_builder.add_item_type(1100, 2000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 4000, 1000, 4000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 1, 4000, 2100, 3000, 4500, 3210, 0, 0},
        {2, -1, 1, 4000, 3000, 2100, 4500, 3210, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf1II)
{
    /**
     *
     * |--------|----------------------------------------| 3210
     * |        |                                        |
     * |        |                                        |
     * |        |                                        |
     * |        |--------------------------|             | 2100
     * |        |                          |             |
     * |   0    |            2             |             |
     * |        |                          |             |
     * |        |--------------------------|--|          | 1000
     * |        |                             |          |
     * |        |             1               |          |
     * |        |                             |          |
     * |--------|-----------------------------|----------|
     *        1000                      3990 4000       6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 3000, -1, 1, false, 0);
    instance_builder.add_item_type(1100, 2990, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 4000, 1000, 4000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 1, 4020, 2100, 3990, 4500, 3210, 1, 0},
        {2, -1, 0, 5100, 2990, 5100, 6000, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf1III)
{
    /**
     *
     * |--------|----------------------------------------| 3210
     * |        |                                        |
     * |        |                                        |
     * |        |                                        |
     * |        |-----------------------------|          | 2100
     * |        |                             |          |
     * |   0    |            2                |          |
     * |        |                             |          |
     * |        |--------------------------|--|          | 1000
     * |        |                          |             |
     * |        |             1            |             |
     * |        |                          |             |
     * |--------|--------------------------|-------------|
     *        1000                      4000 4010       6000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 3000, -1, 1, false, 0);
    instance_builder.add_item_type(1100, 3010, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 4000, 1000, 4000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 1, 4030, 2100, 4010, 4500, 3210, 1, 0},
        {2, -1, 0, 5100, 3010, 5100, 6000, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf1IV)
{
    /**
     *
     * |--------|----------------------------------------| 3210
     * |        |                                        |
     * |        |                                        |
     * |        |                                        |
     * |        |-----------------------------|          | 2100
     * |        |                             |          |
     * |   0    |            2                |          |
     * |        |                             |          |
     * |        |--------------------------|--|          | 1000
     * |        |                          |             |
     * |        |             1            |             |
     * |        |                          |             |
     * |--------|--------------------------|-------------|
     *        1000                      4000 4020       6000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 3000, -1, 1, false, 0);
    instance_builder.add_item_type(1100, 3020, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 4000, 1000, 4000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 1, 4020, 2100, 4020, 4500, 3210, 0, 0},
        {2, -1, 0, 5100, 3020, 5100, 6000, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf1V)
{
    /**
     *
     * |--------|---------------------------------|------| 3210
     * |        |                                 |      |
     * |        |                                 |      |
     * |        |               3                 |      |
     * |        |                                 |      |
     * |        |-----------------------------|---|      | 2100
     * |        |                             |          |
     * |   0    |              2              |          |
     * |        |                             |          |
     * |        |--------------------------|--|          | 1000
     * |        |                          |             |
     * |        |             1            |             |
     * |        |                          |             |
     * |--------|--------------------------|-------------|
     *        1000                      4000 4010 4050  6000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 3000, -1, 1, false, 0);
    instance_builder.add_item_type(1100, 3010, -1, 1, false, 0);
    instance_builder.add_item_type(1110, 3050, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 4000, 1000, 4000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 4030, 2100, 4010, 4500, 3210, 1, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    std::vector<BranchingScheme::Insertion> is {
        {3, -1, 1, 4050, 3210, 4050, 4500, 3210, 0, 0},
        {3, -1, 0, 5140, 3050, 5140, 6000, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_3, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf1MinWasteI)
{
    /**
     *
     * |-------------------------------------------------|
     * |                                                 |
     * |                                                 |
     * |---------|-----------|----------|                | 3000
     * |         |           |          |                |
     * |    1    |     2     |    3     |                |
     * |         |           |          |                |
     * |---------|---------|-|----------|                | 1500
     * |                   |                             |
     * |         0         |                             |
     * |                   |                             |
     * |-------------------|-----------------------------|
     *          1000    2000 2010      3010
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1500, 2000, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(1010, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 1500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 2000, 1500, 2000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 1, 2000, 3000, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 2, 2030, 3000, 2010, 3500, 3210, 1, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    std::vector<BranchingScheme::Insertion> is {
        {3, -1, 2, 3010, 3000, 3010, 3500, 3210, 0, 0},
        {3, -1, 0, 3530, 1000, 3530, 5530, 3210, 0, 0},
        {3, -1, 0, 3030, 1500, 3030, 5530, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_3, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf1MinWasteII)
{
    /**
     *
     * |-----------------------|----------| 3210
     * |                       |          |
     * |           2           |          |
     * |                       |          |
     * |---------------------|-|          | 2010
     * |                     |            |
     * |          1          |            |
     * |                     |            |
     * |-------------------|-|            | 1000
     * |                   |              |
     * |         0         |              |
     * |                   |              |
     * |-------------------|--------------|
     *                  2000 2005 2010
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 2000, -1, 1, false, 0);
    instance_builder.add_item_type(1010, 2005, -1, 1, false, 0);
    instance_builder.add_item_type(1200, 2010, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 2000, 1000, 2000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 1, 2025, 2010, 2005, 3500, 3210, 1, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 2, 3205, 3010, 3205, 3500, 3210, 0, 0},
        {2, -1, 1, 2030, 3210, 2010, 3500, 3210, 1, 0},
        {2, -1, 0, 4035, 1200, 4035, 5525, 3210, 0, 0},
        {2, -1, 0, 3225, 2010, 3225, 5525, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf2I)
{
    /**
     *
     * |--------|----|-----------------------------------| 3210
     * |        |    |                                   |
     * |        |    |                                   |
     * |        |    |                                   |
     * |        | 2  |----|                              | 2010
     * |        |    |    |                              |
     * |   0    |    |  3 |                              |
     * |        |----|----|--------|                     | 1000
     * |        |                  |                     |
     * |        |         1        |                     |
     * |        |                  |                     |
     * |--------|------------------|---------------------| 0
     *        1000 1500  2200    3000                  6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 2000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 2210, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1010, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 3000, 1000, 3000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 3000, 3210, 1500, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    std::vector<BranchingScheme::Insertion> is {
        //{3, -1, 2, 3000, 3210, 2510, 4500, 3210, 0, 0},
        {3, -1, 2, 3000, 3210, 2200, 4500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_3, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf2II)
{
    /**
     *
     * |--------|----------------------------------------| 3210
     * |        |                                        |
     * |        |                                        |
     * |        |----|                                   | 2010
     * |        |    |----|                              | 2000
     * |        |    |    |                              |
     * |   0    | 2  |  3 |                              |
     * |        |----|----|--------|                     | 1000
     * |        |                  |                     |
     * |        |         1        |                     |
     * |        |                  |                     |
     * |--------|------------------|---------------------| 0
     *        1000 1500  2200    3000                  6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 2000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1010, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 3000, 1000, 3000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 3000, 2010, 1500, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    std::vector<BranchingScheme::Insertion> is {
        {3, -1, 2, 3000, 2010, 2500, 4500, 3210, 0, 0},
        {3, -1, 2, 3000, 2030, 2200, 4500, 3210, 0, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_3, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf2III)
{
    /**
     *
     * |--------|----------------------------------------| 3210
     * |        |                                        |
     * |        |                                        |
     * |        |    |----|                              | 2010
     * |        |----|    |                              | 2000
     * |        |    |    |                              |
     * |   0    | 2  |  3 |                              |
     * |        |----|----|--------|                     | 1000
     * |        |                  |                     |
     * |        |         1        |                     |
     * |        |                  |                     |
     * |--------|------------------|---------------------| 0
     *        1000 1500  2200    3000                  6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 2000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1010, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 3000, 1000, 3000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 3000, 2000, 1500, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    std::vector<BranchingScheme::Insertion> is {
        {3, -1, 2, 3000, 2000, 2510, 4500, 3210, 0, 0},
        {3, -1, 2, 3000, 2030, 2200, 4500, 3210, 0, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_3, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf2IV)
{
    /**
     *
     * |--------|----------------------------------------| 3210
     * |        |         |--------|                     | 3000
     * |        |         |        |                     |
     * |        |---------|        |                     | 2500
     * |        |         |    3   |                     |
     * |        |    2    |        |                     |
     * |   0    |         |        |                     |
     * |        |---------|--------|                     | 1000
     * |        |                  |                     |
     * |        |         1        |                     |
     * |        |                  |                     |
     * |--------|------------------|---------------------| 0
     *        1000      2000     3000                  6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 2000, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 2000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 3000, 1000, 3000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 3000, 2500, 2000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    std::vector<BranchingScheme::Insertion> is {
        {3, -1, 2, 4000, 2500, 4000, 4500, 3210, 0, 0},
        {3, -1, 2, 3000, 3000, 3000, 4500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_3, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDf2V)
{
    /**
     *
     * |-------------------------------------------------|
     * |   |----|                                        | 3000
     * |---|    |                                        | 2500
     * |   |    |                                        |
     * | 1 |  2 |                                        |
     * |   |    |                                        |
     * |---|----|                                        | 1500
     * |        |                                        |
     * |        |                                        |
     * |    0   |                                        |
     * |        |                                        |
     * |--------|----------------------------------------| 0
     *    500 1000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 1500, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 1, 1000, 2500, 500, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 2, 2000, 2500, 2000, 3500, 3210, 0, 0},
        {2, -1, 2, 1000, 3000, 1000, 3500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionX1)
{
    /**
     *
     * |-------------------------------------------------|
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |-------------------|                             | 1500
     * |         2         |                             |
     * |------------------||                             | 1000
     * |        1         |                              |
     * |-----------------||                              | 500
     * |       0         |                               |
     * |-----------------|-------------------------------|
     *                 980
     *                  990
     *                   1000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 980, -1, 1, false, 0);
    instance_builder.add_item_type(500, 990, -1, 1, false, 1);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 2);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 980, 500, 980, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 1, 1010, 1000, 990, 3500, 3210, 1, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 2, 1990, 1000, 1990, 3500, 3210, 0, 0},
        {2, -1, 2, 1490, 1500, 1490, 3500, 3210, 0, 0}, // ?
        {2, -1, 1, 1020, 1500, 1000, 3500, 3210, 1, 0},
        {2, -1, 1, 1010, 2000, 500, 3500, 3210, 1, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionX2)
{
    /**
     * Test from a bugged solution of instanace A5.
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(431, 658, -1, 1, false, 0);
    instance_builder.add_item_type(431, 1170, -1, 1, false, 1);
    instance_builder.add_item_type(303, 1054, -1, 1, false, 2);
    instance_builder.add_item_type(568, 1399, -1, 1, false, 3);
    instance_builder.add_item_type(545, 872, -1, 1, false, 4);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 431, 658, 431, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 2, 862, 1170, 862, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 862, 2224, 303, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    BranchingScheme::Insertion i3 = {3, -1, 2, 891, 2569, 871, 3500, 3210, 1, 0};
    std::vector<BranchingScheme::Insertion> is3 = branching_scheme.insertions(node_3, info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    auto node_4 = branching_scheme.child(node_3, i3);

    BranchingScheme::Insertion i4 = {4, -1, 1, 892, 3114, 872, 3500, 3210, 1, 0};
    std::vector<BranchingScheme::Insertion> is4 = branching_scheme.insertions(node_4, info);
    EXPECT_NE(std::find(is4.begin(), is4.end(), i4), is4.end());
    auto node_5 = branching_scheme.child(node_4, i4);
}

TEST(RectangleGuillotineBranchingScheme, InsertionX3)
{
    /**
     *
     * |------------------------|------------------------|
     * |                        |                        |
     * |                        |                        |
     * |                        |                        |
     * |                        |                        |
     * |                        |                        |
     * |                        |                        |
     * |                        |-----------------------|| 1000
     * |           0            |                       ||
     * |                        |            1          ||
     * |                        |                       ||
     * |                        |----------------------||| 1000
     * |                        |                      | |
     * |                        |           0          | |
     * |                        |                      | |
     * |------------------------|----------------------|-|
     *                        3000                   5960
     *                                                5970
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(3000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 2960, -1, 1, false, 1);
    instance_builder.add_item_type(1000, 2970, -1, 1, false, 2);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 3210, 3000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_1).x1_curr, 3000);

    BranchingScheme::Insertion i1 = {1, -1, 0, 5960, 1000, 5960, 6000, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);
    EXPECT_EQ(static_cast<const BranchingScheme::Node&>(*node_2).x1_prev, 3000);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 1, 6000, 2000, 5970, 6000, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionY1)
{
    /**
     *
     * |-------------------------------------------------|
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |       |---|                                     | 1000
     * |   |---|   |                                     | 990
     * |---|   |   |                                     | 980
     * |   |   |   |                                     |
     * |   | 1 | 2 |                                     |
     * | 0 |   |   |                                     |
     * |   |   |   |                                     |
     * |   |   |   |                                     |
     * |---|---|---|-------------------------------------|
     *   500 1000 1500
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 980, -1, 1, false, 0);
    instance_builder.add_item_type(500, 990, -1, 1, false, 1);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 2);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 500, 980, 500, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 2, 1000, 1010, 1000, 3500, 3210, 0, 1};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is {
        {2, -1, 2, 2000, 1010, 2000, 3500, 3210, 0, 1},
        {2, -1, 2, 1500, 1020, 1500, 3500, 3210, 0, 1},
        {2, -1, 1, 1000, 1510, 1000, 3500, 3210, 0, 0},
        {2, -1, 1, 1000, 2010, 500, 3500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionY2)
{
    /**
     *
     * |-------------------------------------------------| 3210
     * |   |-------|                                     | 3200
     * |   |       |                                     |
     * |---|       |                                     | 3000
     * |   |       |                                     |
     * |   |   1   |                                     |
     * | 0 |       |                                     |
     * |   |       |                                     |
     * |   |       |                                     |
     * |---|-------|-------------------------------------|
     *   1000    2000
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 3000, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 3200, -1, 1, false, 1);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3000, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {1, -1, 0, 4200, 1000, 4200, 4500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDfm1)
{
    /**
     *
     * |-------------------------------------------------|
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |                                                 |
     * |--------|                                        | 600
     * |        |                                        |
     * |   1    |--------|                               | 500
     * |--------|        |                               | 400
     * |        |        |                               |
     * |   0    |   2    |                               |
     * |        |        |                               |
     * |--------|--------|-------------------------------|
     *        1000      2001
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(400, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1001, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1002, -1, 1, false, 0);
    instance_builder.add_item_type(200, 1000, -1, 1, false, 1);
    instance_builder.add_item_type(590, 1003, -1, 1, false, 1);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, 3, -1, 1000, 600, 1000, 3500, 3210, 0, 2};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is1 {
        {1, -1, 2, 2001, 600, 2001, 3500, 3210, 0, 2},
        {1, -1, 1, 1021, 1100, 1001, 3500, 3210, 1, 0},
        {1, -1, 1, 1000, 1601, 500, 3500, 3210, 0, 0},
        {4, -1, 1, 1023, 1190, 1003, 3500, 3210, 1, 0},
        {4, -1, 1, 1000, 1603, 590, 3500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is1);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDf0)
{
    /**
     *
     * |-------|-------------------------------------------------|
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |   0   |                                                 |
     * |       |--------|                                        | 600
     * |       |        |                                        |
     * |       |   2    |--------|                               | 500
     * |       |--------|        |                               | 400
     * |       |        |        |                               |
     * |       |   1    |    3   |                               |
     * |       |        |        |                               |
     * |-------|--------|--------|-------------------------------|
     *       1000     2000     3001
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(400, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(500, 1001, -1, 1, false, 0);
    instance_builder.add_item_type(700, 1002, -1, 1, false, 0);
    instance_builder.add_item_type(200, 1000, -1, 1, false, 1);
    instance_builder.add_item_type(590, 1003, -1, 1, false, 1);
    instance_builder.add_item_type(1000, 3210, -1, 1, false, 2);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {5, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {0, 3, 0, 2000, 600, 2000, 4500, 3210, 0, 2};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is2 {
        {1, -1, 2, 3001, 600, 3001, 4500, 3210, 0, 2},
        {1, -1, 1, 2021, 1100, 2001, 4500, 3210, 1, 0},
        {1, -1, 1, 2000, 1601, 1500, 4500, 3210, 0, 0},
        {4, -1, 1, 2023, 1190, 2003, 4500, 3210, 1, 0},
        {4, -1, 1, 2000, 1603, 1590, 4500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is2);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDf1)
{
    /**
     *
     * |-------|-------------------------------------------------|
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |       |                                                 |
     * |       |--------|                                        | 1600
     * |       |   5    |--------|                               | 1500
     * |       |--------|        |                               | 1400
     * |   0   |        |        |                               |
     * |       |   2    |    3   |                               |
     * |       |        |        |                               |
     * |       |--------|--------|                               |
     * |       |                 |                               |
     * |       |        1        |                               |
     * |       |                 |                               |
     * |-------|-----------------|-------------------------------|
     *       1000     2000     3000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(3210, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 2000, -1, 1, false, 1);
    instance_builder.add_item_type(400, 1000, -1, 1, false, 2);
    instance_builder.add_item_type(500, 1001, -1, 1, false, 2);
    instance_builder.add_item_type(700, 1002, -1, 1, false, 2);
    instance_builder.add_item_type(200, 1000, -1, 1, false, 3);
    instance_builder.add_item_type(590, 1003, -1, 1, false, 3);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 3210, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 3000, 1000, 3000, 4500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, 5, 1, 3000, 1600, 2000, 4500, 3210, 0, 2};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    std::vector<BranchingScheme::Insertion> is {
        {3, -1, 2, 3021, 1600, 3001, 4500, 3210, 1, 2},
        {3, -1, 1, 3000, 2100, 2001, 4500, 3210, 0, 0},
        {3, -1, 1, 3000, 2601, 1500, 4500, 3210, 0, 0},
        {6, -1, 1, 3000, 2190, 2003, 4500, 3210, 0, 0},
        {6, -1, 1, 3000, 2603, 1590, 4500, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_3, info), is);
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutDf2)
{
    /**
     *
     * |-------|--------------------------|
     * |       |                          |
     * |       |                          |
     * |       |        |--------|        | 2600
     * |       |--------|   3    |--------| 2500
     * |       |        |--------|        | 2400
     * |       |        |        |        |
     * |       |    2   |   0    |    1   |
     * |       |        |        |        |
     * |   0   |--------|--------|--------| 2000
     * |       |                          |
     * |       |                          |
     * |       |                          |
     * |       |            1             |
     * |       |                          |
     * |       |                          |
     * |       |                          |
     * |-------|--------------------------|
     *       3000     4000     5000      6000
     *
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(3000, 3210, -1, 1, false, 0);
    instance_builder.add_item_type(2000, 3000, -1, 1, false, 1);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 2);
    instance_builder.add_item_type(400, 1000, -1, 1, false, 3);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 3);
    instance_builder.add_item_type(700, 1000, -1, 1, false, 3);
    instance_builder.add_item_type(200, 1000, -1, 1, false, 4);
    instance_builder.add_item_type(590, 1000, -1, 1, false, 4);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 3210, 3000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 0, 6000, 2000, 6000, 6000, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    BranchingScheme::Insertion i2 = {2, -1, 1, 6000, 2500, 4000, 6000, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is2 = branching_scheme.insertions(node_2, info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    auto node_3 = branching_scheme.child(node_2, i2);

    BranchingScheme::Insertion i3 = {3, 6, 2, 6000, 2600, 5000, 6000, 3210, 0, 2};
    std::vector<BranchingScheme::Insertion> is3 = branching_scheme.insertions(node_3, info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    auto node_4 = branching_scheme.child(node_3, i3);

    std::vector<BranchingScheme::Insertion> is {
        {4, -1, 2, 6000, 2600, 6000, 6000, 3210, 0, 2},
    };
    EXPECT_EQ(branching_scheme.insertions(node_4, info), is);
}

TEST(RectangleGuillotineBranchingScheme, PlateRotationI)
{
    /**
     *
     * |--------------------------------------------------| 3210
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |-------------------|                              | 500
     * |         0         | 500                          |
     * |-------------------|------------------------------|
     *                   1000                           6000
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.set_first_stage_orientation(CutOrientation::Any);
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0},
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
        {0, -1, -2, 500, 1000, 500, 3210, 6000, 0, 0},
        {0, -1, -2, 1000, 500, 1000, 3210, 6000, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionSymmetry)
{
    /**
     *
     * |---------------------| 200
     * |                     |
     * |--------|--------|   | 180
     * |        |        |   |
     * |   1    |   2    |   |
     * |        |        |   |
     * |--------|-----|--|   | 100
     * |              |      |
     * |     0        |      |
     * |              |      |
     * |              |      |
     * |--------------|------|
     *         40    70 80 100
     *
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_type_1(CutType1::ThreeStagedGuillotine);
    instance_builder.set_cut_type_2(CutType2::NonExact);
    instance_builder.set_item_types_oriented();
    instance_builder.add_item_type(70, 100, -1, 1, false, 0);
    instance_builder.add_item_type(40, 80, -1, 1, false, 0);
    instance_builder.add_item_type(40, 80, -1, 1, false, 0);
    instance_builder.add_bin_type(100, 200);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 70, 100, 70, 100, 200, 1, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    BranchingScheme::Insertion i1 = {1, -1, 1, 70, 180, 40, 100, 200, 1, 1};
    std::vector<BranchingScheme::Insertion> is1 = branching_scheme.insertions(node_1, info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    auto node_2 = branching_scheme.child(node_1, i1);

    std::vector<BranchingScheme::Insertion> is2 {
        {2, -1, 2, 80, 180, 80, 100, 200, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_2, info), is2);
}

TEST(RectangleGuillotineBranchingScheme, InsertionTwoStagedMinWasteI)
{
    /**
     * This insertion is not valid since it violates the minimum waste
     * constraint. The two insertion is also not valid.
     *
     * |--------------------------------------------------| 3210
     * |------|                                           | 3200
     * |      |                                           |
     * |      |                                           |
     * |      |                                           |
     * |      |                                           |
     * |  0   |                                           |
     * |      |                                           |
     * |      |                                           |
     * |      |                                           |
     * |      |                                           |
     * |------|-------------------------------------------|
     *       500                                        6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.set_cut_type_1(CutType1::TwoStagedGuillotine);
    instance_builder.add_item_type(500, 3200, -1, 1, false, 0);
    instance_builder.add_item_type(500, 3200, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -2, 3210, 3200, 500, 3210, 6000, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionTwoStagedMinWasteII)
{
    /**
     * Same as above, but with an insertion above a defect.
     * The defect insertion remains valid.
     *
     * Note: in the defect insertion, the position of x3 is 3200, which would
     * violate the minimum waste constraint. However, this cut is removed when
     * converting the Node to a Solution.
     *
     * |--------------------------------------------------| 3210
     * |  |---|                                           | 3200
     * | x|   |                                           | 3190
     * |  |   |                                           |
     * |  |   |                                           |
     * |  |   |                                           |
     * |  |   |                                           | 1500
     * |  |   |                                           |
     * |  |   |                                           |
     * |  |   |                                           |
     * |  |   |                                           |
     * |--|---|-------------------------------------------|
     *   250 500                                        6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.set_cut_type_1(CutType1::TwoStagedGuillotine);
    instance_builder.add_item_type(250, 3200, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 240, 3190, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -2, 3210, 3200, 250, 3210, 6000, 0, 0},
        {-1, -1, -2, 3210, 250, 3200, 3210, 6000, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionUpdate)
{
    /**
     * First we insert item 0 and then we try to insert the defect in a new
     * second-level sub-plate (df == 1).
     * We expect x3 = 3015, x1 = 3020, z1 = 1
     * Note that the minimum waste constraint is not satisfied between x3 and
     * x1. This is not an issue since either an item will be inserted next in
     * the same second-level sub-plate or no item will be inserted in the same
     * second-level sub-plate next but then the 3-cut would be removed when
     * converting the Node to a Solution.
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |               x                  | 1600
     * |--------------|                   | 1500
     * |              |                   |
     * |       0      |                   |
     * |--------------|-------------------|
     *              3000                 6000
     *               3005
     *
     */

    Info info = Info()
        //.set_log2stderr(true)
        ;

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(3000, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(3050, 1500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 3005, 1590, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 1500, 3000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root, info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {-1, -1, 2, 3020, 1600, 3020, 3500, 3210, 1, 1},
        {-1, 1, 1, 3050, 3100, 3050, 3500, 3210, 0, 1},
        {-1, -1, 1, 3020, 1600, 3015, 3500, 3210, 1, 1},
        //{-1, -1, 0, 3020, 1600, 3020, 6000, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1, info), is);
}

