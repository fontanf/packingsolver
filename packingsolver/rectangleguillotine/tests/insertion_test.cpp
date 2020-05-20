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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 1000, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 500, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 0, .j2 = -1, .df = -1, .x1 = 500, .y2 = 1000, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };

    EXPECT_EQ(node.children(info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionDfm1II)
{
    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(3000, 3210, -1, 1, false, true);
    instance.add_item(3000, 3210, -1, 1, false, false);
    instance.add_item(3000, 3210, -1, 1, false, false);
    instance.add_bin(6000, 3210);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 3000, .y2 = 3210, .x3 = 3000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 6000, .y2 = 3210, .x3 = 6000, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = -1, .x1 = 3000, .y2 = 3210, .x3 = 3000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 1500, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    std::vector<BranchingScheme::Insertion> is {
        //{.j1 = 1, .j2 = -1, .df = 2, .x1 = 2500, .y2 = 3210, .x3 = 2500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        //{.j1 = 1, .j2 = -1, .df = 2, .x1 = 2000, .y2 = 3210, .x3 = 2000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 1, .j2 = -1, .df = 0, .x1 = 2500, .y2 = 1000, .x3 = 2500, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 1, .j2 = -1, .df = 0, .x1 = 2000, .y2 = 1500, .x3 = 2000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 3000, -1, 1, false, false);
    instance.add_item(1100, 2000, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 4000, .y2 = 1000, .x3 = 4000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 4000, .y2 = 2100, .x3 = 3000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 4000, .y2 = 3000, .x3 = 2100, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };

    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 3000, -1, 1, false, false);
    instance.add_item(1100, 2990, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 4000, .y2 = 1000, .x3 = 4000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 4020, .y2 = 2100, .x3 = 3990, .x1_max = 4500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        //{.j1 = 2, .j2 = -1, .df = 0, .x1 = 5100, .y2 = 2990, .x3 = 5100, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 3000, -1, 1, false, false);
    instance.add_item(1100, 3010, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 4000, .y2 = 1000, .x3 = 4000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 4030, .y2 = 2100, .x3 = 4010, .x1_max = 4500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        //{.j1 = 2, .j2 = -1, .df = 0, .x1 = 5100, .y2 = 3010, .x3 = 5100, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 3000, -1, 1, false, false);
    instance.add_item(1100, 3020, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 4000, .y2 = 1000, .x3 = 4000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 4020, .y2 = 2100, .x3 = 4020, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        //{.j1 = 2, .j2 = -1, .df = 0, .x1 = 5100, .y2 = 3020, .x3 = 5100, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 3000, -1, 1, false, false);
    instance.add_item(1100, 3010, -1, 1, false, false);
    instance.add_item(1110, 3050, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 4000, .y2 = 1000, .x3 = 4000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 1, .x1 = 4030, .y2 = 2100, .x3 = 4010, .x1_max = 4500, .y2_max = 3210, .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 3, .j2 = -1, .df = 1, .x1 = 4050, .y2 = 3210, .x3 = 4050, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 0, .x1 = 5140, .y2 = 3050, .x3 = 5140, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1500, 2000, -1, 1, false, true);
    instance.add_item(1000, 1500, -1, 1, false, false);
    instance.add_item(1010, 1500, -1, 1, false, false);
    instance.add_item(1000, 1500, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 2000, .y2 = 1500, .x3 = 2000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 1, .x1 = 2000, .y2 = 3000, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 2, .x1 = 2030, .y2 = 3000, .x3 = 2010, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3010, .y2 = 3000, .x3 = 3010, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 0, .x1 = 3530, .y2 = 1000, .x3 = 3530, .x1_max = 5530, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 0, .x1 = 3030, .y2 = 1500, .x3 = 3030, .x1_max = 5530, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 2000, -1, 1, false, true);
    instance.add_item(1010, 2005, -1, 1, false, false);
    instance.add_item(1200, 2010, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 2000, .y2 = 1000, .x3 = 2000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 1, .x1 = 2025, .y2 = 2010, .x3 = 2005, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 2, .x1 = 3205, .y2 = 3010, .x3 = 3205, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 2030, .y2 = 3210, .x3 = 2010, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        {.j1 = 2, .j2 = -1, .df = 0, .x1 = 4035, .y2 = 1200, .x3 = 4035, .x1_max = 5525, .y2_max = 3210, .z1 = 0, .z2 = 0},
        //{.j1 = 2, .j2 = -1, .df = 0, .x1 = 3225, .y2 = 2010, .x3 = 3225, .x1_max = 5525, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 2000, -1, 1, false, false);
    instance.add_item(500, 2210, -1, 1, false, false);
    instance.add_item(700, 1010, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 3000, .y2 = 1000, .x3 = 3000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 3210, .x3 = 1500, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    std::vector<BranchingScheme::Insertion> is {
        //{.j1 = 3, .j2 = -1, .df = 2, .x1 = 3000, .y2 = 3210, .x3 = 2510, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3000, .y2 = 3210, .x3 = 2200, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 2000, -1, 1, false, false);
    instance.add_item(500, 1010, -1, 1, false, false);
    instance.add_item(700, 1000, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 3000, .y2 = 1000, .x3 = 3000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 2010, .x3 = 1500, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3000, .y2 = 2010, .x3 = 2500, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3000, .y2 = 2030, .x3 = 2200, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 1},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 2000, -1, 1, false, false);
    instance.add_item(500, 1000, -1, 1, false, false);
    instance.add_item(700, 1010, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 3000, .y2 = 1000, .x3 = 3000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 2000, .x3 = 1500, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3000, .y2 = 2000, .x3 = 2510, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3000, .y2 = 2030, .x3 = 2200, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 1},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_item(1000, 2000, -1, 1, false, false);
    instance.add_item(1000, 1500, -1, 1, false, false);
    instance.add_item(1000, 2000, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 3000, .y2 = 1000, .x3 = 3000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 2500, .x3 = 2000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 4000, .y2 = 2500, .x3 = 4000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3000, .y2 = 3000, .x3 = 3000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 1500, -1, 1, false, true);
    instance.add_item(500, 1000, -1, 1, false, false);
    instance.add_item(500, 1500, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 1500, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 1, .x1 = 1000, .y2 = 2500, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 2, .x1 = 2000, .y2 = 2500, .x3 = 2000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 2, .j2 = -1, .df = 2, .x1 = 1000, .y2 = 3000, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 980, -1, 1, false, true);
    instance.add_item(500, 990, -1, 1, false, true);
    instance.add_item(500, 1000, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 980, .y2 = 500, .x3 = 980, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 1, .x1 = 1010, .y2 = 1000, .x3 = 990, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 2, .x1 = 1990, .y2 = 1000, .x3 = 1990, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 1020, .y2 = 1500, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 1010, .y2 = 2000, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
}

TEST(RectangleGuillotineBranchingScheme, InsertionX2)
{
    /**
     * Test from a bugged solution of instanace A5.
     */

    Info info;

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(431, 658, -1, 1, false, true);
    instance.add_item(431, 1170, -1, 1, false, true);
    instance.add_item(303, 1054, -1, 1, false, true);
    instance.add_item(568, 1399, -1, 1, false, true);
    instance.add_item(545, 872, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 431, .y2 = 658, .x3 = 431, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 2, .x1 = 862, .y2 = 1170, .x3 = 862, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 1, .x1 = 862, .y2 = 2224, .x3 = 303, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    BranchingScheme::Insertion i3 = {.j1 = 3, .j2 = -1, .df = 2, .x1 = 891, .y2 = 2569, .x3 = 871, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is3 = node.children(info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    node.apply_insertion(i3, info);

    BranchingScheme::Insertion i4 = {.j1 = 4, .j2 = -1, .df = 1, .x1 = 892, .y2 = 3114, .x3 = 872, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is4 = node.children(info);
    EXPECT_NE(std::find(is4.begin(), is4.end(), i4), is4.end());
    node.apply_insertion(i4, info);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(3000, 3210, -1, 1, false, true);
    instance.add_item(1000, 2960, -1, 1, false, true);
    instance.add_item(1000, 2970, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 3000, .y2 = 3210, .x3 = 3000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);
    EXPECT_EQ(node.x1_curr(), 3000);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 5960, .y2 = 1000, .x3 = 5960, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);
    EXPECT_EQ(node.x1_prev(), 3000);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 6000, .y2 = 2000, .x3 = 5970, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 980, -1, 1, false, true);
    instance.add_item(500, 990, -1, 1, false, true);
    instance.add_item(500, 1000, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 500, .y2 = 980, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 2, .x1 = 1000, .y2 = 1010, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 1};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 2, .j2 = -1, .df = 2, .x1 = 2000, .y2 = 1010, .x3 = 2000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 1},
        {.j1 = 2, .j2 = -1, .df = 2, .x1 = 1500, .y2 = 1020, .x3 = 1500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 1},
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 1000, .y2 = 1510, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 2, .j2 = -1, .df = 1, .x1 = 1000, .y2 = 2010, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(1000, 3000, -1, 1, false, true);
    instance.add_item(1000, 3200, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3000, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 1, .j2 = -1, .df = 0, .x1 = 4200, .y2 = 1000, .x3 = 4200, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(400, 1000, -1, 1, false, true);
    instance.add_item(500, 1001, -1, 1, false, false);
    instance.add_item(700, 1002, -1, 1, false, false);
    instance.add_item(200, 1000, -1, 1, false, true);
    instance.add_item(590, 1003, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = 3, .df = -1, .x1 = 1000, .y2 = 600, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 2};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    std::vector<BranchingScheme::Insertion> is2 {
        {.j1 = 1, .j2 = -1, .df = 2, .x1 = 2001, .y2 = 600, .x3 = 2001, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 2},
        {.j1 = 1, .j2 = -1, .df = 1, .x1 = 1021, .y2 = 1100, .x3 = 1001, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        {.j1 = 1, .j2 = -1, .df = 1, .x1 = 1000, .y2 = 1601, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 4, .j2 = -1, .df = 1, .x1 = 1023, .y2 = 1190, .x3 = 1003, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        {.j1 = 4, .j2 = -1, .df = 1, .x1 = 1000, .y2 = 1603, .x3 = 590, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is2);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(400, 1000, -1, 1, false, true);
    instance.add_item(500, 1001, -1, 1, false, false);
    instance.add_item(700, 1002, -1, 1, false, false);
    instance.add_item(200, 1000, -1, 1, false, true);
    instance.add_item(590, 1003, -1, 1, false, false);
    instance.add_item(1000, 3210, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 5, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 0, .j2 = 3, .df = 0, .x1 = 2000, .y2 = 600, .x3 = 2000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 2};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is2 {
        {.j1 = 1, .j2 = -1, .df = 2, .x1 = 3001, .y2 = 600, .x3 = 3001, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 2},
        {.j1 = 1, .j2 = -1, .df = 1, .x1 = 2021, .y2 = 1100, .x3 = 2001, .x1_max = 4500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        {.j1 = 1, .j2 = -1, .df = 1, .x1 = 2000, .y2 = 1601, .x3 = 1500, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 4, .j2 = -1, .df = 1, .x1 = 2023, .y2 = 1190, .x3 = 2003, .x1_max = 4500, .y2_max = 3210, .z1 = 1, .z2 = 0},
        {.j1 = 4, .j2 = -1, .df = 1, .x1 = 2000, .y2 = 1603, .x3 = 1590, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is2);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(3210, 1000, -1, 1, false, true);
    instance.add_item(1000, 2000, -1, 1, false, true);
    instance.add_item(400, 1000, -1, 1, false, true);
    instance.add_item(500, 1001, -1, 1, false, false);
    instance.add_item(700, 1002, -1, 1, false, false);
    instance.add_item(200, 1000, -1, 1, false, true);
    instance.add_item(590, 1003, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 3210, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 3000, .y2 = 1000, .x3 = 3000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = 5, .df = 1, .x1 = 3000, .y2 = 1600, .x3 = 2000, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 2};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 3, .j2 = -1, .df = 2, .x1 = 3021, .y2 = 1600, .x3 = 3001, .x1_max = 4500, .y2_max = 3210, .z1 = 1, .z2 = 2},
        {.j1 = 3, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 2100, .x3 = 2001, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 3, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 2601, .x3 = 1500, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 6, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 2190, .x3 = 2003, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 6, .j2 = -1, .df = 1, .x1 = 3000, .y2 = 2603, .x3 = 1590, .x1_max = 4500, .y2_max = 3210, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(3000, 3210, -1, 1, false, true);
    instance.add_item(2000, 3000, -1, 1, false, true);
    instance.add_item(500, 1000, -1, 1, false, true);
    instance.add_item(400, 1000, -1, 1, false, true);
    instance.add_item(500, 1000, -1, 1, false, false);
    instance.add_item(700, 1000, -1, 1, false, false);
    instance.add_item(200, 1000, -1, 1, false, true);
    instance.add_item(590, 1000, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 3000, .y2 = 3210, .x3 = 3000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 0, .x1 = 6000, .y2 = 2000, .x3 = 6000, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    BranchingScheme::Insertion i2 = {.j1 = 2, .j2 = -1, .df = 1, .x1 = 6000, .y2 = 2500, .x3 = 4000, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is2 = node.children(info);
    EXPECT_NE(std::find(is2.begin(), is2.end(), i2), is2.end());
    node.apply_insertion(i2, info);

    BranchingScheme::Insertion i3 = {.j1 = 3, .j2 = 6, .df = 2, .x1 = 6000, .y2 = 2600, .x3 = 5000, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 2};
    std::vector<BranchingScheme::Insertion> is3 = node.children(info);
    EXPECT_NE(std::find(is3.begin(), is3.end(), i3), is3.end());
    node.apply_insertion(i3, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 4, .j2 = -1, .df = 2, .x1 = 6000, .y2 = 2600, .x3 = 6000, .x1_max = 6000, .y2_max = 3210, .z1 = 0, .z2 = 2},
    };
    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 1000, -1, 1, false, true);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.first_stage_orientation = CutOrientation::Any;
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 0, .j2 = -1, .df = -1, .x1 = 1000, .y2 = 500, .x3 = 1000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 0, .j2 = -1, .df = -1, .x1 = 500, .y2 = 1000, .x3 = 500, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0},
        {.j1 = 0, .j2 = -1, .df = -2, .x1 = 500, .y2 = 1000, .x3 = 500, .x1_max = 3210, .y2_max = 6000, .z1 = 0, .z2 = 0},
        {.j1 = 0, .j2 = -1, .df = -2, .x1 = 1000, .y2 = 500, .x3 = 1000, .x1_max = 3210, .y2_max = 6000, .z1 = 0, .z2 = 0},
    };

    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(70, 100, -1, 1, false, true);
    instance.add_item(40, 80, -1, 1, false, false);
    instance.add_item(40, 80, -1, 1, false, false);
    instance.add_bin(100, 200);

    BranchingScheme::Parameters p;
    p.cut_type_1 = CutType1::ThreeStagedGuillotine;
    p.cut_type_2 = CutType2::NonExact;
    p.no_item_rotation = true;
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 70, .y2 = 100, .x3 = 70, .x1_max = 100, .y2_max = 200, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    BranchingScheme::Insertion i1 = {.j1 = 1, .j2 = -1, .df = 1, .x1 = 70, .y2 = 180, .x3 = 40, .x1_max = 100, .y2_max = 200, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is1 = node.children(info);
    EXPECT_NE(std::find(is1.begin(), is1.end(), i1), is1.end());
    node.apply_insertion(i1, info);

    std::vector<BranchingScheme::Insertion> is2 {
        {.j1 = 2, .j2 = -1, .df = 2, .x1 = 80, .y2 = 180, .x3 = 80, .x1_max = 100, .y2_max = 200, .z1 = 0, .z2 = 0},
    };
    EXPECT_EQ(node.children(info), is2);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(500, 3200, -1, 1, false, true);
    instance.add_item(500, 3200, -1, 1, false, false);
    instance.add_bin(6000, 3210);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.cut_type_1 = CutType1::TwoStagedGuillotine;
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 0, .j2 = -1, .df = -2, .x1 = 3210, .y2 = 3200, .x3 = 500, .x1_max = 3210, .y2_max = 6000, .z1 = 0, .z2 = 0},
    };

    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(250, 3200, -1, 1, false, true);
    instance.add_bin(6000, 3210);
    instance.add_defect(0, 240, 3190, 10, 10);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    p.cut_type_1 = CutType1::TwoStagedGuillotine;
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = 0, .j2 = -1, .df = -2, .x1 = 3210, .y2 = 3200, .x3 = 250, .x1_max = 3210, .y2_max = 6000, .z1 = 0, .z2 = 0},
        {.j1 = -1, .j2 = -1, .df = -2, .x1 = 3210, .y2 = 250, .x3 = 3200, .x1_max = 3210, .y2_max = 6000, .z1 = 1, .z2 = 1},
    };

    EXPECT_EQ(node.children(info), is);
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

    Instance instance(Objective::BinPackingWithLeftovers);
    instance.add_item(3000, 1500, -1, 1, false, true);
    instance.add_item(3050, 1500, -1, 1, false, false);
    instance.add_bin(6000, 3210);
    instance.add_defect(0, 3005, 1590, 10, 10);

    BranchingScheme::Parameters p;
    p.set_roadef2018();
    BranchingScheme branching_scheme(instance, p);
    BranchingScheme::Node node(branching_scheme);

    BranchingScheme::Insertion i0 = {.j1 = 0, .j2 = -1, .df = -1, .x1 = 3000, .y2 = 1500, .x3 = 3000, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 0};
    std::vector<BranchingScheme::Insertion> is0 = node.children(info);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    node.apply_insertion(i0, info);

    std::vector<BranchingScheme::Insertion> is {
        {.j1 = -1, .j2 = -1, .df = 2, .x1 = 3020, .y2 = 1600, .x3 = 3020, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 1},
        {.j1 = -1, .j2 = 1, .df = 1, .x1 = 3050, .y2 = 3100, .x3 = 3050, .x1_max = 3500, .y2_max = 3210, .z1 = 0, .z2 = 1},
        {.j1 = -1, .j2 = -1, .df = 1, .x1 = 3020, .y2 = 1600, .x3 = 3015, .x1_max = 3500, .y2_max = 3210, .z1 = 1, .z2 = 1},
        //{.j1 = -1, .j2 = -1, .df = 0, .x1 = 3020, .y2 = 1600, .x3 = 3020, .x1_max = 6000, .y2_max = 3210, .z1 = 1, .z2 = 1},
    };
    EXPECT_EQ(node.children(info), is);
}

