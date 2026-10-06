#include "packingsolver/boxstacks/instance_builder.hpp"
#include "packingsolver/boxstacks/solution.hpp"
#include "boxstacks/solution_builder.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::boxstacks;

TEST(BoxStacks, LeftoverVolumeAlongEitherAxis)
{
    for (bool transpose: {false, true}) {
        InstanceBuilder ib;
        ib.set_objective(Objective::BinPackingWithLeftovers);
        ib.add_bin_type(100, 100, 100);
        ib.add_item_type(transpose? 20: 100, transpose? 100: 20, 100);
        const Instance instance = ib.build();
        SolutionBuilder sb(instance);
        sb.add_bin(0, 1);
        sb.add_stack(0, 0, transpose? 20: 100, 0, transpose? 100: 20);
        sb.add_item(0, 0, 0, Rotation::XYZ);
        const Solution solution = sb.build();
        EXPECT_TRUE(solution.feasible());
        // Cutting the empty slab leaves a container filled by this item.
        EXPECT_EQ(solution.volume(), 200000);
        EXPECT_EQ(solution.waste(), 0);
        EXPECT_EQ(solution.full_waste(), 800000);
    }
}

TEST(BoxStacks, LeftoverVolumeRanksSolutions)
{
    InstanceBuilder ib;
    ib.set_objective(Objective::BinPackingWithLeftovers);
    ib.add_bin_type(100, 100, 100);
    auto item = ib.add_item_type(10, 10, 10);
    ib.set_item_type_copies(item, 2);
    const Instance instance = ib.build();
    auto build = [&](Length x, Length y) {
        SolutionBuilder sb(instance);
        sb.add_bin(0, 1);
        sb.add_stack(0, 0, 10, 0, 10);
        sb.add_item(0, 0, item, Rotation::XYZ);
        sb.add_stack(0, x, x + 10, y, y + 10);
        sb.add_item(0, 1, item, Rotation::XYZ);
        return sb.build();
    };
    const Solution a = build(90, 10);
    const Solution b = build(20, 90);
    ASSERT_TRUE(a.feasible());
    ASSERT_TRUE(b.feasible());
    EXPECT_EQ(a.volume(), 200000);
    EXPECT_EQ(b.volume(), 300000);
    EXPECT_TRUE(b < a);  // operator< means the right solution is better.
    EXPECT_FALSE(a < b);
}

TEST(BoxStacks, LeftoverVolumeOnlyLastPhysicalBin)
{
    for (bool pattern_copies: {false, true}) {
        InstanceBuilder ib;
        ib.set_objective(Objective::BinPackingWithLeftovers);
        auto bin = ib.add_bin_type(100, 100, 100);
        ib.set_bin_type_copies(bin, 2);
        auto item = ib.add_item_type(100, 20, 100);
        ib.set_item_type_copies(item, 2);
        const Instance instance = ib.build();
        SolutionBuilder sb(instance);
        for (BinPos pos = 0; pos < (pattern_copies? 1: 2); ++pos) {
            sb.add_bin(bin, pattern_copies? 2: 1);
            sb.add_stack(pos, 0, 100, 0, 20);
            sb.add_item(pos, 0, item, Rotation::XYZ);
        }
        const Solution solution = sb.build();
        EXPECT_TRUE(solution.feasible());
        EXPECT_EQ(solution.number_of_bins(), 2);
        // The first physical bin stays whole; only the last loses a slab.
        EXPECT_EQ(solution.volume(), 1200000);
        EXPECT_EQ(solution.waste(), 800000);
        EXPECT_EQ(solution.full_waste(), 1600000);
    }
}
