#pragma once

#include "box/block.hpp"

#include <mutex>

namespace packingsolver
{
namespace box
{

/**
 * Cache of the blocks of the tree search maximal spaces of an instance; see
 * 'Instance::get_blocks'.
 */
struct BlocksCache
{
    /** Mutex protecting the other members. */
    std::mutex mutex;

    /** True iff 'blocks' has been computed. */
    bool computed = false;

    /** Blocks of the tree search maximal spaces, for each bin type. */
    std::vector<std::vector<Block>> blocks;
};

}
}
