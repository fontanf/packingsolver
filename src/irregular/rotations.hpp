#pragma once

#include "packingsolver/irregular/instance.hpp"

namespace packingsolver
{
namespace irregular
{

struct ItemTypeRotation
{
    Angle angle;
    bool mirror;
};

/**
 * Compute the rotations to consider for each item type.
 *
 * Candidate rotations are the angles that make a side of the item shape or of
 * its convex hull parallel to the x-axis or y-axis, filtered to those within
 * the item's allowed rotation ranges.
 *
 * Each candidate rotation is scored by
 * (bounding_box_area - convex_hull_area) / convex_hull_area^2,
 * where the bounding box is computed by projecting the convex hull vertices.
 * This criterion favors rotations with a good relative fit (hull fills the
 * bounding box well) and prioritizes larger items over smaller ones.
 * Angles within 5 degrees of an already-selected angle are skipped.
 *
 * Selection proceeds in two passes:
 * 1. For each item type, add the least-wide rotation and its +180° counterpart,
 *    the least-high rotation and its +180° counterpart, and the best-score
 *    rotation.
 * 2. Fill remaining budget with the globally best-score rotations, up to a
 *    total budget of max(100, 5*n) additions (n = number of item types).
 *
 * Mirroring is applied separately to all selected rotations.
 */
std::vector<std::vector<ItemTypeRotation>> compute_item_type_rotations(
        const Instance& instance);

}
}
