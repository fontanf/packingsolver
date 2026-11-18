#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <vector>

namespace packingsolver
{

/**
 * Check the hash of the numbers of copies of the item types of the nodes of
 * a branching scheme, maintained incrementally (see 'ItemCopiesHashKeys').
 *
 * The nodes reachable from 'root' within 'depth' insertions are expanded
 * with 'children', which returns the children of a node; at most
 * 'maximum_number_of_nodes_per_level' nodes of each level are expanded.
 *
 * The history of the beam searches needs the nodes with the same numbers of
 * copies to have the same hash, and its buckets are useful only if the nodes
 * with different numbers of copies (almost always) have different hashes:
 * with 64-bit keys from a fixed seed, a collision is a defect. The nodes must
 * reach the same numbers of copies by different paths, and different numbers
 * of copies, so that both properties are checked.
 */
template <typename Node, typename Hasher, typename Children>
void check_item_copies_hashes(
        const std::shared_ptr<Node>& root,
        const Hasher& hasher,
        const Children& children,
        int depth,
        std::size_t maximum_number_of_nodes_per_level = 1000)
{
    std::map<std::vector<ItemPos>, std::size_t> hash_of_copies;
    std::map<std::size_t, std::vector<ItemPos>> copies_of_hash;
    Counter number_of_nodes_with_already_seen_copies = 0;
    auto check = [&](const std::shared_ptr<Node>& node)
    {
        std::size_t hash = hasher(node);
        auto copies_it = hash_of_copies.find(node->item_number_of_copies);
        if (copies_it != hash_of_copies.end()) {
            ++number_of_nodes_with_already_seen_copies;
            EXPECT_EQ(copies_it->second, hash);
            return;
        }
        hash_of_copies[node->item_number_of_copies] = hash;
        auto hash_it = copies_of_hash.find(hash);
        EXPECT_TRUE(hash_it == copies_of_hash.end())
            << "two different numbers of copies have the same hash";
        copies_of_hash[hash] = node->item_number_of_copies;
    };

    std::vector<std::shared_ptr<Node>> level = {root};
    check(root);
    for (int d = 0; d < depth; ++d) {
        std::vector<std::shared_ptr<Node>> next;
        for (const std::shared_ptr<Node>& node: level) {
            for (const std::shared_ptr<Node>& child: children(node)) {
                check(child);
                if (next.size() < maximum_number_of_nodes_per_level)
                    next.push_back(child);
            }
        }
        level = next;
    }

    EXPECT_GT(number_of_nodes_with_already_seen_copies, 0);
    EXPECT_GT(hash_of_copies.size(), 1);
}

/**
 * Children of a node of a branching scheme which generates them one at a
 * time with 'next_child'.
 */
template <typename BranchingScheme>
std::vector<std::shared_ptr<typename BranchingScheme::Node>> next_children(
        const BranchingScheme& branching_scheme,
        const std::shared_ptr<typename BranchingScheme::Node>& node)
{
    std::vector<std::shared_ptr<typename BranchingScheme::Node>> children;
    for (;;) {
        auto child = branching_scheme.next_child(node);
        if (child == nullptr)
            break;
        children.push_back(child);
    }
    return children;
}

}
