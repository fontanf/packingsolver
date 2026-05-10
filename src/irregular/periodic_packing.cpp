#include "irregular/periodic_packing.hpp"

#include "shape/no_fit_polygon.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/shapes_intersections.hpp"

#include <limits>

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
 * Get the combined shape of an item type for a given rotation.
 *
 * Applies mirror (if requested) then rotation angle to each sub-shape, then
 * returns their union. If the item has a single sub-shape, returns it directly.
 */
ShapeWithHoles get_item_combined_shape(
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
    std::vector<ShapeWithHoles> union_result = shape::compute_union(shapes);
    if (union_result.empty())
        return shapes[0];
    return union_result[0];
}

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

    // Build the base items (each shape shifted by its position in the cell).
    std::vector<ShapeWithHoles> base_items;
    for (int item_idx = 0; item_idx < n_items; ++item_idx) {
        ShapeWithHoles s = item_shapes[item_idx];
        s.shift(item_positions[item_idx].x, item_positions[item_idx].y);
        base_items.push_back(s);
    }

    // Check items within the same cell.
    for (int item_idx = 0; item_idx < n_items; ++item_idx) {
        for (int other_idx = item_idx + 1; other_idx < n_items; ++other_idx) {
            if (shape::intersect(base_items[item_idx], base_items[other_idx], true))
                return false;
        }
    }

    // For each non-zero offset within the neighbourhood, check against base.
    for (int n = -check_range; n <= check_range; ++n) {
        for (int m = -check_range; m <= check_range; ++m) {
            if (n == 0 && m == 0)
                continue;
            Point offset = {
                n * vector_1.x + m * vector_2.x,
                n * vector_1.y + m * vector_2.y,
            };
            for (int item_idx = 0; item_idx < n_items; ++item_idx) {
                ShapeWithHoles copy = item_shapes[item_idx];
                copy.shift(
                    offset.x + item_positions[item_idx].x,
                    offset.y + item_positions[item_idx].y);
                for (int base_idx = 0; base_idx < n_items; ++base_idx) {
                    if (shape::intersect(base_items[base_idx], copy, true))
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
 */
std::vector<PeriodicPacking> find_periodic_packing_lattice(
        const std::vector<ShapeWithHoles>& shapes)
{
    int n = (int)shapes.size();
    std::vector<Point> zero_positions(n, {0.0, 0.0});

    std::vector<ShapeWithHoles> combined_nfp;
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            std::vector<ShapeWithHoles> nfp_ij = shape::no_fit_polygon(shapes[i], shapes[j]);
            if (nfp_ij.empty())
                continue;
            for (const ShapeWithHoles& swh: shape::compute_union(nfp_ij))
                combined_nfp.push_back(swh);
        }
    }
    if (combined_nfp.empty())
        return {};
    std::vector<ShapeWithHoles> combined_nfp_union = shape::compute_union(combined_nfp);

    std::vector<PeriodicPacking> result;

    // Horizontal strategy: v1 = (w, 0).
    {
        Point vector_1 = find_leftmost_y0_point(combined_nfp_union);
        if (shape::strictly_greater(vector_1.x, 0.0)
                && vector_1.x != std::numeric_limits<double>::infinity()) {
            std::vector<ShapeWithHoles> ext_nfp = combined_nfp_union;
            std::vector<ShapeWithHoles> shifted = combined_nfp_union;
            for (ShapeWithHoles& swh: shifted)
                swh.shift(vector_1.x, vector_1.y);
            ext_nfp.insert(ext_nfp.end(), shifted.begin(), shifted.end());
            std::vector<ShapeWithHoles> ext_nfp_union = shape::compute_union(ext_nfp);
            Point vector_2 = find_bottommost_constrained_point(ext_nfp_union, vector_1.x);
            if (shape::strictly_greater(vector_2.y, 0.0)
                    && check_periodic_packing(shapes, zero_positions, vector_1, vector_2, 3)) {
                result.push_back({zero_positions, vector_1, vector_2});
            }
        }
    }

    // Vertical strategy: v1 = (0, h).
    {
        Point vector_1 = find_bottommost_x0_point(combined_nfp_union);
        if (shape::strictly_greater(vector_1.y, 0.0)
                && vector_1.y != std::numeric_limits<double>::infinity()) {
            std::vector<ShapeWithHoles> ext_nfp = combined_nfp_union;
            std::vector<ShapeWithHoles> shifted = combined_nfp_union;
            for (ShapeWithHoles& swh: shifted)
                swh.shift(vector_1.x, vector_1.y);
            ext_nfp.insert(ext_nfp.end(), shifted.begin(), shifted.end());
            std::vector<ShapeWithHoles> ext_nfp_union = shape::compute_union(ext_nfp);
            Point vector_2 = find_leftmost_constrained_point(ext_nfp_union, vector_1.y);
            if (shape::strictly_greater(vector_2.x, 0.0)
                    && check_periodic_packing(shapes, zero_positions, vector_1, vector_2, 3)) {
                result.push_back({zero_positions, vector_1, vector_2});
            }
        }
    }

    return result;
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
    ShapeWithHoles shifted_shape_0 = shape_0;
    shifted_shape_0.shift(position_0.x, position_0.y);

    AxisAlignedBoundingBox aabb_r = shape_r.compute_min_max();
    Point min_position_r = {-aabb_r.x_min, -aabb_r.y_min};

    // NFP(shape_0, shape_r) shifted by position_0 gives translations t_r such
    // that (shape_r + t_r) just touches shifted_shape_0.
    std::vector<ShapeWithHoles> nfp_0r = shape::no_fit_polygon(shape_0, shape_r);
    if (nfp_0r.empty())
        return {};
    std::vector<ShapeWithHoles> nfp_0r_union = shape::compute_union(nfp_0r);
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

    std::vector<PeriodicPacking> result;

    for (const Point& t_r: unique_candidates) {
        // Skip placements where shape_r's AABB goes below x=0 or y=0.
        if (shape::strictly_lesser(t_r.x, min_position_r.x)
                || shape::strictly_lesser(t_r.y, min_position_r.y))
            continue;

        ShapeWithHoles placed_shape_r = shape_r;
        placed_shape_r.shift(t_r.x, t_r.y);

        for (PeriodicPacking pp: find_periodic_packing_lattice({shifted_shape_0, placed_shape_r})) {
            pp.positions = {position_0, t_r};
            add_if_unique(result, std::move(pp));
        }
    }

    return result;
}

std::vector<PeriodicItemPacking> packingsolver::irregular::compute_periodic_packings(
        const Instance& instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_type_rotations)
{
    std::vector<PeriodicItemPacking> output;

    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const std::vector<ItemTypeRotation>& rotations = item_type_rotations[item_type_id];

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
                item_packing.aabb_scaled = merge(
                    item_packing.aabb_scaled, shape_0.compute_min_max());
                output.push_back(item_packing);
            }

            for (int rot_r_pos = rot_0_pos + 1;
                    rot_r_pos < (int)rotations.size();
                    ++rot_r_pos) {
                const ItemTypeRotation& rot_r = rotations[rot_r_pos];
                ShapeWithHoles shape_r = get_item_combined_shape(instance, item_type_id, rot_r);

                for (const PeriodicPacking& pp_two: compute_periodic_packings(shape_0, shape_r)) {
                    PeriodicItemPacking item_packing;
                    item_packing.vector_1 = pp_two.vector_1;
                    item_packing.vector_2 = pp_two.vector_2;
                    SolutionItem item_0;
                    item_0.item_type_id = item_type_id;
                    item_0.bl_corner = pp_two.positions[0];
                    item_0.angle = rot_0.angle;
                    item_0.mirror = rot_0.mirror;
                    item_packing.items.push_back(item_0);
                    item_packing.aabb_scaled = merge(
                        item_packing.aabb_scaled, shape_0.compute_min_max());
                    SolutionItem item_r;
                    item_r.item_type_id = item_type_id;
                    item_r.bl_corner = pp_two.positions[1];
                    item_r.angle = rot_r.angle;
                    item_r.mirror = rot_r.mirror;
                    item_packing.items.push_back(item_r);
                    AxisAlignedBoundingBox bb_r = shape_r.compute_min_max();
                    bb_r.shift(pp_two.positions[1]);
                    item_packing.aabb_scaled = merge(item_packing.aabb_scaled, bb_r);
                    output.push_back(item_packing);
                }
            }
        }
    }

    return output;
}
