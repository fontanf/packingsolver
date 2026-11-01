#include "irregular/milp_nfp.hpp"

#include "irregular/solution_builder.hpp"

#include "shape/no_fit_polygon.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/approximation.hpp"
#include "shape/convex_partition.hpp"

#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>
#include <algorithm>
#include <functional>
#include <map>
#include <tuple>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/** Item type in one of its rotations. */
struct ItemTypeRotationData
{
    /** Shapes of the item, whose reference point is the origin. */
    std::vector<ShapeWithHoles> shapes;

    /** Angle and mirroring of the rotation. */
    Angle angle;
    bool mirror;

    /** Bounding box of the item. */
    AxisAlignedBoundingBox aabb;

    /** Area of the item. */
    AreaDbl area = 0;

    /**
     * Inner-fit rectangle: positions of the reference point for which the
     * bounding box of the item is in the bounding box of the bin.
     */
    LengthDbl x_min;
    LengthDbl x_max;
    LengthDbl y_min;
    LengthDbl y_max;
};

/** Constraint 'nx * dx + ny * dy >= c' on a relative position d. */
struct HalfPlane
{
    LengthDbl nx;
    LengthDbl ny;
    LengthDbl c;
};

/** Convex piece of the feasible relative positions of a pair of items. */
struct Piece
{
    /** Half-planes of the edges. */
    std::vector<HalfPlane> half_planes;

    /** Vertices of the piece (two for a line segment, one for a point). */
    std::vector<Point> vertices;

    /** Bounding box of the relative positions of the piece. */
    LengthDbl x_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_min = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();
};

/** Bin type, and item type and rotation of each item of a pair. */
using MilpNfpPairKey = std::tuple<BinTypeId, ItemTypeId, Counter, ItemTypeId, Counter>;

/**
 * Bin type, and sorted item types and rotations of the items of a rotation
 * combination.
 */
using MilpNfpCombinationKey = std::pair<BinTypeId, std::vector<std::pair<ItemTypeId, Counter>>>;

struct PairData
{
    /** Bounds of the relative position d = p_2 - p_1: R = IFP_2 - IFP_1. */
    LengthDbl x_min;
    LengthDbl x_max;
    LengthDbl y_min;
    LengthDbl y_max;

    /** 'false' iff the two items can't overlap, whatever their positions. */
    bool interact = true;

    /**
     * Convex pieces of the complement of int(NFP) intersecting R, including
     * the feasible line segments and points of null area inside the NFP.
     */
    std::vector<Piece> pieces;
};

/**
 * Return 'true' iff two lists of shapes are equal up to a translation (the
 * one moving the minimum of their bounding boxes to the origin).
 */
bool equal_up_to_translation(
        const std::vector<ShapeWithHoles>& shapes_1,
        const AxisAlignedBoundingBox& aabb_1,
        const std::vector<ShapeWithHoles>& shapes_2,
        const AxisAlignedBoundingBox& aabb_2)
{
    if (shapes_1.size() != shapes_2.size())
        return false;
    for (std::size_t shape_pos = 0; shape_pos < shapes_1.size(); ++shape_pos) {
        ShapeWithHoles shape_1 = shapes_1[shape_pos];
        shape_1.shift(-aabb_1.x_min, -aabb_1.y_min);
        ShapeWithHoles shape_2 = shapes_2[shape_pos];
        shape_2.shift(-aabb_2.x_min, -aabb_2.y_min);
        if (!shape::equal(shape_1, shape_2))
            return false;
    }
    return true;
}

/**
 * Add the half-planes of the edges of a convex polygon, anticlockwise, to a
 * piece.
 */
void add_convex_polygon(
        const std::vector<Point>& vertices,
        Piece& piece)
{
    piece.vertices.insert(piece.vertices.end(), vertices.begin(), vertices.end());
    for (std::size_t vertex_pos = 0; vertex_pos < vertices.size(); ++vertex_pos) {
        const Point& start = vertices[vertex_pos];
        const Point& end = vertices[(vertex_pos + 1) % vertices.size()];
        piece.x_min = (std::min)(piece.x_min, start.x);
        piece.x_max = (std::max)(piece.x_max, start.x);
        piece.y_min = (std::min)(piece.y_min, start.y);
        piece.y_max = (std::max)(piece.y_max, start.y);
        LengthDbl ex = end.x - start.x;
        LengthDbl ey = end.y - start.y;
        LengthDbl norm = std::sqrt(ex * ex + ey * ey);
        if (norm == 0.0)
            continue;
        // The interior is on the left of the edge.
        HalfPlane half_plane;
        half_plane.nx = -ey / norm;
        half_plane.ny = ex / norm;
        half_plane.c = half_plane.nx * start.x + half_plane.ny * start.y;
        piece.half_planes.push_back(half_plane);
    }
}

/** Piece made of a line segment: both sides of its line, and its ends. */
Piece segment_piece(
        const Point& start,
        const Point& end)
{
    Piece piece;
    add_convex_polygon({start, end}, piece);
    LengthDbl ex = end.x - start.x;
    LengthDbl ey = end.y - start.y;
    LengthDbl norm = std::sqrt(ex * ex + ey * ey);
    if (norm > 0.0) {
        piece.half_planes.push_back({ex / norm, ey / norm, (ex * start.x + ey * start.y) / norm});
        piece.half_planes.push_back({-ex / norm, -ey / norm, -(ex * end.x + ey * end.y) / norm});
    }
    return piece;
}

/** Piece made of an axis-aligned rectangle. */
Piece rectangle_piece(const AxisAlignedBoundingBox& aabb)
{
    // The rectangle may be degenerate, so its half-planes are added directly.
    Piece piece;
    piece.half_planes = {
        {1, 0, aabb.x_min},
        {-1, 0, -aabb.x_max},
        {0, 1, aabb.y_min},
        {0, -1, -aabb.y_max}};
    piece.vertices = {
        {aabb.x_min, aabb.y_min},
        {aabb.x_max, aabb.y_min},
        {aabb.x_max, aabb.y_max},
        {aabb.x_min, aabb.y_max}};
    piece.x_min = aabb.x_min;
    piece.x_max = aabb.x_max;
    piece.y_min = aabb.y_min;
    piece.y_max = aabb.y_max;
    return piece;
}

