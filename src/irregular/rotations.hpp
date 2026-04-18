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
 * Given a convex shape and bin dimensions, find all angles θ ∈ [0°, 360°) at
 * which the shape's axis-aligned bounding-box width equals bin_width or its
 * height equals bin_height, subject to the shape fitting (w ≤ bin_width and
 * h ≤ bin_height).
 *
 * These are the critical angles where the shape transitions from fitting to not
 * fitting in one bin dimension, and are the candidates needed when no standard
 * discrete rotation fits.
 *
 * The algorithm: for each ordered pair of vertices (i, j) of the shape, the
 * width function w(θ) = (xi−xj)·cos θ + (yj−yi)·sin θ = W is a sinusoidal
 * equation solved analytically as θ = atan2(B,A) ± acos(W/sqrt(A²+B²)).
 * Each candidate angle is validated by recomputing the full bounding box to
 * discard pairs that are not the actual extremal pair at that angle.
 *
 * Filtering for allowed rotations and mirroring is left to the caller.
 * For mirrored placements, the caller should pass the mirrored shape.
 *
 * shape and bin_width / bin_height must be in the same coordinate system.
 * The result may contain duplicates; deduplication is left to the caller.
 */
std::vector<Angle> compute_bin_fit_rotations(
        const Shape& shape,
        LengthDbl bin_width,
        LengthDbl bin_height);

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
std::vector<std::vector<std::vector<ItemTypeRotation>>> compute_item_type_rotations(
        const Instance& instance);

}
}
