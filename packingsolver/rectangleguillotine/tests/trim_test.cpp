#include "packingsolver/rectangleguillotine/branching_scheme.hpp"

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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(6000, 3210);
    instance.add_trims(
            i,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Soft,
            0, TrimType::Soft);
    instance.add_defect(i, 495, 25, 5, 5);
    instance.add_item_type(1000, 500, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 530, 1000, 3500, 3210, 0, 1},
        {-1, -1, -1, 500, 30, 500, 3500, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(6000, 3210);
    instance.add_trims(
            i,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Hard,
            0, TrimType::Soft);
    instance.add_defect(i, 495, 25, 5, 5);
    instance.add_item_type(1000, 500, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 1000, 540, 1000, 3500, 3210, 0, 1},
        {-1, -1, -1, 500, 40, 500, 3500, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(6000, 3210);
    instance.add_trims(
            i,
            20, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance.add_defect(i, 25, 495, 5, 5);
    instance.add_item_type(500, 1000, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 520, 1500, 520, 3520, 3210, 0, 1},
        {-1, -1, -1, 30, 500, 30, 3520, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(6000, 3210);
    instance.add_trims(
            i,
            20, TrimType::Hard,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance.add_defect(i, 25, 495, 5, 5);
    instance.add_item_type(500, 1000, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {-1, 0, -1, 520, 1500, 520, 3520, 3210, 0, 1},
        {-1, -1, -1, 40, 500, 40, 3520, 3210, 1, 1},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(6000, 3210);
    instance.add_trims(
            i,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Soft);
    instance.add_item_type(1000, 3180, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 1000, 3180, 1000, 3500, 3190, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(6000, 3210);
    instance.add_trims(
            i,
            0, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft,
            20, TrimType::Hard);
    instance.add_item_type(1000, 3180, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(3000, 3210);
    instance.add_trims(
            i,
            0, TrimType::Soft,
            20, TrimType::Soft,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance.add_item_type(2970, 500, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
        {0, -1, -1, 2970, 500, 2970, 2980, 3210, 0, 0},
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
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

    Info info;

    Instance instance;
    instance.set_objective(Objective::BinPackingWithLeftovers);
    instance.set_roadef2018();
    BinTypeId i = instance.add_bin_type(3000, 3210);
    instance.add_trims(
            i,
            0, TrimType::Soft,
            20, TrimType::Hard,
            0, TrimType::Soft,
            0, TrimType::Soft);
    instance.add_item_type(2970, 500, -1, 1, true, true);

    BranchingScheme branching_scheme(instance);
    auto root = branching_scheme.root();

    std::vector<BranchingScheme::Insertion> is {
    };

    EXPECT_EQ(branching_scheme.insertions(root, info), is);
}
