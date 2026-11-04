#include "packingsolver/rectangle/instance_builder.hpp"
#include "packingsolver/rectangle/optimize.hpp"
#include "rectangle/solution_builder.hpp"
#include "packingsolver/rectangle/reduction.hpp"
#include "rectangle/block.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <set>

using namespace packingsolver::rectangle;
namespace fs = boost::filesystem;

struct RectangleTreeSearchMaximalSpacesTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path defects_path;
    fs::path parameters_path;
    fs::path certificate_path;

    /**
     * Path to a JSON instance file; if non-empty, used instead of
     * 'items_path'/'bins_path'/'defects_path'/'parameters_path' (needed for
     * features with no CSV representation, e.g. resources).
     */
    fs::path instance_path;
};

inline std::ostream& operator<<(std::ostream& os, const RectangleTreeSearchMaximalSpacesTestParams& test_params)
{
    os << (!test_params.instance_path.empty()? test_params.instance_path: test_params.items_path);
    return os;
}

class RectangleTreeSearchMaximalSpacesTest: public testing::TestWithParam<RectangleTreeSearchMaximalSpacesTestParams> { };

TEST_P(RectangleTreeSearchMaximalSpacesTest, RectangleTreeSearchMaximalSpaces)
{
    RectangleTreeSearchMaximalSpacesTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    if (!test_params.instance_path.empty()) {
        instance_builder.read(test_params.instance_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_defects(test_params.defects_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
    Instance instance = instance_builder.build();

    OptimizeParameters optimize_parameters;
    optimize_parameters.optimization_mode = packingsolver::OptimizationMode::NotAnytimeSequential;
    optimize_parameters.use_tree_search_maximal_spaces = true;
    Output output = optimize(instance, optimize_parameters);

    SolutionBuilder solution_builder(instance);
    solution_builder.read(test_params.certificate_path.string());
    Solution solution = solution_builder.build();
    std::cout << std::endl
        << "Reference solution" << std::endl
        << "------------------" << std::endl;
    solution.format(std::cout);

    EXPECT_EQ(!(output.solution_pool.best() < solution), true);
    EXPECT_EQ(!(solution < output.solution_pool.best()), true);
}

INSTANTIATE_TEST_SUITE_P(
        Rectangle,
        RectangleTreeSearchMaximalSpacesTest,
        testing::ValuesIn(std::vector<RectangleTreeSearchMaximalSpacesTestParams>{
            {
                // Bin 20x10, a defect covering the 5-wide middle strip
                // (x in [5,10)), a 10x10 item with 2 copies (a single copy
                // would make the bin's tracked width collapse to 10 via the
                // "max reachable length" domain reduction - since one copy
                // could never reach past x=10 anyway - which would place
                // the defect at the truncated bin's far edge instead of its
                // middle, defeating the point of this test). Cutting the
                // initial empty space around the defect leaves a 5x10 space
                // (too narrow for the item) and a 10x10 space (an exact
                // fit) - and the 5x10 one, being closer to the bin's
                // origin, is exactly the kind of unusable-but-preferred
                // space 'remove_unusable_spaces' must discard, otherwise
                // the search would wrongly consider the root infertile
                // despite the still-usable 10x10 space (and, without the
                // defect cut being applied at all, would instead silently
                // pack both copies, one of them overlapping the defect).
                // Only 1 of the 2 copies fits.
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "bins.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "defects.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_defect_blocks_middle" / "solution.csv",
            }, {
                // Bin 12x4, three copies of a 4x4 item tile it exactly side
                // by side (3 * 4 = 12). A resource with capacity 2 and a
                // uniform consumption of 1 per copy caps the item type at 2
                // copies: 'tree_search_maximal_spaces' must reject the third
                // copy instead of silently ignoring the resource.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_blocks_insertion" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_capacity_blocks_insertion" / "instance.json",
            }, {
                // Same layout as the previous case, but the resource is
                // 'penalize': it must not block the third copy from being
                // packed, and must subtract 'penalty' from the profit
                // exactly once (the first time consumption crosses
                // capacity), not once per copy over capacity - so the
                // reference solution packs all 3 items, and its profit
                // (3 * 10 - 5 = 25, via 'Solution::update_indicators')
                // matches what the algorithm reports.
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_penalty_reduces_profit" / "solution.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_resource_penalty_reduces_profit" / "instance.json",
            }, {
                // Bin 8x4, maximum_weight 5: two copies of a 4x4, weight-3
                // item tile it exactly side by side (2 * 4 = 8, geometrically
                // fine), but weigh 6 together, over the bin's own capacity.
                // Regression test for a bug where moving the weight check
                // from 'insertions'/'best_insertion' into 'apply_insertion's
                // 'valid_block_ids' pruning (both only ever accumulate, so
                // the prune is permanent once a block is ruled out) missed
                // that the *root* node never goes through 'apply_insertion'
                // at all - a block whose own weight already exceeds capacity
                // needs to be excluded at compute_blocks() time too (see
                // 'Block::weight''s own doc comment), or it would wrongly
                // stay offered as the very first insertion.
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "items.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "bins.csv",
                fs::path(),
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "parameters.csv",
                fs::path("data") / "rectangle" / "tests" / "knapsack_weight_blocks_insertion" / "solution.csv",
            }}));

namespace
{

/** Instance with 5 item types and a single bin type, with its blocks. */
Instance build_instance_with_blocks()
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.add_bin_type(20, 20);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(3, 4), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(5, 2), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(6, 7), 2);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(2, 9), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(8, 3), 2);
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

using BlockKey = std::pair<std::pair<packingsolver::Length, packingsolver::Length>, std::vector<std::pair<packingsolver::ItemTypeId, packingsolver::ItemPos>>>;

/** Set of the dimensions and items of the blocks. */
std::set<BlockKey> block_keys(
        const std::vector<Block>& blocks)
{
    std::set<BlockKey> keys;
    for (const Block& block: blocks) {
        std::vector<std::pair<packingsolver::ItemTypeId, packingsolver::ItemPos>> item_copies = block.item_copies;
        std::sort(item_copies.begin(), item_copies.end());
        keys.insert({{block.rect.x, block.rect.y}, item_copies});
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
            EXPECT_LE(block.rect.x, bin_type.rect.x);
            EXPECT_LE(block.rect.y, bin_type.rect.y);
            EXPECT_LE(block.weight, bin_type.maximum_weight);
            EXPECT_EQ(block.resource_consumption.size(), (size_t)bin_type.number_of_resources());
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

TEST(RectangleTreeSearchMaximalSpaces, InstanceCacheBlocksSubInstance)
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

TEST(RectangleTreeSearchMaximalSpaces, InstanceCacheBlocksProfitsAndCopies)
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

TEST(RectangleTreeSearchMaximalSpaces, InstanceCacheBlocksNewItemType)
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
    packingsolver::ItemTypeId new_item_type_id = sub_instance_builder.add_item_type(4, 4);
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

TEST(RectangleTreeSearchMaximalSpaces, InstanceCacheBlocksResource)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with a resource of capacity 1 consumed by each copy of
    // item type 0: the blocks with several copies of item type 0 are
    // removed, and the consumptions of the blocks are computed for the
    // resource.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(packingsolver::Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    packingsolver::BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(0);
    packingsolver::ResourceId resource_id = sub_instance_builder.add_bin_type_resource(sub_bin_type_id, 1.0);
    for (packingsolver::ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        sub_instance_builder.add_item_type_from_original_instance(item_type_id);
    }
    sub_instance_builder.add_resource_consumption(sub_bin_type_id, resource_id, 0, {1.0});
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);
    for (const Block& block: (*computed_blocks(sub_instance))[0]) {
        packingsolver::ItemPos copies_0 = 0;
        for (const auto& item_copy: block.item_copies)
            if (item_copy.first == 0)
                copies_0 = item_copy.second;
        EXPECT_LE(copies_0, 1);
        EXPECT_DOUBLE_EQ(block.resource_consumption[resource_id], (double)copies_0);
    }
}

TEST(RectangleTreeSearchMaximalSpaces, InstanceCacheBlocksReduction)
{
    // The reduction keeps the blocks of the item types it doesn't change.
    Instance instance = build_instance_with_blocks();
    Reduction reduction(instance);
    check_cached_blocks(reduction.instance());
}
