#include "packingsolver/irregular/instance_builder.hpp"
#include "packingsolver/irregular/reduction.hpp"
#include "irregular/solution_builder.hpp"

#include <gtest/gtest.h>

#include <set>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/**
 * Builds an axis-aligned 'w'x'h' rectangle shape with its bottom-left
 * corner at the origin - the same manual 'ShapeElement' construction
 * 'Irregular.BinCopies' (in 'irregular_test.cpp') already uses, factored
 * out here since every scenario below needs one (for the bin and/or for
 * item types).
 */
Shape rectangle_shape(LengthDbl w, LengthDbl h)
{
    Shape shape;
    ShapeElement shape_element;
    shape_element.type = ShapeElementType::LineSegment;
    shape_element.start = {0, 0};
    shape_element.end = {w, 0};
    shape.elements.push_back(shape_element);
    shape_element.start = {w, 0};
    shape_element.end = {w, h};
    shape.elements.push_back(shape_element);
    shape_element.start = {w, h};
    shape_element.end = {0, h};
    shape.elements.push_back(shape_element);
    shape_element.start = {0, h};
    shape_element.end = {0, 0};
    shape.elements.push_back(shape_element);
    return shape;
}

/** Same rectangle, wrapped as a single-sub-shape item type footprint. */
std::vector<ItemShape> rectangle_item_shape(LengthDbl w, LengthDbl h)
{
    ItemShape item_shape;
    item_shape.shape_orig.shape = rectangle_shape(w, h);
    return {item_shape};
}

}

TEST(IrregularReduction, MergeIdenticalItems)
{
    // Bin 100x100 (much larger than the items - geometry is irrelevant to
    // 'merge_identical_items', only used here so the instance builds
    // cleanly). Two item types, both 4x4, identical on every property
    // 'items_mergeable' checks: they merge into a single item type with
    // the combined copies.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 3);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_1, 2);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 1);
    EXPECT_EQ(reduced_instance.item_type(0).copies, 5);
    EXPECT_EQ(reduced_instance.item_type(0).copies_min, 5);
}

TEST(IrregularReduction, MergeIdenticalItemsDifferentProfitStillMerges)
{
    // Same as above, but the two item types have different profit - not
    // compared by 'items_mergeable' for 'BinPacking' (profit is never the
    // actual objective here, and 'unreduce_solution' always restores each
    // placed copy's own true original profit regardless of merging), so
    // these two still merge.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 3);
    instance_builder.set_item_type_profit(item_type_id_0, 5);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_1, 2);
    instance_builder.set_item_type_profit(item_type_id_1, 10);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 1);
    EXPECT_EQ(reduced_instance.item_type(0).copies, 5);
}

TEST(IrregularReduction, KnapsackMergeRequiresMatchingProfit)
{
    // Bin 100x100, 'Knapsack' objective. Item types 0 and 1 (both 4x4,
    // profit 5) are identical including profit and merge; item type 2
    // (also 4x4, profit 10) is identical on every *other* property but
    // must stay separate - unlike every other objective this class
    // handles, 'Knapsack' optimizes profit directly, so merging
    // different-profit item types would report one uniform profit for
    // every copy and let the solve choose a suboptimal subset on wrong
    // information.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 1);
    instance_builder.set_item_type_profit(item_type_id_0, 5);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_1, 1);
    instance_builder.set_item_type_profit(item_type_id_1, 5);
    ItemTypeId item_type_id_2 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_2, 1);
    instance_builder.set_item_type_profit(item_type_id_2, 10);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 2);
    EXPECT_EQ(reduced_instance.item_type(0).copies, 2);
    EXPECT_EQ(reduced_instance.item_type(0).profit, 5);
    EXPECT_EQ(reduced_instance.item_type(1).copies, 1);
    EXPECT_EQ(reduced_instance.item_type(1).profit, 10);
}

TEST(IrregularReduction, RemoveNegativeProfitItemsFullyOptional)
{
    // Bin 100x100, 'Knapsack' objective. Item 0 (4x4, profit 10) is worth
    // including; item 1 (4x4, profit -3, fully optional 'copies_min' 0) is
    // never worth choosing on its own merits, so it is removed outright.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 1);
    instance_builder.set_item_type_profit(item_type_id_0, 10);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_1, 1);
    instance_builder.set_item_type_profit(item_type_id_1, -3);
    instance_builder.set_item_type_copies_min(item_type_id_1, 0);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 1);
    EXPECT_EQ(reduced_instance.item_type(0).profit, 10);
}

TEST(IrregularReduction, RemoveNegativeProfitItemsTrimsToMinimum)
{
    // Bin 100x100, 'Knapsack' objective. Item 0 (4x4, profit -3, 5 copies,
    // 'copies_min' 2) has some copies genuinely mandatory - only the 3
    // optional copies beyond 'copies_min' are ever worth dropping, so
    // 'copies' is trimmed to exactly 2, not removed outright. Checked
    // directly against 'Reduction::instance()' rather than through a full
    // 'optimize()' solve: rectangle's own equivalent test documents that a
    // mandatory, negative-profit item type under 'Knapsack' hangs every
    // solver algorithm in this codebase - a pre-existing, 'Reduction'-
    // unrelated defect - so this test deliberately never calls
    // 'optimize()' at all.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 5);
    instance_builder.set_item_type_profit(item_type_id_0, -3);
    instance_builder.set_item_type_copies_min(item_type_id_0, 2);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 1);
    EXPECT_EQ(reduced_instance.item_type(0).copies, 2);
    EXPECT_EQ(reduced_instance.item_type(0).copies_min, 2);
}