/** Convex hull, anticlockwise, of a set of points. */
std::vector<Point> convex_hull(std::vector<Point> points)
{
    std::sort(
            points.begin(),
            points.end(),
            [](const Point& point_1, const Point& point_2)
            {
                return (point_1.x != point_2.x)?
                    point_1.x < point_2.x:
                    point_1.y < point_2.y;
            });
    // Identical points would all be removed below.
    points.erase(
            std::unique(
                points.begin(),
                points.end(),
                [](const Point& point_1, const Point& point_2)
                {
                    return point_1.x == point_2.x && point_1.y == point_2.y;
                }),
            points.end());
    if (points.size() <= 2)
        return points;
    auto cross = [](const Point& o, const Point& a, const Point& b)
    {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    std::vector<Point> hull(2 * points.size());
    std::size_t k = 0;
    for (std::size_t pos = 0; pos < points.size(); ++pos) {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], points[pos]) <= 0)
            k--;
        hull[k++] = points[pos];
    }
    for (std::size_t pos = points.size() - 1, t = k + 1; pos > 0; --pos) {
        while (k >= t && cross(hull[k - 2], hull[k - 1], points[pos - 1]) <= 0)
            k--;
        hull[k++] = points[pos - 1];
    }
    hull.resize(k - 1);
    return hull;
}

/**
 * Intersection of a convex polygon (or segment, or point) with an
 * axis-aligned box.
 */
std::vector<Point> clip(
        const std::vector<Point>& vertices,
        LengthDbl x_min,
        LengthDbl x_max,
        LengthDbl y_min,
        LengthDbl y_max)
{
    std::vector<Point> result = convex_hull(vertices);
    // Half-planes 'n . p >= c' of the box.
    const std::array<HalfPlane, 4> half_planes = {{
        {1, 0, x_min}, {-1, 0, -x_max}, {0, 1, y_min}, {0, -1, -y_max}}};
    for (const HalfPlane& half_plane: half_planes) {
        if (result.empty())
            break;
        auto value = [&half_plane](const Point& point)
        {
            return half_plane.nx * point.x + half_plane.ny * point.y - half_plane.c;
        };
        std::vector<Point> next;
        for (std::size_t pos = 0; pos < result.size(); ++pos) {
            const Point& current = result[pos];
            const Point& following = result[(pos + 1) % result.size()];
            LengthDbl value_current = value(current);
            LengthDbl value_following = value(following);
            if (value_current >= 0)
                next.push_back(current);
            if ((value_current >= 0) != (value_following >= 0)) {
                LengthDbl t = value_current / (value_current - value_following);
                next.push_back({
                        current.x + t * (following.x - current.x),
                        current.y + t * (following.y - current.y)});
            }
            if (result.size() == 1)
                break;
        }
        result = convex_hull(next);
    }
    return result;
}

/**
 * Return 'true' if two convex sets of points are separated by a gap larger
 * than 'tolerance' along an axis: the normal or the direction of one of
 * their edges.
 *
 * Returning 'false' doesn't mean that they intersect.
 */
bool separated(
        const std::vector<Point>& points_1,
        const std::vector<Point>& points_2,
        LengthDbl tolerance)
{
    if (points_1.empty() || points_2.empty())
        return true;
    auto separated_along = [&](LengthDbl ax, LengthDbl ay)
    {
        LengthDbl norm = std::sqrt(ax * ax + ay * ay);
        if (norm == 0)
            return false;
        ax /= norm;
        ay /= norm;
        LengthDbl min_1 = std::numeric_limits<LengthDbl>::infinity();
        LengthDbl max_1 = -std::numeric_limits<LengthDbl>::infinity();
        for (const Point& point: points_1) {
            min_1 = (std::min)(min_1, ax * point.x + ay * point.y);
            max_1 = (std::max)(max_1, ax * point.x + ay * point.y);
        }
        LengthDbl min_2 = std::numeric_limits<LengthDbl>::infinity();
        LengthDbl max_2 = -std::numeric_limits<LengthDbl>::infinity();
        for (const Point& point: points_2) {
            min_2 = (std::min)(min_2, ax * point.x + ay * point.y);
            max_2 = (std::max)(max_2, ax * point.x + ay * point.y);
        }
        return min_2 > max_1 + tolerance || min_1 > max_2 + tolerance;
    };
    if (separated_along(1, 0) || separated_along(0, 1))
        return true;
    for (const std::vector<Point>* points: {&points_1, &points_2}) {
        for (std::size_t pos = 0; pos < points->size(); ++pos) {
            const Point& start = (*points)[pos];
            const Point& end = (*points)[(pos + 1) % points->size()];
            LengthDbl ex = end.x - start.x;
            LengthDbl ey = end.y - start.y;
            if (separated_along(-ey, ex) || separated_along(ex, ey))
                return true;
        }
    }
    return false;
}

