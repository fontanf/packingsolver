#include "irregular/periodic_packing.hpp"

#include "irregular/shape_simplification.hpp"

#include "shape/no_fit_polygon.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/shapes_intersections.hpp"
#include "shape/offset.hpp"
#include "shape/approximation.hpp"

#include <limits>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <map>
#include <deque>

using namespace packingsolver;
using namespace packingsolver::irregular;

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const PeriodicPacking& periodic_packing)
{
    os << "positions";
    for (const Point& position: periodic_packing.positions)
        os << " {" << position.x << ", " << position.y << "}";
    os << " vector_1 {" << periodic_packing.vector_1.x
        << ", " << periodic_packing.vector_1.y << "}";
    os << " vector_2 {" << periodic_packing.vector_2.x
        << ", " << periodic_packing.vector_2.y << "}";
    return os;
}

bool packingsolver::irregular::equal(
        const PeriodicPacking& packing_1,
        const PeriodicPacking& packing_2)
{
    bool same_order = shape::equal(packing_1.vector_1, packing_2.vector_1)
        && shape::equal(packing_1.vector_2, packing_2.vector_2);
    bool swapped = shape::equal(packing_1.vector_1, packing_2.vector_2)
        && shape::equal(packing_1.vector_2, packing_2.vector_1);
    if (!same_order && !swapped)
        return false;
    if (packing_1.positions.size() != packing_2.positions.size())
        return false;
    for (int pos_idx = 0; pos_idx < (int)packing_1.positions.size(); ++pos_idx) {
        if (!shape::equal(packing_1.positions[pos_idx], packing_2.positions[pos_idx]))
            return false;
    }
    return true;
}

namespace
{

/**
 * Apply a rotation (mirror then angle) to each of a set of unrotated
 * sub-shapes and union the result, the same way Instance::item_shape_scaled
 * + get_item_combined_shape do for a real item type, but without needing an
 * Instance/ItemTypeId to look the sub-shapes up from.
 */
ShapeWithHoles compute_combined_shape(
        const std::vector<ShapeWithHoles>& item_shapes,
        const ItemTypeRotation& rotation)
{
    std::vector<ShapeWithHoles> shapes;
    shapes.reserve(item_shapes.size());
    for (const ShapeWithHoles& item_shape: item_shapes) {
        ShapeWithHoles shape = item_shape;
        if (rotation.mirror)
            shape = shape.axial_symmetry_y_axis();
        shape = shape.rotate(rotation.angle);
        shapes.push_back(std::move(shape));
    }
    if (shapes.size() == 1)
        return shapes[0];
    MultiShapeWithHoles union_result = shape::compute_union(shapes);
    if (union_result.shapes_with_holes.empty())
        return shapes[0];
    return union_result.shapes_with_holes[0];
}

/**
 * Return true if 'rotations' contains an entry matching (angle, mirror)
 * (angle normalized to [0, 360)), used in place of
 * ItemType::is_rotation_allowed to check whether a rotation's r + 180
 * counterpart is itself a valid candidate, without needing the ItemType
 * this list of rotations was generated from.
 */
bool contains_rotation(
        const std::vector<ItemTypeRotation>& rotations,
        Angle angle,
        bool mirror)
{
    while (angle < 0)
        angle += 360;
    while (angle >= 360)
        angle -= 360;
    for (const ItemTypeRotation& rotation: rotations)
        if (rotation.mirror == mirror && shape::equal(rotation.angle, angle))
            return true;
    return false;
}

}  // namespace

ShapeWithHoles packingsolver::irregular::get_item_combined_shape(
        const Instance& instance,
        ItemTypeId item_type_id,
        const ItemTypeRotation& rotation)
{
    const ItemType& item_type = instance.item_type(item_type_id);
    std::vector<ShapeWithHoles> item_shapes;
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)item_type.shapes.size();
            ++item_shape_pos) {
        item_shapes.push_back(item_type.shapes[item_shape_pos].shape_scaled);
    }
    return compute_combined_shape(item_shapes, rotation);
}

