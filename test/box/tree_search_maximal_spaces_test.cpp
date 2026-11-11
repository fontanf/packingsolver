#include "packingsolver/box/instance_builder.hpp"
#include "packingsolver/box/reduction.hpp"
#include "box/block.hpp"
#include "box/tree_search_maximal_spaces.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

using namespace packingsolver;
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

TEST(BoxTreeSearchMaximalSpaces, PenalizedResource)
{
    // Only two items fit in the bin. Packing item types 0 and 1 together
    // exceeds the capacity of the resource, of penalty 15: 20 - 15 = 5, less
    // than item types 0 and 2: 14.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 10);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(5, 10, 10);
    instance_builder.set_item_type_profit(item_type_id_0, 10);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(5, 10, 10);
    instance_builder.set_item_type_profit(item_type_id_1, 10);
    ItemTypeId item_type_id_2 = instance_builder.add_item_type(5, 10, 10);
    instance_builder.set_item_type_profit(item_type_id_2, 4);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0, true, 15.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_0, {1.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_1, {1.0});
    Instance instance = instance_builder.build();

    TreeSearchMaximalSpacesParameters parameters;
    parameters.verbosity_level = 0;
    parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    TreeSearchMaximalSpacesOutput output = tree_search_maximal_spaces(instance, parameters);
    EXPECT_TRUE(output.solution_pool.best().feasible());
    EXPECT_EQ(output.solution_pool.best().profit(), 14);
}

TEST(BoxTreeSearchMaximalSpaces, HardResourceBlocks)
{
    // A resource which is not penalized forbids packing item types 0 and 1
    // together: no block contains both.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(10, 10, 10);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(5, 10, 10);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(5, 10, 10);
    instance_builder.add_item_type(5, 10, 10);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_0, {1.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_1, {1.0});
    Instance instance = instance_builder.build();

    std::vector<std::vector<Block>> blocks = compute_blocks(instance);
    for (const Block& block: blocks[0]) {
        bool contains_item_type_0 = false;
        bool contains_item_type_1 = false;
        for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies) {
            if (item_copy.first == item_type_id_0)
                contains_item_type_0 = true;
            if (item_copy.first == item_type_id_1)
                contains_item_type_1 = true;
        }
        EXPECT_FALSE(contains_item_type_0 && contains_item_type_1);
    }
}

TEST(BoxTreeSearchMaximalSpaces, CopyDependentResourceConsumption)
{
    // Only the first copy of item type 0 consumes the resource, of capacity
    // 1: once a copy is packed, the block of another copy remains valid.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    BinTypeId bin_type_id = instance_builder.add_bin_type(15, 10, 10);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(5, 10, 10);
    instance_builder.set_item_type_copies(item_type_id_0, 2);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_0, {1.0, 0.0});
    Instance instance = instance_builder.build();

    std::vector<std::vector<Block>> blocks = compute_blocks(instance);
    ItemPos block_id = -1;
    for (ItemPos candidate_block_id = 0;
            candidate_block_id < (ItemPos)blocks[0].size();
            ++candidate_block_id) {
        if (blocks[0][candidate_block_id].number_of_items == 1)
            block_id = candidate_block_id;
    }
    ASSERT_NE(block_id, -1);

    MaxReachableLengths max_reachable_lengths = compute_max_reachable_lengths(instance);
    BranchingSchemeMaximalSpaces branching_scheme(instance, blocks, max_reachable_lengths, {});
    auto node = std::make_shared<BranchingSchemeMaximalSpaces::Node>(*branching_scheme.root());
    bool inserted = false;
    for (const BranchingSchemeMaximalSpaces::Insertion& insertion: branching_scheme.insertions(node)) {
        if (insertion.block_id == block_id) {
            branching_scheme.apply_insertion(*node, insertion);
            inserted = true;
            break;
        }
    }
    ASSERT_TRUE(inserted);
    EXPECT_EQ(node->resource_consumption[resource_id], 1.0);
    EXPECT_NE(std::find(node->valid_block_ids.begin(), node->valid_block_ids.end(), block_id),
            node->valid_block_ids.end());
}
