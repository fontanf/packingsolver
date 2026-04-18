#include "irregular/rotations.hpp"

#include "shape/convex_hull.hpp"
#include "shape/shape.hpp"

#include <algorithm>
#include <unordered_map>

using namespace packingsolver;
using namespace packingsolver::irregular;

std::vector<Angle> packingsolver::irregular::compute_bin_fit_rotations(
        const Shape& shape,
        LengthDbl bin_width,
        LengthDbl bin_height)
{
    std::vector<Angle> output;

    // Solve coeff_cos*cos(angle) + coeff_sin*sin(angle) = target for
    // angle in [0, 360).  Appends up to two solutions (degrees) to out_angles.
    auto solve_sinusoidal = [](
            double coeff_cos,
            double coeff_sin,
            double target,
            std::vector<Angle>& out_angles)
    {
        double amplitude = std::sqrt(coeff_cos * coeff_cos + coeff_sin * coeff_sin);
        if (amplitude < 1e-12)
            return;
        double ratio = target / amplitude;
        if (ratio < -1.0 - 1e-9 || ratio > 1.0 + 1e-9)
            return;
        ratio = std::max(-1.0, std::min(1.0, ratio));
        double base_angle = std::atan2(coeff_sin, coeff_cos) * 180.0 / M_PI;
        double half_spread = std::acos(ratio) * 180.0 / M_PI;
        for (double candidate: {base_angle + half_spread, base_angle - half_spread}) {
            while (candidate < 0) candidate += 360;
            while (candidate >= 360) candidate -= 360;
            out_angles.push_back(candidate);
        }
    };

    // Recompute the actual bounding-box width and height of the shape at the
    // given angle (degrees), using the non-mirrored rotation convention
    // x' = x*cos(a) - y*sin(a), y' = x*sin(a) + y*cos(a).
    auto compute_aabb_dims = [&](
            Angle angle,
            LengthDbl& out_width,
            LengthDbl& out_height)
    {
        double cos_angle = std::cos(angle * M_PI / 180.0);
        double sin_angle = std::sin(angle * M_PI / 180.0);
        LengthDbl x_min = +std::numeric_limits<LengthDbl>::infinity();
        LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
        LengthDbl y_min = +std::numeric_limits<LengthDbl>::infinity();
        LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();
        for (const ShapeElement& element: shape.elements) {
            LengthDbl rotated_x = element.start.x * cos_angle - element.start.y * sin_angle;
            LengthDbl rotated_y = element.start.x * sin_angle + element.start.y * cos_angle;
            x_min = std::min(x_min, rotated_x);
            x_max = std::max(x_max, rotated_x);
            y_min = std::min(y_min, rotated_y);
            y_max = std::max(y_max, rotated_y);
        }
        out_width = x_max - x_min;
        out_height = y_max - y_min;
    };

    // For a non-mirrored rotation x' = x*cos(a) - y*sin(a), y' = x*sin(a) + y*cos(a):
    //   width  of bbox = max_i(xi*cos - yi*sin) - min_j(xj*cos - yj*sin)
    //                  → for pair (i, j): (xi-xj)*cos(a) + (yj-yi)*sin(a) = bin_width
    //   height of bbox = max_i(xi*sin + yi*cos) - min_j(xj*sin + yj*cos)
    //                  → for pair (i, j): (yi-yj)*cos(a) + (xi-xj)*sin(a) = bin_height
    int num_elements = (int)shape.elements.size();
    for (int elem_i = 0; elem_i < num_elements; ++elem_i) {
        for (int elem_j = 0; elem_j < num_elements; ++elem_j) {
            if (elem_i == elem_j)
                continue;
            const Point& vertex_i = shape.elements[elem_i].start;
            const Point& vertex_j = shape.elements[elem_j].start;
            double diff_x = vertex_i.x - vertex_j.x;
            double diff_y = vertex_i.y - vertex_j.y;

            std::vector<Angle> candidate_angles;
            solve_sinusoidal(diff_x, -diff_y, bin_width,  candidate_angles);
            solve_sinusoidal(diff_y,  diff_x, bin_height, candidate_angles);

            for (Angle angle: candidate_angles) {
                LengthDbl item_width, item_height;
                compute_aabb_dims(angle, item_width, item_height);
                if (!shape::strictly_greater(item_width, bin_width)
                        && !shape::strictly_greater(item_height, bin_height))
                    output.push_back(angle);
            }
        }
    }

    return output;
}