namespace
{

/**
 * Collect candidate points (with y = 0) from boundary elements of a set of
 * ShapeWithHoles components, using shape::compute_intersections with a
 * horizontal ray at y = 0 as done in Shape::contains.
 */
void collect_y0_candidates(
        const std::vector<ShapeWithHoles>& nfp,
        std::vector<Point>& candidates)
{
    // Compute the combined AABB to set the ray x range.
    AxisAlignedBoundingBox aabb;
    for (const ShapeWithHoles& swh: nfp)
        aabb = merge(aabb, swh.compute_min_max());

    ShapeElement ray;
    ray.type = ShapeElementType::LineSegment;
    ray.start = {aabb.x_min, 0.0};
    ray.end = {aabb.x_max, 0.0};

    for (const ShapeWithHoles& swh: nfp) {
        for (const ShapeElement& elem: swh.shape.elements) {
            ShapeElementIntersectionsOutput intersections
                = shape::compute_intersections(ray, elem);
            for (const ShapeElement& part: intersections.overlapping_parts) {
                candidates.push_back(part.start);
                candidates.push_back(part.end);
            }
            for (const Point& p: intersections.proper_intersections)
                candidates.push_back(p);
            for (const Point& p: intersections.improper_intersections)
                candidates.push_back(p);
        }
    }
}

/**
 * Find the leftmost point on the boundary of the NFP (union of all
 * components) with y = 0 and x satisfying the given lower bound.
 *
 * When allow_zero is false (default), requires x > 0, which gives the minimum
 * positive horizontal spacing for same-rotation rows.
 * When allow_zero is true, requires x >= 0, which is needed for two-rotation
 * packings where two complementary shapes can be placed at the same origin.
 *
 * Returns {+infinity, 0} if no such point exists.
 */
Point find_leftmost_y0_point(
        const std::vector<ShapeWithHoles>& nfp,
        bool allow_zero = false)
{
    std::vector<Point> candidates;
    collect_y0_candidates(nfp, candidates);

    Point best = {std::numeric_limits<double>::infinity(), 0.0};
    for (const Point& p: candidates) {
        bool x_ok = allow_zero?
            !shape::strictly_lesser(p.x, 0.0):
            shape::strictly_greater(p.x, 0.0);
        if (x_ok && shape::strictly_lesser(p.x, best.x))
            best = p;
    }
    return best;
}

/**
 * Collect candidate points (with x = 0) from boundary elements of a set of
 * ShapeWithHoles components, using a vertical ray at x = 0.
 */
void collect_x0_candidates(
        const std::vector<ShapeWithHoles>& nfp,
        std::vector<Point>& candidates)
{
    AxisAlignedBoundingBox aabb;
    for (const ShapeWithHoles& swh: nfp)
        aabb = merge(aabb, swh.compute_min_max());

    ShapeElement ray;
    ray.type = ShapeElementType::LineSegment;
    ray.start = {0.0, aabb.y_min};
    ray.end   = {0.0, aabb.y_max};

    for (const ShapeWithHoles& swh: nfp) {
        for (const ShapeElement& elem: swh.shape.elements) {
            ShapeElementIntersectionsOutput intersections =
                shape::compute_intersections(ray, elem);
            for (const ShapeElement& part: intersections.overlapping_parts) {
                candidates.push_back(part.start);
                candidates.push_back(part.end);
            }
            for (const Point& p: intersections.proper_intersections)
                candidates.push_back(p);
            for (const Point& p: intersections.improper_intersections)
                candidates.push_back(p);
        }
    }
}

/**
 * Find the bottommost point on the boundary of the NFP with x = 0 and y > 0.
 *
 * This gives the minimum positive vertical spacing when items are stacked in
 * the same column, i.e. when the first lattice vector is vertical.
 *
 * Returns {0, +infinity} if no such point exists.
 */
Point find_bottommost_x0_point(
        const std::vector<ShapeWithHoles>& nfp)
{
    std::vector<Point> candidates;
    collect_x0_candidates(nfp, candidates);

    Point best = {0.0, std::numeric_limits<double>::infinity()};
    for (const Point& p: candidates) {
        if (shape::strictly_greater(p.y, 0.0) && shape::strictly_lesser(p.y, best.y))
            best = p;
    }
    return best;
}

/**
 * Collect candidate points for find_bottommost_constrained_point:
 * all element endpoints plus intersections with the vertical rays at x = 0
 * and x = x_bound, using shape::compute_intersections.
 */
void collect_constrained_x_candidates(
        const std::vector<ShapeWithHoles>& nfp,
        double x_bound,
        std::vector<Point>& candidates)
{
    AxisAlignedBoundingBox aabb;
    for (const ShapeWithHoles& swh: nfp)
        aabb = merge(aabb, swh.compute_min_max());

    auto add_ray_intersections = [&](double x) {
        ShapeElement ray;
        ray.type = ShapeElementType::LineSegment;
        ray.start = {x, aabb.y_min};
        ray.end   = {x, aabb.y_max};
        for (const ShapeWithHoles& swh: nfp) {
            for (const ShapeElement& elem: swh.shape.elements) {
                ShapeElementIntersectionsOutput inter =
                    shape::compute_intersections(ray, elem);
                for (const ShapeElement& part: inter.overlapping_parts) {
                    candidates.push_back(part.start);
                    candidates.push_back(part.end);
                }
                for (const Point& p: inter.proper_intersections)
                    candidates.push_back(p);
                for (const Point& p: inter.improper_intersections)
                    candidates.push_back(p);
            }
        }
    };

    for (const ShapeWithHoles& swh: nfp)
        for (const ShapeElement& elem: swh.shape.elements) {
            candidates.push_back(elem.start);
            candidates.push_back(elem.end);
        }
    add_ray_intersections(0.0);
    add_ray_intersections(x_bound);
}

/**
 * Find the bottommost point on the boundary of the given NFP union with
 * y > 0 and 0 ≤ x < x_max.
 *
 * Returns {0, +infinity} if no such point exists.
 */
Point find_bottommost_constrained_point(
        const std::vector<ShapeWithHoles>& nfp,
        double x_max)
{
    std::vector<Point> candidates;
    collect_constrained_x_candidates(nfp, x_max, candidates);

    Point best = {0.0, std::numeric_limits<double>::infinity()};
    for (const Point& p: candidates) {
        if (!shape::strictly_greater(p.y, 0.0)
                || shape::strictly_lesser(p.x, 0.0)
                || !shape::strictly_greater(x_max, p.x))
            continue;
        if (shape::strictly_lesser(p.y, best.y)
                || (shape::equal(p.y, best.y) && shape::strictly_lesser(p.x, best.x)))
            best = p;
    }
    return best;
}

/**
 * Collect candidate points for find_leftmost_constrained_point:
 * all element endpoints plus intersections with the horizontal rays at y = 0
 * and y = y_bound, using shape::compute_intersections.
 */
void collect_constrained_y_candidates(
        const std::vector<ShapeWithHoles>& nfp,
        double y_bound,
        std::vector<Point>& candidates)
{
    AxisAlignedBoundingBox aabb;
    for (const ShapeWithHoles& swh: nfp)
        aabb = merge(aabb, swh.compute_min_max());

    auto add_ray_intersections = [&](double y) {
        ShapeElement ray;
        ray.type = ShapeElementType::LineSegment;
        ray.start = {aabb.x_min, y};
        ray.end   = {aabb.x_max, y};
        for (const ShapeWithHoles& swh: nfp) {
            for (const ShapeElement& elem: swh.shape.elements) {
                ShapeElementIntersectionsOutput inter =
                    shape::compute_intersections(ray, elem);
                for (const ShapeElement& part: inter.overlapping_parts) {
                    candidates.push_back(part.start);
                    candidates.push_back(part.end);
                }
                for (const Point& p: inter.proper_intersections)
                    candidates.push_back(p);
                for (const Point& p: inter.improper_intersections)
                    candidates.push_back(p);
            }
        }
    };

    for (const ShapeWithHoles& swh: nfp)
        for (const ShapeElement& elem: swh.shape.elements) {
            candidates.push_back(elem.start);
            candidates.push_back(elem.end);
        }
    add_ray_intersections(0.0);
    add_ray_intersections(y_bound);
}

/**
 * Find the leftmost point on the boundary of the given NFP union with
 * x > 0 and 0 ≤ y < y_max.
 *
 * Returns {+infinity, 0} if no such point exists.
 */
Point find_leftmost_constrained_point(
        const std::vector<ShapeWithHoles>& nfp,
        double y_max)
{
    std::vector<Point> candidates;
    collect_constrained_y_candidates(nfp, y_max, candidates);

    Point best = {std::numeric_limits<double>::infinity(), 0.0};
    for (const Point& p: candidates) {
        if (!shape::strictly_greater(p.x, 0.0)
                || shape::strictly_lesser(p.y, 0.0)
                || !shape::strictly_greater(y_max, p.y))
            continue;
        if (shape::strictly_lesser(p.x, best.x)
                || (shape::equal(p.x, best.x) && shape::strictly_lesser(p.y, best.y)))
            best = p;
    }
    return best;
}

/**
 * Check that no two items in the periodic tiling strictly overlap.
 *
 * item_shapes[i] is the ShapeWithHoles of the i-th base item (at natural
 * coordinates), item_positions[i] is the translation (bl_corner) applied to
 * it in the unit cell. The full tiling is obtained by repeating the unit cell
 * at all offsets n * vector_1 + m * vector_2.
 *
 * Only checks copies with |n|, |m| <= check_range, which is sufficient when
 * the lattice vectors are at least item-diameter wide.
 */
bool check_periodic_packing(
        const std::vector<ShapeWithHoles>& item_shapes,
        const std::vector<Point>& item_positions,
        Point vector_1,
        Point vector_2,
        int check_range)
{
    int n_items = (int)item_shapes.size();

    // Items within the same cell of at most 2 items are NOT re-checked for
    // overlap below: such callers place them at a relative offset taken
    // directly from the boundary of their own no_fit_polygon
    // (item_positions is all-zero and the offset is already baked into
    // item_shapes for the two-shape case; for the single-shape case there
    // is only one item, so there is no pair to check regardless). A point
    // on an NFP's boundary is, by definition, an exact-touching
    // (zero-overlap) placement, so
    // re-deriving that same fact via an independent intersect() call is not
    // just redundant but actively harmful: it is a second,
    // differently-rounded computation of a razor-thin tangency, which can
    // disagree with the NFP by a hairline (observed: ~1.7e-8 of the item's
    // own area) and reject an otherwise valid, exactly-touching candidate.
    // If no_fit_polygon itself ever produced a boundary point that is not
    // genuinely a valid touching placement, that would be a bug in
    // no_fit_polygon to fix directly, not something to paper over with a
    // redundant recheck here.
    //
    // That guarantee has only been established for exactly this n_items <=
    // 2 usage (see the two call sites above: 1 item for self-tiling, 2 for
    // a shape paired with its own NFP-derived placement against a second
    // rotation). It does NOT automatically extend to n_items >= 3: nothing
    // here would guarantee every pairwise relative offset among 3+ items is
    // itself an NFP boundary point. So for 3 or more items, the pairs of
    // items within the cell are checked too, with a strict intersection
    // test, so that items exactly touching are not rejected.

    // Build the base items (each shape shifted by its position in the cell)
    // and their bounding boxes.
    std::vector<ShapeWithHoles> base_items;
    std::vector<AxisAlignedBoundingBox> base_aabbs;
    for (int item_idx = 0; item_idx < n_items; ++item_idx) {
        ShapeWithHoles s = item_shapes[item_idx];
        s.shift(item_positions[item_idx].x, item_positions[item_idx].y);
        base_aabbs.push_back(s.compute_min_max());
        base_items.push_back(std::move(s));
    }

    // Natural (unshifted) bounding boxes: a copy's bounding box at any offset
    // is then just a translation of these, avoiding a compute_min_max() call
    // on the actual (potentially complex) shifted shape for every offset.
    std::vector<AxisAlignedBoundingBox> natural_aabbs;
    for (int item_idx = 0; item_idx < n_items; ++item_idx)
        natural_aabbs.push_back(item_shapes[item_idx].compute_min_max());

    auto aabb_overlap = [](const AxisAlignedBoundingBox& a, const AxisAlignedBoundingBox& b)
    {
        return !shape::strictly_greater(a.x_min, b.x_max)
            && !shape::strictly_greater(b.x_min, a.x_max)
            && !shape::strictly_greater(a.y_min, b.y_max)
            && !shape::strictly_greater(b.y_min, a.y_max);
    };

    // Pairs of items within the cell (see above).
    if (n_items >= 3) {
        for (int item_idx_1 = 0; item_idx_1 < n_items; ++item_idx_1) {
            for (int item_idx_2 = item_idx_1 + 1; item_idx_2 < n_items; ++item_idx_2) {
                if (aabb_overlap(base_aabbs[item_idx_1], base_aabbs[item_idx_2])
                        && shape::intersect(base_items[item_idx_1], base_items[item_idx_2], true)) {
                    return false;
                }
            }
        }
    }

    // For each non-zero offset within the neighbourhood, check against base.
    // Most of this check_range neighbourhood is far enough away that a cheap
    // bounding-box test alone rules out overlap, without ever needing the
    // exact (and, for complex item shapes, expensive) polygon intersection
    // test below.
    for (int n = -check_range; n <= check_range; ++n) {
        for (int m = -check_range; m <= check_range; ++m) {
            if (n == 0 && m == 0)
                continue;
            Point offset = {
                n * vector_1.x + m * vector_2.x,
                n * vector_1.y + m * vector_2.y,
            };
            for (int item_idx = 0; item_idx < n_items; ++item_idx) {
                Point shift = {
                    offset.x + item_positions[item_idx].x,
                    offset.y + item_positions[item_idx].y};
                AxisAlignedBoundingBox copy_aabb = natural_aabbs[item_idx];
                copy_aabb.x_min += shift.x;
                copy_aabb.x_max += shift.x;
                copy_aabb.y_min += shift.y;
                copy_aabb.y_max += shift.y;

                bool any_aabb_overlap = false;
                for (int base_idx = 0; base_idx < n_items; ++base_idx) {
                    if (aabb_overlap(base_aabbs[base_idx], copy_aabb)) {
                        any_aabb_overlap = true;
                        break;
                    }
                }
                if (!any_aabb_overlap)
                    continue;

                ShapeWithHoles copy = item_shapes[item_idx];
                copy.shift(shift.x, shift.y);
                for (int base_idx = 0; base_idx < n_items; ++base_idx) {
                    if (aabb_overlap(base_aabbs[base_idx], copy_aabb)
                            && shape::intersect(base_items[base_idx], copy, true))
                        return false;
                }
            }
        }
    }
    return true;
}

/**
 * Find the shortest vector_2 perpendicular to an axis-aligned vector_1 such
 * that the lattice (vector_1, vector_2) is valid, given the union of the
 * combined NFP: for a horizontal vector_1, the lowest (0, y), y > 0, and for
 * a vertical vector_1, the leftmost (x, 0), x > 0.
 *
 * Unlike the constrained vector_2 of
 * find_periodic_packing_lattice_from_combined_nfp, which may have any offset
 * along vector_1 and gives the densest lattice, this one gives a rectangular
 * lattice: its rows (or columns) are aligned, which may fit a bin better.
 *
 * Returns a null vector if no such vector is found.
 */
Point find_perpendicular_vector(
        const std::vector<ShapeWithHoles>& shapes,
        const std::vector<ShapeWithHoles>& nfp_union,
        const Point& vector_1)
{
    bool horizontal = (vector_1.y == 0.0);
    LengthDbl step = (horizontal)? vector_1.x: vector_1.y;
    if (!shape::strictly_greater(step, 0.0))
        return {0.0, 0.0};
    // Repeat the NFP along vector_1, over the extent of the NFP.
    AxisAlignedBoundingBox aabb;
    for (const ShapeWithHoles& swh: nfp_union)
        aabb = shape::merge(aabb, swh.compute_min_max());
    LengthDbl extent = (horizontal)?
        (std::max)(std::abs(aabb.x_min), std::abs(aabb.x_max)):
        (std::max)(std::abs(aabb.y_min), std::abs(aabb.y_max));
    int number_of_repetitions = (int)std::ceil(extent / step) + 1;
    // The union of the copies isn't computed, since it is expensive for
    // complex shapes: the candidates are the intersections of the boundary
    // of each copy with the perpendicular line, and a candidate is rejected
    // if it is strictly inside one of the copies.
    std::vector<ShapeWithHoles> ext_nfp;
    for (int k = -number_of_repetitions; k <= number_of_repetitions; ++k) {
        for (ShapeWithHoles swh: nfp_union) {
            swh.shift(k * vector_1.x, k * vector_1.y);
            ext_nfp.push_back(std::move(swh));
        }
    }
    std::vector<Point> candidates;
    if (horizontal) {
        collect_x0_candidates(ext_nfp, candidates);
    } else {
        collect_y0_candidates(ext_nfp, candidates);
    }
    std::vector<LengthDbl> lengths;
    for (const Point& point: candidates) {
        LengthDbl length = (horizontal)? point.y: point.x;
        if (shape::strictly_greater(length, 0.0))
            lengths.push_back(length);
    }
    std::sort(lengths.begin(), lengths.end());
    std::vector<Point> zero_positions(shapes.size(), {0.0, 0.0});
    for (LengthDbl length: lengths) {
        Point vector_2 = (horizontal)? Point{0.0, length}: Point{length, 0.0};
        bool inside = false;
        for (const ShapeWithHoles& swh: ext_nfp) {
            if (swh.contains(vector_2, true)) {
                inside = true;
                break;
            }
        }
        if (inside)
            continue;
        if (check_periodic_packing(
                    shapes,
                    zero_positions,
                    vector_1,
                    vector_2,
                    (std::max)(3, number_of_repetitions + 1))) {
            return vector_2;
        }
    }
    return {0.0, 0.0};
}

/**
 * Given a set of shapes already placed at their positions in the unit cell,
 * find lattice vectors v1, v2 to tile them periodically without overlap.
 *
 * Because the shapes are pre-shifted, the combined forbidden region for v1 is
 * simply: union_{i,j} NFP(shapes[i], shapes[j]) (no per-pair offset needed).
 *
 * Two strategies are tried:
 *   Horizontal: v1 = (w, 0), v2 = bottommost constrained stagger.
 *   Vertical:   v1 = (0, h), v2 = leftmost constrained stagger.
 *
 * Returns up to two PeriodicPackings (horizontal then vertical). On success
 * each carries positions = n × {0,0} (shapes are already at their positions)
 * and the computed lattice vectors.
 *
 * This is the second half of the computation: given the combined forbidden
 * region already assembled by the caller (either
 * compute_periodic_packings(shape, ...) below, which computes it from a
 * single shape's self-NFP, or find_periodic_packing_lattice_two_shapes_cached,
 * which derives it cheaply from precomputed NFPs), search the two strategies
 * and validate each candidate against the actual shapes.
 */
std::vector<PeriodicPacking> find_periodic_packing_lattice_from_combined_nfp(
        const std::vector<ShapeWithHoles>& shapes,
        const std::vector<ShapeWithHoles>& combined_nfp)
{
    int n = (int)shapes.size();
    std::vector<Point> zero_positions(n, {0.0, 0.0});

    if (combined_nfp.empty())
        return {};
    MultiShapeWithHoles combined_nfp_union = shape::compute_union(combined_nfp);

    std::vector<PeriodicPacking> result;

    // Horizontal strategy: v1 = (w, 0).
    {
        Point vector_1 = find_leftmost_y0_point(combined_nfp_union.shapes_with_holes);
        if (shape::strictly_greater(vector_1.x, 0.0)
                && vector_1.x != std::numeric_limits<double>::infinity()) {
            std::vector<ShapeWithHoles> ext_nfp = combined_nfp_union.shapes_with_holes;
            std::vector<ShapeWithHoles> shifted = combined_nfp_union.shapes_with_holes;
            for (ShapeWithHoles& swh: shifted)
                swh.shift(vector_1.x, vector_1.y);
            ext_nfp.insert(ext_nfp.end(), shifted.begin(), shifted.end());
            std::vector<ShapeWithHoles> ext_nfp_union = shape::compute_union(ext_nfp).shapes_with_holes;
            Point vector_2 = find_bottommost_constrained_point(ext_nfp_union, vector_1.x);
            if (shape::strictly_greater(vector_2.y, 0.0)
                    && check_periodic_packing(shapes, zero_positions, vector_1, vector_2, 3)) {
                result.push_back({zero_positions, vector_1, vector_2});
            }
            // Rectangular lattice.
            Point vector_2_perpendicular = find_perpendicular_vector(
                    shapes,
                    combined_nfp_union.shapes_with_holes,
                    vector_1);
            if (shape::strictly_greater(vector_2_perpendicular.y, 0.0)
                    && !shape::equal(vector_2_perpendicular, vector_2)) {
                result.push_back({zero_positions, vector_1, vector_2_perpendicular});
            }
        }
    }

    // Vertical strategy: v1 = (0, h).
    {
        Point vector_1 = find_bottommost_x0_point(combined_nfp_union.shapes_with_holes);
        if (shape::strictly_greater(vector_1.y, 0.0)
                && vector_1.y != std::numeric_limits<double>::infinity()) {
            std::vector<ShapeWithHoles> ext_nfp = combined_nfp_union.shapes_with_holes;
            std::vector<ShapeWithHoles> shifted = combined_nfp_union.shapes_with_holes;
            for (ShapeWithHoles& swh: shifted)
                swh.shift(vector_1.x, vector_1.y);
            ext_nfp.insert(ext_nfp.end(), shifted.begin(), shifted.end());
            std::vector<ShapeWithHoles> ext_nfp_union = shape::compute_union(ext_nfp).shapes_with_holes;
            Point vector_2 = find_leftmost_constrained_point(ext_nfp_union, vector_1.y);
            if (shape::strictly_greater(vector_2.x, 0.0)
                    && check_periodic_packing(shapes, zero_positions, vector_1, vector_2, 3)) {
                result.push_back({zero_positions, vector_1, vector_2});
            }
            // Rectangular lattice.
            Point vector_2_perpendicular = find_perpendicular_vector(
                    shapes,
                    combined_nfp_union.shapes_with_holes,
                    vector_1);
            if (shape::strictly_greater(vector_2_perpendicular.x, 0.0)
                    && !shape::equal(vector_2_perpendicular, vector_2)) {
                result.push_back({zero_positions, vector_1, vector_2_perpendicular});
            }
        }
    }

    return result;
}

/**
 * Same as find_periodic_packing_lattice_from_combined_nfp's combined-NFP
 * assembly, specialized for exactly two shapes, given the four pairwise NFPs
 * already computed in each shape's own natural
 * (position-independent) frame, plus the two shapes' current positions.
 *
 * NFP(A + a, B + b) = NFP(A, B) + (a - b): translating either operand only
 * translates the NFP by the same amount, it does not change its shape. So
 * when a caller invokes this for many different relative offsets between the
 * same two shapes (e.g. once per placement candidate in
 * compute_periodic_packings(shape_0, shape_r), which can have hundreds of
 * candidates for a complex shape), the four NFPs need only be computed once
 * by the caller and reused here via cheap shifts, instead of recomputing
 * shape::no_fit_polygon (the dominant cost) on every call.
 */
std::vector<PeriodicPacking> find_periodic_packing_lattice_two_shapes_cached(
        const ShapeWithHoles& shape_0,
        const ShapeWithHoles& shape_1,
        Point position_0,
        Point position_1,
        const std::vector<ShapeWithHoles>& nfp_00_union,
        const std::vector<ShapeWithHoles>& nfp_01_union,
        const std::vector<ShapeWithHoles>& nfp_10_union,
        const std::vector<ShapeWithHoles>& nfp_11_union)
{
    std::vector<ShapeWithHoles> combined_nfp;
    combined_nfp.reserve(
            nfp_00_union.size() + nfp_01_union.size()
            + nfp_10_union.size() + nfp_11_union.size());

    for (const ShapeWithHoles& swh: nfp_00_union)
        combined_nfp.push_back(swh);
    for (ShapeWithHoles swh: nfp_01_union) {
        swh.shift(position_0.x - position_1.x, position_0.y - position_1.y);
        combined_nfp.push_back(std::move(swh));
    }
    for (ShapeWithHoles swh: nfp_10_union) {
        swh.shift(position_1.x - position_0.x, position_1.y - position_0.y);
        combined_nfp.push_back(std::move(swh));
    }
    for (const ShapeWithHoles& swh: nfp_11_union)
        combined_nfp.push_back(swh);

    ShapeWithHoles placed_0 = shape_0;
    placed_0.shift(position_0.x, position_0.y);
    ShapeWithHoles placed_1 = shape_1;
    placed_1.shift(position_1.x, position_1.y);

    return find_periodic_packing_lattice_from_combined_nfp(
            {placed_0, placed_1}, combined_nfp);
}

void add_if_unique(
        std::vector<PeriodicPacking>& packings,
        PeriodicPacking pp)
{
    for (const PeriodicPacking& existing: packings)
        if (packingsolver::irregular::equal(pp, existing))
            return;
    packings.push_back(std::move(pp));
}

/**
 * Union of the no-fit polygon of two shapes, in their natural frame, inflated
 * by the item-item minimum spacing.
 *
 * NFP(A, B) inflated by d == NFP(A inflated by d', B inflated by d'') for any
 * split d' + d'' = d (Minkowski sum is associative and a disk is symmetric
 * under negation), so it is equivalent -- and exact, no polygon approximation
 * needed -- to compute the raw NFP and inflate its result by the full spacing
 * amount instead of inflating the shapes beforehand.
 */
std::vector<ShapeWithHoles> compute_inflated_nfp_union(
        const ShapeWithHoles& shape_1,
        const ShapeWithHoles& shape_2,
        LengthDbl item_item_minimum_spacing)
{
    std::vector<ShapeWithHoles> result;
    MultiShapeWithHoles nfp = shape::no_fit_polygon(shape_1, shape_2);
    for (const ShapeWithHoles& swh: shape::compute_union(nfp.shapes_with_holes).shapes_with_holes) {
        result.push_back(item_item_minimum_spacing > 0.0?
                shape::inflate(swh, item_item_minimum_spacing):
                swh);
    }
    return result;
}

/**
 * Same as compute_periodic_packings(shape, ...), given the union of the
 * self-NFP of the shape (see compute_inflated_nfp_union).
 */
std::vector<PeriodicPacking> compute_periodic_packings_from_nfp(
        const ShapeWithHoles& shape,
        const std::vector<ShapeWithHoles>& nfp_union)
{
    AxisAlignedBoundingBox aabb = shape.compute_min_max();
    Point position = {-aabb.x_min, -aabb.y_min};
    ShapeWithHoles shifted = shape;
    shifted.shift(position.x, position.y);

    // The forbidden region for tiling a single shape against copies of
    // itself is exactly its self-NFP, which doesn't depend on the position of
    // the shape.
    std::vector<PeriodicPacking> result;
    for (PeriodicPacking pp: find_periodic_packing_lattice_from_combined_nfp({shifted}, nfp_union)) {
        pp.positions = {position};
        add_if_unique(result, std::move(pp));
    }
    return result;
}

/**
 * Same as compute_periodic_packings(shape_0, shape_r, ...), given the unions
 * of the four pairwise NFPs of the shapes, in their natural frame (see
 * compute_inflated_nfp_union).
 */
std::vector<PeriodicPacking> compute_periodic_packings_from_nfps(
        const ShapeWithHoles& shape_0,
        const ShapeWithHoles& shape_r,
        const std::vector<ShapeWithHoles>& nfp_00_union,
        const std::vector<ShapeWithHoles>& nfp_0r_union_natural,
        const std::vector<ShapeWithHoles>& nfp_r0_union,
        const std::vector<ShapeWithHoles>& nfp_rr_union);

}  // namespace

