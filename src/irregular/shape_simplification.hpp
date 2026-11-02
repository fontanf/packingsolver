#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

/**
 * 'minimum_number_of_vertices' is forwarded as-is to shape::simplify (see
 * its own doc): every border, defect, and item shape (and their inflated
 * counterparts) already at or below it is left unapproximated, regardless
 * of how much of the area budget implied by maximum_approximation_ratio is
 * left unspent.
 */
SimplifiedInstance shape_simplification(
        const Instance& instance,
        double maximum_approximation_ratio,
        shape::ElementPos minimum_number_of_vertices = 4);

/**
 * Compute the simplified shapes of the cache of an instance; see
 * 'InstanceCache'.
 */
void compute_simplified_instance_cache(
        Instance& instance,
        double maximum_approximation_ratio);

}
}
