#include "irregular/periodic_packing.hpp"

#include "shape/no_fit_polygon.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/shapes_intersections.hpp"

#include <limits>
#include <algorithm>
#include <cmath>

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

ShapeWithHoles packingsolver::irregular::get_item_combined_shape(
        const Instance& instance,
        ItemTypeId item_type_id,
        const ItemTypeRotation& rotation)
{
    const ItemType& item_type = instance.item_type(item_type_id);
    std::vector<ShapeWithHoles> shapes;
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)item_type.shapes.size();
            ++item_shape_pos) {
        shapes.push_back(instance.item_shape_scaled(
            item_type_id, item_shape_pos, rotation.angle, rotation.mirror));
    }
    if ((ItemShapePos)shapes.size() == 1)
        return shapes[0];
    MultiShapeWithHoles union_result = shape::compute_union(shapes);
    if (union_result.shapes_with_holes.empty())
        return shapes[0];
    return union_result.shapes_with_holes[0];
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

    // Check items within the same cell.
    for (int item_idx = 0; item_idx < n_items; ++item_idx) {
        for (int other_idx = item_idx + 1; other_idx < n_items; ++other_idx) {
            if (shape::intersect(base_items[item_idx], base_items[other_idx], true))
                return false;
        }
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
 * region already assembled by the caller (either find_periodic_packing_lattice
 * below, which computes it from scratch, or
 * find_periodic_packing_lattice_two_shapes_cached, which derives it cheaply
 * from precomputed NFPs), search the two strategies and validate each
 * candidate against the actual shapes.
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
        }
    }

    return result;
}

std::vector<PeriodicPacking> find_periodic_packing_lattice(
        const std::vector<ShapeWithHoles>& shapes)
{
    int n = (int)shapes.size();
    std::vector<ShapeWithHoles> combined_nfp;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            MultiShapeWithHoles nfp_ij = shape::no_fit_polygon(shapes[i], shapes[j]);
            if (nfp_ij.shapes_with_holes.empty())
                continue;
            for (const ShapeWithHoles& swh: shape::compute_union(nfp_ij.shapes_with_holes).shapes_with_holes)
                combined_nfp.push_back(swh);
        }
    }
    return find_periodic_packing_lattice_from_combined_nfp(shapes, combined_nfp);
}

/**
 * Same as find_periodic_packing_lattice, specialized for exactly two shapes,
 * given the four pairwise NFPs already computed in each shape's own natural
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

}  // namespace

std::vector<PeriodicPacking> packingsolver::irregular::compute_periodic_packings(
        const ShapeWithHoles& shape)
{
    AxisAlignedBoundingBox aabb = shape.compute_min_max();
    Point position = {-aabb.x_min, -aabb.y_min};
    ShapeWithHoles shifted = shape;
    shifted.shift(position.x, position.y);
    std::vector<PeriodicPacking> result;
    for (PeriodicPacking pp: find_periodic_packing_lattice({shifted})) {
        pp.positions = {position};
        add_if_unique(result, std::move(pp));
    }
    return result;
}

/**
 * Compute periodic packings for two shapes with different rotations.
 *
 * shape_0 is pre-shifted to have its BL corner at the origin. For each
 * boundary vertex of NFP(shape_0, shape_r) that places shape_r with a
 * non-negative AABB (x_min >= 0 and y_min >= 0), the two shapes are passed
 * to find_periodic_packing_lattice to search for horizontal and vertical
 * lattice vectors.
 *
 * Returns all valid PeriodicPackings found across all candidate placements,
 * each with positions = {position_0, t_r} where t_r is the translation
 * applied to the natural shape_r.
 */
