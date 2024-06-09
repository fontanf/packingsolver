#include "packingsolver/rectangle/instance_builder.hpp"
#include "rectangle/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangle;

TEST(RectangleBranchingScheme, Defects1)
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

    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 100, 50, 20, 10);
    instance_builder.add_item_type(1000, 500);
    instance_builder.set_item_types_oriented();
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters p;
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, false, 1, 120, 0},
        {0, false, 1, 0, 60},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is);

    auto child = branching_scheme.child(root, {0, false, 1, 0, 0});
}

TEST(RectangleBranchingScheme, Defects2)
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

    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 100, 600, 20, 10);
    instance_builder.add_item_type(1000, 500);
    instance_builder.set_item_types_oriented();
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters p;
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, false, 1, 0, 0},
        {0, false, 1, 0, 610},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is);

    auto child = branching_scheme.child(root, {0, false, 1, 0, 0});
}

TEST(RectangleBranchingScheme, Defects3)
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

    InstanceBuilder instance_builder;
    instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(0, 1000, 50, 20, 10);
    instance_builder.add_item_type(1000, 500);
    instance_builder.set_item_types_oriented();
    Instance instance = instance_builder.build();

    BranchingScheme::Parameters p;
    BranchingScheme branching_scheme(instance, p);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, false, 1, 0, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is);

    auto child = branching_scheme.child(root, {0, false, 1, 0, 0});
}