std::vector<PeriodicPacking> packingsolver::irregular::compute_periodic_packings(
        const ShapeWithHoles& shape,
        LengthDbl item_item_minimum_spacing)
{
    return compute_periodic_packings_from_nfp(
            shape,
            compute_inflated_nfp_union(shape, shape, item_item_minimum_spacing));
}

/**
 * Compute periodic packings for two shapes with different rotations.
 *
 * shape_0 is pre-shifted to have its BL corner at the origin. The boundary
 * vertices of NFP(shape_0, shape_r) are candidate placements of shape_r. The
 * ones passed to find_periodic_packing_lattice_two_shapes_cached to search
 * for horizontal and vertical lattice vectors are: among the placements of
 * shape_r with a non-negative AABB, the ones minimizing the width, the
 * height and the area of the combined AABB; then, among all the placements,
 * the ones whose combined AABB is on the Pareto front of its width and
 * height, and not almost the same as a placement already selected.
 *
 * Returns all valid PeriodicPackings found across all candidate placements,
 * each with positions = {position_0, t_r} where t_r is the translation
 * applied to the natural shape_r.
 */
std::vector<PeriodicPacking> packingsolver::irregular::compute_periodic_packings(
        const ShapeWithHoles& shape_0,
        const ShapeWithHoles& shape_r,
        LengthDbl item_item_minimum_spacing)
{
    return compute_periodic_packings_from_nfps(
            shape_0,
            shape_r,
            compute_inflated_nfp_union(shape_0, shape_0, item_item_minimum_spacing),
            compute_inflated_nfp_union(shape_0, shape_r, item_item_minimum_spacing),
            compute_inflated_nfp_union(shape_r, shape_0, item_item_minimum_spacing),
            compute_inflated_nfp_union(shape_r, shape_r, item_item_minimum_spacing));
}