TEST(IrregularReduction, MergeIdenticalItemsRequiresMatchingRotations)
{
    // Bin 100x100. Item types 0 and 1 are both 4x4 with the identical
    // default (no-rotation) 'allowed_rotations' and merge; item type 2 is
    // also 4x4 but has an extra allowed rotation range added, so it must
    // not merge with either, despite sharing the exact same shape.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.add_bin_type(rectangle_shape(100, 100));
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 1);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_1, 1);
    ItemTypeId item_type_id_2 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_2, 1);
    instance_builder.add_item_type_allowed_rotation(item_type_id_2, 0, 90, false);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 2);
    EXPECT_EQ(reduced_instance.item_type(0).copies, 2);
    EXPECT_EQ(reduced_instance.item_type(1).copies, 1);
}

TEST(IrregularReduction, MergeIdenticalItemsRespectsResources)
{
    // Bin 100x100 with one resource. Item types 0 and 1 (both 4x4) have
    // the exact same per-copy consumption schedule and merge; item type 2
    // (also 4x4) has a different schedule and must stay separate.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    BinTypeId bin_type_id = instance_builder.add_bin_type(rectangle_shape(100, 100));
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1000.0);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 1);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_0, {1.0});
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_1, 1);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_1, {1.0});
    ItemTypeId item_type_id_2 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_2, 1);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_2, {2.0});
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 2);
    EXPECT_EQ(reduced_instance.item_type(0).copies, 2);
    EXPECT_EQ(reduced_instance.item_type(1).copies, 1);
}

TEST(IrregularReduction, UnreduceSolutionRoundTrip)
{
    // Bin 20x20. Item types 0 and 1 (both 5x10, 1 copy each) are identical
    // and merge into a single reduced item type with 2 copies; placing
    // both reduced copies side by side (built directly via
    // 'SolutionBuilder' on the reduced instance, rather than through a
    // full 'optimize()' solve, to test 'unreduce_solution' in isolation)
    // and unreducing must recover a valid solution using original item
    // type ids 0 and 1, each exactly once - never the same original id
    // placed twice.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.add_bin_type(rectangle_shape(20, 20));
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(5, 10));
    instance_builder.set_item_type_copies(item_type_id_0, 1);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(5, 10));
    instance_builder.set_item_type_copies(item_type_id_1, 1);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 1);
    ASSERT_EQ(reduced_instance.item_type(0).copies, 2);

    SolutionBuilder solution_builder(reduced_instance);
    solution_builder.add_bin(0, 1);
    solution_builder.add_item(0, 0, {0, 0}, 0, false);
    solution_builder.add_item(0, 0, {5, 0}, 0, false);
    Solution reduced_solution = solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    ASSERT_EQ(solution.number_of_different_bins(), 1);
    const SolutionBin& solution_bin = solution.bin(0);
    ASSERT_EQ(solution_bin.items.size(), 2);
    std::set<ItemTypeId> placed_item_type_ids;
    for (const SolutionItem& solution_item: solution_bin.items)
        placed_item_type_ids.insert(solution_item.item_type_id);
    EXPECT_EQ(placed_item_type_ids, (std::set<ItemTypeId>{item_type_id_0, item_type_id_1}));
}

TEST(IrregularReduction, UnreduceSolutionUnequalCopiesProducesCompactGroups)
{
    // Item types 0 (30 copies) and 1 (20 copies), both 4x4, merge into a
    // single reduced item type with 50 copies. A reduced solution placing
    // one reduced copy per bin, 50 bins total (all sharing one
    // 'SolutionBin' entry with 'copies = 50', exactly like a real solve
    // would report a pattern reused across many physical bins) - built
    // directly via 'SolutionBuilder', not through a full 'optimize()'
    // solve, to test 'unreduce_solution' in isolation. Regression test:
    // 'unreduce_solution' must recognize that the physical bins resolving
    // to original item type 0 form one contiguous run (and likewise for
    // type 1), and group them into exactly 2 'add_bin' entries (30 copies
    // then 20, or vice versa) - not one entry per physical bin (50),
    // which is what an earlier, unconditionally-splitting version of this
    // class actually did whenever a merged pattern was reused across more
    // than one physical bin.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.add_bin_type(rectangle_shape(4, 4));
    instance_builder.set_bin_type_copies(0, 50);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_0, 30);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(rectangle_item_shape(4, 4));
    instance_builder.set_item_type_copies(item_type_id_1, 20);
    Instance instance = instance_builder.build();

    Reduction reduction(instance);
    const Instance& reduced_instance = reduction.instance();
    ASSERT_EQ(reduced_instance.number_of_item_types(), 1);
    ASSERT_EQ(reduced_instance.item_type(0).copies, 50);

    SolutionBuilder solution_builder(reduced_instance);
    solution_builder.add_bin(0, 50);
    solution_builder.add_item(0, 0, {0, 0}, 0, false);
    Solution reduced_solution = solution_builder.build();

    Solution solution = reduction.unreduce_solution(reduced_solution);
    EXPECT_EQ(solution.number_of_different_bins(), 2);
    ItemPos copies_0 = 0;
    ItemPos copies_1 = 0;
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = solution.bin(bin_pos);
        ASSERT_EQ(solution_bin.items.size(), 1);
        if (solution_bin.items[0].item_type_id == item_type_id_0)
            copies_0 += solution_bin.copies;
        else if (solution_bin.items[0].item_type_id == item_type_id_1)
            copies_1 += solution_bin.copies;
    }
    EXPECT_EQ(copies_0, 30);
    EXPECT_EQ(copies_1, 20);
}
