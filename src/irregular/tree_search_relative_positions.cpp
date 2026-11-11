#include "irregular/tree_search_relative_positions.hpp"

#include "irregular/solution_builder.hpp"
#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "shape/no_fit_polygon.hpp"
#include "shape/boolean_operations.hpp"
#include "shape/approximation.hpp"
#include "shape/convex_partition.hpp"

#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <array>
#include <deque>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>
#include <algorithm>
#include <functional>
#include <map>
#include <numeric>
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

/** Item type and rotation of each item of a pair. */
using TreeSearchRelativePositionsPairKey = std::tuple<ItemTypeId, Counter, ItemTypeId, Counter>;

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

/**
 * Bounding box of the intersection of a convex polygon (or segment, or
 * point), whose vertices are in order, with an axis-aligned box; return
 * 'false' if it is empty.
 *
 * 'buffer_1' and 'buffer_2' are reused between calls to avoid allocations.
 */
bool clip_bounding_box(
        const std::vector<Point>& vertices,
        LengthDbl x_min,
        LengthDbl x_max,
        LengthDbl y_min,
        LengthDbl y_max,
        std::vector<Point>& buffer_1,
        std::vector<Point>& buffer_2,
        AxisAlignedBoundingBox& bounding_box)
{
    buffer_1.assign(vertices.begin(), vertices.end());
    // Half-planes 'n . p >= c' of the box.
    const std::array<HalfPlane, 4> half_planes = {{
        {1, 0, x_min}, {-1, 0, -x_max}, {0, 1, y_min}, {0, -1, -y_max}}};
    for (const HalfPlane& half_plane: half_planes) {
        if (buffer_1.empty())
            return false;
        auto value = [&half_plane](const Point& point)
        {
            return half_plane.nx * point.x + half_plane.ny * point.y - half_plane.c;
        };
        buffer_2.clear();
        for (std::size_t pos = 0; pos < buffer_1.size(); ++pos) {
            const Point& current = buffer_1[pos];
            const Point& following = buffer_1[(pos + 1) % buffer_1.size()];
            LengthDbl value_current = value(current);
            LengthDbl value_following = value(following);
            if (value_current >= 0)
                buffer_2.push_back(current);
            if ((value_current >= 0) != (value_following >= 0)) {
                LengthDbl t = value_current / (value_current - value_following);
                buffer_2.push_back({
                        current.x + t * (following.x - current.x),
                        current.y + t * (following.y - current.y)});
            }
            if (buffer_1.size() == 1)
                break;
        }
        buffer_1.swap(buffer_2);
    }
    if (buffer_1.empty())
        return false;
    bounding_box = AxisAlignedBoundingBox();
    for (const Point& point: buffer_1) {
        bounding_box.x_min = (std::min)(bounding_box.x_min, point.x);
        bounding_box.x_max = (std::max)(bounding_box.x_max, point.x);
        bounding_box.y_min = (std::min)(bounding_box.y_min, point.y);
        bounding_box.y_max = (std::max)(bounding_box.y_max, point.y);
    }
    return true;
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

/**
 * Areas of the parts of an item between two coordinates along an axis.
 *
 * The width of a polygon across the axis is linear between the coordinates
 * of its vertices, so the area below a coordinate is piecewise quadratic.
 */
struct AreaProfile
{
    /** Coordinates of the vertices, sorted. */
    std::vector<LengthDbl> coordinates;

    /** Area below each coordinate. */
    std::vector<AreaDbl> cumulative_areas;

    /** Widths at the start and at the end of each interval. */
    std::vector<LengthDbl> widths_start;
    std::vector<LengthDbl> widths_end;

    /**
     * 'false' if the item has circular arcs; then the area between two
     * coordinates is overestimated by the area of the item.
     */
    bool exact = true;

    /** Area of the item. */
    AreaDbl total_area = 0;

    /** Area below a coordinate. */
    AreaDbl area_below(LengthDbl coordinate) const
    {
        if (coordinates.empty() || coordinate <= coordinates.front())
            return 0;
        if (coordinate >= coordinates.back())
            return cumulative_areas.back();
        std::size_t pos = std::upper_bound(
                coordinates.begin(),
                coordinates.end(),
                coordinate) - coordinates.begin() - 1;
        LengthDbl length = coordinate - coordinates[pos];
        LengthDbl width = widths_start[pos] + (widths_end[pos] - widths_start[pos])
            * length / (coordinates[pos + 1] - coordinates[pos]);
        return cumulative_areas[pos] + length * (widths_start[pos] + width) / 2;
    }

    /** Area between two coordinates; it may be overestimated. */
    AreaDbl area(LengthDbl start, LengthDbl end) const
    {
        if (!exact)
            return total_area;
        return area_below(end) - area_below(start);
    }
};

/**
 * Area profile of the shapes of an item along an axis (0 for x, 1 for y).
 *
 * The widths of the shapes are summed, so that the areas are overestimated
 * if they overlap.
 */
AreaProfile compute_area_profile(
        const std::vector<ShapeWithHoles>& shapes,
        int axis)
{
    AreaProfile area_profile;
    auto coordinate = [axis](const Point& point) { return (axis == 0)? point.x: point.y; };
    auto other_coordinate = [axis](const Point& point) { return (axis == 0)? point.y: point.x; };
    for (const ShapeWithHoles& shape: shapes) {
        area_profile.total_area += shape.compute_area();
        if (!shape.is_polygon())
            area_profile.exact = false;
        for (const ShapeElement& element: shape.shape.elements)
            area_profile.coordinates.push_back(coordinate(element.start));
    }
    if (!area_profile.exact)
        return area_profile;
    std::sort(area_profile.coordinates.begin(), area_profile.coordinates.end());
    area_profile.coordinates.erase(
            std::unique(area_profile.coordinates.begin(), area_profile.coordinates.end()),
            area_profile.coordinates.end());
    area_profile.cumulative_areas = {0};
    // Crossings of the elements of a shape with the interval: their other
    // coordinates in the middle, at the start and at the end.
    std::vector<std::array<LengthDbl, 3>> crossings;
    for (std::size_t pos = 0; pos + 1 < area_profile.coordinates.size(); ++pos) {
        LengthDbl start = area_profile.coordinates[pos];
        LengthDbl end = area_profile.coordinates[pos + 1];
        LengthDbl middle = (start + end) / 2;
        LengthDbl width_start = 0;
        LengthDbl width_end = 0;
        for (const ShapeWithHoles& shape: shapes) {
            crossings.clear();
            auto add_crossings = [&](const Shape& polygon)
            {
                for (const ShapeElement& element: polygon.elements) {
                    LengthDbl coordinate_1 = coordinate(element.start);
                    LengthDbl coordinate_2 = coordinate(element.end);
                    if ((std::min)(coordinate_1, coordinate_2) > start
                            || (std::max)(coordinate_1, coordinate_2) < end) {
                        continue;
                    }
                    LengthDbl other_1 = other_coordinate(element.start);
                    LengthDbl other_2 = other_coordinate(element.end);
                    auto other = [&](LengthDbl c)
                    {
                        return other_1 + (other_2 - other_1) * (c - coordinate_1) / (coordinate_2 - coordinate_1);
                    };
                    crossings.push_back({other(middle), other(start), other(end)});
                }
            };
            add_crossings(shape.shape);
            for (const Shape& hole: shape.holes)
                add_crossings(hole);
            std::sort(crossings.begin(), crossings.end());
            for (std::size_t crossing_pos = 0; crossing_pos + 1 < crossings.size(); crossing_pos += 2) {
                width_start += crossings[crossing_pos + 1][1] - crossings[crossing_pos][1];
                width_end += crossings[crossing_pos + 1][2] - crossings[crossing_pos][2];
            }
        }
        area_profile.widths_start.push_back(width_start);
        area_profile.widths_end.push_back(width_end);
        area_profile.cumulative_areas.push_back(
                area_profile.cumulative_areas.back()
                + (end - start) * (width_start + width_end) / 2);
    }
    return area_profile;
}

int add_variable(
        mathoptsolverscmake::MathOptModel& model,
        double lower_bound,
        double upper_bound)
{
    model.variables_lower_bounds.push_back(lower_bound);
    model.variables_upper_bounds.push_back(upper_bound);
    model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
    model.objective_coefficients.push_back(0.0);
    return model.variables_lower_bounds.size() - 1;
}

void add_constraint(
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
}

}

