#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, InsertionCutOnDefect1)
{
    /**
     * If cutting through defects is not allowed, the position of the 2-cut
     * above item 1 needs to be increased.
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
     * |   x   |-------|                  | 1000
     * |       |       |                  |
     * |       |       |                  |
     * |-------|   1   |                  | 500
     * |   0   |       |                  |
     * |       |       |                  |
     * |-------|-------|------------------|
     *       1000    2000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(200, 3180, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 500, 995, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {1, -1, 2, 2000, 1020, 2000, 3500, 3210, 0, 1},
        {-1, 1, 1, 1000, 2005, 1000, 3500, 3210, 0, 1},
        {-1, -1, 1, 1000, 1005, 510, 3500, 3210, 0, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);

    {
        // Test where cutting through defects is allowed.
        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::BinPackingWithLeftovers);
        instance_builder.set_roadef2018();
        instance_builder.set_cut_through_defects(true);
        instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
        instance_builder.add_item_type(1000, 1000, -1, 1, false, 0);
        instance_builder.add_item_type(200, 3180, -1, 1, false, 0);
        instance_builder.add_bin_type(6000, 3210);
        instance_builder.add_defect(0, 500, 995, 10, 10);
        Instance instance = instance_builder.build();

        BranchingScheme branching_scheme(instance);
        auto root = branching_scheme.root();

        BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0};
        std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
        EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
        auto node_1 = branching_scheme.child(root, i0);

        std::vector<BranchingScheme::Insertion> is {
            {1, -1, 2, 2000, 1000, 2000, 3500, 3210, 0, 0},
            {-1, 1, 1, 1000, 2005, 1000, 3500, 3210, 0, 1},
            {-1, -1, 1, 1000, 1005, 510, 3500, 3210, 0, 1},
        };
        EXPECT_EQ(branching_scheme.insertions(node_1), is);
    }
}

TEST(RectangleGuillotineBranchingScheme, Insertion4CutOnDefect4)
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
     * |-------|                          | 1000
     * |       |                          |
     * |   0   |                          |
     * |       |                          |
     * |-------|                          | 500
     * |       |                          |
     * |       |                          |
     * |       |                          |
     * |-------x--------------------------|
     *       1000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 990, 0, 20, 20);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
        {-1, -1, -1, 1010, 20, 1010, 3500, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is);

    {
        // Test where cutting through defects is allowed.
        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::BinPackingWithLeftovers);
        instance_builder.set_roadef2018();
        instance_builder.set_cut_through_defects(true);
        instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
        instance_builder.add_bin_type(6000, 3210);
        instance_builder.add_defect(0, 990, 0, 20, 20);
        Instance instance = instance_builder.build();

        BranchingScheme branching_scheme(instance);
        auto root = branching_scheme.root();

        std::vector<BranchingScheme::Insertion> is {
            {-1, 0, -1, 1000, 520, 1000, 3500, 3210, 0, 1},
            {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
            {-1, -1, -1, 1010, 20, 1010, 3500, 3210, 1, 1},
        };
        EXPECT_EQ(branching_scheme.insertions(root), is);
    }

}

TEST(RectangleGuillotineBranchingScheme, InsertionXMaxDefect)
{
    /**
     * This configuration is not valid since the 2-cut between items 0 and 1
     * intersects the defect. When inserting item 1, x1_max needs to be set to
     * 2000.
     *
     * |----------------------------------|
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |                                  |
     * |-------------------|              | 1530
     * |                   |              |
     * |        2          |              |
     * |-----------|-------|              | 1010
     * |           |                      |
     * |    1      |                      |
     * |-------|---|  x                   | 500
     * |       |                          |
     * |   0   |                          |
     * |-------|--------------------------|
     *       1000    2000
     *          1500     2500
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(510, 1500, -1, 1, false, 0);
    instance_builder.add_item_type(520, 2500, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 2000, 495, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    // x_max = 2000 for the last two insertions.
    std::vector<BranchingScheme::Insertion> is {
        {-1, 1, 2, 2500, 1015, 2500, 3500, 3210, 0, 1},
        {1, -1, 2, 1510, 1500, 1510, 3500, 3210, 0, 0},
        {-1, -1, 2, 2010, 520, 2010, 3500, 3210, 1, 1},
        {1, -1, 1, 1500, 1010, 1500, 2000, 3210, 0, 0},
        {1, -1, 1, 1000, 2000, 510, 2000, 3210, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);

    {
        // Test where cutting through defects is allowed.
        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::BinPackingWithLeftovers);
        instance_builder.set_roadef2018();
        instance_builder.set_cut_through_defects(true);
        instance_builder.add_item_type(500, 1000, -1, 1, false, 0);
        instance_builder.add_item_type(510, 1500, -1, 1, false, 0);
        instance_builder.add_item_type(520, 2500, -1, 1, false, 0);
        instance_builder.add_bin_type(6000, 3210);
        instance_builder.add_defect(0, 2000, 495, 10, 10);
        Instance instance = instance_builder.build();

        BranchingScheme branching_scheme(instance);
        auto root = branching_scheme.root();

        BranchingScheme::Insertion i0 = {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0};
        std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
        EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
        auto node_1 = branching_scheme.child(root, i0);

        // x_max = 3500 for the last two insertions.
        std::vector<BranchingScheme::Insertion> is {
            {-1, 1, 2, 2500, 1015, 2500, 3500, 3210, 0, 1},
            {1, -1, 2, 1510, 1500, 1510, 3500, 3210, 0, 0},
            {-1, -1, 2, 2010, 520, 2010, 3500, 3210, 1, 1},
            {1, -1, 1, 1500, 1010, 1500, 3500, 3210, 0, 0},
            {1, -1, 1, 1000, 2000, 510, 3500, 3210, 0, 0},
        };
        EXPECT_EQ(branching_scheme.insertions(node_1), is);
    }

}

TEST(RectangleGuillotineBranchingScheme, InsertionYMaxDefect)
{
    /**
     * This configuration is not valid since the 3-cut between items 0 and 1
     * intersects the defect. When inserting item 0, y2_max needs to be set to
     * 900.
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
     * |              |-----|             | 1000
     * |       x      |     |             | 900
     * |              |     |             |
     * |-------|      |  2  |             | 500
     * |   0   |------|     |             | 400
     * |       |  1   |     |             |
     * |-------|------|-----|-------------|
     *       1000    2010  3030
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    instance_builder.add_item_type(1000, 500, -1, 1, false, 0);
    instance_builder.add_item_type(1010, 400, -1, 1, false, 0);
    instance_builder.add_item_type(1020, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 995, 900, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    // y_max = 900 for the first insertion.
    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 1020, 500, 1000, 3500, 900, 1, 0},
        {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
        {-1, -1, -1, 1005, 910, 1005, 3500, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is);

    {
        // Test where cutting through defects is allowed.
        InstanceBuilder instance_builder;
        instance_builder.set_objective(Objective::BinPackingWithLeftovers);
        instance_builder.set_roadef2018();
        instance_builder.set_cut_through_defects(true);
        instance_builder.add_item_type(1000, 500, -1, 1, false, 0);
        instance_builder.add_item_type(1010, 400, -1, 1, false, 0);
        instance_builder.add_item_type(1020, 1000, -1, 1, false, 0);
        instance_builder.add_bin_type(6000, 3210);
        instance_builder.add_defect(0, 995, 900, 10, 10);
        Instance instance = instance_builder.build();

        BranchingScheme branching_scheme(instance);
        auto root = branching_scheme.root();

        // y_max = 3210 for the first insertion.
        // The defect insertion is dominated.
        std::vector<BranchingScheme::Insertion> is {
            {0, -1, -1, 1000, 500, 1000, 3500, 3210, 0, 0},
            {0, -1, -1, 500, 1000, 500, 3500, 3210, 0, 0},
        };
        EXPECT_EQ(branching_scheme.insertions(root), is);
    }
}

