#pragma once

#include "packingsolver/algorithms/common.hpp"

#include <random>
#include <vector>

namespace packingsolver
{

/**
 * Keys of the incremental hash of the numbers of copies of the item types
 * packed in the nodes of a tree search.
 *
 * The beam searches look the nodes up in their history by these numbers of
 * copies. Instead of hashing the whole vector at each lookup, a node keeps
 * its hash, the sum over the item types of their numbers of copies times
 * their keys (modulo 2^64), and packing 'c' copies of an item type adds 'c'
 * times its key. Unlike a Zobrist hash with a key per item type and number of
 * copies, it needs a single key per item type, whatever the numbers of
 * copies.
 *
 * The keys come from a fixed seed, so that the search is reproducible. The
 * history compares the vectors themselves, so a collision only puts two
 * nodes in the same bucket.
 */
class ItemCopiesHashKeys
{

public:

    /** Constructor. */
    explicit ItemCopiesHashKeys(ItemTypeId number_of_item_types = 0):
        keys_(number_of_item_types)
    {
        std::mt19937_64 generator(0);
        for (std::size_t& key: keys_)
            key = static_cast<std::size_t>(generator());
    }

    /** Get the hash increment of 'copies' copies of an item type. */
    inline std::size_t increment(
            ItemTypeId item_type_id,
            ItemPos copies = 1) const
    {
        return keys_[item_type_id] * static_cast<std::size_t>(copies);
    }

private:

    /** Key of each item type. */
    std::vector<std::size_t> keys_;

};

}
