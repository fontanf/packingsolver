#include "irregular/rotations.hpp"

#include "shape/convex_hull.hpp"

#include <algorithm>

using namespace packingsolver;
using namespace packingsolver::irregular;

std::vector<std::vector<Angle>> packingsolver::irregular::compute_item_type_rotations(
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

    // Compute candidate rotations: angles that make a side of the item shape or
    // of its convex hull parallel to the x-axis or y-axis, restricted to
    // the item's allowed rotation ranges.
    std::vector<std::vector<Angle>> candidate_rotations(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        std::vector<Angle> angles;
        for (const auto& range: item_type.allowed_rotations) {
            angles.push_back(range.first);
            angles.push_back(range.second);
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
                    if (item_type.is_rotation_allowed(a))
                        angles.push_back(a);
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
                    if (item_type.is_rotation_allowed(a))
                        angles.push_back(a);
                }
            }
        }  // item_shape_pos
        std::sort(angles.begin(), angles.end());
        for (Angle angle: angles) {
            if (candidate_rotations[item_type_id].empty()
                    || !equal(angle, candidate_rotations[item_type_id].back())) {
                candidate_rotations[item_type_id].push_back(angle);
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

    // Score each candidate rotation by (bounding_box_area - convex_hull_area).
    // Convex hull area is rotation-invariant; bounding box area is not.
    struct RotationCandidate
    {
        ItemTypeId item_type_id = -1;
        Angle angle;
        LengthDbl width;
        LengthDbl height;
        AreaDbl score;
    };
    std::vector<RotationCandidate> candidates;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        AreaDbl convex_hull_area = 0.0;
        for (const Shape& ch: item_shapes_convex_hulls[item_type_id])
            convex_hull_area += ch.compute_area();

        const std::vector<Point>& points = item_types_convex_hull_points[item_type_id];
        for (Angle angle: candidate_rotations[item_type_id]) {
            double cos_a = std::cos(angle * M_PI / 180.0);
            double sin_a = std::sin(angle * M_PI / 180.0);
            LengthDbl x_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_min = +std::numeric_limits<LengthDbl>::infinity();
            LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();
            for (const Point& p: points) {
                LengthDbl x = p.x * cos_a - p.y * sin_a;
                LengthDbl y = p.x * sin_a + p.y * cos_a;
                x_min = (std::min)(x_min, x);
                x_max = (std::max)(x_max, x);
                y_min = (std::min)(y_min, y);
                y_max = (std::max)(y_max, y);
            }
            RotationCandidate candidate;
            candidate.item_type_id = item_type_id;
            candidate.angle = angle;
            candidate.width = x_max - x_min;
            candidate.height = y_max - y_min;
            candidate.score = (candidate.width * candidate.height - convex_hull_area) / (convex_hull_area * convex_hull_area);
            candidates.push_back(candidate);
        }
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

    // Helper to add an angle to output if not already present.
    std::vector<std::vector<Angle>> output(instance.number_of_item_types());
    ItemPos budget = (std::max)(100, 5 * instance.number_of_item_types());
    auto add_if_new = [&](ItemTypeId item_type_id, Angle angle) {
        for (Angle a: output[item_type_id]) {
            Angle diff = std::abs(angle - a);
            if (diff < 5 || diff > 355)
                return;
        }
        output[item_type_id].push_back(angle);
        budget--;
    };

    // Pass 1: select the best-score, least-wide and least-high rotation for each item type.
    std::vector<RotationCandidate> min_width(instance.number_of_item_types());
    std::vector<RotationCandidate> min_height(instance.number_of_item_types());
    for (const RotationCandidate& candidate: candidates) {
        if (min_width[candidate.item_type_id].item_type_id == -1
                || candidate.width < min_width[candidate.item_type_id].width) {
            min_width[candidate.item_type_id] = candidate;
        }
        if (min_height[candidate.item_type_id].item_type_id == -1
                || candidate.height < min_height[candidate.item_type_id].height) {
            min_height[candidate.item_type_id] = candidate;
        }
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        add_if_new(item_type_id, min_width[item_type_id].angle);
        {
            Angle angle = min_width[item_type_id].angle + 180;
            while (angle >= 360)
                angle -= 360;
            if (item_type.is_rotation_allowed(angle))
                add_if_new(item_type_id, angle);
        }
        add_if_new(item_type_id, min_height[item_type_id].angle);
        {
            Angle angle = min_height[item_type_id].angle + 180;
            while (angle >= 360)
                angle -= 360;
            if (item_type.is_rotation_allowed(angle))
                add_if_new(item_type_id, angle);
        }
    }

    std::vector<bool> item_type_covered(instance.number_of_item_types(), false);
    for (const RotationCandidate& candidate: candidates) {
        if (item_type_covered[candidate.item_type_id])
            continue;
        add_if_new(candidate.item_type_id, candidate.angle);
        item_type_covered[candidate.item_type_id] = true;
    }

    // Pass 2: fill up to 4 * number_of_item_types total selected rotations.
    for (const RotationCandidate& candidate: candidates) {
        if (budget == 0)
            break;
        add_if_new(candidate.item_type_id, candidate.angle);
    }

    //for (ItemTypeId item_type_id = 0;
    //        item_type_id < instance.number_of_item_types();
    //        ++item_type_id) {
    //    std::cout << "item_type_id " << item_type_id << ":";
    //    for (Angle angle: output[item_type_id])
    //        std::cout << " " << angle;
    //    std::cout << std::endl;
    //}

    return output;
}