PairData compute_pair_data(
        const ItemTypeRotationData& item_1,
        const ItemTypeRotationData& item_2)
{
    PairData pair_data;
    pair_data.x_min = item_2.x_min - item_1.x_max;
    pair_data.x_max = item_2.x_max - item_1.x_min;
    pair_data.y_min = item_2.y_min - item_1.y_max;
    pair_data.y_max = item_2.y_max - item_1.y_min;

    // No-fit polygon of the two items: union of the no-fit polygons of their
    // shapes, with the feasible elements and points of null area strictly
    // inside them.
    std::vector<ShapeWithHoles> nfps;
    std::vector<Piece> null_area_pieces;
    AxisAlignedBoundingBox nfp_aabb;
    for (const ShapeWithHoles& shape_1: item_1.shapes) {
        for (const ShapeWithHoles& shape_2: item_2.shapes) {
            shape::NoFitPolygonOutput nfp = shape::no_fit_polygon(shape_1, shape_2);
            for (const ShapeWithHoles& part: nfp.nfp.shapes_with_holes) {
                nfp_aabb = merge(nfp_aabb, part.compute_min_max());
                nfps.push_back(part);
            }
            // With several shapes, an element or a point may be inside the
            // no-fit polygon of other shapes; keeping it only weakens the
            // relaxation.
            for (const ShapeElement& element: nfp.feasible_elements) {
                // An arc is relaxed by its bounding box.
                null_area_pieces.push_back((element.type == shape::ShapeElementType::LineSegment)?
                        segment_piece(element.start, element.end):
                        rectangle_piece(element.min_max()));
            }
            for (const Point& point: nfp.feasible_points)
                null_area_pieces.push_back(rectangle_piece({point.x, point.x, point.y, point.y}));
        }
    }
    if (nfps.empty()
            || nfp_aabb.x_min >= pair_data.x_max
            || nfp_aabb.x_max <= pair_data.x_min
            || nfp_aabb.y_min >= pair_data.y_max
            || nfp_aabb.y_max <= pair_data.y_min) {
        pair_data.interact = false;
        return pair_data;
    }

    // Feasible relative positions. Since the bounds of the positions already
    // keep d in R, the rectangle may be larger than R; it is, so that it
    // doesn't vanish if R is degenerate.
    LengthDbl enlargement = (pair_data.x_max - pair_data.x_min)
        + (pair_data.y_max - pair_data.y_min) + 1;
    shape::MultiShapeWithHoles region;
    region.shapes_with_holes.push_back({shape::build_rectangle(
                pair_data.x_min - enlargement,
                pair_data.x_max + enlargement,
                pair_data.y_min - enlargement,
                pair_data.y_max + enlargement)});
    shape::MultiShapeWithHoles feasible_region = shape::compute_difference(
            region,
            shape::compute_union(nfps));

    std::vector<Piece> pieces = null_area_pieces;
    for (const ShapeWithHoles& part: feasible_region.shapes_with_holes) {
        // The approximation of the arcs contains the region.
        ShapeWithHoles polygon = (part.is_polygon())?
            part:
            shape::approximate_by_line_segments(part, 1e-2 * enlargement);
        for (const Shape& convex_part: shape::compute_convex_partition(polygon)) {
            // The parts are anticlockwise.
            std::vector<Point> vertices;
            for (const shape::ShapeElement& element: convex_part.elements)
                vertices.push_back(element.start);
            Piece piece;
            add_convex_polygon(vertices, piece);
            pieces.push_back(piece);
        }
    }

    // Keep the pieces intersecting R.
    for (const Piece& piece: pieces) {
        if (piece.x_min > pair_data.x_max
                || piece.x_max < pair_data.x_min
                || piece.y_min > pair_data.y_max
                || piece.y_max < pair_data.y_min) {
            continue;
        }
        pair_data.pieces.push_back(piece);
    }
    return pair_data;
}

/** Bin type, and item type and rotation of each item of a triple. */
using MilpNfpTripleKey = std::tuple<BinTypeId, ItemTypeId, Counter, ItemTypeId, Counter, ItemTypeId, Counter>;

/**
 * Three-item cut of a triple of items (i, j, u): if the pieces a of (i, j)
 * and b of (j, u) are selected, the pieces of (i, u) which don't intersect
 * a + b can't be.
 */
struct ThreeItemCut
{
    Counter piece_pos_12;
    Counter piece_pos_23;
    std::vector<Counter> pieces_pos_13;
};

}

struct packingsolver::irregular::MilpNfpCache::Data
{
    std::map<MilpNfpPairKey, PairData> pairs_data;

    /** Three-item cuts of the triples of items. */
    std::map<MilpNfpTripleKey, std::vector<ThreeItemCut>> three_item_cuts;

    /** Mutex of 'three_item_cuts', which the threads update. */
    std::mutex three_item_cuts_mutex;

    /** Proven statuses of rotation combinations. */
    std::map<MilpNfpCombinationKey, MilpNfpStatus> combinations_statuses;

    /** Mutex of 'combinations_statuses', which the threads update. */
    std::mutex combinations_statuses_mutex;
};

packingsolver::irregular::MilpNfpCache::MilpNfpCache():
    data_(new Data()) { }

packingsolver::irregular::MilpNfpCache::~MilpNfpCache() { }

