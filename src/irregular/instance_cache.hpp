#pragma once

#include "irregular/shape_simplification.hpp"

#include <map>
#include <mutex>

namespace packingsolver
{
namespace irregular
{

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
    /** Mutex protecting the other members. */
    std::mutex mutex;

    /** Simplified shapes, for each maximum approximation ratio. */
    std::map<double, SimplifiedInstance> simplified_instances;
};

}
}