namespace
{

std::vector<PeriodicPacking> compute_periodic_packings_from_nfps(
        const ShapeWithHoles& shape_0,
        const ShapeWithHoles& shape_r,
        const std::vector<ShapeWithHoles>& nfp_00_union,
        const std::vector<ShapeWithHoles>& nfp_0r_union_natural,
        const std::vector<ShapeWithHoles>& nfp_r0_union,
        const std::vector<ShapeWithHoles>& nfp_rr_union)
{
    AxisAlignedBoundingBox aabb_0 = shape_0.compute_min_max();
    Point position_0 = {-aabb_0.x_min, -aabb_0.y_min};

    AxisAlignedBoundingBox aabb_r = shape_r.compute_min_max();
    Point min_position_r = {-aabb_r.x_min, -aabb_r.y_min};

    if (nfp_0r_union_natural.empty())
        return {};
    // NFP(shape_0, shape_r) shifted by position_0 gives translations t_r such
    // that (shape_r + t_r) just touches (shape_0 shifted by position_0):
    // NFP(A + a, B + b) = NFP(A, B) + (a - b), so only the relative offset
    // changes, never the NFP shape itself.
    std::vector<ShapeWithHoles> nfp_0r_union = nfp_0r_union_natural;
    for (ShapeWithHoles& swh: nfp_0r_union) swh.shift(position_0.x, position_0.y);

    // Collect all boundary vertices as candidate placements for shape_r.
    std::vector<Point> candidates;
    for (const ShapeWithHoles& swh: nfp_0r_union) {
        for (const ShapeElement& elem: swh.shape.elements) {
            candidates.push_back(elem.start);
            candidates.push_back(elem.end);
        }
    }

    // Also collect intersections of the NFP boundary with the two constraint
    // lines (x = min_position_r.x and y = min_position_r.y) so that boundary
    // points that lie on a constraint edge but not at a polygon vertex are not
    // missed.
    AxisAlignedBoundingBox nfp_aabb;
    for (const ShapeWithHoles& swh: nfp_0r_union)
        nfp_aabb = merge(nfp_aabb, swh.compute_min_max());

    auto collect_ray_intersections = [&](ShapeElement ray) {
        for (const ShapeWithHoles& swh: nfp_0r_union) {
            for (const ShapeElement& elem: swh.shape.elements) {
                ShapeElementIntersectionsOutput out =
                    shape::compute_intersections(ray, elem);
                for (const ShapeElement& part: out.overlapping_parts) {
                    candidates.push_back(part.start);
                    candidates.push_back(part.end);
                }
                for (const Point& p: out.proper_intersections)
                    candidates.push_back(p);
                for (const Point& p: out.improper_intersections)
                    candidates.push_back(p);
            }
        }
    };

    ShapeElement h_ray;  // horizontal ray at y = min_position_r.y
    h_ray.type  = ShapeElementType::LineSegment;
    h_ray.start = {nfp_aabb.x_min, min_position_r.y};
    h_ray.end   = {nfp_aabb.x_max, min_position_r.y};
    collect_ray_intersections(h_ray);

    ShapeElement v_ray;  // vertical ray at x = min_position_r.x
    v_ray.type  = ShapeElementType::LineSegment;
    v_ray.start = {min_position_r.x, nfp_aabb.y_min};
    v_ray.end   = {min_position_r.x, nfp_aabb.y_max};
    collect_ray_intersections(v_ray);

    // Deduplicate.
    std::vector<Point> unique_candidates;
    for (const Point& p: candidates) {
        bool is_dup = false;
        for (const Point& q: unique_candidates) {
            if (shape::equal(p, q)) { is_dup = true; break; }
        }
        if (!is_dup)
            unique_candidates.push_back(p);
    }

    // Running the expensive lattice search (NFP unions + check_periodic_packing)
    // on every candidate dominates this function's cost. As a cheap proxy for
    // how compact the resulting periodic cell is likely to be, use the
    // combined AABB of shape_0 (at position_0) and shape_r (at t_r), and only
    // run the expensive search on the (up to 3) candidates that minimize its
    // width, height, and area respectively.
    struct Candidate
    {
        Point t_r;
        double dx;
        double dy;
        double area;
    };
    std::vector<Candidate> valid_candidates;
    for (const Point& t_r: unique_candidates) {
        AxisAlignedBoundingBox aabb_r_here = aabb_r;
        aabb_r_here.shift(t_r.x, t_r.y);
        AxisAlignedBoundingBox combined = merge(aabb_0, aabb_r_here);
        double dx = combined.x_max - combined.x_min;
        double dy = combined.y_max - combined.y_min;
        valid_candidates.push_back({t_r, dx, dy, dx * dy});
    }

    // First, among the placements where the AABB of shape_r doesn't go
    // below or to the left of the one of shape_0, the ones minimizing the
    // width, the height and the area of the combined AABB.
    std::vector<int> selected_candidates;
    {
        auto add_argmin = [&](auto metric) {
            int best = -1;
            for (int i = 0; i < (int)valid_candidates.size(); ++i) {
                const Point& t_r = valid_candidates[i].t_r;
                if (shape::strictly_lesser(t_r.x, min_position_r.x)
                        || shape::strictly_lesser(t_r.y, min_position_r.y)) {
                    continue;
                }
                if (best == -1 || metric(valid_candidates[i]) < metric(valid_candidates[best]))
                    best = i;
            }
            if (best != -1
                    && std::find(selected_candidates.begin(), selected_candidates.end(), best)
                    == selected_candidates.end()) {
                selected_candidates.push_back(best);
            }
        };
        add_argmin([](const Candidate& c) { return c.dx; });
        add_argmin([](const Candidate& c) { return c.dy; });
        add_argmin([](const Candidate& c) { return c.area; });
    }

    // Then, among the placements on every side of shape_0, the ones whose
    // combined AABB is on the Pareto front of its width and height, the ones
    // with the smallest area first, up to
    // 'maximum_number_of_pareto_candidates': a placement which is neither
    // the narrowest, the lowest nor the smallest may still give the best
    // periodic packing (e.g. a pair whose rows are aligned), and with both
    // orders of the two shapes, only keeping the placements where the AABB
    // of shape_r doesn't go below or to the left of the one of shape_0
    // misses the pairs where a shape is above and to the left of the other
    // one.
    const int maximum_number_of_pareto_candidates = 6;
    std::vector<int> pareto_candidates;
    for (int i = 0; i < (int)valid_candidates.size(); ++i) {
        bool dominated = false;
        for (int j = 0; j < (int)valid_candidates.size() && !dominated; ++j) {
            if (i == j)
                continue;
            const Candidate& c_i = valid_candidates[i];
            const Candidate& c_j = valid_candidates[j];
            bool not_worse = !shape::strictly_greater(c_j.dx, c_i.dx)
                && !shape::strictly_greater(c_j.dy, c_i.dy);
            bool better = shape::strictly_lesser(c_j.dx, c_i.dx)
                || shape::strictly_lesser(c_j.dy, c_i.dy);
            // Among candidates with the same dimensions, keep the first one.
            if (not_worse && (better || j < i))
                dominated = true;
        }
        if (!dominated)
            pareto_candidates.push_back(i);
    }
    std::sort(pareto_candidates.begin(), pareto_candidates.end(),
            [&valid_candidates](int i, int j)
            {
                return valid_candidates[i].area < valid_candidates[j].area;
            });
    // Skip the candidates whose combined AABB has almost the same dimensions
    // as the one of a candidate already selected: consecutive vertices of
    // the NFP often give almost the same pair.
    {
        const double relative_tolerance = 0.05;
        std::vector<int> distinct_candidates = selected_candidates;
        int number_of_pareto_candidates = 0;
        for (int i: pareto_candidates) {
            if (number_of_pareto_candidates >= maximum_number_of_pareto_candidates)
                break;
            if (std::find(distinct_candidates.begin(), distinct_candidates.end(), i)
                    != distinct_candidates.end()) {
                continue;
            }
            const Candidate& c_i = valid_candidates[i];
            bool close = false;
            for (int j: distinct_candidates) {
                const Candidate& c_j = valid_candidates[j];
                if (std::abs(c_i.dx - c_j.dx) <= relative_tolerance * (std::max)(c_i.dx, c_j.dx)
                        && std::abs(c_i.dy - c_j.dy) <= relative_tolerance * (std::max)(c_i.dy, c_j.dy)) {
                    close = true;
                    break;
                }
            }
            if (!close) {
                distinct_candidates.push_back(i);
                number_of_pareto_candidates++;
            }
        }
        selected_candidates.swap(distinct_candidates);
    }

    std::vector<PeriodicPacking> result;
    for (int candidate_idx: selected_candidates) {
        const Point& t_r = valid_candidates[candidate_idx].t_r;
        for (PeriodicPacking pp: find_periodic_packing_lattice_two_shapes_cached(
                shape_0, shape_r, position_0, t_r,
                nfp_00_union, nfp_0r_union_natural, nfp_r0_union, nfp_rr_union)) {
            pp.positions = {position_0, t_r};
            add_if_unique(result, std::move(pp));
        }
    }

    return result;
}

}  // namespace

