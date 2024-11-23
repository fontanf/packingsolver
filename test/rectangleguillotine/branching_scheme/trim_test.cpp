#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/branching_scheme.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

TEST(RectangleGuillotineBranchingScheme, BottomTrimSoft)
{
    /**
     * Since the bottom trim is soft, the item can be packed right above the
     * defect.
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
     * |-------------------|                              |
     * |         x                                        |
     * |--------------------------------------------------| 20
     * |--------------------------------------------------|
     *                   1000                           6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Soft,
            0, TrimType::Soft);
    instance_builder.add_defect(bin_type_id, 495, 25, 5, 5);
    instance_builder.add_item_type(1000, 500, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 530, 1000, 3500, 3210, 0, 1},
        {-1, -1, -1, 500, 30, 500, 3500, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, BottomTrimHard)
{
    /**
     * Since the bottom trim is hard, the item must be packed higher than right
     * above the defect.
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
     * |-------------------|                              |
     * |         x                                        |
     * |--------------------------------------------------| 20
     * |--------------------------------------------------|
     *                   1000                           6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Hard,
            0, TrimType::Soft);
    instance_builder.add_defect(bin_type_id, 495, 25, 5, 5);
    instance_builder.add_item_type(1000, 500, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 540, 1000, 3500, 3210, 0, 1},
        {-1, -1, -1, 500, 40, 500, 3500, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, LeftTrimSoft)
{
    /**
     * Since the left trim is soft, when packing the defect, the 1-cut can be
     * placed just to the right of the defect.
     *
     * |-|--------------------------------------------------| 3210
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |x                                                 |
     * | |                                                  |
     * |-|--------------------------------------------------|
     *                                                     6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            20, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance_builder.add_defect(bin_type_id, 25, 495, 5, 5);
    instance_builder.add_item_type(500, 1000, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 520, 1500, 520, 3520, 3210, 0, 1},
        {-1, -1, -1, 30, 500, 30, 3520, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, LeftTrimHard)
{
    /**
     * Since the left trim is hard, when packing the defect, the 1-cut must be
     * placed farer than just on the right of the defect.
     *
     * |-|--------------------------------------------------| 3210
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |                                                  |
     * | |x                                                 |
     * | |                                                  |
     * |-|--------------------------------------------------|
     *                                                     6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            20, TrimType::Hard,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance_builder.add_defect(bin_type_id, 25, 495, 5, 5);
    instance_builder.add_item_type(500, 1000, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 520, 1500, 520, 3520, 3210, 0, 1},
        {-1, -1, -1, 40, 500, 40, 3520, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, TopTrimSoft)
{
    /**
     * Since the top trim is soft, the item can be packed.
     *
     * |--------------------------------------------------| 3210
     * |--------------------------------------------------| 3190
     * |-------------------|                              | 3180
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |         0         |                              |
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |-------------------|------------------------------|
     *                   1000                           6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Soft);
    instance_builder.add_item_type(1000, 3180, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 1000, 3180, 1000, 3500, 3190, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, TopTrimHard)
{
    /**
     * Since the top trim is hard, the item cannot be packed.
     *
     * |--------------------------------------------------| 3210
     * |--------------------------------------------------| 3190
     * |-------------------|                              | 3180
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |         0         |                              |
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |                   |                              |
     * |-------------------|------------------------------|
     *                   1000                           6000
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(6000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Hard);
    instance_builder.add_item_type(1000, 3180, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, RightTrimSoft)
{
    /**
     * Since the right trim is soft, the item can be packed.
     *
     * |---------------------|-| 3210
     * |                     | |
     * |                     | |
     * |                     | |
     * |                     | |
     * |                     | |
     * |                     | |
     * |-------------------| | | 500
     * |                   | | |
     * |         0         | | |
     * |                   | | |
     * |-------------------|-|-|
     *                   2970 3000
     *                     2980
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(3000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            0, TrimType::Soft,
            20, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance_builder.add_item_type(2970, 500, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 2970, 500, 2970, 2980, 3210, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, RightTrimHard)
{
    /**
     * Since the right trim is hard, the item cannot be packed.
     *
     * |---------------------|-| 3210
     * |                     | |
     * |                     | |
     * |                     | |
     * |                     | |
     * |                     | |
     * |                     | |
     * |-------------------| | | 500
     * |                   | | |
     * |         0         | | |
     * |                   | | |
     * |-------------------|-|-|
     *                   2970 3000
     *                     2980
     */

    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_roadef2018();
    BinTypeId bin_type_id = instance_builder.add_bin_type(3000, 3210);
    instance_builder.add_trims(
            bin_type_id,
            0, TrimType::Soft,
            20, TrimType::Hard,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance_builder.add_item_type(2970, 500, -1, 1, true, 0);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}

TEST(RectangleGuillotineBranchingScheme, TrimAndDefect)
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPackingWithLeftovers);
    instance_builder.set_predefined("3EVR");
    instance_builder.set_cut_through_defects(true);
    instance_builder.set_minimum_waste_length(10);
    instance_builder.set_minimum_distance_1_cuts(10);
    instance_builder.set_maximum_distance_1_cuts(3210);
    instance_builder.set_cut_thickness(3);
    BinTypeId bin_type_id = instance_builder.add_bin_type(3210, 2250);
    instance_builder.add_trims(
            bin_type_id,
            10, TrimType::Soft,
            10, TrimType::Hard,
            10, TrimType::Soft,
            10, TrimType::Soft);
    instance_builder.add_defect(bin_type_id, 12, 9, 300, 54);
    instance_builder.add_item_type(910, 846);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, -1, -1, 312, 63, 312, 3200, 2240, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root), is);
}