std::vector<std::vector<std::vector<ItemTypeRotation>>> packingsolver::irregular::compute_item_type_rotations(
        const Instance& instance)
{
    // Precompute the convex hull of each item shape (reused for both candidate
    // angle generation and scoring).
    std::vector<std::vector<Shape>> item_shapes_convex_hulls(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (const ItemShape& item_shape: item_type.shapes)
            item_shapes_convex_hulls[item_type_id].push_back(
                    shape::convex_hull(item_shape.shape_scaled.shape));
    }

    // Compute candidate rotations: (angle, mirror) pairs where the angle makes
    // a side of the item shape or of its convex hull parallel to the x-axis or
    // y-axis, restricted to the item's allowed rotation ranges.
    std::vector<std::vector<std::pair<Angle, bool>>> candidate_rotations(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        std::vector<std::pair<Angle, bool>> angle_mirror_pairs;
        for (const AllowedRotation& rotation: item_type.allowed_rotations) {
            angle_mirror_pairs.push_back({rotation.start_angle, rotation.mirror});
            angle_mirror_pairs.push_back({rotation.end_angle, rotation.mirror});
        }
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            const ItemShape& item_shape = item_type.shapes[item_shape_pos];
            // Find all the angles that make a side of the shape parallel
            // to the x-axis or to the y-axis.
            for (const ShapeElement& element: item_shape.shape_scaled.shape.elements) {
                LengthDbl dx = element.end.x - element.start.x;
                LengthDbl dy = element.end.y - element.start.y;
                Angle phi = std::atan2(dx, dy) * 180 / M_PI;
                for (Angle a: {
                        phi,
                        90 - phi,
                        90 + phi,
                        180 - phi,
                        180 + phi,
                        180 + 90 - phi,
                        180 + 90 + phi,
                        180 + 180 - phi}) {
                    while (a < 0)
                        a += 360;
                    while (a >= 360)
                        a -= 360;
                    if (item_type.is_rotation_allowed(a, false))
                        angle_mirror_pairs.push_back({a, false});
                    if (item_type.is_rotation_allowed(a, true))
                        angle_mirror_pairs.push_back({a, true});
                }
            }
            // Find all the angles that make a side of the convex hull
            // parallel to the x-axis or to the y-axis.
            const Shape& convex_hull = item_shapes_convex_hulls[item_type_id][item_shape_pos];
            for (const ShapeElement& element: convex_hull.elements) {
                LengthDbl dx = element.end.x - element.start.x;
                LengthDbl dy = element.end.y - element.start.y;
                Angle phi = std::atan2(dx, dy) * 180 / M_PI;
                for (Angle a: {
                        phi,
                        90 - phi,
                        90 + phi,
                        180 - phi,
                        180 + phi,
                        180 + 90 - phi,
                        180 + 90 + phi,
                        180 + 180 - phi}) {
                    while (a < 0)
                        a += 360;
                    while (a >= 360)
                        a -= 360;
                    if (item_type.is_rotation_allowed(a, false))
                        angle_mirror_pairs.push_back({a, false});
                    if (item_type.is_rotation_allowed(a, true))
                        angle_mirror_pairs.push_back({a, true});
                }
            }
        }  // item_shape_pos
        // Sort by (angle, mirror) and deduplicate.
        std::sort(
                angle_mirror_pairs.begin(),
                angle_mirror_pairs.end(),
                [](const std::pair<Angle, bool>& p1, const std::pair<Angle, bool>& p2) {
                    if (!equal(p1.first, p2.first))
                        return p1.first < p2.first;
                    return (int)p1.second < (int)p2.second;
                });
        for (const std::pair<Angle, bool>& p: angle_mirror_pairs) {
            if (candidate_rotations[item_type_id].empty()
                    || candidate_rotations[item_type_id].back().second != p.second
                    || !equal(p.first, candidate_rotations[item_type_id].back().first)) {
                candidate_rotations[item_type_id].push_back(p);
            }
        }
    }

    // Collect convex hull vertex points per item type (used for fast AABB computation).
    // The AABB of a shape depends only on its convex hull, and for a convex polygon
    // the extremes are always at vertices, so projecting vertices is exact.
    std::vector<std::vector<Point>> item_types_convex_hull_points(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        for (const Shape& ch: item_shapes_convex_hulls[item_type_id])
            for (const ShapeElement& element: ch.elements)
                item_types_convex_hull_points[item_type_id].push_back(element.start);
    }

    // Shared deduplication infrastructure used in both candidate generation and
    // per-bin rotation selection.  Key: quantized (width, height) bucket pair.
    struct PairHash {
        size_t operator()(const std::pair<int64_t, int64_t>& p) const {
            return std::hash<int64_t>{}(p.first) ^ (std::hash<int64_t>{}(p.second) << 32);
        }
    };
    using ShapesByDimsMap = std::unordered_map<
        std::pair<int64_t, int64_t>,
        std::vector<std::vector<ShapeWithHoles>>,
        PairHash>;
    // Step 1e-3 with tolerance 1e-6: returns (floor_lo, floor_hi) bucket pair so
    // that pairs straddling a bucket boundary are still checked on both sides.
    auto dim_buckets = [](double v) -> std::pair<int64_t, int64_t> {
        return std::make_pair(
            (int64_t)std::floor((v - 1e-6) / 1e-3),
            (int64_t)std::floor((v + 1e-6) / 1e-3));
    };

    // Score each (angle, mirror) candidate by (bounding_box_area - convex_hull_area).
    // Convex hull area is rotation-invariant; bounding box area is not.
    // Candidates that produce the same shape (up to translation) are deduplicated
    // by comparing normalized transformed shapes using shape::equal.
    struct RotationCandidate
    {
        ItemTypeId item_type_id = -1;
        Angle angle;
        bool mirror;
        LengthDbl width;
        LengthDbl height;
        AreaDbl score;
    };
    std::vector<RotationCandidate> candidates;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        AreaDbl convex_hull_area = 0.0;
        for (const Shape& ch: item_shapes_convex_hulls[item_type_id])
            convex_hull_area += ch.compute_area();

        const std::vector<Point>& points = item_types_convex_hull_points[item_type_id];

        std::vector<RotationCandidate> item_candidates;

        // Hash map for O(1) deduplication by quantized (width, height).
        // Values in the map are normalized transformed shapes already accepted.
        ShapesByDimsMap shapes_by_dims;

        for (const std::pair<Angle, bool>& angle_mirror: candidate_rotations[item_type_id]) {
            Angle angle = angle_mirror.first;
            bool mirror = angle_mirror.second;
            double cos_a = std::cos(angle * M_PI / 180.0);
            double sin_a = std::sin(angle * M_PI / 180.0);
            LengthDbl x_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();
            for (const Point& p: points) {
                // Apply mirror (axial_symmetry_y_axis: x -> -x) then rotation.
                LengthDbl px = mirror? -p.x: p.x;
                LengthDbl x = px * cos_a - p.y * sin_a;
                LengthDbl y = px * sin_a + p.y * cos_a;
                x_min = (std::min)(x_min, x);
                x_max = (std::max)(x_max, x);
                y_min = (std::min)(y_min, y);
                y_max = (std::max)(y_max, y);
            }
            RotationCandidate candidate;
            candidate.item_type_id = item_type_id;
            candidate.angle = angle;
            candidate.mirror = mirror;
            candidate.width = x_max - x_min;
            candidate.height = y_max - y_min;
            candidate.score = (candidate.width * candidate.height - convex_hull_area) / (convex_hull_area * convex_hull_area);

            // Build normalized transformed shapes for deduplication.
            std::vector<ShapeWithHoles> transformed_shapes;
            for (const ItemShape& item_shape: item_type.shapes) {
                ShapeWithHoles swh = mirror?
                    item_shape.shape_scaled.axial_symmetry_y_axis():
                    item_shape.shape_scaled;
                swh = swh.rotate(angle);
                swh.shift(-x_min, -y_min);
                transformed_shapes.push_back(std::move(swh));
            }

            // Check if this (angle, mirror) produces the same shape as an existing candidate.
            // Look up all buckets that could contain a matching entry.
            std::pair<int64_t, int64_t> wb = dim_buckets(candidate.width);
            std::pair<int64_t, int64_t> hb = dim_buckets(candidate.height);
            int64_t bw0 = wb.first;
            int64_t bw1 = wb.second;
            int64_t bh0 = hb.first;
            int64_t bh1 = hb.second;
            bool is_duplicate = false;
            for (int64_t bw: {bw0, bw1}) {
                if (is_duplicate) break;
                for (int64_t bh: {bh0, bh1}) {
                    if (is_duplicate) break;
                    auto it = shapes_by_dims.find({bw, bh});
                    if (it == shapes_by_dims.end())
                        continue;
                    for (const std::vector<ShapeWithHoles>& existing: it->second) {
                        bool all_equal = (existing.size() == transformed_shapes.size());
                        for (ItemShapePos item_shape_pos = 0;
                                item_shape_pos < (ItemShapePos)transformed_shapes.size() && all_equal;
                                ++item_shape_pos) {
                            all_equal = shape::equal(transformed_shapes[item_shape_pos], existing[item_shape_pos]);
                        }
                        if (all_equal) {
                            is_duplicate = true;
                            break;
                        }
                    }
                }
            }

            if (is_duplicate) {
                //std::cout << "item_type_id " << item_type_id
                //    << " angle " << angle
                //    << " mirror " << mirror
                //    << std::endl;
                continue;
            }

            item_candidates.push_back(candidate);
            // Insert only under the canonical bucket (bw0, bh0).
            shapes_by_dims[{bw0, bh0}].push_back(std::move(transformed_shapes));
        }

        for (const RotationCandidate& candidate: item_candidates)
            candidates.push_back(candidate);
    }

    // Sort by ascending score (lower = better).
    std::sort(
            candidates.begin(),
            candidates.end(),
            [](
                const RotationCandidate& candidate_1,
                const RotationCandidate& candidate_2)
            {
                return candidate_1.score < candidate_2.score;
            });

    // Compute global min-width and min-height candidates (bin-type independent).
    std::vector<RotationCandidate> min_width_cand(instance.number_of_item_types());
    std::vector<RotationCandidate> min_height_cand(instance.number_of_item_types());
    for (const RotationCandidate& candidate: candidates) {
        if (min_width_cand[candidate.item_type_id].item_type_id == -1
                || candidate.width < min_width_cand[candidate.item_type_id].width) {
            min_width_cand[candidate.item_type_id] = candidate;
        }
        if (min_height_cand[candidate.item_type_id].item_type_id == -1
                || candidate.height < min_height_cand[candidate.item_type_id].height) {
            min_height_cand[candidate.item_type_id] = candidate;
        }
    }

    std::vector<std::vector<std::vector<ItemTypeRotation>>> output(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);

        // Bin dimensions in scaled coordinates.
        AxisAlignedBoundingBox bin_aabb_scaled = bin_type.shape_scaled.compute_min_max();
        LengthDbl bin_width = bin_aabb_scaled.x_max - bin_aabb_scaled.x_min;
        LengthDbl bin_height = bin_aabb_scaled.y_max - bin_aabb_scaled.y_min;

        auto fits_in_bin = [&](const RotationCandidate& candidate) {
            return !shape::strictly_greater(candidate.width, bin_width)
                && !shape::strictly_greater(candidate.height, bin_height);
        };

        std::vector<std::vector<ItemTypeRotation>> bin_output(instance.number_of_item_types());
        ItemPos bin_budget = (std::max)(100, 5 * instance.number_of_item_types());

        // Per-item-type deduplication maps for this bin type: track normalized
        // transformed shapes already added so that shape-equivalent (angle, mirror)
        // pairs are rejected regardless of where they came from (candidates list or
        // compute_bin_fit_rotations).
        std::vector<ShapesByDimsMap> bin_shapes_by_dims(instance.number_of_item_types());

        auto add_if_new_for_bin = [&](ItemTypeId item_type_id, Angle angle, bool mirror) {
            const ItemType& item_type = instance.item_type(item_type_id);
            const std::vector<Point>& hull_points = item_types_convex_hull_points[item_type_id];

            // Compute the AABB of the transformed convex hull to get (x_min, y_min)
            // for normalizing the shape position.
            double cos_a = std::cos(angle * M_PI / 180.0);
            double sin_a = std::sin(angle * M_PI / 180.0);
            LengthDbl x_min_t = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl x_max_t = -std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_min_t = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_max_t = -std::numeric_limits<LengthDbl>::infinity();
            for (const Point& p: hull_points) {
                LengthDbl px = mirror ? -p.x : p.x;
                LengthDbl x = px * cos_a - p.y * sin_a;
                LengthDbl y = px * sin_a + p.y * cos_a;
                x_min_t = std::min(x_min_t, x);
                x_max_t = std::max(x_max_t, x);
                y_min_t = std::min(y_min_t, y);
                y_max_t = std::max(y_max_t, y);
            }
            LengthDbl width_t = x_max_t - x_min_t;
            LengthDbl height_t = y_max_t - y_min_t;

            // Build normalized transformed shapes for this (angle, mirror).
            std::vector<ShapeWithHoles> transformed_shapes;
            for (const ItemShape& item_shape: item_type.shapes) {
                ShapeWithHoles swh = mirror ?
                    item_shape.shape_scaled.axial_symmetry_y_axis() :
                    item_shape.shape_scaled;
                swh = swh.rotate(angle);
                swh.shift(-x_min_t, -y_min_t);
                transformed_shapes.push_back(std::move(swh));
            }

            // Check whether an identical shape is already in bin_output.
            std::pair<int64_t, int64_t> wb = dim_buckets(width_t);
            std::pair<int64_t, int64_t> hb = dim_buckets(height_t);
            int64_t bw0 = wb.first;
            int64_t bw1 = wb.second;
            int64_t bh0 = hb.first;
            int64_t bh1 = hb.second;
            bool is_dup = false;
            ShapesByDimsMap& sdm = bin_shapes_by_dims[item_type_id];
            for (int64_t bw: {bw0, bw1}) {
                if (is_dup) break;
                for (int64_t bh: {bh0, bh1}) {
                    if (is_dup) break;
                    auto it = sdm.find({bw, bh});
                    if (it == sdm.end())
                        continue;
                    for (const std::vector<ShapeWithHoles>& existing: it->second) {
                        bool all_eq = (existing.size() == transformed_shapes.size());
                        for (ItemShapePos sp = 0;
                                sp < (ItemShapePos)transformed_shapes.size() && all_eq;
                                ++sp) {
                            all_eq = shape::equal(transformed_shapes[sp], existing[sp]);
                        }
                        if (all_eq) { is_dup = true; break; }
                    }
                }
            }
            if (is_dup)
                return;

            bin_output[item_type_id].push_back({angle, mirror});
            bin_budget--;
            sdm[{bw0, bh0}].push_back(std::move(transformed_shapes));
        };

        // Pass 1: add min-width and min-height rotations for each item type.
        // If a rotation does not fit in the bin AABB, use compute_bin_fit_rotations
        // to find critical angles where the item just touches a bin dimension.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            const RotationCandidate& mw = min_width_cand[item_type_id];
            const RotationCandidate& mh = min_height_cand[item_type_id];

            bool mw_fits = (mw.item_type_id != -1) && fits_in_bin(mw);
            bool mh_fits = (mh.item_type_id != -1) && fits_in_bin(mh);

            if (!mw_fits || !mh_fits) {
                // At least one of min-width / min-height doesn't fit in this bin.
                // compute_bin_fit_rotations finds the critical angles where the item
                // just touches a bin boundary (one call covers both constraints).
                for (bool mirror: {false, true}) {
                    bool mirror_allowed = false;
                    for (const AllowedRotation& ar: item_type.allowed_rotations)
                        if (ar.mirror == mirror) { mirror_allowed = true; break; }
                    if (!mirror_allowed)
                        continue;

                    Shape hull_shape;
                    for (const Point& p: item_types_convex_hull_points[item_type_id]) {
                        ShapeElement elem;
                        elem.type = ShapeElementType::LineSegment;
                        elem.start = {mirror ? -p.x : p.x, p.y};
                        elem.end = elem.start;
                        hull_shape.elements.push_back(elem);
                    }

                    for (Angle angle: compute_bin_fit_rotations(hull_shape, bin_width, bin_height)) {
                        if (item_type.is_rotation_allowed(angle, mirror))
                            add_if_new_for_bin(item_type_id, angle, mirror);
                    }
                }
            }

            if (mw_fits) {
                add_if_new_for_bin(item_type_id, mw.angle, mw.mirror);
                Angle angle = mw.angle + 180;
                while (angle >= 360) angle -= 360;
                if (item_type.is_rotation_allowed(angle, mw.mirror))
                    add_if_new_for_bin(item_type_id, angle, mw.mirror);
            }

            if (mh_fits) {
                add_if_new_for_bin(item_type_id, mh.angle, mh.mirror);
                Angle angle = mh.angle + 180;
                while (angle >= 360) angle -= 360;
                if (item_type.is_rotation_allowed(angle, mh.mirror))
                    add_if_new_for_bin(item_type_id, angle, mh.mirror);
            }
        }

        // Coverage pass: ensure at least one rotation per item type.
        std::vector<bool> item_type_covered(instance.number_of_item_types(), false);
        for (const RotationCandidate& candidate: candidates) {
            if (!fits_in_bin(candidate))
                continue;
            if (item_type_covered[candidate.item_type_id])
                continue;
            add_if_new_for_bin(candidate.item_type_id, candidate.angle, candidate.mirror);
            item_type_covered[candidate.item_type_id] = true;
        }

        // Pass 2: fill up to budget with best-score rotations that fit in this bin.
        for (const RotationCandidate& candidate: candidates) {
            if (bin_budget == 0)
                break;
            if (!fits_in_bin(candidate))
                continue;
            add_if_new_for_bin(candidate.item_type_id, candidate.angle, candidate.mirror);
        }

        //for (ItemTypeId item_type_id = 0;
        //        item_type_id < instance.number_of_item_types();
        //        ++item_type_id) {
        //    std::cout << "item_type_id " << item_type_id << ":";
        //    for (const ItemTypeRotation& r: bin_output[item_type_id])
        //        std::cout << " (" << r.angle << "," << r.mirror << ")";
        //    std::cout << std::endl;
        //}

        output[bin_type_id] = bin_output;
    }
    return output;
}