std::vector<PeriodicItemPacking> packingsolver::irregular::compute_periodic_packings_for_item_type(
        const std::vector<ShapeWithHoles>& item_shapes,
        const std::vector<ItemTypeRotation>& rotations,
        LengthDbl item_item_minimum_spacing)
{
    std::vector<PeriodicItemPacking> output;

    // The NFPs are the dominant cost, and most of them are rotations of each
    // other: the combined shape in rotation (angle, mirror) is the combined
    // shape in rotation (0, mirror) rotated by 'angle', and
    // NFP(R(A), R(B)) = R(NFP(A, B)) for a rotation R (inflation commutes
    // with rotations too). So the NFP of the combined shapes in rotations
    // (a, m1) and (b, m2) is the NFP of the combined shapes in rotations
    // (0, m1) and (b - a, m2), rotated by a: it is only computed once for
    // each (m1, m2, b - a).
    struct CachedNfp
    {
        bool mirror_1;
        bool mirror_2;
        Angle angle;
        std::vector<ShapeWithHoles> nfp_union;
    };
    std::vector<CachedNfp> cached_nfps;
    auto nfp_union = [&item_shapes, item_item_minimum_spacing, &cached_nfps](
            const ItemTypeRotation& rotation_1,
            const ItemTypeRotation& rotation_2)
    {
        Angle angle = rotation_2.angle - rotation_1.angle;
        while (angle < 0)
            angle += 360;
        while (angle >= 360)
            angle -= 360;
        const CachedNfp* cached_nfp = nullptr;
        for (const CachedNfp& cached_nfp_cur: cached_nfps) {
            if (cached_nfp_cur.mirror_1 == rotation_1.mirror
                    && cached_nfp_cur.mirror_2 == rotation_2.mirror
                    && shape::equal(cached_nfp_cur.angle, angle)) {
                cached_nfp = &cached_nfp_cur;
                break;
            }
        }
        if (cached_nfp == nullptr) {
            CachedNfp cached_nfp_new;
            cached_nfp_new.mirror_1 = rotation_1.mirror;
            cached_nfp_new.mirror_2 = rotation_2.mirror;
            cached_nfp_new.angle = angle;
            cached_nfp_new.nfp_union = compute_inflated_nfp_union(
                    compute_combined_shape(item_shapes, {0.0, rotation_1.mirror}),
                    compute_combined_shape(item_shapes, {angle, rotation_2.mirror}),
                    item_item_minimum_spacing);
            cached_nfps.push_back(std::move(cached_nfp_new));
            cached_nfp = &cached_nfps.back();
        }
        if (rotation_1.angle == 0.0)
            return cached_nfp->nfp_union;
        std::vector<ShapeWithHoles> result;
        for (const ShapeWithHoles& swh: cached_nfp->nfp_union)
            result.push_back(swh.rotate(rotation_1.angle));
        return result;
    };

    for (int rot_0_pos = 0;
            rot_0_pos < (int)rotations.size();
            ++rot_0_pos) {
        const ItemTypeRotation& rot_0 = rotations[rot_0_pos];
        ShapeWithHoles shape_0 = compute_combined_shape(item_shapes, rot_0);

        for (const PeriodicPacking& pp_same: compute_periodic_packings_from_nfp(
                    shape_0,
                    nfp_union(rot_0, rot_0))) {
            PeriodicItemPacking item_packing;
            item_packing.vector_1 = pp_same.vector_1;
            item_packing.vector_2 = pp_same.vector_2;
            SolutionItem item;
            item.item_type_id = -1;
            item.bl_corner = pp_same.positions[0];
            item.angle = rot_0.angle;
            item.mirror = rot_0.mirror;
            item_packing.items.push_back(item);
            AxisAlignedBoundingBox bb_0 = shape_0.compute_min_max();
            bb_0.shift(pp_same.positions[0]);
            item_packing.aabb_scaled = merge(item_packing.aabb_scaled, bb_0);
            output.push_back(item_packing);
        }

        // Only pair a rotation r < 180° with its r + 180° counterpart (if
        // allowed), rather than every combination of rotations: pairing a
        // shape with its own half-turn is what enables interlocking and,
        // in practice, is the only pairing that ever wins over the
        // single-shape packing, while the two-shape search dominates the
        // overall running time. Rotations with angle >= 180° are only
        // ever considered as the r + 180 partner here, never as rot_0,
        // so each (r, r + 180) pair is still only considered once for
        // *which two rotations* get paired.
        //
        // However, compute_periodic_packings(shape_a, shape_b) is not
        // symmetric in its two arguments: the NFP-based candidate search
        // is anchored on shape_a, so swapping the argument order can (and
        // in practice does) turn up a tighter nesting that the other
        // order misses entirely -- e.g. a perfectly-interlocking,
        // zero-overhang pairing found only via (shape_r, shape_0), never
        // via (shape_0, shape_r). So both orders are tried here.
        if (shape::strictly_lesser(rot_0.angle, 180.0)
                && contains_rotation(rotations, rot_0.angle + 180.0, rot_0.mirror)) {
            ItemTypeRotation rot_r{rot_0.angle + 180.0, rot_0.mirror};
            ShapeWithHoles shape_r = compute_combined_shape(item_shapes, rot_r);

            auto add_two_shape_packings = [&](
                    const ItemTypeRotation& rotation_first,
                    const ShapeWithHoles& shape_first,
                    const ItemTypeRotation& rotation_second,
                    const ShapeWithHoles& shape_second)
            {
                for (const PeriodicPacking& pp_two: compute_periodic_packings_from_nfps(
                            shape_first,
                            shape_second,
                            nfp_union(rotation_first, rotation_first),
                            nfp_union(rotation_first, rotation_second),
                            nfp_union(rotation_second, rotation_first),
                            nfp_union(rotation_second, rotation_second))) {
                    PeriodicItemPacking item_packing;
                    item_packing.vector_1 = pp_two.vector_1;
                    item_packing.vector_2 = pp_two.vector_2;
                    SolutionItem item_first;
                    item_first.item_type_id = -1;
                    item_first.bl_corner = pp_two.positions[0];
                    item_first.angle = rotation_first.angle;
                    item_first.mirror = rotation_first.mirror;
                    item_packing.items.push_back(item_first);
                    AxisAlignedBoundingBox bb_first = shape_first.compute_min_max();
                    bb_first.shift(pp_two.positions[0]);
                    item_packing.aabb_scaled = merge(item_packing.aabb_scaled, bb_first);
                    SolutionItem item_second;
                    item_second.item_type_id = -1;
                    item_second.bl_corner = pp_two.positions[1];
                    item_second.angle = rotation_second.angle;
                    item_second.mirror = rotation_second.mirror;
                    item_packing.items.push_back(item_second);
                    AxisAlignedBoundingBox bb_second = shape_second.compute_min_max();
                    bb_second.shift(pp_two.positions[1]);
                    item_packing.aabb_scaled = merge(item_packing.aabb_scaled, bb_second);
                    output.push_back(item_packing);
                }
            };

            add_two_shape_packings(rot_0, shape_0, rot_r, shape_r);
            add_two_shape_packings(rot_r, shape_r, rot_0, shape_0);
        }
    }

    return output;
}

std::vector<PeriodicItemPacking> packingsolver::irregular::compute_periodic_packings(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations)
{
    std::vector<PeriodicItemPacking> output;

    LengthDbl item_item_minimum_spacing = instance.item_spacing_scaled();
    // See instance_builder.cpp's periodic_packings_simplification_minimum_
    // number_of_vertices for why 64.
    SimplifiedInstance simplified_instance = shape_simplification(instance, 0.001, 64);

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        std::vector<ShapeWithHoles> item_shapes;
        for (const SimplifiedShape& simplified_shape:
                simplified_instance.item_types[item_type_id].shapes) {
            item_shapes.push_back(simplified_shape.shape);
        }
        std::vector<PeriodicItemPacking> item_type_output
            = compute_periodic_packings_for_item_type(
                    item_shapes,
                    item_type_rotations[item_type_id],
                    item_item_minimum_spacing);
        for (PeriodicItemPacking& item_packing: item_type_output)
            for (SolutionItem& item: item_packing.items)
                item.item_type_id = item_type_id;
        output.insert(
                output.end(),
                std::make_move_iterator(item_type_output.begin()),
                std::make_move_iterator(item_type_output.end()));
    }

    return output;
}

bool packingsolver::irregular::periodic_packing_has_single_item_type(
        const PeriodicItemPacking& packing)
{
    for (const SolutionItem& item: packing.items)
        if (item.item_type_id != packing.items[0].item_type_id)
            return false;
    return true;
}

AxisAlignedBoundingBox packingsolver::irregular::compute_largest_empty_side_rectangle(
        const ShapeWithHoles& shape,
        BoundingBoxSide side,
        LengthDbl segment_length)
{
    // Outline as a polygon.
    Shape outline = shape.shape;
    for (const ShapeElement& element: outline.elements) {
        if (element.type == ShapeElementType::CircularArc) {
            outline = shape::approximate_shape_by_line_segments(
                    shape.shape,
                    segment_length,
                    true).shape;
            break;
        }
    }

    // Change coordinates such that 'side' becomes the top side: (u, v) are
    // the coordinates of a point in the new frame.
    auto to_frame = [side](const Point& point)
    {
        switch (side) {
        case BoundingBoxSide::Top:
            return Point{point.x, point.y};
        case BoundingBoxSide::Bottom:
            return Point{point.x, -point.y};
        case BoundingBoxSide::Right:
            return Point{point.y, point.x};
        case BoundingBoxSide::Left:
            return Point{point.y, -point.x};
        }
        return point;
    };
    std::vector<Point> points;
    for (const ShapeElement& element: outline.elements)
        points.push_back(to_frame(element.start));
    if (points.size() < 3)
        return AxisAlignedBoundingBox();

    // Breakpoints of the profile: the distinct abscissas of the vertices.
    std::vector<LengthDbl> us;
    LengthDbl v_max = -std::numeric_limits<LengthDbl>::infinity();
    for (const Point& point: points) {
        us.push_back(point.x);
        v_max = (std::max)(v_max, point.y);
    }
    std::sort(us.begin(), us.end());
    us.erase(std::unique(us.begin(), us.end()), us.end());
    if (us.size() < 2)
        return AxisAlignedBoundingBox();

    // Highest point of the outline over each interval between consecutive
    // breakpoints. Over an interval, the outline is made of the non-vertical
    // edges spanning it; the highest one is the maximum of their linear
    // functions, which is convex, so its maximum is at an end of the
    // interval. The vertical edges don't matter: they are on the
    // breakpoints, and their ends are also ends of non-vertical edges.
    std::size_t number_of_intervals = us.size() - 1;
    std::vector<LengthDbl> profile(number_of_intervals, -std::numeric_limits<LengthDbl>::infinity());
    for (std::size_t point_pos = 0; point_pos < points.size(); ++point_pos) {
        const Point& point_1 = points[point_pos];
        const Point& point_2 = points[(point_pos + 1) % points.size()];
        if (point_1.x == point_2.x)
            continue;
        LengthDbl u_min = (std::min)(point_1.x, point_2.x);
        LengthDbl u_max = (std::max)(point_1.x, point_2.x);
        auto value = [&point_1, &point_2](LengthDbl u)
        {
            return point_1.y + (point_2.y - point_1.y) * (u - point_1.x) / (point_2.x - point_1.x);
        };
        std::size_t first = std::lower_bound(us.begin(), us.end(), u_min) - us.begin();
        std::size_t last = std::lower_bound(us.begin(), us.end(), u_max) - us.begin();
        for (std::size_t interval_pos = first; interval_pos < last; ++interval_pos) {
            profile[interval_pos] = (std::max)({
                    profile[interval_pos],
                    value(us[interval_pos]),
                    value(us[interval_pos + 1])});
        }
    }

    // Depth of each interval.
    std::vector<LengthDbl> depths(number_of_intervals);
    for (std::size_t interval_pos = 0; interval_pos < number_of_intervals; ++interval_pos)
        depths[interval_pos] = v_max - profile[interval_pos];

    // Largest rectangle under this histogram, with a stack of intervals of
    // increasing depths.
    LengthDbl best_area = 0.0;
    LengthDbl best_u_1 = 0.0;
    LengthDbl best_u_2 = 0.0;
    LengthDbl best_depth = 0.0;
    std::vector<std::size_t> stack;
    for (std::size_t interval_pos = 0; interval_pos <= number_of_intervals; ++interval_pos) {
        LengthDbl depth = (interval_pos < number_of_intervals)?
            depths[interval_pos]: -1.0;
        while (!stack.empty() && depths[stack.back()] >= depth) {
            LengthDbl rectangle_depth = depths[stack.back()];
            stack.pop_back();
            LengthDbl u_1 = (stack.empty())? us[0]: us[stack.back() + 1];
            LengthDbl u_2 = us[interval_pos];
            LengthDbl area = (u_2 - u_1) * rectangle_depth;
            if (area > best_area) {
                best_area = area;
                best_u_1 = u_1;
                best_u_2 = u_2;
                best_depth = rectangle_depth;
            }
        }
        stack.push_back(interval_pos);
    }
    if (!shape::strictly_greater(best_area, 0.0))
        return AxisAlignedBoundingBox();

    // Back to the original coordinates.
    LengthDbl v_1 = v_max - best_depth;
    LengthDbl v_2 = v_max;
    AxisAlignedBoundingBox rectangle;
    switch (side) {
    case BoundingBoxSide::Top:
        rectangle = {best_u_1, best_u_2, v_1, v_2};
        break;
    case BoundingBoxSide::Bottom:
        rectangle = {best_u_1, best_u_2, -v_2, -v_1};
        break;
    case BoundingBoxSide::Right:
        rectangle = {v_1, v_2, best_u_1, best_u_2};
        break;
    case BoundingBoxSide::Left:
        rectangle = {-v_2, -v_1, best_u_1, best_u_2};
        break;
    }
    return rectangle;
}