namespace
{

/**
 * Status of the tree search for a set of items.
 *
 * For the open dimension objectives:
 * - 'Infeasible' means that the relaxation has no solution with an open
 *   dimension smaller than the upper bound, which is then a bound;
 * - 'Feasible' means that its minimum open dimension, the bound, is smaller.
 */
enum class Status
{
    /** Proven: the items don't fit in the bin. */
    Infeasible,

    /** The relaxation is feasible, so the items may fit in the bin. */
    Feasible,

    /** Not proven, because of the time limit or of a continuous rotation. */
    Unknown,
};

/** Output of the tree search for a set of items. */
struct ItemsOutput
{
    /** Status. */
    Status status = Status::Unknown;

    /** Number of nodes of the tree search. */
    Counter number_of_nodes = 0;

    /** Number of linear programs solved at the leaves of the tree search. */
    Counter number_of_linear_programs = 0;

    /** Maximum number of convex pieces of a pair of items. */
    Counter maximum_number_of_pieces = 0;

    /**
     * For the open dimension objectives, if the status is 'Feasible' or
     * 'Infeasible', bound on the open dimension: the largest coordinate of
     * the items along the open axis, in the original coordinates.
     */
    LengthDbl open_dimension_bound = 0;

    /**
     * If the status is 'Feasible', the positions of the items in a solution
     * of the relaxation (in the original coordinates); for the open dimension
     * objectives, in a solution of minimum open dimension.
     *
     * They are only a packing up to the relaxation: the shape of the bin,
     * its defects, its fixed items and the item-item spacing are ignored.
     */
    std::vector<SolutionItem> items;
};

/**
 * Tree search for the items of 'item_type_ids' (one entry per copy) in a bin
 * of type 'bin_type_id'; for the open dimension objectives, the search stops
 * once a solution with the open dimension 'open_dimension_lower_bound' (in
 * the original coordinates) is found.
 *
 * 'pairs_data' contains the data of the pairs of item types in each of their
 * rotations, computed when first needed; it is shared by the searches of the
 * subsets of items of a call to 'tree_search_relative_positions'.
 */
ItemsOutput tree_search_relative_positions_items(
        const Instance& instance,
        BinTypeId bin_type_id,
        const std::vector<ItemTypeId>& item_type_ids,
        const TreeSearchRelativePositionsParameters& parameters,
        LengthDbl open_dimension_lower_bound,
        std::map<TreeSearchRelativePositionsPairKey, PairData>& pairs_data)
{
    ItemsOutput output;
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const AxisAlignedBoundingBox& bin_aabb = bin_type.aabb_scaled;
    LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
    LengthDbl bin_width = bin_aabb.x_max - bin_aabb.x_min - 2 * bin_spacing;
    LengthDbl bin_height = bin_aabb.y_max - bin_aabb.y_min - 2 * bin_spacing;
    LengthDbl scale_value = instance.parameters().scale_value;
    LengthDbl tolerance = 1e-6 * (bin_width + bin_height + 1);
    const LengthDbl inf = std::numeric_limits<LengthDbl>::infinity();

    // Open axis: 0 for x, 1 for y, -1 if both dimensions are fixed.
    int open_axis = (instance.objective() == Objective::OpenDimensionX)? 0:
        (instance.objective() == Objective::OpenDimensionY)? 1: -1;
    // Start of the strip along the open axis.
    LengthDbl open_start = (open_axis == 0)?
        bin_aabb.x_min + bin_spacing:
        bin_aabb.y_min + bin_spacing;
    // Only the solutions whose open dimension (scaled) is smaller than the
    // threshold are searched; it decreases when better ones are found.
    std::atomic<LengthDbl> threshold(inf);
    if (open_axis != -1) {
        LengthDbl bin_end = (open_axis == 0)?
            bin_aabb.x_max - bin_spacing:
            bin_aabb.y_max - bin_spacing;
        threshold = (std::min)(
                bin_end + 2 * tolerance,
                parameters.open_dimension_upper_bound * scale_value);
    }

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
            output.status = Status::Unknown;
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
            if (packingsolver::strictly_lesser(rotation.x_max, rotation.x_min)
                    || packingsolver::strictly_lesser(rotation.y_max, rotation.y_min)) {
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
            output.status = Status::Infeasible;
            return output;
        }
    }
    if (number_of_items <= 1 && open_axis == -1) {
        output.status = Status::Feasible;
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

    // Data of the pairs of item types in each of their rotations.
    auto pair_data = [&](
            ItemPos item_pos_1,
            ItemPos item_pos_2,
            Counter rotation_id_1,
            Counter rotation_id_2) -> const PairData&
    {
        ItemTypeId item_type_id_1 = sorted_item_type_ids[item_pos_1];
        ItemTypeId item_type_id_2 = sorted_item_type_ids[item_pos_2];
        TreeSearchRelativePositionsPairKey key = std::make_tuple(
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

    // Solve a linear program.
    auto solve_linear_program = [&parameters](
            const mathoptsolverscmake::MathOptModel& model,
            std::vector<double>& values) -> Status
    {
        if (parameters.solver == mathoptsolverscmake::SolverName::Highs) {
#ifdef HIGHS_FOUND
            Highs highs;
            mathoptsolverscmake::reduce_printout(highs);
            mathoptsolverscmake::set_time_limit(highs, parameters.timer.remaining_time());
            mathoptsolverscmake::load(highs, model);
            mathoptsolverscmake::solve(highs);
            HighsModelStatus status = highs.getModelStatus();
            if (status == HighsModelStatus::kInfeasible)
                return Status::Infeasible;
            if (status == HighsModelStatus::kOptimal) {
                values = highs.getSolution().col_value;
                return Status::Feasible;
            }
            return Status::Unknown;
#else
            throw std::invalid_argument(FUNC_SIGNATURE);
#endif
        } else {
            throw std::invalid_argument(FUNC_SIGNATURE);
        }
    };

    // Rotations allowed for each item at the root. The item restricted by the
    // symmetry of the bin only keeps one rotation of each symmetric pair.
    std::vector<std::vector<Counter>> root_rotation_domains(number_of_items);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        ItemTypeId item_type_id = sorted_item_type_ids[item_pos];
        for (Counter rotation_id = 0;
                rotation_id < (Counter)rotations[item_type_id].size();
                ++rotation_id) {
            if (item_pos == symmetry_item_pos
                    && symmetric_rotation_ids[rotation_id] < rotation_id) {
                continue;
            }
            root_rotation_domains[item_pos].push_back(rotation_id);
        }
    }
    auto item_rotation = [&](ItemPos item_pos, Counter rotation_id) -> const ItemTypeRotationData&
    {
        return rotations[sorted_item_type_ids[item_pos]][rotation_id];
    };

    // Pairs of items, with the pieces of all their pairs of rotations. If two
    // items can't overlap in a pair of rotations, the piece of this pair of
    // rotations is the rectangle R, whose constraints are implied by the
    // bounds of the positions of the items. All the pair data are computed
    // here, so that they are only read by the threads.
    struct RotationPairPiece
    {
        Counter rotation_id_1;
        Counter rotation_id_2;
        const PairData* pair;
        const Piece* piece;
        /** 'true' iff the items can't overlap in these rotations. */
        bool free;
    };
    struct ItemPair
    {
        ItemPos item_pos_1;
        ItemPos item_pos_2;
        std::vector<RotationPairPiece> pieces;
    };
    std::deque<Piece> free_pieces;
    std::vector<ItemPair> pairs;
    // Pairs of each item.
    std::vector<std::vector<std::size_t>> item_pairs(number_of_items);
    for (ItemPos item_pos_1 = 0; item_pos_1 < number_of_items; ++item_pos_1) {
        for (ItemPos item_pos_2 = item_pos_1 + 1; item_pos_2 < number_of_items; ++item_pos_2) {
            if (parameters.timer.needs_to_end()) {
                output.status = Status::Unknown;
                return output;
            }
            ItemPair item_pair;
            item_pair.item_pos_1 = item_pos_1;
            item_pair.item_pos_2 = item_pos_2;
            for (Counter rotation_id_1: root_rotation_domains[item_pos_1]) {
                for (Counter rotation_id_2: root_rotation_domains[item_pos_2]) {
                    const PairData& pair = pair_data(item_pos_1, item_pos_2, rotation_id_1, rotation_id_2);
                    if (!pair.interact) {
                        free_pieces.push_back(rectangle_piece({pair.x_min, pair.x_max, pair.y_min, pair.y_max}));
                        item_pair.pieces.push_back({rotation_id_1, rotation_id_2, &pair, &free_pieces.back(), true});
                        continue;
                    }
                    output.maximum_number_of_pieces = (std::max)(
                            output.maximum_number_of_pieces,
                            (Counter)pair.pieces.size());
                    for (const Piece& piece: pair.pieces)
                        item_pair.pieces.push_back({rotation_id_1, rotation_id_2, &pair, &piece, false});
                }
            }
            item_pairs[item_pos_1].push_back(pairs.size());
            item_pairs[item_pos_2].push_back(pairs.size());
            pairs.push_back(item_pair);
        }
    }

    // Area profiles of the items in their rotations along the fixed axis (the
    // y-axis if both dimensions are fixed), for the core bound.
    int fixed_axis = (open_axis == 0 || open_axis == -1)? 1: 0;
    std::map<std::pair<ItemTypeId, Counter>, AreaProfile> area_profiles;
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        ItemTypeId item_type_id = sorted_item_type_ids[item_pos];
        for (Counter rotation_id: root_rotation_domains[item_pos]) {
            auto key = std::make_pair(item_type_id, rotation_id);
            if (area_profiles.find(key) == area_profiles.end())
                area_profiles[key] = compute_area_profile(item_rotation(item_pos, rotation_id).shapes, fixed_axis);
        }
    }
    std::vector<std::vector<const AreaProfile*>> item_area_profiles(number_of_items);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        ItemTypeId item_type_id = sorted_item_type_ids[item_pos];
        item_area_profiles[item_pos].resize(rotations[item_type_id].size(), nullptr);
        for (Counter rotation_id: root_rotation_domains[item_pos])
            item_area_profiles[item_pos][rotation_id] = &area_profiles[std::make_pair(item_type_id, rotation_id)];
    }

