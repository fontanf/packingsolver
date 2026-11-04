#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/reduction.hpp"
#include "box/block.hpp"

#include <gtest/gtest.h>

#include <set>

using namespace packingsolver::box;

namespace
{

/** Instance with 5 item types and a single bin type, with its blocks. */
Instance build_instance_with_blocks()
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.add_bin_type(10, 10, 10);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(3, 4, 5), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(5, 2, 2), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(6, 7, 3), 2);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(2, 9, 4), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(8, 3, 2), 2);
    Instance instance = instance_builder.build();
    bool end = false;
    instance.get_blocks(end);
    return instance;
}

/**
 * Blocks of 'instance' if they have already been computed (or copied from
 * an original instance), 'nullptr' otherwise: with 'end' already true,
 * 'Instance::get_blocks' doesn't compute them.
 */
const std::vector<std::vector<Block>>* computed_blocks(
        const Instance& instance)
{
    bool end = true;
    return instance.get_blocks(end);
}

using BlockKey = std::pair<std::vector<packingsolver::Length>, std::vector<std::pair<packingsolver::ItemTypeId, packingsolver::ItemPos>>>;

/** Set of the dimensions and items of the blocks. */
std::set<BlockKey> block_keys(
        const std::vector<Block>& blocks)
{
    std::set<BlockKey> keys;
    for (const Block& block: blocks) {
        std::vector<std::pair<packingsolver::ItemTypeId, packingsolver::ItemPos>> item_copies = block.item_copies;
        std::sort(item_copies.begin(), item_copies.end());
        keys.insert({{block.box.x, block.box.y, block.box.z}, item_copies});
    }
    return keys;
}

/**
 * Check that the blocks of the cache of 'instance' are consistent with its
 * item types and bin types.
 */
void check_cached_blocks(
        const Instance& instance)
{
    const std::vector<std::vector<Block>>* blocks = computed_blocks(instance);
    ASSERT_NE(blocks, nullptr);
    ASSERT_EQ(blocks->size(), (size_t)instance.number_of_bin_types());
    for (packingsolver::BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        for (const Block& block: (*blocks)[bin_type_id]) {
            EXPECT_LE(block.box.x, bin_type.box.x);
            EXPECT_LE(block.box.y, bin_type.box.y);
            EXPECT_LE(block.box.z, bin_type.box.z);
            packingsolver::Profit profit = 0;
            for (const auto& item_copy: block.item_copies) {
                EXPECT_LE(item_copy.second, instance.item_type(item_copy.first).copies);
                profit += item_copy.second * instance.item_type(item_copy.first).profit;
            }
            EXPECT_DOUBLE_EQ(block.item_profit, profit);
            EXPECT_EQ((packingsolver::ItemPos)block.items.size(), block.number_of_items);
            for (const SolutionItem& item: block.items) {
                bool found = false;
                for (const auto& item_copy: block.item_copies)
                    if (item_copy.first == item.item_type_id)
                        found = true;
                EXPECT_TRUE(found);
            }
        }
    }
}

}

TEST(BoxTreeSearchMaximalSpaces, InstanceCacheBlocksSubInstance)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with the same item types in reverse order: the blocks are
    // the ones 'compute_blocks' would compute.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    for (packingsolver::ItemTypeId item_type_id = instance.number_of_item_types() - 1;
            item_type_id >= 0;
            --item_type_id) {
        sub_instance_builder.add_item_type_from_original_instance(item_type_id);
    }
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);
    EXPECT_EQ(
            block_keys((*computed_blocks(sub_instance))[0]),
            block_keys(compute_blocks(sub_instance)[0]));
}

TEST(BoxTreeSearchMaximalSpaces, InstanceCacheBlocksProfitsAndCopies)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with other profits, a single copy of each item type and
    // without the last item type.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    for (packingsolver::ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types() - 1;
            ++item_type_id) {
        packingsolver::ItemTypeId sub_item_type_id
            = sub_instance_builder.add_item_type_from_original_instance(item_type_id);
        sub_instance_builder.set_item_type_profit(sub_item_type_id, item_type_id + 1);
        sub_instance_builder.set_item_type_copies(sub_item_type_id, 1);
    }
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);
    EXPECT_FALSE((*computed_blocks(sub_instance))[0].empty());
    EXPECT_LT(
            (*computed_blocks(sub_instance))[0].size(),
            (*computed_blocks(instance))[0].size());
}

TEST(BoxTreeSearchMaximalSpaces, InstanceCacheBlocksNewItemType)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with an item type which doesn't come from the original
    // instance: only its simple blocks are added to the blocks of the other
    // item types.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    for (packingsolver::ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        sub_instance_builder.add_item_type_from_original_instance(item_type_id);
    }
    packingsolver::ItemTypeId new_item_type_id = sub_instance_builder.add_item_type(4, 4, 4);
    sub_instance_builder.set_item_type_copies(new_item_type_id, 2);
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);
    packingsolver::ItemPos number_of_new_item_type_blocks = 0;
    for (const Block& block: (*computed_blocks(sub_instance))[0]) {
        for (const auto& item_copy: block.item_copies) {
            if (item_copy.first != new_item_type_id)
                continue;
            EXPECT_TRUE(block.is_simple);
            number_of_new_item_type_blocks++;
        }
    }
    EXPECT_EQ(
            number_of_new_item_type_blocks,
            (packingsolver::ItemPos)compute_simple_blocks(sub_instance, 0, new_item_type_id).size());
}

TEST(BoxTreeSearchMaximalSpaces, InstanceCacheBlocksReduction)
{
    // The reduction keeps the blocks of the item types it doesn't change.
    Instance instance = build_instance_with_blocks();
    Reduction reduction(instance);
    check_cached_blocks(reduction.instance());
}