namespace
{

/** Item type of compute_wrapping_periodic_packings in a given rotation. */
struct OrientedShape
{
    /** Position of the item type in 'item_types'. */
    ItemTypeId item_type_pos;

    ItemTypeRotation rotation;

    ShapeWithHoles shape;
};

/**
 * Oriented shapes of compute_wrapping_periodic_packings and the no-fit
 * polygons of their pairs.
 *
 * NFP(A + a, B + b) = NFP(A, B) + (a - b), so the no-fit polygon of each
 * pair of oriented shapes is computed once, in their natural frame, and
 * shifted for each placement. Besides, as in
 * compute_periodic_packings_for_item_type, NFP(R(A), R(B)) = R(NFP(A, B))
 * for a rotation R, so the no-fit polygon of the item types in rotations
 * (a, m1) and (b, m2) is the one of the item types in rotations (0, m1) and
 * (b - a, m2), rotated by a: it is only computed once for each (item type 1,
 * m1, item type 2, m2, b - a).
 */
class OrientedShapes
{

public:

    OrientedShapes(
            const std::vector<PeriodicPackingItemType>& item_types,
            LengthDbl item_item_minimum_spacing):
        item_types_(item_types),
        item_item_minimum_spacing_(item_item_minimum_spacing) { }

    /** Get the id of an item type in a rotation. */
    int id(
            ItemTypeId item_type_pos,
            const ItemTypeRotation& rotation)
    {
        // The angles are compared normalized, but the rotation is kept as
        // given, since it is given to the items of the periodic packings,
        // and the allowed rotations of the item types are not normalized
        // (see 'ItemType::is_rotation_allowed').
        for (int shape_id = 0; shape_id < (int)shapes_.size(); ++shape_id) {
            const OrientedShape& oriented_shape = shapes_[shape_id];
            if (oriented_shape.item_type_pos == item_type_pos
                    && oriented_shape.rotation.mirror == rotation.mirror
                    && shape::equal(
                        shape::normalize_angle(oriented_shape.rotation.angle),
                        shape::normalize_angle(rotation.angle))) {
                return shape_id;
            }
        }
        OrientedShape oriented_shape;
        oriented_shape.item_type_pos = item_type_pos;
        oriented_shape.rotation = rotation;
        oriented_shape.shape = compute_combined_shape(
                item_types_[item_type_pos].shapes,
                rotation);
        shapes_.push_back(oriented_shape);
        return (int)shapes_.size() - 1;
    }

    const OrientedShape& shape(int shape_id) const { return shapes_[shape_id]; }

    /**
     * Get the no-fit polygon, inflated by the item-item minimum spacing, of
     * two oriented shapes in their natural frame: the translations of the
     * second one for which it is too close to the first one.
     */
    const std::vector<ShapeWithHoles>& no_fit_polygon(
            int shape_id_1,
            int shape_id_2)
    {
        auto it = no_fit_polygons_.find({shape_id_1, shape_id_2});
        if (it != no_fit_polygons_.end())
            return it->second;

        const OrientedShape& oriented_shape_1 = shapes_[shape_id_1];
        const OrientedShape& oriented_shape_2 = shapes_[shape_id_2];
        Angle angle = shape::normalize_angle(
                oriented_shape_2.rotation.angle - oriented_shape_1.rotation.angle);
        const RelativeNoFitPolygon* relative_nfp = nullptr;
        for (const RelativeNoFitPolygon& relative_nfp_cur: relative_no_fit_polygons_) {
            if (relative_nfp_cur.item_type_pos_1 == oriented_shape_1.item_type_pos
                    && relative_nfp_cur.mirror_1 == oriented_shape_1.rotation.mirror
                    && relative_nfp_cur.item_type_pos_2 == oriented_shape_2.item_type_pos
                    && relative_nfp_cur.mirror_2 == oriented_shape_2.rotation.mirror
                    && shape::equal(relative_nfp_cur.angle, angle)) {
                relative_nfp = &relative_nfp_cur;
                break;
            }
        }
        if (relative_nfp == nullptr) {
            RelativeNoFitPolygon relative_nfp_new;
            relative_nfp_new.item_type_pos_1 = oriented_shape_1.item_type_pos;
            relative_nfp_new.mirror_1 = oriented_shape_1.rotation.mirror;
            relative_nfp_new.item_type_pos_2 = oriented_shape_2.item_type_pos;
            relative_nfp_new.mirror_2 = oriented_shape_2.rotation.mirror;
            relative_nfp_new.angle = angle;
            relative_nfp_new.nfp_union = compute_inflated_nfp_union(
                    compute_combined_shape(
                        item_types_[oriented_shape_1.item_type_pos].shapes,
                        {0.0, oriented_shape_1.rotation.mirror}),
                    compute_combined_shape(
                        item_types_[oriented_shape_2.item_type_pos].shapes,
                        {angle, oriented_shape_2.rotation.mirror}),
                    item_item_minimum_spacing_);
            relative_no_fit_polygons_.push_back(std::move(relative_nfp_new));
            relative_nfp = &relative_no_fit_polygons_.back();
        }
        std::vector<ShapeWithHoles> result;
        for (const ShapeWithHoles& swh: relative_nfp->nfp_union) {
            result.push_back((oriented_shape_1.rotation.angle == 0.0)?
                    swh:
                    swh.rotate(oriented_shape_1.rotation.angle));
        }
        return no_fit_polygons_[{shape_id_1, shape_id_2}] = result;
    }

private:

    const std::vector<PeriodicPackingItemType>& item_types_;

    LengthDbl item_item_minimum_spacing_;

    /**
     * No-fit polygon of two item types in rotations (0, mirror_1) and
     * (angle, mirror_2).
     */
    struct RelativeNoFitPolygon
    {
        ItemTypeId item_type_pos_1;
        bool mirror_1;
        ItemTypeId item_type_pos_2;
        bool mirror_2;
        Angle angle;
        std::vector<ShapeWithHoles> nfp_union;
    };

    std::vector<OrientedShape> shapes_;

    /**
     * Relative no-fit polygons computed. A deque, since pointers to its
     * elements are kept while adding new ones.
     */
    std::deque<RelativeNoFitPolygon> relative_no_fit_polygons_;

    std::map<std::pair<int, int>, std::vector<ShapeWithHoles>> no_fit_polygons_;

};

/** Item of a unit cell of compute_wrapping_periodic_packings. */
struct CellItem
{
    int shape_id;

    /** Translation applied to the oriented shape. */
    Point position;
};

/** Base of compute_wrapping_periodic_packings. */
struct WrappingBase
{
    ItemTypeId item_type_pos;

    std::vector<CellItem> items;

    AxisAlignedBoundingBox aabb;

