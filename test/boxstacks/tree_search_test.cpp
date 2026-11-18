#include "packingsolver/boxstacks/instance_builder.hpp"
#include "boxstacks/tree_search.hpp"

#include "../node_hash_test.hpp"

#include <gtest/gtest.h>

using namespace packingsolver;
using namespace packingsolver::boxstacks;

TEST(BoxstacksBranchingScheme, NodeHashFollowsItemNumberOfCopies)
{
    // Three item types with two copies each: the same items can be packed
    // in several orders, so different paths reach the same numbers of
    // copies.
    InstanceBuilder instance_builder;
    instance_builder.set_objective(packingsolver::Objective::Knapsack);
    instance_builder.add_bin_type(100, 100, 100);
    // Item types with the same stackability id must have the same x and y
    // dimensions.
    StackabilityId stackability_id = 0;
    for (const std::vector<Length>& box: std::vector<std::vector<Length>>{{10, 20, 30}, {20, 20, 20}, {30, 10, 10}}) {
        ItemTypeId item_type_id = instance_builder.add_item_type(box[0], box[1], box[2]);
        instance_builder.set_item_type_copies(item_type_id, 2);
        instance_builder.set_item_type_stackability_id(item_type_id, stackability_id++);
    }
    Instance instance = instance_builder.build();

    BranchingScheme branching_scheme(instance, BranchingScheme::Parameters());
    check_item_copies_hashes(
            branching_scheme.root(),
            branching_scheme.node_hasher(),
            [&branching_scheme](const std::shared_ptr<BranchingScheme::Node>& node)
            {
                return branching_scheme.children(node);
            },
            3);
}
