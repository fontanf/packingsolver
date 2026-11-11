#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/tree_search_maximal_spaces.hpp"
#include "rectangleguillotine/block.hpp"
#include "packingsolver/rectangleguillotine/reduction.hpp"
#include "rectangleguillotine/solution_builder.hpp"

#include "treesearchsolver/iterative_beam_search.hpp"

#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include <algorithm>
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
    if (test_params.items_path.extension() == ".json") {
        instance_builder.read(test_params.items_path.string());
    } else {
        instance_builder.read_item_types(test_params.items_path.string());
        instance_builder.read_bin_types(test_params.bins_path.string());
        instance_builder.read_parameters(test_params.parameters_path.string());
    }
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
            }, {
                // A resource of capacity 1 consumed by item types 0 and 1:
                // they can't be packed together, item types 0 and 2 are.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_hard_resource" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_hard_resource" / "solution.csv",
            }, {
                // A resource of capacity 2 consumed by the 5 copies of item
                // type 0: the blocks with more than 2 copies of it are not
                // generated, 2 copies are packed with 3 copies of item type 1.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_hard_resource_copies" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_hard_resource_copies" / "solution.csv",
            }, {
                // The same resource, penalized with a penalty of 60: packing
                // item types 0 and 1 together (190 - 60) is worse than
                // packing item types 0 and 2 (150).
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_penalized_resource" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_penalized_resource" / "solution.csv",
            }, {
                // A resource of capacity 1 with the consumption schedule
                // [1, 0] for item types 0 and 1: only their first copy
                // consumes it, so the two copies of item type 0 can be
                // packed together, with item type 2 but not item type 1.
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_resource_schedule" / "instance.json",
                fs::path(""),
                fs::path(""),
                fs::path("data") / "rectangleguillotine" / "tests" / "knapsack_unlimited_stages_resource_schedule" / "solution.csv",
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
    const std::vector<std::vector<Block>>* blocks = computed_blocks(instance);
    ASSERT_NE(blocks, nullptr);
    ASSERT_EQ(blocks->size(), (size_t)instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        const std::vector<Block>& bin_type_blocks = (*blocks)[bin_type_id];
        for (ItemPos block_id = 0; block_id < (ItemPos)bin_type_blocks.size(); ++block_id) {
            const Block& block = bin_type_blocks[block_id];
            EXPECT_LE(block.rect.w, bin_type.rect.w);
            EXPECT_LE(block.rect.h, bin_type.rect.h);
            Profit profit = 0;
            for (const std::pair<ItemTypeId, ItemPos>& item_copy: block.item_copies) {
                EXPECT_LE(item_copy.second, instance.item_type(item_copy.first).copies);
                profit += item_copy.second * instance.item_type(item_copy.first).profit;
            }
            EXPECT_DOUBLE_EQ(block.item_profit, profit);
            EXPECT_EQ(
                    block.resource_consumption,
                    compute_resource_consumption(instance, bin_type_id, block.item_copies));
            EXPECT_TRUE(resource_feasible(instance, bin_type_id, {}, block.resource_consumption));
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
            block_keys((*computed_blocks(sub_instance))[0]),
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
    EXPECT_FALSE((*computed_blocks(sub_instance))[0].empty());
    EXPECT_LT(
            (*computed_blocks(sub_instance))[0].size(),
            (*computed_blocks(instance))[0].size());
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
    EXPECT_FALSE((*computed_blocks(sub_instance))[0].empty());
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
        for (const Block& block: (*computed_blocks(sub_instance))[0]) {
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
            block_keys((*computed_blocks(reduction.instance()))[0]).size(),
            block_keys((*computed_blocks(instance))[0]).size());
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
    EXPECT_EQ(computed_blocks(sub_instance), nullptr);
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, ToSolutionSpacePrunedByResource)
{
    // Item type 0 (10 x 6) is placed first in the bottom-left corner of the
    // bin, with vertical first cuts: the empty spaces are the one to its
    // right (10 x 10) and the one above it (10 x 4). Only item type 1 (4 x 3)
    // fits above it, but a resource which is not penalized forbids packing
    // item types 0 and 1 together: the space above is removed, and item type
    // 2 (8 x 10) is placed to the right, although the space above is
    // smaller.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.set_number_of_stages_unlimited();
    BinTypeId bin_type_id = instance_builder.add_bin_type(20, 10);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(10, 6, true);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(4, 3, true);
    ItemTypeId item_type_id_2 = instance_builder.add_item_type(8, 10, true);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_0, {1.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_1, {1.0});
    Instance instance = instance_builder.build();

    std::vector<std::vector<Block>> blocks = compute_blocks(instance);
    auto simple_block_id = [&blocks, &instance](ItemTypeId item_type_id)
    {
        for (ItemPos block_id = 0; block_id < (ItemPos)blocks[0].size(); ++block_id) {
            const Block& block = blocks[0][block_id];
            if (block.is_simple
                    && block.item_type_id == item_type_id
                    && block.number_of_items == 1) {
                return block_id;
            }
        }
        return (ItemPos)-1;
    };

    BranchingSchemeMaximalSpaces::Parameters parameters;
    parameters.first_cut_orientation = CutOrientation::Vertical;
    BranchingSchemeMaximalSpaces branching_scheme(instance, blocks, parameters);
    auto node = std::make_shared<BranchingSchemeMaximalSpaces::Node>(*branching_scheme.root());

    BranchingSchemeMaximalSpaces::Insertion insertion_0;
    insertion_0.block_id = simple_block_id(item_type_id_0);
    insertion_0.space_id = 0;
    insertion_0.cut_orientation = CutOrientation::Vertical;
    ASSERT_NE(insertion_0.block_id, -1);
    branching_scheme.apply_insertion(*node, insertion_0);
    ASSERT_EQ(node->empty_spaces.size(), (size_t)1);
    EXPECT_EQ(node->empty_spaces[0].bl_corner.x, 10);

    BranchingSchemeMaximalSpaces::Insertion insertion_2;
    insertion_2.block_id = simple_block_id(item_type_id_2);
    insertion_2.space_id = 0;
    insertion_2.cut_orientation = CutOrientation::Vertical;
    ASSERT_NE(insertion_2.block_id, -1);
    branching_scheme.apply_insertion(*node, insertion_2);

    Solution solution = branching_scheme.to_solution(node);
    EXPECT_TRUE(solution.feasible());
    EXPECT_EQ(solution.number_of_items(), 2);
    EXPECT_EQ(solution.item_copies(item_type_id_0), 1);
    EXPECT_EQ(solution.item_copies(item_type_id_2), 1);
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, FlippedFirstCut)
{
    // A bin of the 21-bin solution of martello1998/Class_07.2bp_80_4: a strip
    // of three thin items stacked on top of each other, below two columns of
    // two items each:
    //   - column 1: 50 x 66 and 40 x 23 above it;
    //   - column 2: 47 x 71 and 47 x 10 above it.
    // With the simple blocks only, the items can't all be packed if the
    // orientation of the first cut in each empty space is forced by its
    // depth: the space above the first thin item would be cut vertically
    // first, at the right of the second thin item.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.set_number_of_stages_unlimited();
    instance_builder.add_bin_type(100, 100);
    instance_builder.add_item_type(40, 23, true);
    instance_builder.add_item_type(50, 66, true);
    instance_builder.add_item_type(47, 71, true);
    instance_builder.add_item_type(47, 10, true);
    instance_builder.add_item_type(72, 4, true);
    instance_builder.add_item_type(68, 3, true);
    instance_builder.add_item_type(88, 3, true);
    Instance instance = instance_builder.build();

    std::vector<std::vector<Block>> all_blocks = compute_blocks(instance);
    std::vector<std::vector<Block>> blocks(1);
    for (const Block& block: all_blocks[0])
        if (block.is_simple)
            blocks[0].push_back(block);

    BranchingSchemeMaximalSpaces branching_scheme(instance, blocks, {});
    treesearchsolver::IterativeBeamSearchParameters<BranchingSchemeMaximalSpaces> ibs_parameters;
    ibs_parameters.verbosity_level = 0;
    ibs_parameters.global_history = true;
    ibs_parameters.maximum_size_of_the_queue = 1024;
    auto ibs_output = treesearchsolver::iterative_beam_search<BranchingSchemeMaximalSpaces>(
            branching_scheme,
            ibs_parameters);

    Solution solution = branching_scheme.to_solution(ibs_output.solution_pool.best());
    EXPECT_TRUE(solution.feasible());
    EXPECT_EQ(solution.number_of_items(), instance.number_of_items());
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, CopyDependentResourceConsumption)
{
    // Only the first copy of item type 0 consumes the resource, of capacity
    // 1: once a copy is packed, the block of another copy remains valid.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.set_number_of_stages_unlimited();
    BinTypeId bin_type_id = instance_builder.add_bin_type(15, 10);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_copies(item_type_id_0, 2);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(5, 10, true);
    ResourceId resource_id = instance_builder.add_bin_type_resource(bin_type_id, 1.0);
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_0, {1.0, 0.0});
    instance_builder.add_resource_consumption(bin_type_id, resource_id, item_type_id_1, {1.0, 0.0});
    Instance instance = instance_builder.build();

    std::vector<std::vector<Block>> blocks = compute_blocks(instance);
    ItemPos block_id = -1;
    for (ItemPos candidate_block_id = 0;
            candidate_block_id < (ItemPos)blocks[0].size();
            ++candidate_block_id) {
        const Block& block = blocks[0][candidate_block_id];
        if (block.is_simple
                && block.item_type_id == item_type_id_0
                && block.number_of_items == 1) {
            block_id = candidate_block_id;
        }
    }
    ASSERT_NE(block_id, -1);

    BranchingSchemeMaximalSpaces branching_scheme(instance, blocks, {});
    auto node = std::make_shared<BranchingSchemeMaximalSpaces::Node>(*branching_scheme.root());
    BranchingSchemeMaximalSpaces::Insertion insertion;
    insertion.block_id = block_id;
    insertion.space_id = 0;
    insertion.cut_orientation = CutOrientation::Vertical;
    branching_scheme.apply_insertion(*node, insertion);

    EXPECT_EQ(node->resource_consumption[resource_id], 1.0);
    EXPECT_NE(std::find(node->valid_block_ids.begin(), node->valid_block_ids.end(), block_id),
            node->valid_block_ids.end());
}

TEST(RectangleGuillotineTreeSearchMaximalSpaces, InsertionOrderProfitDensity)
{
    // The tree search only generates the first children of a node: they must
    // be the insertions of greatest profit per unit of area, not of greatest
    // profit. Otherwise, with a queue of size 1, item type 0 is packed alone.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.set_number_of_stages_unlimited();
    instance_builder.add_bin_type(10, 10);
    ItemTypeId item_type_id_0 = instance_builder.add_item_type(10, 10, true);
    instance_builder.set_item_type_profit(item_type_id_0, 30);
    ItemTypeId item_type_id_1 = instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_profit(item_type_id_1, 20);
    ItemTypeId item_type_id_2 = instance_builder.add_item_type(5, 10, true);
    instance_builder.set_item_type_profit(item_type_id_2, 20);
    Instance instance = instance_builder.build();

    std::vector<std::vector<Block>> all_blocks = compute_blocks(instance);
    std::vector<std::vector<Block>> blocks(1);
    for (const Block& block: all_blocks[0])
        if (block.is_simple)
            blocks[0].push_back(block);

    BranchingSchemeMaximalSpaces branching_scheme(instance, blocks, {});
    treesearchsolver::IterativeBeamSearchParameters<BranchingSchemeMaximalSpaces> ibs_parameters;
    ibs_parameters.verbosity_level = 0;
    ibs_parameters.global_history = true;
    ibs_parameters.maximum_size_of_the_queue = 1;
    auto ibs_output = treesearchsolver::iterative_beam_search<BranchingSchemeMaximalSpaces>(
            branching_scheme,
            ibs_parameters);

    Solution solution = branching_scheme.to_solution(ibs_output.solution_pool.best());
    EXPECT_TRUE(solution.feasible());
    EXPECT_EQ(solution.profit(), 40);
}