    // Depth-first tree search on the rotations of the items and on the
    // pieces of the pairs.
    //
    // A node restricts the rotations of each item and the pieces of each
    // pair. Its positions are bounded by two difference-bound matrices, one
    // for each axis, on the reference points of the items and an origin:
    // 'ub[a][b]' bounds 'v_b - v_a' from above. For the open dimension
    // objectives, the matrices have another node, the end of the strip along
    // the open axis, and only the solutions whose open dimension is smaller
    // than the threshold are searched; the tree search continues after
    // finding one, to find the minimum open dimension.
    ItemPos l = (open_axis != -1)? number_of_items + 1: -1;
    ItemPos m = (open_axis != -1)? number_of_items + 2: number_of_items + 1;
    bool optimize = (open_axis != -1);
    std::atomic<bool> stop(false);
    std::atomic<Counter> number_of_nodes(0);
    std::atomic<Counter> number_of_linear_programs(0);
    std::mutex best_mutex;
    // Set when a solution reaches the lower bound of the parameters.
    std::atomic<bool> lower_bound_reached(false);
    // Open dimension of the best solution found.
    LengthDbl best_value = inf;
    // Write the positions of a solution in the output.
    auto set_output_items = [&](
            const std::vector<std::vector<Counter>>& rotation_domains,
            const std::vector<double>& values)
    {
        output.items.clear();
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            const ItemTypeRotationData& rotation = item_rotation(item_pos, rotation_domains[item_pos].front());
            SolutionItem item;
            item.item_type_id = sorted_item_type_ids[item_pos];
            item.bl_corner.x = values[2 * item_pos] / scale_value;
            item.bl_corner.y = values[2 * item_pos + 1] / scale_value;
            item.angle = rotation.angle;
            item.mirror = rotation.mirror;
            output.items.push_back(item);
        }
    };

    struct Node
    {
        /** Difference-bound matrices, for x and y, of size m * m. */
        std::vector<LengthDbl> ub_x;
        std::vector<LengthDbl> ub_y;

        /** Remaining rotations of each item, sorted. */
        std::vector<std::vector<Counter>> rotation_domains;

        /** Remaining pieces of each pair. */
        std::vector<std::vector<Counter>> domains;

        /**
         * Box of the relative positions of each pair when its pieces were
         * last filtered; infinite if they must be filtered again.
         */
        std::vector<std::array<LengthDbl, 4>> filtered_boxes;

        /** 'true' iff the matrices are closed. */
        bool closed = false;

        /** 'true' iff the node has been propagated. */
        bool propagated = false;
    };

    // Data of a thread.
    struct ThreadData
    {
        /** Entries tightened since the last update of the closure. */
        std::vector<std::tuple<int, ItemPos, ItemPos>> tightened_entries;

        std::vector<Point> clip_buffer_1;
        std::vector<Point> clip_buffer_2;

        /** Rotations supported by the pieces of a pair. */
        std::vector<char> supported_1;
        std::vector<char> supported_2;

        /** Allowed rotations of each item. */
        std::vector<std::vector<char>> allowed;

        /** Ranges of the other items, for the core bound. */
        std::vector<std::pair<LengthDbl, LengthDbl>> intrusions;

        /** Positions of the solution of the last linear program. */
        std::vector<double> values;
    };

    Node root;
    root.ub_x.assign(m * m, inf);
    root.ub_y.assign(m * m, inf);
    for (ItemPos a = 0; a < m; ++a) {
        root.ub_x[a * m + a] = 0;
        root.ub_y[a * m + a] = 0;
    }
    root.rotation_domains = root_rotation_domains;
    for (const ItemPair& pair: pairs) {
        std::vector<Counter> domain(pair.pieces.size());
        std::iota(domain.begin(), domain.end(), 0);
        root.domains.push_back(domain);
    }
    root.filtered_boxes.assign(pairs.size(), {inf, inf, inf, inf});

    // Tighten 'ub[a][b]' to 'value'; return 'true' if it changed. The
    // tightened entries are recorded (the axis and the nodes), for the
    // update of the closure.
    auto tighten = [&](
            ThreadData& thread_data,
            std::vector<LengthDbl>& ub,
            int axis,
            ItemPos a,
            ItemPos b,
            LengthDbl value)
    {
        if (value < ub[a * m + b] - tolerance) {
            ub[a * m + b] = value;
            thread_data.tightened_entries.push_back(std::make_tuple(axis, a, b));
            return true;
        }
        return false;
    };
    // Update the closure of a closed matrix after the tightening of entry
    // (a, b); return 'false' if it is inconsistent.
    auto close_entry = [&](std::vector<LengthDbl>& ub, ItemPos a, ItemPos b)
    {
        LengthDbl value = ub[a * m + b];
        for (ItemPos c = 0; c < m; ++c) {
            LengthDbl value_ca = ub[c * m + a];
            if (value_ca == inf)
                continue;
            for (ItemPos d = 0; d < m; ++d)
                if (value_ca + value + ub[b * m + d] < ub[c * m + d])
                    ub[c * m + d] = value_ca + value + ub[b * m + d];
        }
        for (ItemPos c = 0; c < m; ++c)
            if (ub[c * m + c] < -tolerance)
                return false;
        return true;
    };
    // Close a matrix; return 'false' if it is inconsistent.
    auto close = [&](std::vector<LengthDbl>& ub)
    {
        for (ItemPos c = 0; c < m; ++c)
            for (ItemPos a = 0; a < m; ++a)
                for (ItemPos b = 0; b < m; ++b)
                    if (ub[a * m + c] + ub[c * m + b] < ub[a * m + b])
                        ub[a * m + b] = ub[a * m + c] + ub[c * m + b];
        for (ItemPos a = 0; a < m; ++a)
            if (ub[a * m + a] < -tolerance)
                return false;
        return true;
    };
    // Bound on the open dimension of the solutions of a node.
    auto node_bound = [&](const Node& node)
    {
        return (open_axis == 0)? -node.ub_x[l * m + 0]: -node.ub_y[l * m + 0];
    };
    auto axis_min = [](const AxisAlignedBoundingBox& aabb, int axis) { return (axis == 0)? aabb.x_min: aabb.y_min; };
    auto axis_max = [](const AxisAlignedBoundingBox& aabb, int axis) { return (axis == 0)? aabb.x_max: aabb.y_max; };
    // Restrict the rotations of an item to the ones satisfying 'keep';
    // return 'true' if some are removed. The pieces of its pairs must then
    // be filtered again.
    auto restrict_rotations = [&](Node& node, ItemPos item_pos, const std::function<bool(Counter)>& keep)
    {
        std::vector<Counter>& rotation_domain = node.rotation_domains[item_pos];
        std::size_t size = 0;
        for (Counter rotation_id: rotation_domain)
            if (keep(rotation_id))
                rotation_domain[size++] = rotation_id;
        if (size == rotation_domain.size())
            return false;
        rotation_domain.resize(size);
        for (std::size_t pair_pos: item_pairs[item_pos])
            node.filtered_boxes[pair_pos] = {inf, inf, inf, inf};
        return true;
    };

    // Core bound: the core of an item along the fixed axis is the part of
    // its range which the other items can't reach, given the pieces and the
    // relative positions of the node. The band of the core of an item, along
    // the bin, contains no other item, so the item can be widened to the
    // length of the bin on its core. The cores of different items are
    // disjoint, so the node is infeasible if the sum of the widened areas
    // exceeds the area of the bin. For the open dimension objectives, the
    // length of the bin is the threshold. Since the rotations of the items
    // may not be fixed, the smallest widened area of their rotations is
    // taken. Return 'false' if the node is infeasible.
    LengthDbl fixed_size = (fixed_axis == 1)? bin_height: bin_width;
    auto core_bound = [&](ThreadData& thread_data, const Node& node) -> bool
    {
        LengthDbl length = (open_axis == -1)?
            bin_width:
            (LengthDbl)threshold - open_start;
        if (length == inf)
            return true;
        const std::vector<LengthDbl>& ub = (fixed_axis == 0)? node.ub_x: node.ub_y;
        AreaDbl widened_area = 0;
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            ItemPos a = item_pos + 1;
            AreaDbl item_widened_area = inf;
            for (Counter rotation_id: node.rotation_domains[item_pos]) {
                const ItemTypeRotationData& rotation = item_rotation(item_pos, rotation_id);
                LengthDbl range_min = axis_min(rotation.aabb, fixed_axis);
                LengthDbl range_max = axis_max(rotation.aabb, fixed_axis);
                std::vector<std::pair<LengthDbl, LengthDbl>>& intrusions = thread_data.intrusions;
                intrusions.clear();
                for (std::size_t pair_pos: item_pairs[item_pos]) {
                    const ItemPair& pair = pairs[pair_pos];
                    // The relative positions are d = p_2 - p_1; the one of
                    // the other item is the opposite if it comes first.
                    bool reversed = (pair.item_pos_2 == item_pos);
                    ItemPos b = (reversed)? pair.item_pos_1 + 1: pair.item_pos_2 + 1;
                    // Range of the relative position of the other item.
                    LengthDbl d_min_box = -ub[b * m + a];
                    LengthDbl d_max_box = ub[a * m + b];
                    for (Counter piece_pos: node.domains[pair_pos]) {
                        const RotationPairPiece& rotation_pair_piece = pair.pieces[piece_pos];
                        Counter item_rotation_id = (reversed)?
                            rotation_pair_piece.rotation_id_2:
                            rotation_pair_piece.rotation_id_1;
                        if (item_rotation_id != rotation_id)
                            continue;
                        Counter other_rotation_id = (reversed)?
                            rotation_pair_piece.rotation_id_1:
                            rotation_pair_piece.rotation_id_2;
                        const AxisAlignedBoundingBox& other_aabb = item_rotation(
                                (reversed)? pair.item_pos_1: pair.item_pos_2,
                                other_rotation_id).aabb;
                        const Piece& piece = *rotation_pair_piece.piece;
                        LengthDbl d_min = (fixed_axis == 0)? piece.x_min: piece.y_min;
                        LengthDbl d_max = (fixed_axis == 0)? piece.x_max: piece.y_max;
                        if (reversed) {
                            LengthDbl tmp = d_min;
                            d_min = -d_max;
                            d_max = -tmp;
                        }
                        d_min = (std::max)(d_min, d_min_box - tolerance);
                        d_max = (std::min)(d_max, d_max_box + tolerance);
                        LengthDbl intrusion_min = (std::max)(d_min + axis_min(other_aabb, fixed_axis), range_min);
                        LengthDbl intrusion_max = (std::min)(d_max + axis_max(other_aabb, fixed_axis), range_max);
                        if (intrusion_min < intrusion_max)
                            intrusions.push_back({intrusion_min, intrusion_max});
                    }
                }
                // Core: complement of the union of the intrusions.
                std::sort(intrusions.begin(), intrusions.end());
                const AreaProfile& area_profile = *item_area_profiles[item_pos][rotation_id];
                LengthDbl core_length = 0;
                AreaDbl core_area = 0;
                LengthDbl current = range_min;
                auto add_core = [&](LengthDbl start, LengthDbl end)
                {
                    core_length += end - start;
                    core_area += area_profile.area(start, end);
                };
                for (const auto& intrusion: intrusions) {
                    if (intrusion.first > current)
                        add_core(current, intrusion.first);
                    current = (std::max)(current, intrusion.second);
                }
                if (range_max > current)
                    add_core(current, range_max);
                AreaDbl outside_area = (std::max)((AreaDbl)0, rotation.area - core_area);
                item_widened_area = (std::min)(
                        item_widened_area,
                        outside_area + length * core_length);
            }
            widened_area += item_widened_area;
        }
        // The tolerance keeps the bound valid despite the rounding errors.
        return !strictly_greater_profit(widened_area, length * (fixed_size + tolerance));
    };

    // Propagate; return 'false' if the node is infeasible or can't contain a
    // solution whose open dimension is smaller than the threshold.
    //
    // The propagation alternates the closure of the matrices with the
    // filtering of the rotations and of the pieces:
    // - the rotations of an item whose inner-fit rectangle doesn't intersect
    //   the bounds of its position are removed, and the bounds are shrunk to
    //   the union of the inner-fit rectangles of the remaining ones;
    // - the pieces which don't intersect the box of the relative positions
    //   of their pair (and the rectangle R of their rotations) are removed,
    //   and the box is shrunk to the bounding box of the remaining pieces
    //   clipped to it;
    // - the rotations of an item which no remaining piece of one of its pairs
    //   supports are removed.
    auto propagate = [&](ThreadData& thread_data, Node& node) -> bool
    {
        thread_data.tightened_entries.clear();
        if (open_axis != -1) {
            LengthDbl current_threshold = threshold;
            std::vector<LengthDbl>& ub = (open_axis == 0)? node.ub_x: node.ub_y;
            if (tighten(thread_data, ub, open_axis, 0, l, current_threshold))
                node.propagated = false;
        }
        if (node.propagated)
            return true;
        node.propagated = true;
        if (!node.closed) {
            if (!close(node.ub_x) || !close(node.ub_y))
                return false;
            node.closed = true;
            thread_data.tightened_entries.clear();
        }
        for (;;) {
            for (const auto& entry: thread_data.tightened_entries) {
                std::vector<LengthDbl>& ub = (std::get<0>(entry) == 0)? node.ub_x: node.ub_y;
                if (!close_entry(ub, std::get<1>(entry), std::get<2>(entry)))
                    return false;
            }
            thread_data.tightened_entries.clear();
            if (open_axis != -1 && node_bound(node) >= threshold - tolerance)
                return false;
            bool changed = false;

            // Rotations of the items.
            for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
                ItemPos a = item_pos + 1;
                // Identical items: their rotations are non-decreasing, and
                // the ones with the same rotation are sorted by x.
                if (item_pos + 1 < number_of_items
                        && sorted_item_type_ids[item_pos] == sorted_item_type_ids[item_pos + 1]) {
                    Counter rotation_id_min = node.rotation_domains[item_pos].front();
                    Counter rotation_id_max = node.rotation_domains[item_pos + 1].back();
                    if (restrict_rotations(node, item_pos + 1, [rotation_id_min](Counter rotation_id) { return rotation_id >= rotation_id_min; }))
                        changed = true;
                    if (restrict_rotations(node, item_pos, [rotation_id_max](Counter rotation_id) { return rotation_id <= rotation_id_max; }))
                        changed = true;
                    if (node.rotation_domains[item_pos].empty() || node.rotation_domains[item_pos + 1].empty())
                        return false;
                    if (node.rotation_domains[item_pos].size() == 1
                            && node.rotation_domains[item_pos + 1] == node.rotation_domains[item_pos]) {
                        tighten(thread_data, node.ub_x, 0, a + 1, a, 0);
                    }
                }
                LengthDbl x_min = -node.ub_x[a * m + 0] - tolerance;
                LengthDbl x_max = node.ub_x[0 * m + a] + tolerance;
                LengthDbl y_min = -node.ub_y[a * m + 0] - tolerance;
                LengthDbl y_max = node.ub_y[0 * m + a] + tolerance;
                LengthDbl open_max = (open_axis == 0)? node.ub_x[a * m + l]: (open_axis == 1)? node.ub_y[a * m + l]: inf;
                if (restrict_rotations(node, item_pos, [&](Counter rotation_id)
                            {
                                const ItemTypeRotationData& rotation = item_rotation(item_pos, rotation_id);
                                return rotation.x_min <= x_max && rotation.x_max >= x_min
                                    && rotation.y_min <= y_max && rotation.y_max >= y_min
                                    && (open_axis == -1 || axis_max(rotation.aabb, open_axis) <= open_max + tolerance);
                            })) {
                    changed = true;
                }
                if (node.rotation_domains[item_pos].empty())
                    return false;
                LengthDbl union_x_min = inf;
                LengthDbl union_x_max = -inf;
                LengthDbl union_y_min = inf;
                LengthDbl union_y_max = -inf;
                LengthDbl open_extent = inf;
                for (Counter rotation_id: node.rotation_domains[item_pos]) {
                    const ItemTypeRotationData& rotation = item_rotation(item_pos, rotation_id);
                    union_x_min = (std::min)(union_x_min, rotation.x_min);
                    union_x_max = (std::max)(union_x_max, rotation.x_max);
                    union_y_min = (std::min)(union_y_min, rotation.y_min);
                    union_y_max = (std::max)(union_y_max, rotation.y_max);
                    if (open_axis != -1)
                        open_extent = (std::min)(open_extent, axis_max(rotation.aabb, open_axis));
                }
                tighten(thread_data, node.ub_x, 0, 0, a, union_x_max);
                tighten(thread_data, node.ub_x, 0, a, 0, -union_x_min);
                tighten(thread_data, node.ub_y, 1, 0, a, union_y_max);
                tighten(thread_data, node.ub_y, 1, a, 0, -union_y_min);
                // The items precede the end of the strip.
                if (open_axis == 0)
                    tighten(thread_data, node.ub_x, 0, l, a, -open_extent);
                if (open_axis == 1)
                    tighten(thread_data, node.ub_y, 1, l, a, -open_extent);
            }
            if (!thread_data.tightened_entries.empty())
                continue;

            // Pieces of the pairs.
            for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
                std::vector<char>& allowed = thread_data.allowed[item_pos];
                std::fill(allowed.begin(), allowed.end(), 0);
                for (Counter rotation_id: node.rotation_domains[item_pos])
                    allowed[rotation_id] = 1;
            }
            for (std::size_t pair_pos = 0; pair_pos < pairs.size(); ++pair_pos) {
                const ItemPair& pair = pairs[pair_pos];
                ItemPos a = pair.item_pos_1 + 1;
                ItemPos b = pair.item_pos_2 + 1;
                std::array<LengthDbl, 4> box = {{
                    -node.ub_x[b * m + a],
                    node.ub_x[a * m + b],
                    -node.ub_y[b * m + a],
                    node.ub_y[a * m + b]}};
                // The pieces have already been filtered with this box.
                if (box == node.filtered_boxes[pair_pos])
                    continue;
                node.filtered_boxes[pair_pos] = box;
                const std::vector<char>& allowed_1 = thread_data.allowed[pair.item_pos_1];
                const std::vector<char>& allowed_2 = thread_data.allowed[pair.item_pos_2];
                std::vector<char>& supported_1 = thread_data.supported_1;
                std::vector<char>& supported_2 = thread_data.supported_2;
                supported_1.assign(allowed_1.size(), 0);
                supported_2.assign(allowed_2.size(), 0);
                std::vector<Counter>& domain = node.domains[pair_pos];
                std::size_t domain_size = 0;
                LengthDbl hull_x_min = inf;
                LengthDbl hull_x_max = -inf;
                LengthDbl hull_y_min = inf;
                LengthDbl hull_y_max = -inf;
                for (Counter piece_pos: domain) {
                    const RotationPairPiece& rotation_pair_piece = pair.pieces[piece_pos];
                    if (!allowed_1[rotation_pair_piece.rotation_id_1]
                            || !allowed_2[rotation_pair_piece.rotation_id_2]) {
                        continue;
                    }
                    const PairData& pair_data = *rotation_pair_piece.pair;
                    LengthDbl x_min = (std::max)(box[0], pair_data.x_min) - tolerance;
                    LengthDbl x_max = (std::min)(box[1], pair_data.x_max) + tolerance;
                    LengthDbl y_min = (std::max)(box[2], pair_data.y_min) - tolerance;
                    LengthDbl y_max = (std::min)(box[3], pair_data.y_max) + tolerance;
                    const Piece& piece = *rotation_pair_piece.piece;
                    if (piece.x_min > x_max || piece.x_max < x_min
                            || piece.y_min > y_max || piece.y_max < y_min) {
                        continue;
                    }
                    AxisAlignedBoundingBox clipped;
                    if (piece.x_min >= x_min && piece.x_max <= x_max
                            && piece.y_min >= y_min && piece.y_max <= y_max) {
                        // The piece is inside the box.
                        clipped.x_min = piece.x_min;
                        clipped.x_max = piece.x_max;
                        clipped.y_min = piece.y_min;
                        clipped.y_max = piece.y_max;
                    } else if (!clip_bounding_box(
                                piece.vertices,
                                x_min,
                                x_max,
                                y_min,
                                y_max,
                                thread_data.clip_buffer_1,
                                thread_data.clip_buffer_2,
                                clipped)) {
                        continue;
                    }
                    domain[domain_size] = piece_pos;
                    domain_size++;
                    supported_1[rotation_pair_piece.rotation_id_1] = 1;
                    supported_2[rotation_pair_piece.rotation_id_2] = 1;
                    hull_x_min = (std::min)(hull_x_min, clipped.x_min);
                    hull_x_max = (std::max)(hull_x_max, clipped.x_max);
                    hull_y_min = (std::min)(hull_y_min, clipped.y_min);
                    hull_y_max = (std::max)(hull_y_max, clipped.y_max);
                }
                if (domain_size == 0)
                    return false;
                domain.resize(domain_size);
                tighten(thread_data, node.ub_x, 0, a, b, hull_x_max);
                tighten(thread_data, node.ub_x, 0, b, a, -hull_x_min);
                tighten(thread_data, node.ub_y, 1, a, b, hull_y_max);
                tighten(thread_data, node.ub_y, 1, b, a, -hull_y_min);
                if (restrict_rotations(node, pair.item_pos_1, [&supported_1](Counter rotation_id) { return supported_1[rotation_id]; })) {
                    changed = true;
                    for (Counter rotation_id = 0; rotation_id < (Counter)allowed_1.size(); ++rotation_id)
                        thread_data.allowed[pair.item_pos_1][rotation_id] = supported_1[rotation_id];
                }
                if (restrict_rotations(node, pair.item_pos_2, [&supported_2](Counter rotation_id) { return supported_2[rotation_id]; })) {
                    changed = true;
                    for (Counter rotation_id = 0; rotation_id < (Counter)allowed_2.size(); ++rotation_id)
                        thread_data.allowed[pair.item_pos_2][rotation_id] = supported_2[rotation_id];
                }
            }
            if (thread_data.tightened_entries.empty() && !changed)
                break;
        }
        return core_bound(thread_data, node);
    };

    // Linear program of a leaf, in which each item has a single rotation and
    // each pair a single piece; for the open dimension objectives, it
    // minimizes the open dimension, which is the last variable.
    auto solve_leaf = [&](ThreadData& thread_data, const Node& node) -> Status
    {
        number_of_linear_programs++;
        mathoptsolverscmake::MathOptModel model;
        model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Minimize;
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            const ItemTypeRotationData& rotation = item_rotation(item_pos, node.rotation_domains[item_pos].front());
            add_variable(model, rotation.x_min, rotation.x_max);
            add_variable(model, rotation.y_min, rotation.y_max);
        }
        if (open_axis != -1) {
            const std::vector<LengthDbl>& ub = (open_axis == 0)? node.ub_x: node.ub_y;
            int variable_id = add_variable(
                    model,
                    -ub[l * m + 0] - tolerance,
                    ub[0 * m + l] + tolerance);
            model.objective_coefficients[variable_id] = 1.0;
        }
        // Variable of a node along an axis, -1 for the end of the strip
        // along the fixed axis.
        auto node_variable_id = [&](ItemPos node_id, int axis) -> int
        {
            if (node_id == l)
                return (axis == open_axis)? 2 * number_of_items: -1;
            return 2 * (node_id - 1) + axis;
        };
        for (ItemPos a = 1; a < m; ++a) {
            for (ItemPos b = 1; b < m; ++b) {
                if (a == b)
                    continue;
                for (int axis = 0; axis < 2; ++axis) {
                    const std::vector<LengthDbl>& ub = (axis == 0)? node.ub_x: node.ub_y;
                    int variable_id_a = node_variable_id(a, axis);
                    int variable_id_b = node_variable_id(b, axis);
                    if (ub[a * m + b] == inf
                            || variable_id_a == -1
                            || variable_id_b == -1) {
                        continue;
                    }
                    add_constraint(model, -inf, ub[a * m + b] + tolerance,
                            {{variable_id_b, 1.0}, {variable_id_a, -1.0}});
                }
            }
        }
        for (std::size_t pair_pos = 0; pair_pos < pairs.size(); ++pair_pos) {
            const ItemPair& pair = pairs[pair_pos];
            const RotationPairPiece& rotation_pair_piece = pair.pieces[node.domains[pair_pos].front()];
            if (rotation_pair_piece.free)
                continue;
            for (const HalfPlane& half_plane: rotation_pair_piece.piece->half_planes) {
                add_constraint(model, half_plane.c, inf, {
                        {2 * pair.item_pos_2, half_plane.nx},
                        {2 * pair.item_pos_2 + 1, half_plane.ny},
                        {2 * pair.item_pos_1, -half_plane.nx},
                        {2 * pair.item_pos_1 + 1, -half_plane.ny}});
            }
        }
        return solve_linear_program(model, thread_data.values);
    };

    // Children of a propagated node; none if it is a leaf.
    //
    // The search first fixes the rotations of the items, the largest ones
    // first; then, it branches on the pair with the fewest pieces left.
    auto branch = [&](const Node& node) -> std::vector<Node>
    {
        ItemPos branching_item_pos = -1;
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            if (node.rotation_domains[item_pos].size() <= 1)
                continue;
            if (branching_item_pos == -1
                    || item_rotation(item_pos, 0).area > item_rotation(branching_item_pos, 0).area) {
                branching_item_pos = item_pos;
            }
        }
        std::size_t branching_pair_pos = pairs.size();
        if (branching_item_pos == -1) {
            for (std::size_t pair_pos = 0; pair_pos < pairs.size(); ++pair_pos) {
                if (node.domains[pair_pos].size() <= 1)
                    continue;
                if (branching_pair_pos == pairs.size()
                        || node.domains[pair_pos].size() < node.domains[branching_pair_pos].size()) {
                    branching_pair_pos = pair_pos;
                }
            }
        }
        std::vector<Node> children;
        if (branching_item_pos != -1) {
            for (Counter rotation_id: node.rotation_domains[branching_item_pos]) {
                Node child = node;
                child.rotation_domains[branching_item_pos] = {rotation_id};
                for (std::size_t pair_pos: item_pairs[branching_item_pos])
                    child.filtered_boxes[pair_pos] = {inf, inf, inf, inf};
                child.propagated = false;
                children.push_back(child);
            }
        } else if (branching_pair_pos != pairs.size()) {
            for (Counter piece_pos: node.domains[branching_pair_pos]) {
                Node child = node;
                child.domains[branching_pair_pos] = {piece_pos};
                child.filtered_boxes[branching_pair_pos] = {inf, inf, inf, inf};
                child.propagated = false;
                children.push_back(child);
            }
        }
        return children;
    };

    // Evaluate a leaf.
    auto evaluate_leaf = [&](ThreadData& thread_data, const Node& node) -> Status
    {
        Status leaf_status = solve_leaf(thread_data, node);
        if (leaf_status != Status::Feasible)
            return leaf_status;
        const std::vector<double>& values = thread_data.values;
        if (open_axis == -1) {
            std::lock_guard<std::mutex> lock(best_mutex);
            if (!stop) {
                set_output_items(node.rotation_domains, values);
                stop = true;
            }
            return leaf_status;
        }
        // Open dimension of the solution, which may exceed the value of the
        // linear program by the tolerance; so that it is equal to the one of
        // the packing if the solution is one.
        LengthDbl leaf_value = values[2 * number_of_items];
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
            const AxisAlignedBoundingBox& aabb = item_rotation(item_pos, node.rotation_domains[item_pos].front()).aabb;
            leaf_value = (std::max)(leaf_value, values[2 * item_pos + open_axis] + axis_max(aabb, open_axis));
        }
        std::lock_guard<std::mutex> lock(best_mutex);
        if (leaf_value >= threshold - tolerance)
            return Status::Infeasible;
        threshold = leaf_value;
        best_value = leaf_value;
        set_output_items(node.rotation_domains, values);
        if (leaf_value <= open_dimension_lower_bound * scale_value + tolerance) {
            lower_bound_reached = true;
            stop = true;
        }
        return leaf_status;
    };

    // Propagate the children of a node for the open dimension objectives,
    // and sort them by bound.
    auto propagate_children = [&](ThreadData& thread_data, std::vector<Node>& children)
    {
        if (!optimize)
            return;
        std::vector<Node> kept_children;
        for (Node& child: children)
            if (propagate(thread_data, child))
                kept_children.push_back(child);
        std::stable_sort(
                kept_children.begin(),
                kept_children.end(),
                [&node_bound](const Node& node_1, const Node& node_2)
                {
                    return node_bound(node_1) < node_bound(node_2);
                });
        children.swap(kept_children);
    };

    // Depth-first search. For the feasibility, return 'Feasible' as soon as
    // a solution is found; for the open dimension objectives, 'Feasible' if a
    // solution has been found in the subtree.
    std::function<Status(ThreadData&, Node&)> search = [&](
            ThreadData& thread_data,
            Node& node) -> Status
    {
        number_of_nodes++;
        if (stop || parameters.timer.needs_to_end())
            return Status::Unknown;
        if (!propagate(thread_data, node))
            return Status::Infeasible;
        std::vector<Node> children = branch(node);
        if (children.empty())
            return evaluate_leaf(thread_data, node);
        propagate_children(thread_data, children);
        Status node_status = Status::Infeasible;
        for (Node& child: children) {
            Status child_status = search(thread_data, child);
            if (child_status == Status::Feasible) {
                if (!optimize)
                    return child_status;
                if (node_status == Status::Infeasible)
                    node_status = child_status;
            }
            if (child_status == Status::Unknown)
                node_status = Status::Unknown;
        }
        return node_status;
    };

    // The first levels of the tree are expanded breadth-first, until there
    // are enough subtrees for the threads, which then explore them
    // depth-first.
    auto new_thread_data = [&]()
    {
        ThreadData thread_data;
        thread_data.allowed.resize(number_of_items);
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
            thread_data.allowed[item_pos].assign(rotations[sorted_item_type_ids[item_pos]].size(), 0);
        return thread_data;
    };
    std::mutex status_mutex;
    bool feasible = false;
    bool unknown = false;
    auto record_status = [&](Status node_status)
    {
        std::lock_guard<std::mutex> lock(status_mutex);
        if (node_status == Status::Feasible)
            feasible = true;
        if (node_status == Status::Unknown)
            unknown = true;
    };
    Counter number_of_threads = (std::max)((Counter)1, parameters.number_of_threads);
    std::vector<Node> subtrees;
    {
        ThreadData thread_data = new_thread_data();
        std::deque<Node> queue = {root};
        while (!queue.empty()
                && (Counter)queue.size() < 4 * number_of_threads
                && number_of_threads > 1) {
            if (stop || parameters.timer.needs_to_end()) {
                record_status(Status::Unknown);
                queue.clear();
                break;
            }
            Node node = queue.front();
            queue.pop_front();
            number_of_nodes++;
            if (!propagate(thread_data, node))
                continue;
            std::vector<Node> children = branch(node);
            if (children.empty()) {
                record_status(evaluate_leaf(thread_data, node));
                continue;
            }
            propagate_children(thread_data, children);
            for (Node& child: children)
                queue.push_back(child);
        }
        subtrees.assign(queue.begin(), queue.end());
        if (optimize) {
            std::stable_sort(
                    subtrees.begin(),
                    subtrees.end(),
                    [&node_bound](const Node& node_1, const Node& node_2)
                    {
                        return node_bound(node_1) < node_bound(node_2);
                    });
        }
    }
    std::atomic<Counter> next_subtree_pos(0);
    auto worker = [&]()
    {
        ThreadData thread_data = new_thread_data();
        for (;;) {
            Counter subtree_pos = next_subtree_pos++;
            if (subtree_pos >= (Counter)subtrees.size())
                return;
            record_status(search(thread_data, subtrees[subtree_pos]));
        }
    };
    std::vector<std::thread> threads;
    for (Counter thread_id = 1;
            thread_id < (std::min)(number_of_threads, (Counter)subtrees.size());
            ++thread_id) {
        threads.push_back(std::thread(worker));
    }
    worker();
    for (std::thread& thread: threads)
        thread.join();
    output.number_of_nodes = number_of_nodes;
    output.number_of_linear_programs = number_of_linear_programs;

    if (open_axis == -1) {
        output.status = (feasible)? Status::Feasible:
            (unknown)? Status::Unknown:
            Status::Infeasible;
        return output;
    }
    if (!lower_bound_reached
            && (unknown || parameters.timer.needs_to_end())) {
        output.status = Status::Unknown;
        return output;
    }
    // The threshold may come from the upper bound of the parameters.
    output.status = (best_value <= threshold)?
        Status::Feasible:
        Status::Infeasible;
    output.open_dimension_bound = threshold / scale_value;
    return output;
}