    AreaDbl area = 0.0;
};

/**
 * Sides of an empty side rectangle of a wrapper which are along the
 * wrapper, i.e. not on the bounding box of the wrapper.
 */
struct InteriorSides
{
    bool left = false;
    bool right = false;
    bool bottom = false;
    bool top = false;
};

InteriorSides compute_interior_sides(
        const AxisAlignedBoundingBox& rectangle,
        const AxisAlignedBoundingBox& wrapper_aabb)
{
    InteriorSides interior_sides;
    interior_sides.left = shape::strictly_greater(rectangle.x_min, wrapper_aabb.x_min);
    interior_sides.right = shape::strictly_lesser(rectangle.x_max, wrapper_aabb.x_max);
    interior_sides.bottom = shape::strictly_greater(rectangle.y_min, wrapper_aabb.y_min);
    interior_sides.top = shape::strictly_lesser(rectangle.y_max, wrapper_aabb.y_max);
    return interior_sides;
}

/**
 * Compute the translation of a wrapper such that its empty rectangle
 * contains the base, against the interior sides of the rectangle, at the
 * item-item minimum spacing.
 *
 * The base is then at the minimum spacing of the wrapper: the wrapper is
 * inside its bounding box and outside its empty rectangle, so it is beyond
 * an interior side of the rectangle, while the base is inside the
 * rectangle shrunk by the spacing on its interior sides, and possibly
 * beyond its other sides, which are on the bounding box of the wrapper.
 *
 * Return false if the base doesn't fit between two opposite interior
 * sides.
 */
bool place_wrapper(
        const AxisAlignedBoundingBox& rectangle,
        const InteriorSides& interior_sides,
        const AxisAlignedBoundingBox& base_aabb,
        LengthDbl item_item_minimum_spacing,
        Point& position)
{
    LengthDbl spacing = item_item_minimum_spacing;
    if (interior_sides.left && interior_sides.right
            && shape::strictly_greater(
                base_aabb.x_max - base_aabb.x_min + 2 * spacing,
                rectangle.x_max - rectangle.x_min)) {
        return false;
    }
    if (interior_sides.bottom && interior_sides.top
            && shape::strictly_greater(
                base_aabb.y_max - base_aabb.y_min + 2 * spacing,
                rectangle.y_max - rectangle.y_min)) {
        return false;
    }
    if (interior_sides.left) {
        position.x = base_aabb.x_min - spacing - rectangle.x_min;
    } else if (interior_sides.right) {
        position.x = base_aabb.x_max + spacing - rectangle.x_max;
    } else {
        position.x = base_aabb.x_min - rectangle.x_min;
    }
    if (interior_sides.bottom) {
        position.y = base_aabb.y_min - spacing - rectangle.y_min;
    } else if (interior_sides.top) {
        position.y = base_aabb.y_max + spacing - rectangle.y_max;
    } else {
        position.y = base_aabb.y_min - rectangle.y_min;
    }
    return true;
}

/**
 * Return true iff a point is strictly inside one of a set of shapes.
 */
bool strictly_inside(
        const std::vector<ShapeWithHoles>& shapes,
        const Point& point)
{
    for (const ShapeWithHoles& shape: shapes)
        if (shape.contains(point, true))
            return true;
    return false;
}

/**
 * Find the smallest distance along 'direction' (a unit vector) from 'point'
 * to a point which is not strictly inside 'shapes'.
 *
 * Return +inf if there is none within 'maximum_distance'.
 */
LengthDbl distance_to_outside(
        const std::vector<ShapeWithHoles>& shapes,
        const Point& point,
        const Point& direction,
        LengthDbl maximum_distance)
{
    if (!strictly_inside(shapes, point))
        return 0.0;
    ShapeElement ray = shape::build_line_segment(
            point,
            {point.x + maximum_distance * direction.x,
             point.y + maximum_distance * direction.y});
    std::vector<LengthDbl> distances;
    auto add_point = [&point, &direction, &distances](const Point& intersection)
    {
        distances.push_back(
                (intersection.x - point.x) * direction.x
                + (intersection.y - point.y) * direction.y);
    };
    for (const ShapeWithHoles& shape: shapes) {
        std::vector<const Shape*> boundaries = {&shape.shape};
        for (const Shape& hole: shape.holes)
            boundaries.push_back(&hole);
        for (const Shape* boundary: boundaries) {
            for (const ShapeElement& element: boundary->elements) {
                ShapeElementIntersectionsOutput intersections
                    = shape::compute_intersections(ray, element);
                for (const ShapeElement& part: intersections.overlapping_parts) {
                    add_point(part.start);
                    add_point(part.end);
                }
                for (const Point& intersection: intersections.improper_intersections)
                    add_point(intersection);
                for (const Point& intersection: intersections.proper_intersections)
                    add_point(intersection);
            }
        }
    }
    std::sort(distances.begin(), distances.end());
    for (LengthDbl distance: distances) {
        if (!shape::strictly_greater(distance, 0.0))
            continue;
        Point candidate = {
            point.x + distance * direction.x,
            point.y + distance * direction.y};
        if (!strictly_inside(shapes, candidate))
            return distance;
    }
    return std::numeric_limits<LengthDbl>::infinity();
}

/**
 * Return true iff two periodic packings have the same items, in the same
 * order, at the same relative positions, and the same lattice vectors.
 */
bool equal_up_to_translation(
        const PeriodicItemPacking& packing_1,
        const PeriodicItemPacking& packing_2)
{
    if (packing_1.items.size() != packing_2.items.size())
        return false;
    if (!shape::equal(packing_1.vector_1, packing_2.vector_1)
            || !shape::equal(packing_1.vector_2, packing_2.vector_2)) {
        return false;
    }
    if (packing_1.items.empty())
        return true;
    Point shift = packing_2.items[0].bl_corner - packing_1.items[0].bl_corner;
    for (std::size_t item_pos = 0; item_pos < packing_1.items.size(); ++item_pos) {
        const SolutionItem& item_1 = packing_1.items[item_pos];
        const SolutionItem& item_2 = packing_2.items[item_pos];
        if (item_1.item_type_id != item_2.item_type_id
                || item_1.mirror != item_2.mirror
                || !shape::equal(item_1.angle, item_2.angle)
                || !shape::equal(item_1.bl_corner + shift, item_2.bl_corner)) {
            return false;
        }
    }
    return true;
}

BoundingBoxSide opposite_side(BoundingBoxSide side)
{
    switch (side) {
    case BoundingBoxSide::Bottom:
        return BoundingBoxSide::Top;
    case BoundingBoxSide::Top:
        return BoundingBoxSide::Bottom;
    case BoundingBoxSide::Left:
        return BoundingBoxSide::Right;
    case BoundingBoxSide::Right:
        return BoundingBoxSide::Left;
    }
    return side;
}

AreaDbl rectangle_area(const AxisAlignedBoundingBox& rectangle)
{
    return (rectangle.x_max - rectangle.x_min) * (rectangle.y_max - rectangle.y_min);
}

}

