#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/tree_search_maximal_spaces.hpp"
#include "rectangleguillotine/block.hpp"
#include "packingsolver/rectangleguillotine/reduction.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <set>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;
namespace fs = boost::filesystem;

struct RectangleGuillotineTreeSearchMaximalSpacesTestParams
{
    fs::path items_path;
    fs::path bins_path;
    fs::path parameters_path;
    fs::path certificate_path;
};

inline std::ostream& operator<<(
        std::ostream& os,
        const RectangleGuillotineTreeSearchMaximalSpacesTestParams& test_params)
{
    os << test_params.items_path;
    return os;
}

class RectangleGuillotineTreeSearchMaximalSpacesTest:
    public testing::TestWithParam<RectangleGuillotineTreeSearchMaximalSpacesTestParams> { };

TEST_P(
        RectangleGuillotineTreeSearchMaximalSpacesTest,
        RectangleGuillotineTreeSearchMaximalSpaces)
{
    RectangleGuillotineTreeSearchMaximalSpacesTestParams test_params = GetParam();
    InstanceBuilder instance_builder;
    instance_builder.read_item_types(test_params.items_path.string());
    instance_builder.read_bin_types(test_params.bins_path.string());
    instance_builder.read_parameters(test_params.parameters_path.string());
    Instance instance = instance_builder.build();

    TreeSearchMaximalSpacesParameters tree_search_maximal_spaces_parameters;
    tree_search_maximal_spaces_parameters.optimization_mode = OptimizationMode::NotAnytimeSequential;
    TreeSearchMaximalSpacesOutput output = tree_search_maximal_spaces(instance, tree_search_maximal_spaces_parameters);

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
        RectangleGuillotineTreeSearchMaximalSpaces,
        RectangleGuillotineTreeSearchMaximalSpacesTest,
        testing::ValuesIn(std::vector<RectangleGuillotineTreeSearchMaximalSpacesTestParams>{
            // Unlimited stages, rotation allowed, cut thickness 4.
            // Items 1 (610x140) and 0 rotated (2050x136) are placed side by
            // side, so item 0's space is taller than the item by exactly the
            // cut thickness. A trim cut must still separate the item from the
            // gap (regression test for issue #660).
            {
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_ur_cut_thickness_small_gap" / "solution.csv",
            }, {
                // Soft trims of 5 and cut thickness 2: the four items only fit
                // inside the trims, starting after the waste of the left and
                // bottom trims and the cut separating it.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_soft_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_soft_trims" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_soft_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_soft_trims" / "solution.csv",
            }, {
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "items.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "bins.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "parameters.csv",
                fs::path("data") / "rectangleguillotine" / "tests" / "bin_packing_unlimited_stages_hard_trims" / "solution.csv",
            }}));

namespace
{

/** Instance with 5 item types and a single bin type, with its blocks. */
Instance build_instance_with_blocks()
{
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::BinPacking);
    instance_builder.set_number_of_stages_unlimited();
    instance_builder.set_cut_type(CutType::NonExact);
    instance_builder.add_bin_type(20, 20);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(3, 4), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(5, 2), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(6, 7), 2);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(2, 9), 3);
    instance_builder.set_item_type_copies(instance_builder.add_item_type(8, 3), 2);
    Instance instance = instance_builder.build();
    instance.compute_blocks_cache();
    return instance;
}

using BlockKey = std::pair<std::pair<Length, Length>, std::vector<std::pair<ItemTypeId, ItemPos>>>;

/** Set of the dimensions and items of the blocks. */
std::set<BlockKey> block_keys(
        const std::vector<Block>& blocks)
{
    std::set<BlockKey> keys;
    for (const Block& block: blocks) {
        std::vector<std::pair<ItemTypeId, ItemPos>> item_copies = block.item_copies;
        std::sort(item_copies.begin(), item_copies.end());
        keys.insert({{block.rect.w, block.rect.h}, item_copies});
    }
    return keys;
}

/**
 * Check that the blocks of the cache of 'instance' are consistent with its
 * item types: profits, numbers of copies, dimensions and children.
 */
