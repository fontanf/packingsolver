#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, CutThickness1)
{
    /**
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
     * |       |-------|                  | 1000
     * |       |       |                  |
     * |       |       |                  |
     * |-------|   1   |                  | 500
     * |   0   |       |                  |
     * |       |       |                  |
     * |-------|-------|------------------|
     *       1000    1520
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.add_item_type(500, 500, -1, 1, false, 0);
    instance_builder.add_item_type(1000, 1000, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 500, 500, 500, 6000, 3210, 1, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {1, -1, 2, 1520, 1000, 1520, 6000, 3210, 1, 1},
        {1, -1, 1, 1000, 1520, 1000, 6000, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness2)
{
    /**
     * Test with "partial" cutting.
     *
     * |---------------|---------------|
     * |               |               |
     * |               |               |
     * |               |               |
     * |               |               |
     * |               |       1       |
     * |               |               |
     * |---------------|               |
     * |       0       |               |
     * |               |               |
     * |---------------|---------------|
     *               3000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.add_item_type(3000, 500, -1, 1, false, 0);
    instance_builder.add_item_type(2970, 3210, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 500, 3000, 6000, 3210, 1, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {1, -1, 0, 5990, 3210, 5990, 6000, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness3)
{
    /**
     * If min_waste is > 0, then "partial" cutting is not allowed.
     *
     * |---------------|---------------|
     * |               |               |
     * |               |               |
     * |               |               |
     * |               |               |
     * |               |       1       |
     * |               |               |
     * |---------------|               |
     * |       0       |               |
     * |               |               |
     * |---------------|---------------|
     *               3000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.set_minimum_waste_length(10);
    instance_builder.add_item_type(3000, 500, -1, 1, false, 0);
    instance_builder.add_item_type(2970, 3210, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 500, 3000, 6000, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness4)
{
    /**
     * Test with "partial" cutting in Y.
     *
     * |-------------------------------|
     * |                               |
     * |                               |
     * |               1               |
     * |                               |
     * |                               |
     * |---------------|---------------| 1000
     * |               |               |
     * |       0       |               |
     * |               |               |
     * |---------------|---------------|
     *               3000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.add_item_type(3000, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(6000, 2180, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 1000, 3000, 6000, 3210, 1, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {1, -1, 1, 6000, 3200, 6000, 6000, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness5)
{
    /**
     * If min_waste is > 0, then "partial" cutting is not allowed.
     *
     * |-------------------------------|
     * |                               |
     * |                               |
     * |               1               |
     * |                               |
     * |                               |
     * |---------------|---------------| 1000
     * |               |               |
     * |       0       |               |
     * |               |               |
     * |---------------|---------------|
     *               3000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.set_minimum_waste_length(10);
    instance_builder.add_item_type(3000, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(6000, 2180, -1, 1, false, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 1000, 3000, 6000, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness6)
{
    /**
     * Test the value of x1_max when cutting through defects is NOT allowed.
     * The defects falls inside the 2-cut horizontally.
     *
     * |-------------------|-----------|
     * |                   |           |
     * |                   |           |
     * |         1         |           |
     * |                   |           |
     * |                   |           |
     * |---------------|---|     x     | 1000
     * |               |               |
     * |       0       |               |
     * |               |               |
     * |---------------|---------------|
     *               3000      4000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.set_cut_through_defects(false);
    instance_builder.add_item_type(3000, 1000, -1, 1, false, 0);
    instance_builder.add_item_type(3500, 2190, -1, 1, false, 0);
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(bin_type_id, 4000, 1000, 20, 20);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 1000, 3000, 6000, 3210, 1, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {-1, -1, 2, 4020, 1020, 4020, 6000, 3210, 1, 1},
        {1, -1, 1, 3500, 3210, 3500, 3980, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness7)
{
    /**
     * Test the value of x1_max when cutting through defects is NOT allowed.
     * The defects falls inside the 3-cut vertically.
     *
     * |-------------------------------|
     * |                               |
     * |                               |
     * |               x               | 2500
     * |                               |
     * |               |---------|     | 2000
     * |               |         |     |
     * |               |         |     |
     * |---------------|    1    |     | 1500
     * |               |         |     |
     * |       0       |         |     |
     * |               |         |     |
     * |---------------|---------|-----|
     *               3000      4000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.set_cut_through_defects(false);
    instance_builder.add_item_type(3000, 1500, -1, 1, true, 0);
    instance_builder.add_item_type(1000, 2000, -1, 1, true, 0);
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(bin_type_id, 3000, 2500, 20, 20);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3020, 1500, 3000, 6000, 2480, 1, 1};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {1, -1, 2, 4020, 2000, 4020, 6000, 2480, 1, 1},
        {-1, -1, 1, 3020, 2520, 3020, 6000, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness8)
{
    /**
     * The item cannot be inserted because otherwise the cut goes through the
     * defect.
     *
     * |-------------------------------|
     * |                               |
     * |                               |
     * |                               |
     * |                               |
     * |                               |
     * |                               |
     * |                               |
     * |---------------|               | 1000
     * |               |               |
     * |       0       |x              | 500
     * |               |               |
     * |---------------|---------------|
     *               3000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.set_cut_through_defects(false);
    instance_builder.add_item_type(3000, 1000, -1, 1, true, 0);
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_defect(bin_type_id, 3005, 500, 10, 10);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, -1, -1, 3015, 510, 3015, 6000, 3210, 1, 1},
    };
    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, CutThickness9)
{
    /**
     * When inserting item 1, the 1-cut.must be moved by cut_thickness +
     * min_waste if min_waste > 1.
     *
     * |-----------------|-----------|
     * |                 |           |
     * |         1       |           |
     * |                 |           |
     * |---------------|-|           | 1000
     * |               |             |
     * |       0       |             |
     * |               |             |
     * |---------------|-------------|
     *               3000
     *
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_cut_thickness(20);
    instance_builder.set_minimum_waste_length(30);
    instance_builder.add_item_type(3000, 1000, -1, 1, true, 0);
    instance_builder.add_item_type(3010, 2190, -1, 1, true, 0);
    instance_builder.add_bin_type(6000, 3210);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    BranchingScheme::Insertion i0 = {0, -1, -1, 3000, 1000, 3000, 6000, 3210, 0, 0};
    std::vector<BranchingScheme::Insertion> is0 = branching_scheme.insertions(root);
    EXPECT_NE(std::find(is0.begin(), is0.end(), i0), is0.end());
    auto node_1 = branching_scheme.child(root, i0);

    std::vector<BranchingScheme::Insertion> is {
        {1, -1, 1, 3060, 3210, 3010, 6000, 3210, 1, 0},
    };
    EXPECT_EQ(branching_scheme.insertions(node_1), is);
}