/**
 * Build the solution, with a single bin of type 'bin_type_id', of the
 * positions of the items of a 'Feasible' output, and return 'true' if it is
 * feasible.
 *
 * The solution is only checked for overlaps, so it isn't built if the items
 * must be spaced or if the bin has fixed items, which the tree search
 * ignores.
 */
bool build_solution(
        const Instance& instance,
        BinTypeId bin_type_id,
        const ItemsOutput& items_output,
        Solution& solution)
{
    if (items_output.status != Status::Feasible
            || items_output.items.empty()
            || instance.parameters().item_item_minimum_spacing > 0
            || !instance.bin_type(bin_type_id).fixed_items.empty()) {
        return false;
    }
    SolutionBuilder solution_builder(instance);
    BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);
    for (const SolutionItem& item: items_output.items) {
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

}

TreeSearchRelativePositionsOutput packingsolver::irregular::tree_search_relative_positions(
        const Instance& instance,
        const TreeSearchRelativePositionsParameters& parameters)
{
    TreeSearchRelativePositionsOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    bool open_dimension = (instance.objective() == Objective::OpenDimensionX
            || instance.objective() == Objective::OpenDimensionY);
    if (!open_dimension
            && (instance.objective() != Objective::Feasibility
                || instance.number_of_bins() != 1)) {
        throw std::invalid_argument(FUNC_SIGNATURE + ": unsupported objective.");
    }

    BinTypeId bin_type_id = 0;
    while (instance.bin_type(bin_type_id).copies == 0)
        bin_type_id++;
    // The fixed items are ignored.
    std::vector<ItemTypeId> item_type_ids;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = item_type.copies_fixed; copy < item_type.copies; ++copy)
            item_type_ids.push_back(item_type_id);
    }
    std::map<TreeSearchRelativePositionsPairKey, PairData> pairs_data;
    auto update_statistics = [&output](const ItemsOutput& items_output)
    {
        output.number_of_nodes += items_output.number_of_nodes;
        output.number_of_linear_programs += items_output.number_of_linear_programs;
        output.maximum_number_of_pieces = (std::max)(
                output.maximum_number_of_pieces,
                items_output.maximum_number_of_pieces);
    };

    // The bound of a subset of the items is a bound for all of them, so the
    // increasing subsets of the largest items are solved one after the other.
    // For the feasibility objective, the search stops at the first subset
    // that doesn't fit in the bin; this avoids branching on the rotations
    // of the small items when the large ones are enough to prove the
    // infeasibility. For the open dimension objectives, the bounds of the
    // subsets are computed until the time limit.
    std::stable_sort(
            item_type_ids.begin(),
            item_type_ids.end(),
            [&instance](ItemTypeId item_type_id_1, ItemTypeId item_type_id_2)
            {
                return instance.item_type(item_type_id_1).space()
                    > instance.item_type(item_type_id_2).space();
            });

    // Infeasible subset of the 'number_of_items' largest items.
    auto infeasible_subset = [&instance, &item_type_ids](ItemPos number_of_items)
    {
        std::vector<ItemPos> copies(instance.number_of_item_types(), 0);
        for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos)
            copies[item_type_ids[item_pos]]++;
        std::vector<std::pair<ItemTypeId, ItemPos>> subset;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            if (copies[item_type_id] > 0)
                subset.push_back({item_type_id, copies[item_type_id]});
        }
        return subset;
    };

    if (!open_dimension) {
        for (ItemPos number_of_items = (parameters.growing_subsets)?
                    1: (std::max)((ItemPos)1, (ItemPos)item_type_ids.size());
                number_of_items <= (ItemPos)item_type_ids.size();
                ++number_of_items) {
            if (parameters.timer.needs_to_end())
                break;
            std::vector<ItemTypeId> subset(
                    item_type_ids.begin(),
                    item_type_ids.begin() + number_of_items);
            ItemsOutput items_output = tree_search_relative_positions_items(
                    instance,
                    bin_type_id,
                    subset,
                    parameters,
                    0,
                    pairs_data);
            update_statistics(items_output);
            if (items_output.status == Status::Unknown)
                break;
            if (items_output.status == Status::Infeasible) {
                algorithm_formatter.update_is_proven_infeasible(
                        infeasible_subset(number_of_items));
                break;
            }
            if (number_of_items == (ItemPos)item_type_ids.size()) {
                Solution solution(instance);
                if (build_solution(instance, bin_type_id, items_output, solution))
                    algorithm_formatter.update_solution(solution, "");
            }
        }
        algorithm_formatter.end();
        return output;
    }

    bool open_dimension_x = (instance.objective() == Objective::OpenDimensionX);
    const AxisAlignedBoundingBox& bin_aabb = instance.bin_type(bin_type_id).aabb_orig;
    LengthDbl open_start = (open_dimension_x)? bin_aabb.x_min: bin_aabb.y_min;
    LengthDbl fixed_dimension = (open_dimension_x)?
        bin_aabb.y_max - bin_aabb.y_min:
        bin_aabb.x_max - bin_aabb.x_min;
    LengthDbl bound = 0;
    AreaDbl subset_area = 0;
    for (ItemPos number_of_items = 1;
            number_of_items <= (ItemPos)item_type_ids.size();
            ++number_of_items) {
        if (parameters.timer.needs_to_end())
            break;
        std::vector<ItemTypeId> subset(
                item_type_ids.begin(),
                item_type_ids.begin() + number_of_items);
        subset_area += instance.item_type(subset.back()).space();
        // The search stops if it finds a solution of the subset reaching its
        // bound.
        ItemsOutput items_output = tree_search_relative_positions_items(
                instance,
                bin_type_id,
                subset,
                parameters,
                (std::max)(bound, open_start + subset_area / fixed_dimension),
                pairs_data);
        update_statistics(items_output);
        if (items_output.status == Status::Unknown)
            break;
        if (items_output.status == Status::Infeasible
                && parameters.open_dimension_upper_bound
                == std::numeric_limits<LengthDbl>::infinity()) {
            // The subset doesn't fit in the bin.
            algorithm_formatter.update_is_proven_infeasible(
                    infeasible_subset(number_of_items));
            break;
        }
        bound = (std::max)(bound, items_output.open_dimension_bound);
        if (open_dimension_x) {
            algorithm_formatter.update_open_dimension_x_bound(bound);
        } else {
            algorithm_formatter.update_open_dimension_y_bound(bound);
        }
        if (number_of_items == (ItemPos)item_type_ids.size()) {
            Solution solution(instance);
            if (build_solution(instance, bin_type_id, items_output, solution))
                algorithm_formatter.update_solution(solution, "");
        }
        // No solution is better than the upper bound.
        if (items_output.status == Status::Infeasible)
            break;
    }

    algorithm_formatter.end();
    return output;
}