void check_cached_blocks(
        const Instance& instance)
{
    ASSERT_TRUE(instance.cache().blocks_computed);
    ASSERT_EQ(instance.cache().blocks.size(), (size_t)instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        const std::vector<Block>& blocks = instance.cache().blocks[bin_type_id];
        for (ItemPos block_id = 0; block_id < (ItemPos)blocks.size(); ++block_id) {
            const Block& block = blocks[block_id];
            EXPECT_LE(block.rect.w, bin_type.rect.w);
            EXPECT_LE(block.rect.h, bin_type.rect.h);
            Profit profit = 0;
            for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies) {
                EXPECT_LE(item_copy.second, instance.item_type(item_copy.first).copies);
                profit += item_copy.second * instance.item_type(item_copy.first).profit;
            }
            EXPECT_DOUBLE_EQ(block.item_profit, profit);
            if (block.is_simple) {
                const ItemType& item_type = instance.item_type(block.item_type_id);
                EXPECT_EQ(block.rect.w % item_type.width(block.rotate), 0);
                EXPECT_EQ(block.rect.h % item_type.height(block.rotate), 0);
            } else {
                EXPECT_LT(block.child_1_id, block_id);
                EXPECT_LT(block.child_2_id, block_id);
            }
        }
    }
}

}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, InstanceCacheBlocksSubInstance)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with the same item types in reverse order: the blocks are
    // the ones 'compute_blocks' would compute.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    for (ItemTypeId item_type_id = instance.number_of_item_types() - 1;
            item_type_id >= 0;
            --item_type_id) {
        sub_instance_builder.add_item_type_from_original_instance(item_type_id);
    }
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);
    EXPECT_EQ(
            block_keys(sub_instance.cache().blocks[0]),
            block_keys(compute_blocks(sub_instance)[0]));
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, InstanceCacheBlocksProfitsAndCopies)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with other profits, a single copy of each item type and
    // without the last item type.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types() - 1;
            ++item_type_id) {
        ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type_from_original_instance(item_type_id);
        sub_instance_builder.set_item_type_profit(sub_item_type_id, item_type_id + 1);
        sub_instance_builder.set_item_type_copies(sub_item_type_id, 1);
    }
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);
    EXPECT_FALSE(sub_instance.cache().blocks[0].empty());
    EXPECT_LT(
            sub_instance.cache().blocks[0].size(),
            instance.cache().blocks[0].size());
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, InstanceCacheBlocksCroppedBin)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with a cropped bin: only the blocks which fit in it are
    // kept.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    BinTypeId sub_bin_type_id = sub_instance_builder.add_bin_type_from_original_instance(0);
    sub_instance_builder.crop_bin_type(sub_bin_type_id, 10, 8);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        sub_instance_builder.add_item_type_from_original_instance(item_type_id);
    }
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);
    EXPECT_FALSE(sub_instance.cache().blocks[0].empty());
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, InstanceCacheBlocksNewItemTypes)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with two item types which don't come from the original
    // instance: their simple blocks are added to the blocks of the other
    // item types.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    std::vector<bool> new_item_type;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        if (item_type_id == 2) {
            ItemTypeId sub_item_type_id = sub_instance_builder.add_item_type(7, 7, false);
            sub_instance_builder.set_item_type_stack_id(sub_item_type_id, 2);
            sub_instance_builder.set_item_type_copies(sub_item_type_id, 2);
        } else {
            sub_instance_builder.add_item_type_from_original_instance(item_type_id);
        }
        new_item_type.push_back(item_type_id == 2);
    }
    ItemTypeId foreign_item_type_id = sub_instance_builder.add_item_type(4, 4, false);
    sub_instance_builder.set_item_type_stack_id(foreign_item_type_id, 5);
    sub_instance_builder.set_item_type_copies(foreign_item_type_id, 2);
    new_item_type.push_back(true);
    Instance sub_instance = sub_instance_builder.build();
    check_cached_blocks(sub_instance);

    // The new item types are only in simple blocks, and all their simple
    // blocks are there.
    for (ItemTypeId item_type_id = 0;
            item_type_id < sub_instance.number_of_item_types();
            ++item_type_id) {
        ItemPos number_of_simple_blocks = 0;
        ItemPos number_of_combined_blocks = 0;
        for (const Block& block: sub_instance.cache().blocks[0]) {
            for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies) {
                if (item_copy.first != item_type_id)
                    continue;
                if (block.is_simple) {
                    number_of_simple_blocks++;
                } else {
                    number_of_combined_blocks++;
                }
            }
        }
        if (new_item_type[item_type_id]) {
            EXPECT_EQ(number_of_combined_blocks, 0);
            EXPECT_EQ(
                    number_of_simple_blocks,
                    (ItemPos)compute_simple_blocks(sub_instance, 0, item_type_id).size());
        } else {
            EXPECT_GT(number_of_combined_blocks, 0);
        }
    }
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, InstanceCacheBlocksReduction)
{
    // The reduction keeps the blocks of the item types it doesn't change.
    Instance instance = build_instance_with_blocks();
    Reduction reduction(instance);
    check_cached_blocks(reduction.instance());
    EXPECT_EQ(
            block_keys(reduction.instance().cache().blocks[0]).size(),
            block_keys(instance.cache().blocks[0]).size());
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, InstanceCacheBlocksDropped)
{
    Instance instance = build_instance_with_blocks();

    // Sub-instance with another cut thickness: the blocks are not carried
    // over.
    InstanceBuilder sub_instance_builder;
    sub_instance_builder.set_objective(Objective::Knapsack);
    sub_instance_builder.set_parameters(instance.parameters());
    sub_instance_builder.set_cut_thickness(1);
    sub_instance_builder.set_original_instance(instance);
    sub_instance_builder.add_bin_type_from_original_instance(0);
    sub_instance_builder.add_item_type_from_original_instance(0);
    Instance sub_instance = sub_instance_builder.build();
    EXPECT_FALSE(sub_instance.cache().blocks_computed);
}
