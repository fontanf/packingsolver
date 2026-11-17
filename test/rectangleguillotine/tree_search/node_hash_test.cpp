#include "packingsolver/rectangleguillotine/instance_builder.hpp"
#include "rectangleguillotine/tree_search.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

namespace
{

/**
 * Every node reachable from the root within 'depth' insertions, in the order
 * 'children' produces them (several nodes may share the same 'pos_stack' when
 * the same items are placed in another order).
 */
std::vector<std::shared_ptr<BranchingScheme::Node>> expand(
        const BranchingScheme& branching_scheme,
        int depth)
{
    std::vector<std::shared_ptr<BranchingScheme::Node>> nodes = {branching_scheme.root()};
    std::vector<std::shared_ptr<BranchingScheme::Node>> level = nodes;
    for (int d = 0; d < depth; ++d) {
        std::vector<std::shared_ptr<BranchingScheme::Node>> next;
        for (const auto& node: level)
            for (const auto& child: branching_scheme.children(node))
                next.push_back(child);
        nodes.insert(nodes.end(), next.begin(), next.end());
        level = next;
    }
    return nodes;
}

}

TEST(RectangleGuillotineBranchingScheme, NodeHashFollowsPosStack)
{
    // Three item types with two copies each: the same items can be placed in
    // several orders, so different paths reach the same 'pos_stack'.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(Objective::Knapsack);
    instance_builder.set_number_of_stages(3);
    instance_builder.set_cut_type(CutType::NonExact);
    for (const auto& size: std::vector<std::pair<Length, Length>>{{300, 200}, {250, 400}, {500, 150}}) {
        ItemTypeId item_type_id = instance_builder.add_item_type(size.first, size.second);
        instance_builder.set_item_type_copies(item_type_id, 2);
    }
    instance_builder.add_bin_type(2000, 1000);
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance);
    auto nodes = expand(branching_scheme, 3);
    auto hasher = branching_scheme.node_hasher();

    Counter number_of_same_pos_stack_pairs = 0;
    Counter number_of_different_pos_stack_pairs = 0;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (std::size_t j = i + 1; j < nodes.size(); ++j) {
            bool same_pos_stack = (nodes[i]->pos_stack == nodes[j]->pos_stack);
            bool same_hash = (hasher(nodes[i]) == hasher(nodes[j]));
            // The history map needs equal nodes to have equal hashes...
            if (same_pos_stack) {
                ++number_of_same_pos_stack_pairs;
                EXPECT_TRUE(same_hash) << "nodes " << i << " and " << j;
            } else {
                // ... and buckets are useful only if different nodes
                // (almost always) get different hashes. With 64-bit keys
                // from a fixed seed, a collision here is a defect.
                ++number_of_different_pos_stack_pairs;
                EXPECT_FALSE(same_hash) << "nodes " << i << " and " << j;
            }
        }
    }

    // Guard against a vacuous pass: the expansion must reach the same
    // 'pos_stack' by different paths, and also reach different ones.
    EXPECT_GT(number_of_same_pos_stack_pairs, 0);
    EXPECT_GT(number_of_different_pos_stack_pairs, 0);
}
