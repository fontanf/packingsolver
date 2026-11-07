#pragma once

#include "irregular/shape_simplification.hpp"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace packingsolver
{
namespace irregular
{

/**
 * Fully resolved periodic packing with item metadata (item type, angle,
 * mirror) and a scaled bounding box.
 *
 * A unit cell may contain items of several item types. A periodic packing
 * only depends on the shapes and rotations of its item types, not on the bin
 * dimensions or on the number of copies, so it is computed once, stored in
 * the cache of the instance (see 'Instance::get_periodic_packings'), and
 * carried over to the sub-instances built from it.
 */
struct PeriodicItemPacking
{
    std::vector<SolutionItem> items;

    Point vector_1 = {0, 0};

    Point vector_2 = {0, 0};

    /** Axis-aligned bounding box of all items in one cell (scaled coordinates). */
    AxisAlignedBoundingBox aabb_scaled;
};

/**
 * Cache of the simplified shapes of an instance, for each maximum
 * approximation ratio; see 'Instance::get_simplified_instance'.
 *
 * The simplified shapes only depend on the shapes and on the parameters of
 * the instance, not on the number of copies or on the profits of the item
 * types, nor on the objective, which may differ in the sub-instances. When a
 * sub-instance is built from an original instance (see
 * 'InstanceBuilder::set_original_instance'), 'InstanceBuilder::build' copies
 * the simplified shapes already computed for the original instance into it,
 * with the item type and bin type ids of the sub-instance.
 */
struct SimplifiedInstancesCache
{
    /** Mutex held while computing simplified shapes. */
    std::mutex mutex;

    /**
     * Simplified shapes, for each maximum approximation ratio.
     *
     * The map is never modified: an insertion replaces it by a new one, so
     * that it can be read without the mutex. It is accessed with
     * 'std::atomic_load' and 'std::atomic_store'. The simplified shapes are
     * never removed, so a pointer to them stays valid while the cache
     * exists.
     */
    std::shared_ptr<const std::map<double, std::shared_ptr<const SimplifiedInstance>>> simplified_instances
        = std::make_shared<const std::map<double, std::shared_ptr<const SimplifiedInstance>>>();
};

/**
 * Cache of the periodic packings of an instance; see
 * 'Instance::get_periodic_packings'.
 */
struct PeriodicPackingsCache
{
    /** Mutex held while computing periodic packings. */
    std::mutex mutex;

    /**
     * For each item type, true iff its periodic packings have been computed.
     * Empty if none has been computed. Only accessed under the mutex until
     * 'computed' is true.
     */
    std::vector<bool> item_type_periodic_packings_computed;

    /**
     * True iff the periodic packings of all the item types have been
     * computed. The cache is not modified once computed, so it can then be
     * read without the mutex.
     */
    std::atomic<bool> computed{false};

    /**
     * Periodic packings of the instance. The item types of a periodic
     * packing are given by the 'item_type_id' of its items.
     */
    std::vector<PeriodicItemPacking> periodic_packings;
};

}
}