MilpNfpOutput packingsolver::irregular::milp_nfp(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemTypeId>& item_type_ids,
        const MilpNfpParameters& parameters)
{
    MilpNfpOutput output;
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const AxisAlignedBoundingBox& bin_aabb = bin_type.aabb_scaled;
    LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
    LengthDbl bin_width = bin_aabb.x_max - bin_aabb.x_min - 2 * bin_spacing;
    LengthDbl bin_height = bin_aabb.y_max - bin_aabb.y_min - 2 * bin_spacing;

    // Copies of the same item type are consecutive, for the symmetry
    // breaking.
    std::vector<ItemTypeId> sorted_item_type_ids = item_type_ids;
    std::sort(sorted_item_type_ids.begin(), sorted_item_type_ids.end());
    ItemPos number_of_items = sorted_item_type_ids.size();

    // Rotations of the item types, whose inner-fit rectangle isn't empty.
    std::map<ItemTypeId, std::vector<ItemTypeRotationData>> rotations;
    for (ItemTypeId item_type_id: sorted_item_type_ids) {
        if (rotations.find(item_type_id) != rotations.end())
            continue;
        const ItemType& item_type = instance.item_type(item_type_id);
        if (!item_type.has_only_discrete_rotations()) {
            output.status = MilpNfpStatus::Unknown;
            return output;
        }
        std::vector<ItemTypeRotationData>& item_type_rotations = rotations[item_type_id];
        for (const AllowedRotation& allowed_rotation: item_type.allowed_rotations) {
            ItemTypeRotationData rotation;
            rotation.angle = allowed_rotation.start_angle;
            rotation.mirror = allowed_rotation.mirror;
            AxisAlignedBoundingBox aabb;
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type.shapes.size();
                    ++item_shape_pos) {
                rotation.shapes.push_back(instance.item_shape_scaled(
                            item_type_id,
                            item_shape_pos,
                            allowed_rotation.start_angle,
                            allowed_rotation.mirror));
                aabb = merge(aabb, rotation.shapes.back().compute_min_max());
                rotation.area += rotation.shapes.back().compute_area();
            }
            rotation.aabb = aabb;
            rotation.x_min = bin_aabb.x_min + bin_spacing - aabb.x_min;
            rotation.x_max = bin_aabb.x_max - bin_spacing - aabb.x_max;
            rotation.y_min = bin_aabb.y_min + bin_spacing - aabb.y_min;
            rotation.y_max = bin_aabb.y_max - bin_spacing - aabb.y_max;
            // An item which fits exactly may get a slightly empty inner-fit
            // rectangle because of rounding errors.
            if (strictly_lesser(rotation.x_max, rotation.x_min)
                    || strictly_lesser(rotation.y_max, rotation.y_min)) {
                continue;
            }
            rotation.x_max = (std::max)(rotation.x_max, rotation.x_min);
            rotation.y_max = (std::max)(rotation.y_max, rotation.y_min);
            // Two rotations giving the same shape (for example, the
            // rotations by 0 and 180 degrees of a rectangle) are the same.
            bool duplicate = false;
            for (const ItemTypeRotationData& other_rotation: item_type_rotations) {
                if (equal_up_to_translation(
                            rotation.shapes, rotation.aabb,
                            other_rotation.shapes, other_rotation.aabb)) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate)
                continue;
            item_type_rotations.push_back(rotation);
        }
        if (item_type_rotations.empty()) {
            output.status = MilpNfpStatus::Infeasible;
            return output;
        }
    }
    if (number_of_items <= 1) {
        output.status = MilpNfpStatus::Feasible;
        return output;
    }

    // Symmetry of the bin: the relaxation only considers the bounding box of
    // the bin, so rotating a solution by 180 degrees gives another solution,
    // in which the rotation of each item is turned by 180 degrees. If every
    // rotation has such a counterpart, then one of the items may be
    // restricted to one rotation of each pair.
    // 'symmetric_rotation_ids[r]' is the rotation of the item type of
    // 'symmetry_item_pos' turned by 180 degrees.
    ItemPos symmetry_item_pos = -1;
    std::vector<Counter> symmetric_rotation_ids;
    {
        bool symmetric = true;
        std::map<ItemTypeId, std::vector<Counter>> symmetric_rotations;
        for (auto& entry: rotations) {
            ItemTypeId item_type_id = entry.first;
            const std::vector<ItemTypeRotationData>& item_type_rotations = entry.second;
            std::vector<Counter>& item_type_symmetric_rotations = symmetric_rotations[item_type_id];
            for (const ItemTypeRotationData& rotation: item_type_rotations) {
                std::vector<ShapeWithHoles> turned_shapes;
                AxisAlignedBoundingBox turned_aabb;
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)rotation.shapes.size();
                        ++item_shape_pos) {
                    turned_shapes.push_back(instance.item_shape_scaled(
                                item_type_id,
                                item_shape_pos,
                                rotation.angle + 180,
                                rotation.mirror));
                    turned_aabb = merge(turned_aabb, turned_shapes.back().compute_min_max());
                }
                Counter symmetric_rotation_id = -1;
                for (Counter rotation_id = 0;
                        rotation_id < (Counter)item_type_rotations.size();
                        ++rotation_id) {
                    if (equal_up_to_translation(
                                turned_shapes, turned_aabb,
                                item_type_rotations[rotation_id].shapes,
                                item_type_rotations[rotation_id].aabb)) {
                        symmetric_rotation_id = rotation_id;
                        break;
                    }
                }
                if (symmetric_rotation_id == -1) {
                    symmetric = false;
                    break;
                }
                item_type_symmetric_rotations.push_back(symmetric_rotation_id);
            }
            if (!symmetric)
                break;
        }
        // The restricted item must be the only copy of its item type, so
        // that the symmetry breaking between identical items still holds.
        if (symmetric) {
            for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
                ItemTypeId item_type_id = sorted_item_type_ids[item_pos];
                if ((item_pos > 0 && sorted_item_type_ids[item_pos - 1] == item_type_id)
                        || (item_pos + 1 < number_of_items && sorted_item_type_ids[item_pos + 1] == item_type_id)) {
                    continue;
                }
                const std::vector<Counter>& item_type_symmetric_rotations = symmetric_rotations[item_type_id];
                bool useful = false;
                for (Counter rotation_id = 0;
                        rotation_id < (Counter)item_type_symmetric_rotations.size();
                        ++rotation_id) {
                    if (item_type_symmetric_rotations[rotation_id] != rotation_id)
                        useful = true;
                }
                if (useful) {
                    symmetry_item_pos = item_pos;
                    symmetric_rotation_ids = item_type_symmetric_rotations;
                    break;
                }
            }
        }
    }

    // Data of the pairs of item types in each of their rotations, computed
    // when first needed, and kept in the cache if one is given.
    MilpNfpCache local_cache;
    MilpNfpCache::Data& cache_data = (parameters.cache != nullptr)?
        *parameters.cache->data_:
        *local_cache.data_;
    std::map<MilpNfpPairKey, PairData>& pairs_data = cache_data.pairs_data;
    auto pair_data = [&](
            ItemPos item_pos_1,
            ItemPos item_pos_2,
            Counter rotation_id_1,
            Counter rotation_id_2) -> const PairData&
    {
        ItemTypeId item_type_id_1 = sorted_item_type_ids[item_pos_1];
        ItemTypeId item_type_id_2 = sorted_item_type_ids[item_pos_2];
        MilpNfpPairKey key = std::make_tuple(
                bin_type_id,
                item_type_id_1,
                rotation_id_1,
                item_type_id_2,
                rotation_id_2);
        auto it = pairs_data.find(key);
        if (it == pairs_data.end()) {
            it = pairs_data.insert({key, compute_pair_data(
                        rotations[item_type_id_1][rotation_id_1],
                        rotations[item_type_id_2][rotation_id_2])}).first;
        }
        return it->second;
    };

    // Rotation combinations; the rotations of identical items are
    // non-decreasing. A partial combination in which a pair of items doesn't
    // fit isn't extended. All the pair data needed later are computed here,
    // so that they are only read by the threads.
    std::vector<std::vector<Counter>> combinations;
    {
        std::vector<Counter> rotation_ids(number_of_items, 0);
        std::function<void(ItemPos)> enumerate = [&](ItemPos item_pos)
        {
            if (item_pos == number_of_items) {
                combinations.push_back(rotation_ids);
                return;
            }
            if (parameters.timer.needs_to_end())
                return;
            ItemTypeId item_type_id = sorted_item_type_ids[item_pos];
            Counter rotation_id_start = (item_pos > 0 && sorted_item_type_ids[item_pos - 1] == item_type_id)?
                rotation_ids[item_pos - 1]: 0;
            for (Counter rotation_id = rotation_id_start;
                    rotation_id < (Counter)rotations[item_type_id].size();
                    ++rotation_id) {
                if (item_pos == symmetry_item_pos
                        && symmetric_rotation_ids[rotation_id] < rotation_id) {
                    continue;
                }
                bool fits = true;
                for (ItemPos item_pos_1 = 0; item_pos_1 < item_pos; ++item_pos_1) {
                    const PairData& pair = pair_data(
                            item_pos_1,
                            item_pos,
                            rotation_ids[item_pos_1],
                            rotation_id);
                    if (pair.interact && pair.pieces.empty()) {
                        fits = false;
                        break;
                    }
                }
                if (!fits)
                    continue;
                rotation_ids[item_pos] = rotation_id;
                enumerate(item_pos + 1);
            }
        };
        enumerate(0);
    }
    if (parameters.timer.needs_to_end()) {
        output.status = MilpNfpStatus::Unknown;
        return output;
    }

    // Return 'true' if a rotation combination is proven infeasible without a
    // MILP, by the core widening.
    auto prune = [&](const std::vector<Counter>& rotation_ids) -> bool
    {
        // Core widening.
        //
        // The core of an item is the part of its y-range which no other item
        // can reach, whatever their positions. Since the rotations are fixed,
        // the horizontal band of the core of an item, across the bin, doesn't
        // contain any other item. Thus the item can be widened to the width
        // of the bin on its core. The cores of different items are disjoint,
        // so the set of items doesn't fit if the sum of the widened areas
        // exceeds the area of the bin.
        AreaDbl widened_area = 0;
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            const ItemTypeRotationData& rotation = rotations[sorted_item_type_ids[item_pos]][rotation_ids[item_pos]];
            const AxisAlignedBoundingBox& aabb = rotation.aabb;

            // Parts of the y-range of the item, relative to its reference
            // point, which another item can reach.
            std::vector<std::pair<LengthDbl, LengthDbl>> intrusions;
            for (ItemPos item_pos_2 = 0; item_pos_2 < number_of_items; ++item_pos_2) {
                if (item_pos_2 == item_pos)
                    continue;
                const AxisAlignedBoundingBox& aabb_2 = rotations[sorted_item_type_ids[item_pos_2]][rotation_ids[item_pos_2]].aabb;
                // The pair data are for d = p_{pos_2} - p_{pos_1} with
                // pos_1 < pos_2; the relative position of the other item is
                // the opposite if it comes first.
                bool reversed = (item_pos_2 < item_pos);
                const PairData& pair = (reversed)?
                    pair_data(item_pos_2, item_pos, rotation_ids[item_pos_2], rotation_ids[item_pos]):
                    pair_data(item_pos, item_pos_2, rotation_ids[item_pos], rotation_ids[item_pos_2]);
                auto add_intrusion = [&](LengthDbl dy_min, LengthDbl dy_max)
                {
                    if (reversed) {
                        LengthDbl tmp = dy_min;
                        dy_min = -dy_max;
                        dy_max = -tmp;
                    }
                    LengthDbl y_min = (std::max)(dy_min + aabb_2.y_min, aabb.y_min);
                    LengthDbl y_max = (std::min)(dy_max + aabb_2.y_max, aabb.y_max);
                    if (y_min < y_max)
                        intrusions.push_back({y_min, y_max});
                };
                if (!pair.interact) {
                    add_intrusion(pair.y_min, pair.y_max);
                } else {
                    // A piece is convex, so it contains a relative position
                    // for each y-coordinate of its range.
                    for (const Piece& piece: pair.pieces)
                        add_intrusion(piece.y_min, piece.y_max);
                }
            }

            // Core: complement of the union of the intrusions.
            std::sort(intrusions.begin(), intrusions.end());
            std::vector<std::pair<LengthDbl, LengthDbl>> core;
            LengthDbl y_current = aabb.y_min;
            for (const auto& intrusion: intrusions) {
                if (intrusion.first > y_current)
                    core.push_back({y_current, intrusion.first});
                y_current = (std::max)(y_current, intrusion.second);
            }
            if (aabb.y_max > y_current)
                core.push_back({y_current, aabb.y_max});

            // Widened area: the core across the bin, plus the area of the
            // item outside of its core.
            widened_area += rotation.area;
            LengthDbl item_width = aabb.x_max - aabb.x_min;
            for (const auto& strip: core) {
                ShapeWithHoles strip_shape = {shape::build_rectangle(
                        aabb.x_min - item_width,
                        aabb.x_max + item_width,
                        strip.first,
                        strip.second)};
                widened_area += bin_width * (strip.second - strip.first);
                for (const ShapeWithHoles& shape: rotation.shapes)
                    for (const ShapeWithHoles& part: shape::compute_intersection({shape, strip_shape}).shapes_with_holes)
                        widened_area -= part.compute_area();
            }
        }
        return strictly_greater_profit(widened_area, bin_width * bin_height);
    };

    // Add the constraints 'n . (p_2 - p_1) >= c - M * (1 - z)' of the pieces
    // of a pair, where M is such that the constraint holds for every relative
    // position d of 'relative_box' (x_min, x_max, y_min, y_max).
    auto add_piece_constraints = [](
            mathoptsolverscmake::MathOptModel& model,
            ItemPos item_pos_1,
            ItemPos item_pos_2,
            const Piece& piece,
            int piece_variable_id,
            const std::array<LengthDbl, 4>& relative_box)
    {
        for (const HalfPlane& half_plane: piece.half_planes) {
            LengthDbl c = half_plane.c;
            LengthDbl minimum = half_plane.nx * ((half_plane.nx > 0)? relative_box[0]: relative_box[1])
                + half_plane.ny * ((half_plane.ny > 0)? relative_box[2]: relative_box[3]);
            LengthDbl big_m = c - minimum;
            if (big_m <= 0)
                continue;
            model.constraints_starts.push_back(model.elements_variables.size());
            model.constraints_lower_bounds.push_back(c - big_m);
            model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
            model.elements_variables.push_back(2 * item_pos_2);
            model.elements_coefficients.push_back(half_plane.nx);
            model.elements_variables.push_back(2 * item_pos_2 + 1);
            model.elements_coefficients.push_back(half_plane.ny);
            model.elements_variables.push_back(2 * item_pos_1);
            model.elements_coefficients.push_back(-half_plane.nx);
            model.elements_variables.push_back(2 * item_pos_1 + 1);
            model.elements_coefficients.push_back(-half_plane.ny);
            model.elements_variables.push_back(piece_variable_id);
            model.elements_coefficients.push_back(-big_m);
        }
    };
    auto add_variable = [](
            mathoptsolverscmake::MathOptModel& model,
            double lower_bound,
            double upper_bound,
            mathoptsolverscmake::VariableType type) -> int
    {
        model.variables_lower_bounds.push_back(lower_bound);
        model.variables_upper_bounds.push_back(upper_bound);
        model.variables_types.push_back(type);
        model.objective_coefficients.push_back(0.0);
        return model.variables_lower_bounds.size() - 1;
    };
    auto add_constraint = [](
            mathoptsolverscmake::MathOptModel& model,
            double lower_bound,
            double upper_bound,
            const std::vector<std::pair<int, double>>& elements)
    {
        model.constraints_starts.push_back(model.elements_variables.size());
        model.constraints_lower_bounds.push_back(lower_bound);
        model.constraints_upper_bounds.push_back(upper_bound);
        for (const auto& element: elements) {
            model.elements_variables.push_back(element.first);
            model.elements_coefficients.push_back(element.second);
        }
    };
    // Three-item cuts of the triples of (item type, rotation), computed when
    // first needed, and kept in the cache.
    std::map<MilpNfpTripleKey, std::vector<ThreeItemCut>>& three_item_cuts = cache_data.three_item_cuts;
    std::mutex& three_item_cuts_mutex = cache_data.three_item_cuts_mutex;
    auto compute_three_item_cuts = [](
            const PairData& pair_12,
            const PairData& pair_23,
            const PairData& pair_13)
    {
        auto clipped_pieces = [](const PairData& pair)
        {
            std::vector<std::vector<Point>> pieces;
            for (const Piece& piece: pair.pieces)
                pieces.push_back(clip(piece.vertices, pair.x_min, pair.x_max, pair.y_min, pair.y_max));
            return pieces;
        };
        std::vector<std::vector<Point>> pieces_12 = clipped_pieces(pair_12);
        std::vector<std::vector<Point>> pieces_23 = clipped_pieces(pair_23);
        std::vector<std::vector<Point>> pieces_13 = clipped_pieces(pair_13);
        LengthDbl tolerance = 1e-6 * (
                (pair_13.x_max - pair_13.x_min)
                + (pair_13.y_max - pair_13.y_min) + 1);
        std::vector<ThreeItemCut> cuts;
        for (Counter piece_pos_12 = 0; piece_pos_12 < (Counter)pieces_12.size(); ++piece_pos_12) {
            for (Counter piece_pos_23 = 0; piece_pos_23 < (Counter)pieces_23.size(); ++piece_pos_23) {
                // An empty clipped piece can't be selected.
                std::vector<Point> sum_points;
                for (const Point& point_12: pieces_12[piece_pos_12])
                    for (const Point& point_23: pieces_23[piece_pos_23])
                        sum_points.push_back({point_12.x + point_23.x, point_12.y + point_23.y});
                std::vector<Point> sum = convex_hull(sum_points);
                ThreeItemCut cut;
                cut.piece_pos_12 = piece_pos_12;
                cut.piece_pos_23 = piece_pos_23;
                for (Counter piece_pos_13 = 0; piece_pos_13 < (Counter)pieces_13.size(); ++piece_pos_13)
                    if (separated(sum, pieces_13[piece_pos_13], tolerance))
                        cut.pieces_pos_13.push_back(piece_pos_13);
                if (!cut.pieces_pos_13.empty())
                    cuts.push_back(cut);
            }
        }
        return cuts;
    };

    // Build the MILP of a rotation combination, restricted to the items
    // 'active' (all the items if it is empty); the positions of the other
    // items are free.
    auto build_model = [&](
            const std::vector<Counter>& rotation_ids,
            const std::vector<bool>& active = {})
    {
        auto is_active = [&active](ItemPos item_pos)
        {
            return active.empty() || active[item_pos];
        };
        mathoptsolverscmake::MathOptModel model;
        model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;

        // Positions of the reference points.
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            const ItemTypeRotationData& rotation = rotations[sorted_item_type_ids[item_pos]][rotation_ids[item_pos]];
            add_variable(model, rotation.x_min, rotation.x_max, mathoptsolverscmake::VariableType::Continuous);
            add_variable(model, rotation.y_min, rotation.y_max, mathoptsolverscmake::VariableType::Continuous);
        }

        // Symmetry breaking: identical items are sorted by x.
        for (ItemPos item_pos = 0; item_pos + 1 < number_of_items; ++item_pos) {
            if (sorted_item_type_ids[item_pos] != sorted_item_type_ids[item_pos + 1]
                    || rotation_ids[item_pos] != rotation_ids[item_pos + 1]
                    || !is_active(item_pos)
                    || !is_active(item_pos + 1)) {
                continue;
            }
            add_constraint(
                    model,
                    -std::numeric_limits<double>::infinity(),
                    0.0,
                    {{2 * item_pos, 1.0}, {2 * (item_pos + 1), -1.0}});
        }

        // Non-overlap constraints. First variable of the pieces of each pair,
        // -1 if the items don't interact.
        std::vector<std::vector<int>> first_piece_variable_ids(
                number_of_items,
                std::vector<int>(number_of_items, -1));
        for (ItemPos item_pos_1 = 0; item_pos_1 < number_of_items; ++item_pos_1) {
            if (!is_active(item_pos_1))
                continue;
            for (ItemPos item_pos_2 = item_pos_1 + 1; item_pos_2 < number_of_items; ++item_pos_2) {
                if (!is_active(item_pos_2))
                    continue;
                const PairData& pair = pair_data(
                        item_pos_1,
                        item_pos_2,
                        rotation_ids[item_pos_1],
                        rotation_ids[item_pos_2]);
                if (!pair.interact)
                    continue;
                // Exactly one piece.
                first_piece_variable_ids[item_pos_1][item_pos_2] = model.variables_lower_bounds.size();
                std::vector<std::pair<int, double>> elements;
                for (const Piece& piece: pair.pieces) {
                    int piece_variable_id = add_variable(model, 0, 1, mathoptsolverscmake::VariableType::Binary);
                    elements.push_back({piece_variable_id, 1.0});
                    add_piece_constraints(
                            model,
                            item_pos_1,
                            item_pos_2,
                            piece,
                            piece_variable_id,
                            {pair.x_min, pair.x_max, pair.y_min, pair.y_max});
                }
                add_constraint(model, 1.0, 1.0, elements);
            }
        }

        // Three-item cuts: 'z_a + z_b + sum_c z_c <= 2'.
        ItemPos number_of_active_items = 0;
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
            if (is_active(item_pos))
                number_of_active_items++;
        // With 3 items, the cuts don't pay off.
        if (parameters.three_item_cuts && number_of_active_items >= 4) {
            for (ItemPos item_pos_1 = 0; item_pos_1 < number_of_items; ++item_pos_1) {
                for (ItemPos item_pos_2 = item_pos_1 + 1; item_pos_2 < number_of_items; ++item_pos_2) {
                    for (ItemPos item_pos_3 = item_pos_2 + 1; item_pos_3 < number_of_items; ++item_pos_3) {
                        int first_12 = first_piece_variable_ids[item_pos_1][item_pos_2];
                        int first_23 = first_piece_variable_ids[item_pos_2][item_pos_3];
                        int first_13 = first_piece_variable_ids[item_pos_1][item_pos_3];
                        if (first_12 == -1 || first_23 == -1 || first_13 == -1)
                            continue;
                        MilpNfpTripleKey key = std::make_tuple(
                                bin_type_id,
                                sorted_item_type_ids[item_pos_1], rotation_ids[item_pos_1],
                                sorted_item_type_ids[item_pos_2], rotation_ids[item_pos_2],
                                sorted_item_type_ids[item_pos_3], rotation_ids[item_pos_3]);
                        std::vector<ThreeItemCut> cuts;
                        bool found = false;
                        {
                            std::lock_guard<std::mutex> lock(three_item_cuts_mutex);
                            auto it = three_item_cuts.find(key);
                            if (it != three_item_cuts.end()) {
                                cuts = it->second;
                                found = true;
                            }
                        }
                        if (!found) {
                            cuts = compute_three_item_cuts(
                                    pair_data(item_pos_1, item_pos_2, rotation_ids[item_pos_1], rotation_ids[item_pos_2]),
                                    pair_data(item_pos_2, item_pos_3, rotation_ids[item_pos_2], rotation_ids[item_pos_3]),
                                    pair_data(item_pos_1, item_pos_3, rotation_ids[item_pos_1], rotation_ids[item_pos_3]));
                            std::lock_guard<std::mutex> lock(three_item_cuts_mutex);
                            three_item_cuts.insert({key, cuts});
                        }
                        for (const ThreeItemCut& cut: cuts) {
                            std::vector<std::pair<int, double>> elements = {
                                {first_12 + cut.piece_pos_12, 1.0},
                                {first_23 + cut.piece_pos_23, 1.0}};
                            for (Counter piece_pos_13: cut.pieces_pos_13)
                                elements.push_back({first_13 + piece_pos_13, 1.0});
                            add_constraint(
                                    model,
                                    -std::numeric_limits<double>::infinity(),
                                    2.0,
                                    elements);
                        }
                    }
                }
            }
        }
        return model;
    };

    // Solve a MILP; 'stop' interrupts it.
    std::atomic<bool> stop(false);
    auto solve_model = [&](
            const mathoptsolverscmake::MathOptModel& model,
            std::vector<double>& values) -> MilpNfpStatus
    {
        if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
            Highs highs;
            mathoptsolverscmake::reduce_printout(highs);
            mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
            mathoptsolverscmake::load(highs, model);
            highs.setCallback([&stop, &parameters](
                        const int callback_type,
                        const std::string&,
                        const HighsCallbackOutput*,
                        HighsCallbackInput* highs_input,
                        void*)
                    {
                        if (callback_type != HighsCallbackType::kCallbackMipInterrupt)
                            return;
                        if (stop || parameters.timer.needs_to_end())
                            highs_input->user_interrupt = 1;
                    },
                    nullptr);
            highs.startCallback(HighsCallbackType::kCallbackMipInterrupt);
            mathoptsolverscmake::solve(highs);
            HighsModelStatus status = highs.getModelStatus();
            if (status == HighsModelStatus::kInfeasible)
                return MilpNfpStatus::Infeasible;
            if (status == HighsModelStatus::kOptimal) {
                values = highs.getSolution().col_value;
                return MilpNfpStatus::Feasible;
            }
            return MilpNfpStatus::Unknown;
#else
            throw std::invalid_argument(FUNC_SIGNATURE);
#endif
        } else {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }
    };

    // Prune the rotation combinations.
    std::vector<std::vector<Counter>> kept_combinations;
    for (const std::vector<Counter>& combination: combinations) {
        if (parameters.timer.needs_to_end()) {
            output.status = MilpNfpStatus::Unknown;
            return output;
        }
        output.number_of_rotation_combinations++;
        if (prune(combination)) {
            output.number_of_rotation_combinations_pruned++;
        } else {
            kept_combinations.push_back(combination);
        }
    }
    for (const std::vector<Counter>& combination: kept_combinations) {
        for (ItemPos item_pos_1 = 0; item_pos_1 < number_of_items; ++item_pos_1) {
            for (ItemPos item_pos_2 = item_pos_1 + 1; item_pos_2 < number_of_items; ++item_pos_2) {
                output.maximum_number_of_pieces = (std::max)(
                        output.maximum_number_of_pieces,
                        (Counter)pair_data(
                            item_pos_1,
                            item_pos_2,
                            combination[item_pos_1],
                            combination[item_pos_2]).pieces.size());
            }
        }
    }
    if (kept_combinations.empty()) {
        output.status = MilpNfpStatus::Infeasible;
        return output;
    }

    // Solve the MILPs of the kept combinations, in parallel. Stop as soon as
    // one isn't proven infeasible.
    //
    // A combination is infeasible if one of its sub-combinations without one
    // of the items is. These are checked first: they are cheaper, and shared
    // by several combinations. Their statuses, and the ones of the
    // combinations, are kept in the cache, so that they are also reused by
    // the next calls.
    std::atomic<Counter> next_combination_pos(0);
    std::atomic<Counter> number_of_milps(0);
    std::atomic<Counter> number_of_sub_combination_milps(0);
    std::atomic<Counter> number_of_rotation_combinations_pruned_by_sub_combinations(0);
    std::mutex status_mutex;
    MilpNfpStatus status = MilpNfpStatus::Infeasible;
    auto combination_key = [&](
            const std::vector<Counter>& rotation_ids,
            const std::vector<bool>& active)
    {
        MilpNfpCombinationKey key;
        key.first = bin_type_id;
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
            if (active.empty() || active[item_pos])
                key.second.push_back({sorted_item_type_ids[item_pos], rotation_ids[item_pos]});
        std::sort(key.second.begin(), key.second.end());
        return key;
    };
    auto cached_status = [&cache_data](const MilpNfpCombinationKey& key)
    {
        std::lock_guard<std::mutex> lock(cache_data.combinations_statuses_mutex);
        auto it = cache_data.combinations_statuses.find(key);
        return (it == cache_data.combinations_statuses.end())?
            MilpNfpStatus::Unknown:
            it->second;
    };
    auto cache_status = [&cache_data](
            const MilpNfpCombinationKey& key,
            MilpNfpStatus combination_status)
    {
        if (combination_status == MilpNfpStatus::Unknown)
            return;
        std::lock_guard<std::mutex> lock(cache_data.combinations_statuses_mutex);
        cache_data.combinations_statuses[key] = combination_status;
    };
    auto worker = [&]()
    {
        for (;;) {
            if (stop)
                return;
            Counter combination_pos = next_combination_pos++;
            if (combination_pos >= (Counter)kept_combinations.size())
                return;
            const std::vector<Counter>& combination = kept_combinations[combination_pos];
            MilpNfpCombinationKey key = combination_key(combination, {});
            if (cached_status(key) == MilpNfpStatus::Infeasible)
                continue;

            // Sub-combinations without one of the items.
            bool infeasible_sub_combination = false;
            if (parameters.check_sub_combinations && number_of_items >= 3) {
                for (ItemPos removed_item_pos = 0;
                        removed_item_pos < number_of_items;
                        ++removed_item_pos) {
                    // Removing an item identical to the previous one gives
                    // the same sub-combination.
                    if (removed_item_pos > 0
                            && sorted_item_type_ids[removed_item_pos] == sorted_item_type_ids[removed_item_pos - 1]
                            && combination[removed_item_pos] == combination[removed_item_pos - 1]) {
                        continue;
                    }
                    std::vector<bool> active(number_of_items, true);
                    active[removed_item_pos] = false;
                    MilpNfpCombinationKey sub_key = combination_key(combination, active);
                    MilpNfpStatus sub_status = cached_status(sub_key);
                    if (sub_status == MilpNfpStatus::Unknown) {
                        number_of_sub_combination_milps++;
                        std::vector<double> sub_values;
                        sub_status = solve_model(build_model(combination, active), sub_values);
                        if (stop)
                            return;
                        cache_status(sub_key, sub_status);
                    }
                    if (sub_status == MilpNfpStatus::Infeasible) {
                        infeasible_sub_combination = true;
                        break;
                    }
                }
            }
            if (infeasible_sub_combination) {
                number_of_rotation_combinations_pruned_by_sub_combinations++;
                cache_status(key, MilpNfpStatus::Infeasible);
                continue;
            }

            number_of_milps++;
            std::vector<double> values;
            MilpNfpStatus combination_status = solve_model(build_model(combination), values);
            cache_status(key, combination_status);
            if (combination_status != MilpNfpStatus::Infeasible) {
                std::lock_guard<std::mutex> lock(status_mutex);
                // 'Feasible' is a definitive answer; 'Unknown' may come from
                // the interruption of another thread.
                if (status != MilpNfpStatus::Feasible) {
                    status = combination_status;
                    if (status == MilpNfpStatus::Feasible) {
                        // Positions of the items.
                        LengthDbl scale_value = instance.parameters().scale_value;
                        output.items.clear();
                        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
                            const ItemTypeRotationData& rotation = rotations[sorted_item_type_ids[item_pos]][combination[item_pos]];
                            SolutionItem item;
                            item.item_type_id = sorted_item_type_ids[item_pos];
                            item.bl_corner.x = values[2 * item_pos] / scale_value;
                            item.bl_corner.y = values[2 * item_pos + 1] / scale_value;
                            item.angle = rotation.angle;
                            item.mirror = rotation.mirror;
                            output.items.push_back(item);
                        }
                    }
                }
                stop = true;
                return;
            }
        }
    };
    Counter number_of_threads = (std::max)((Counter)1, (std::min)(
                parameters.number_of_threads,
                (Counter)kept_combinations.size()));
    std::vector<std::thread> threads;
    for (Counter thread_id = 1; thread_id < number_of_threads; ++thread_id)
        threads.push_back(std::thread(worker));
    worker();
    for (std::thread& thread: threads)
        thread.join();
    output.number_of_milps = number_of_milps;
    output.number_of_sub_combination_milps = number_of_sub_combination_milps;
    output.number_of_rotation_combinations_pruned_by_sub_combinations
        = number_of_rotation_combinations_pruned_by_sub_combinations;
    {
        std::lock_guard<std::mutex> lock(three_item_cuts_mutex);
        for (const auto& p: three_item_cuts)
            output.number_of_three_item_cuts += p.second.size();
    }
    output.status = status;
    return output;
}

bool packingsolver::irregular::milp_nfp_solution(
        const Instance& instance,
        BinTypeId bin_type_id,
        const MilpNfpOutput& output,
        Solution& solution)
{
    if (output.status != MilpNfpStatus::Feasible
            || output.items.empty()
            || instance.parameters().item_item_minimum_spacing > 0
            || !instance.bin_type(bin_type_id).fixed_items.empty()) {
        return false;
    }
    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);
    for (const SolutionItem& item: output.items) {
        solution_builder.add_item(
                bin_pos,
                item.item_type_id,
                item.bl_corner,
                item.angle,
                item.mirror);
    }
    Solution new_solution = solution_builder.build();
    Solution::OverlappingItems overlapping_items = new_solution.compute_overlapping_items(bin_pos);
    if (!overlapping_items.item_border_pairs.empty()
            || !overlapping_items.item_defect_pairs.empty()
            || !overlapping_items.item_item_pairs.empty()
            || !overlapping_items.items_outside_bin.empty()) {
        return false;
    }
    solution = new_solution;
    return true;
}
