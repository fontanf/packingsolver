#pragma once

#include "rectangle/block.hpp"

#include <atomic>
#include <mutex>

namespace packingsolver
{
namespace rectangle
{

/**
 * Cache of the blocks of the tree search maximal spaces of an instance; see
 * 'Instance::get_blocks'.
 */
struct BlocksCache
{
    /** Mutex held while computing the blocks. */
    std::mutex mutex;

    /**
     * True iff 'blocks' has been computed. 'blocks' is not modified once
     * computed, so it can then be read without the mutex.
     */
    std::atomic<bool> computed{false};

    /** Blocks of the tree search maximal spaces, for each bin type. */
    std::vector<std::vector<Block>> blocks;
};

}
}
