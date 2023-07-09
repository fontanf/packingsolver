#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangle;

TEST(RectangleBranchingScheme, Insertion1)
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
    instance_builder.add_item_type(1000, 500);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters p;
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, false, 1, 0, 0},
        {0, true, 1, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is);

    auto child = branching_scheme.child(root, {0, false, 1, 0, 0});

    std::vector<BranchingScheme::UncoveredItem> uncovered_items {
        {0, 0, 1000, 0, 500},
        {-1, 0, 0, 500, 3210}
    };
    EXPECT_EQ(child->uncovered_items, uncovered_items);
}

TEST(RectangleBranchingScheme, Insertion2)
{
    /**
     *
     * |--------------------------------------------------| 3210
     * |                                                  |
     * |                                                  |
     * |                                                  |
     * |------------------------|                         | 1000
     * |                        |                         |
     * |          1             |                         |
     * |                        |                         |
     * |-------------------|----|                         | 1000
     * |                   |    |                         |
     * |-------------------| 2  |                         | 500
     * |         0         |    |                         |
     * |-------------------|----|-------------------------|
     *                   1000 1250                      6000
     */

    Info info;

    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_item_type(1000, 500, -1, 1, true);
    instance_builder.add_item_type(1250, 1210, -1, 1, true);
    instance_builder.add_item_type(250, 1000, -1, 1, true);
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters p;
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is_root {
        {0, false, 1, 0, 0},
        {1, false, 1, 0, 0},
        {2, false, 1, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is_root);
    EXPECT_EQ(root->waste, 0);


    auto child_1 = branching_scheme.child(root, {0, false, 1, 0, 0});

    std::vector<BranchingScheme::UncoveredItem> uncovered_items_child_1 {
        {0, 0, 1000, 0, 500},
        {-1, 0, 0, 500, 3210}
    };
    EXPECT_EQ(child_1->uncovered_items, uncovered_items_child_1);

    std::vector<BranchingScheme::Insertion> is_child_1 {
        {1, false, 0, 1000, 0},
        {2, false, 0, 1000, 0},
        {1, false, 0, 0, 500},
        {2, false, 0, 0, 500},
    };
    EXPECT_EQ(branching_scheme.insertions(child_1), is_child_1);
    EXPECT_EQ(child_1->waste, 0);


    auto child_2 = branching_scheme.child(child_1, {2, false, 0, 1000, 0});

    EXPECT_EQ(child_2->current_area, 1000 * 1250);
    EXPECT_EQ(child_2->item_area, 1000 * 500 + 250 * 1000);
    EXPECT_EQ(child_2->waste, 1000 * 500);

    std::vector<BranchingScheme::UncoveredItem> uncovered_items_child_2 {
        {2, 1000, 1250, 0, 1000},
        {-1, 0, 0, 1000, 3210}
    };
    EXPECT_EQ(child_2->uncovered_items, uncovered_items_child_2);

    std::vector<BranchingScheme::Insertion> is_child_2 {
        {1, false, 0, 1250, 0},
        {1, false, 0, 0, 1000},
    };
    EXPECT_EQ(branching_scheme.insertions(child_2), is_child_2);


    auto child_3 = branching_scheme.child(child_2, {1, false, 0, 0, 1000});

    EXPECT_EQ(child_3->current_area, 1250 * (1000 + 1210));
    EXPECT_EQ(child_3->item_area, 1000 * 500 + 250 * 1000 + 1250 * 1210);
    EXPECT_EQ(child_3->waste, 1000 * 500);

    std::vector<BranchingScheme::UncoveredItem> uncovered_items_child_3 {
        {2, 1000, 1250, 0, 1000},
        {1, 0, 1250, 1000, 2210},
        {-1, 0, 0, 2210, 3210}
    };
    EXPECT_EQ(child_3->uncovered_items, uncovered_items_child_3);

    std::vector<BranchingScheme::Insertion> is_child_3 {
    };
    EXPECT_EQ(branching_scheme.insertions(child_3), is_child_3);
}