std::vector<PeriodicPacking> packingsolver::irregular::compute_periodic_packings(
        const ShapeWithHoles& shape_0,
        const ShapeWithHoles& shape_r)
{
    AxisAlignedBoundingBox aabb_0 = shape_0.compute_min_max();
    Point position_0 = {-aabb_0.x_min, -aabb_0.y_min};

    AxisAlignedBoundingBox aabb_r = shape_r.compute_min_max();
    Point min_position_r = {-aabb_r.x_min, -aabb_r.y_min};

    // NFP(shape_0, shape_r) shifted by position_0 gives translations t_r such
    // that (shape_r + t_r) just touches (shape_0 shifted by position_0).
    MultiShapeWithHoles nfp_0r = shape::no_fit_polygon(shape_0, shape_r);
    if (nfp_0r.shapes_with_holes.empty())
        return {};
    // Natural (position-independent) frame, reused below for every candidate
    // instead of recomputing no_fit_polygon(shape_0, shape_r) once per
    // candidate: NFP(A + a, B + b) = NFP(A, B) + (a - b), so only the
    // relative offset changes, never the NFP shape itself.
    std::vector<ShapeWithHoles> nfp_0r_union_natural =
        shape::compute_union(nfp_0r.shapes_with_holes).shapes_with_holes;
    std::vector<ShapeWithHoles> nfp_0r_union = nfp_0r_union_natural;
    for (ShapeWithHoles& swh: nfp_0r_union) swh.shift(position_0.x, position_0.y);

    // The other three pairwise NFPs needed by find_periodic_packing_lattice
    // for a two-shape placement are likewise position-independent, so
    // compute each once here rather than once per candidate below.
    std::vector<ShapeWithHoles> nfp_00_union = shape::compute_union(
            shape::no_fit_polygon(shape_0, shape_0).shapes_with_holes).shapes_with_holes;
    std::vector<ShapeWithHoles> nfp_r0_union = shape::compute_union(
            shape::no_fit_polygon(shape_r, shape_0).shapes_with_holes).shapes_with_holes;
    std::vector<ShapeWithHoles> nfp_rr_union = shape::compute_union(
            shape::no_fit_polygon(shape_r, shape_r).shapes_with_holes).shapes_with_holes;

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
        // Skip placements where shape_r's AABB goes below x=0 or y=0.
        if (shape::strictly_lesser(t_r.x, min_position_r.x)
                || shape::strictly_lesser(t_r.y, min_position_r.y))
            continue;

        AxisAlignedBoundingBox aabb_r_here = aabb_r;
        aabb_r_here.shift(t_r.x, t_r.y);
        AxisAlignedBoundingBox combined = merge(aabb_0, aabb_r_here);
        double dx = combined.x_max - combined.x_min;
        double dy = combined.y_max - combined.y_min;
        valid_candidates.push_back({t_r, dx, dy, dx * dy});
    }

    std::vector<int> selected_candidates;
    if (!valid_candidates.empty()) {
        auto add_argmin = [&](auto metric) {
            int best = 0;
            for (int i = 1; i < (int)valid_candidates.size(); ++i)
                if (metric(valid_candidates[i]) < metric(valid_candidates[best]))
                    best = i;
            if (std::find(selected_candidates.begin(), selected_candidates.end(), best)
                    == selected_candidates.end())
                selected_candidates.push_back(best);
        };
        add_argmin([](const Candidate& c) { return c.dx; });
        add_argmin([](const Candidate& c) { return c.dy; });
        add_argmin([](const Candidate& c) { return c.area; });
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

std::vector<PeriodicItemPacking> packingsolver::irregular::compute_periodic_packings_for_item_type(
        const Instance& instance,
        ItemTypeId item_type_id,
        const std::vector<ItemTypeRotation>& rotations)
{
    std::vector<PeriodicItemPacking> output;

    for (int rot_0_pos = 0;
            rot_0_pos < (int)rotations.size();
            ++rot_0_pos) {
        const ItemTypeRotation& rot_0 = rotations[rot_0_pos];
        ShapeWithHoles shape_0 = get_item_combined_shape(instance, item_type_id, rot_0);

        for (const PeriodicPacking& pp_same: compute_periodic_packings(shape_0)) {
            PeriodicItemPacking item_packing;
            item_packing.vector_1 = pp_same.vector_1;
            item_packing.vector_2 = pp_same.vector_2;
            SolutionItem item;
            item.item_type_id = item_type_id;
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
                && instance.item_type(item_type_id).is_rotation_allowed(
                    rot_0.angle + 180.0, rot_0.mirror)) {
            ItemTypeRotation rot_r{rot_0.angle + 180.0, rot_0.mirror};
            ShapeWithHoles shape_r = get_item_combined_shape(instance, item_type_id, rot_r);

            auto add_two_shape_packings = [&](
                    const ItemTypeRotation& rotation_first,
                    const ShapeWithHoles& shape_first,
                    const ItemTypeRotation& rotation_second,
                    const ShapeWithHoles& shape_second)
            {
                for (const PeriodicPacking& pp_two:
                        compute_periodic_packings(shape_first, shape_second)) {
                    PeriodicItemPacking item_packing;
                    item_packing.vector_1 = pp_two.vector_1;
                    item_packing.vector_2 = pp_two.vector_2;
                    SolutionItem item_first;
                    item_first.item_type_id = item_type_id;
                    item_first.bl_corner = pp_two.positions[0];
                    item_first.angle = rotation_first.angle;
                    item_first.mirror = rotation_first.mirror;
                    item_packing.items.push_back(item_first);
                    AxisAlignedBoundingBox bb_first = shape_first.compute_min_max();
                    bb_first.shift(pp_two.positions[0]);
                    item_packing.aabb_scaled = merge(item_packing.aabb_scaled, bb_first);
                    SolutionItem item_second;
                    item_second.item_type_id = item_type_id;
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

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        std::vector<PeriodicItemPacking> item_type_output
            = compute_periodic_packings_for_item_type(
                    instance, item_type_id, item_type_rotations[item_type_id]);
        output.insert(
                output.end(),
                std::make_move_iterator(item_type_output.begin()),
                std::make_move_iterator(item_type_output.end()));
    }

    return output;
}