std::vector<PeriodicItemPacking> packingsolver::irregular::compute_wrapping_periodic_packings(
        const std::vector<PeriodicPackingItemType>& item_types,
        const std::vector<PeriodicItemPacking>& periodic_packings,
        LengthDbl item_item_minimum_spacing)
{
    // Minimum ratio between the area of the empty side rectangle of a
    // wrapper and the area of its bounding box.
    const double minimum_wrapper_ratio = 0.5;
    // Maximum number of bases of each item type tried for each wrapper
    // rectangle, those filling the rectangle the most.
    const int maximum_number_of_bases = 1;
    // Maximum number of periodic packings returned for each pair of item
    // types (wrapper, base), the densest ones.
    const int maximum_number_of_packings = 4;

    // Periodic packings found, with their density, for each pair of item
    // types (wrapper, base).
    std::map<std::pair<ItemTypeId, ItemTypeId>, std::vector<std::pair<double, PeriodicItemPacking>>> packings;
    OrientedShapes oriented_shapes(item_types, item_item_minimum_spacing);

    // Bases: the single items and the unit cells of the periodic packings of
    // a single item type.
    std::vector<WrappingBase> bases;
    for (ItemTypeId item_type_pos = 0;
            item_type_pos < (ItemTypeId)item_types.size();
            ++item_type_pos) {
        for (const ItemTypeRotation& rotation: item_types[item_type_pos].rotations) {
            WrappingBase base;
            base.item_type_pos = item_type_pos;
            base.items.push_back({oriented_shapes.id(item_type_pos, rotation), {0.0, 0.0}});
            bases.push_back(base);
        }
    }
    for (const PeriodicItemPacking& packing: periodic_packings) {
        if (packing.items.empty() || !periodic_packing_has_single_item_type(packing))
            continue;
        ItemTypeId item_type_pos = -1;
        for (ItemTypeId pos = 0; pos < (ItemTypeId)item_types.size(); ++pos)
            if (item_types[pos].item_type_id == packing.items[0].item_type_id)
                item_type_pos = pos;
        if (item_type_pos == -1)
            continue;
        WrappingBase base;
        base.item_type_pos = item_type_pos;
        for (const SolutionItem& item: packing.items) {
            base.items.push_back({
                    oriented_shapes.id(item_type_pos, {item.angle, item.mirror}),
                    item.bl_corner});
        }
        bases.push_back(base);
    }
    // Area of the item types, computed once so that it is the same for all
    // their rotations.
    std::vector<AreaDbl> item_type_areas(item_types.size(), 0.0);
    for (ItemTypeId item_type_pos = 0;
            item_type_pos < (ItemTypeId)item_types.size();
            ++item_type_pos) {
        for (const ShapeWithHoles& shape: item_types[item_type_pos].shapes)
            item_type_areas[item_type_pos] += shape.compute_area();
    }
    for (WrappingBase& base: bases) {
        for (const CellItem& item: base.items) {
            const OrientedShape& oriented_shape = oriented_shapes.shape(item.shape_id);
            AxisAlignedBoundingBox aabb = oriented_shape.shape.compute_min_max();
            aabb.shift(item.position);
            base.aabb = shape::merge(base.aabb, aabb);
            base.area += item_type_areas[oriented_shape.item_type_pos];
        }
    }

    const std::vector<BoundingBoxSide> sides = {
        BoundingBoxSide::Bottom,
        BoundingBoxSide::Top,
        BoundingBoxSide::Left,
        BoundingBoxSide::Right};

    for (ItemTypeId wrapper_pos = 0;
            wrapper_pos < (ItemTypeId)item_types.size();
            ++wrapper_pos) {
        const PeriodicPackingItemType& wrapper_item_type = item_types[wrapper_pos];
        for (const ItemTypeRotation& rotation_1: wrapper_item_type.rotations) {
            // The rotation r + 180 of the second wrapper, as given in the
            // allowed rotations.
            const ItemTypeRotation* rotation_2 = nullptr;
            for (const ItemTypeRotation& rotation: wrapper_item_type.rotations) {
                if (rotation.mirror == rotation_1.mirror
                        && shape::equal(
                            shape::normalize_angle(rotation.angle),
                            shape::normalize_angle(rotation_1.angle + 180.0))) {
                    rotation_2 = &rotation;
                    break;
                }
            }
            if (rotation_2 == nullptr)
                continue;
            int wrapper_1_id = oriented_shapes.id(wrapper_pos, rotation_1);
            int wrapper_2_id = oriented_shapes.id(wrapper_pos, *rotation_2);
            const ShapeWithHoles& wrapper_1 = oriented_shapes.shape(wrapper_1_id).shape;
            const ShapeWithHoles& wrapper_2 = oriented_shapes.shape(wrapper_2_id).shape;
            AxisAlignedBoundingBox wrapper_1_aabb = wrapper_1.compute_min_max();
            AxisAlignedBoundingBox wrapper_2_aabb = wrapper_2.compute_min_max();
            LengthDbl segment_length = (std::max)(
                    wrapper_1_aabb.x_max - wrapper_1_aabb.x_min,
                    wrapper_1_aabb.y_max - wrapper_1_aabb.y_min) / 128;

            std::vector<AxisAlignedBoundingBox> rectangles_1;
            for (BoundingBoxSide side: sides) {
                AxisAlignedBoundingBox rectangle_1 = compute_largest_empty_side_rectangle(
                        wrapper_1, side, segment_length);
                if (rectangle_1.x_min > rectangle_1.x_max
                        || rectangle_area(rectangle_1)
                        < minimum_wrapper_ratio * rectangle_area(wrapper_1_aabb)) {
                    continue;
                }
                // The same rectangle may touch several sides.
                bool duplicate = false;
                for (const AxisAlignedBoundingBox& previous_rectangle: rectangles_1) {
                    if (shape::equal(previous_rectangle.x_min, rectangle_1.x_min)
                            && shape::equal(previous_rectangle.x_max, rectangle_1.x_max)
                            && shape::equal(previous_rectangle.y_min, rectangle_1.y_min)
                            && shape::equal(previous_rectangle.y_max, rectangle_1.y_max)) {
                        duplicate = true;
                    }
                }
                if (duplicate)
                    continue;
                rectangles_1.push_back(rectangle_1);
                AxisAlignedBoundingBox rectangle_2 = compute_largest_empty_side_rectangle(
                        wrapper_2, opposite_side(side), segment_length);
                if (rectangle_2.x_min > rectangle_2.x_max)
                    continue;
                InteriorSides interior_sides_1 = compute_interior_sides(rectangle_1, wrapper_1_aabb);
                InteriorSides interior_sides_2 = compute_interior_sides(rectangle_2, wrapper_2_aabb);

                // Bases fitting in the rectangle, filling the most the
                // bounding box of the rectangle and of the base first, and
                // then the most compact ones first.
                std::vector<std::pair<std::pair<double, double>, int>> sorted_bases;
                for (int base_pos = 0; base_pos < (int)bases.size(); ++base_pos) {
                    const WrappingBase& base = bases[base_pos];
                    if (base.item_type_pos == wrapper_pos)
                        continue;
                    if (!wrapper_item_type.compute
                            && !item_types[base.item_type_pos].compute) {
                        continue;
                    }
                    Point position;
                    if (!place_wrapper(rectangle_1, interior_sides_1, base.aabb, item_item_minimum_spacing, position)
                            || !place_wrapper(rectangle_2, interior_sides_2, base.aabb, item_item_minimum_spacing, position)) {
                        continue;
                    }
                    Point position_1;
                    place_wrapper(rectangle_1, interior_sides_1, base.aabb, item_item_minimum_spacing, position_1);
                    AxisAlignedBoundingBox placed_rectangle_1 = rectangle_1;
                    placed_rectangle_1.shift(position_1);
                    double fill = base.area / rectangle_area(shape::merge(placed_rectangle_1, base.aabb));
                    sorted_bases.push_back({{-fill, rectangle_area(base.aabb)}, base_pos});
                }
                std::sort(sorted_bases.begin(), sorted_bases.end());
                // Keep the best bases of each item type: the bases of an item
                // type filling the rectangle less than the bases of another
                // item type are still useful, e.g. when there aren't enough
                // copies of the other item type.
                {
                    std::vector<int> number_of_bases(item_types.size(), 0);
                    std::vector<std::pair<std::pair<double, double>, int>> kept_bases;
                    for (const auto& sorted_base: sorted_bases) {
                        ItemTypeId item_type_pos = bases[sorted_base.second].item_type_pos;
                        if (number_of_bases[item_type_pos] >= maximum_number_of_bases)
                            continue;
                        number_of_bases[item_type_pos]++;
                        kept_bases.push_back(sorted_base);
                    }
                    sorted_bases.swap(kept_bases);
                }

                for (const auto& sorted_base: sorted_bases) {
                    const WrappingBase& base = bases[sorted_base.second];
                    Point position_1;
                    Point position_2;
                    place_wrapper(rectangle_1, interior_sides_1, base.aabb, item_item_minimum_spacing, position_1);
                    place_wrapper(rectangle_2, interior_sides_2, base.aabb, item_item_minimum_spacing, position_2);

                    // Translations of the second wrapper for which it is
                    // too close to the first wrapper or to an item of the
                    // base.
                    std::vector<ShapeWithHoles> forbidden_positions_2
                        = oriented_shapes.no_fit_polygon(wrapper_1_id, wrapper_2_id);
                    for (ShapeWithHoles& swh: forbidden_positions_2)
                        swh.shift(position_1.x, position_1.y);
                    for (const CellItem& base_item: base.items) {
                        for (ShapeWithHoles swh: oriented_shapes.no_fit_polygon(base_item.shape_id, wrapper_2_id)) {
                            swh.shift(base_item.position.x, base_item.position.y);
                            forbidden_positions_2.push_back(std::move(swh));
                        }
                    }

                    // Move the second wrapper away from the base, along the
                    // interior sides of its rectangle, if it is too close to
                    // the first wrapper.
                    std::vector<Point> directions;
                    if (interior_sides_2.left)
                        directions.push_back({-1.0, 0.0});
                    if (interior_sides_2.right)
                        directions.push_back({1.0, 0.0});
                    if (interior_sides_2.bottom)
                        directions.push_back({0.0, -1.0});
                    if (interior_sides_2.top)
                        directions.push_back({0.0, 1.0});
                    LengthDbl maximum_distance = 2 * (
                            wrapper_2_aabb.x_max - wrapper_2_aabb.x_min
                            + wrapper_2_aabb.y_max - wrapper_2_aabb.y_min);
                    std::vector<Point> positions_2;
                    auto add_position_2 = [&](const Point& position)
                    {
                        auto add = [&positions_2](const Point& position_new)
                        {
                            for (const Point& position_cur: positions_2)
                                if (shape::equal(position_cur, position_new))
                                    return;
                            positions_2.push_back(position_new);
                        };
                        if (!strictly_inside(forbidden_positions_2, position)) {
                            add(position);
                            return;
                        }
                        for (const Point& direction: directions) {
                            LengthDbl distance = distance_to_outside(
                                    forbidden_positions_2, position, direction, maximum_distance);
                            if (distance == std::numeric_limits<LengthDbl>::infinity())
                                continue;
                            add({
                                    position.x + distance * direction.x,
                                    position.y + distance * direction.y});
                        }
                    };
                    add_position_2(position_2);
                    // Positions aligning a side of the bounding box of the
                    // second wrapper with the same side of the bounding box
                    // of the first wrapper: the second wrapper may have some
                    // slack around the base, and placing it against the base
                    // may make one wrapper stick out of the other one,
                    // making the bounding box of the unit cell larger than
                    // needed (e.g. two L-shapes forming a rectangle).
                    AxisAlignedBoundingBox placed_wrapper_1_aabb = wrapper_1_aabb;
                    placed_wrapper_1_aabb.shift(position_1);
                    add_position_2({
                            position_2.x,
                            placed_wrapper_1_aabb.y_min - wrapper_2_aabb.y_min});
                    add_position_2({
                            position_2.x,
                            placed_wrapper_1_aabb.y_max - wrapper_2_aabb.y_max});
                    add_position_2({
                            placed_wrapper_1_aabb.x_min - wrapper_2_aabb.x_min,
                            position_2.y});
                    add_position_2({
                            placed_wrapper_1_aabb.x_max - wrapper_2_aabb.x_max,
                            position_2.y});

                    for (const Point& position_2_moved: positions_2) {
                        std::vector<CellItem> cell_items = base.items;
                        cell_items.push_back({wrapper_1_id, position_1});
                        cell_items.push_back({wrapper_2_id, position_2_moved});

                        std::vector<ShapeWithHoles> placed_shapes;
                        for (const CellItem& item: cell_items) {
                            ShapeWithHoles shape = oriented_shapes.shape(item.shape_id).shape;
                            shape.shift(item.position.x, item.position.y);
                            placed_shapes.push_back(std::move(shape));
                        }
                        std::vector<ShapeWithHoles> combined_nfp;
                        for (const CellItem& item_1: cell_items) {
                            for (const CellItem& item_2: cell_items) {
                                for (ShapeWithHoles swh: oriented_shapes.no_fit_polygon(item_1.shape_id, item_2.shape_id)) {
                                    swh.shift(
                                            item_1.position.x - item_2.position.x,
                                            item_1.position.y - item_2.position.y);
                                    combined_nfp.push_back(std::move(swh));
                                }
                            }
                        }

                        for (const PeriodicPacking& periodic_packing:
                                find_periodic_packing_lattice_from_combined_nfp(placed_shapes, combined_nfp)) {
                            PeriodicItemPacking item_packing;
                            item_packing.vector_1 = periodic_packing.vector_1;
                            item_packing.vector_2 = periodic_packing.vector_2;
                            for (int item_pos = 0; item_pos < (int)cell_items.size(); ++item_pos) {
                                const CellItem& cell_item = cell_items[item_pos];
                                const OrientedShape& oriented_shape = oriented_shapes.shape(cell_item.shape_id);
                                SolutionItem item;
                                item.item_type_id = item_types[oriented_shape.item_type_pos].item_type_id;
                                item.bl_corner = cell_item.position;
                                item.angle = oriented_shape.rotation.angle;
                                item.mirror = oriented_shape.rotation.mirror;
                                item_packing.items.push_back(item);
                                item_packing.aabb_scaled = shape::merge(
                                        item_packing.aabb_scaled,
                                        placed_shapes[item_pos].compute_min_max());
                            }
                            AreaDbl cell_area = std::abs(
                                    item_packing.vector_1.x * item_packing.vector_2.y
                                    - item_packing.vector_1.y * item_packing.vector_2.x);
                            double density = (base.area
                                    + 2 * item_type_areas[wrapper_pos]) / cell_area;
                            std::vector<std::pair<double, PeriodicItemPacking>>& pair_packings
                                = packings[{wrapper_pos, base.item_type_pos}];
                            // Periodic packings with the same density and the
                            // same unit cell dimensions (e.g. mirrored ones)
                            // give the same periodic blocks: only one of them
                            // is kept, so that they don't take the place of
                            // different periodic packings among the ones kept
                            // (e.g. a less dense periodic packing whose unit
                            // cell is an exact rectangle).
                            bool duplicate = false;
                            for (const auto& previous: pair_packings) {
                                const AxisAlignedBoundingBox& previous_aabb = previous.second.aabb_scaled;
                                if (equal_up_to_translation(previous.second, item_packing)
                                        || (shape::equal(previous.first, density)
                                            && shape::equal(
                                                previous_aabb.x_max - previous_aabb.x_min,
                                                item_packing.aabb_scaled.x_max - item_packing.aabb_scaled.x_min)
                                            && shape::equal(
                                                previous_aabb.y_max - previous_aabb.y_min,
                                                item_packing.aabb_scaled.y_max - item_packing.aabb_scaled.y_min))) {
                                    duplicate = true;
                                    break;
                                }
                            }
                            if (!duplicate)
                                pair_packings.push_back({density, item_packing});
                        }
                    }
                }
            }
        }
    }

    std::vector<PeriodicItemPacking> output;
    for (auto& pair_packings: packings) {
        // Densest first, and then those with the smallest unit cell, since
        // the bounding box of the unit cell is lost at the borders of a
        // block.
        std::stable_sort(
                pair_packings.second.begin(),
                pair_packings.second.end(),
                [](const std::pair<double, PeriodicItemPacking>& packing_1,
                    const std::pair<double, PeriodicItemPacking>& packing_2)
                {
                    if (!shape::equal(packing_1.first, packing_2.first))
                        return packing_1.first > packing_2.first;
                    return rectangle_area(packing_1.second.aabb_scaled)
                        < rectangle_area(packing_2.second.aabb_scaled);
                });
        for (int packing_pos = 0;
                packing_pos < (int)pair_packings.second.size()
                && packing_pos < maximum_number_of_packings;
                ++packing_pos) {
            output.push_back(pair_packings.second[packing_pos].second);
        }
    }
    return output;
}
