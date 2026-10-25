#include "irregular/direct_trigonometry/separator.hpp"

#include "irregular/rotations.hpp"
#include "irregular/shape_simplification.hpp"
#include "irregular/solution_builder.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/** Tolerance on distances, in scaled coordinates. */
const LengthDbl tolerance = 1e-7;

/** Ratio of the diameter used as the epsilon of the overlap proxy. */
const double overlap_proxy_epsilon_diameter_ratio = 0.01;

/** Guided local search weights. */
const double weight_minimum_increase_ratio = 1.2;
const double weight_maximum_increase_ratio = 2.0;
const double weight_decay = 0.95;

/** Coordinate descent step multipliers. */
const double coordinate_descent_step_success = 1.1;
const double coordinate_descent_step_failure = 0.5;

/**
 * Ratios of the smallest dimension of an item for the initial and limit steps
 * of the first and of the final coordinate descents.
 */
const double first_refinement_step_initial_ratio = 0.25;
const double first_refinement_step_limit_ratio = 0.02;
const double final_refinement_step_initial_ratio = 0.01;
const double final_refinement_step_limit_ratio = 0.001;

/**
 * Two samples closer than this ratio of the smallest dimension of the item
 * are considered identical.
 */
const double unique_sample_threshold_ratio = 0.05;

/**
 * Probability, for each coordinate of a sampled position, to snap it to a
 * side of the container.
 */
const double snap_probability = 0.1;

/** Proportion of the convex hull area of all the items considered large. */
const double large_item_convex_hull_area_ratio = 0.75;

AxisAlignedBoundingBox translate(
        const AxisAlignedBoundingBox& aabb,
        const Point& translation)
{
    AxisAlignedBoundingBox aabb_new;
    aabb_new.x_min = aabb.x_min + translation.x;
    aabb_new.x_max = aabb.x_max + translation.x;
    aabb_new.y_min = aabb.y_min + translation.y;
    aabb_new.y_max = aabb.y_max + translation.y;
    return aabb_new;
}

/** Return true iff the interiors of the two boxes intersect. */
bool aabbs_intersect(
        const AxisAlignedBoundingBox& aabb_1,
        const AxisAlignedBoundingBox& aabb_2)
{
    return aabb_1.x_min < aabb_2.x_max - tolerance
        && aabb_2.x_min < aabb_1.x_max - tolerance
        && aabb_1.y_min < aabb_2.y_max - tolerance
        && aabb_2.y_min < aabb_1.y_max - tolerance;
}

AreaDbl aabb_area(const AxisAlignedBoundingBox& aabb)
{
    return (aabb.x_max - aabb.x_min) * (aabb.y_max - aabb.y_min);
}

/** Convex hull of a set of points (Andrew's monotone chain). */
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
    if (points.size() < 3)
        return points;
    auto cross = [](const Point& origin, const Point& point_1, const Point& point_2)
    {
        return (point_1.x - origin.x) * (point_2.y - origin.y)
            - (point_1.y - origin.y) * (point_2.x - origin.x);
    };
    std::vector<Point> hull(2 * points.size());
    ElementPos hull_size = 0;
    for (ElementPos point_pos = 0; point_pos < (ElementPos)points.size(); ++point_pos) {
        while (hull_size >= 2
                && cross(hull[hull_size - 2], hull[hull_size - 1], points[point_pos]) <= 0) {
            hull_size--;
        }
        hull[hull_size++] = points[point_pos];
    }
    ElementPos lower_size = hull_size + 1;
    for (ElementPos point_pos = (ElementPos)points.size() - 2; point_pos >= 0; --point_pos) {
        while (hull_size >= lower_size
                && cross(hull[hull_size - 2], hull[hull_size - 1], points[point_pos]) <= 0) {
            hull_size--;
        }
        hull[hull_size++] = points[point_pos];
    }
    hull.resize(hull_size - 1);
    return hull;
}

AreaDbl polygon_area(const std::vector<Point>& points)
{
    AreaDbl area = 0.0;
    for (ElementPos point_pos = 0; point_pos < (ElementPos)points.size(); ++point_pos) {
        const Point& point_1 = points[point_pos];
        const Point& point_2 = points[(point_pos + 1) % points.size()];
        area += point_1.x * point_2.y - point_2.x * point_1.y;
    }
    return std::abs(area) / 2;
}

/** Convex hull area and diameter of a set of polygons. */
void compute_convex_hull_area_and_diameter(
        const std::vector<Polygon>& polygons,
        AreaDbl& convex_hull_area,
        LengthDbl& diameter)
{
    std::vector<Point> points;
    for (const Polygon& polygon: polygons)
        points.insert(points.end(), polygon.vertices.begin(), polygon.vertices.end());
    std::vector<Point> hull = convex_hull(points);
    convex_hull_area = polygon_area(hull);
    diameter = 0.0;
    for (ElementPos point_pos_1 = 0; point_pos_1 < (ElementPos)hull.size(); ++point_pos_1)
        for (ElementPos point_pos_2 = point_pos_1 + 1; point_pos_2 < (ElementPos)hull.size(); ++point_pos_2)
            diameter = (std::max)(diameter, distance(hull[point_pos_1], hull[point_pos_2]));
}

Shape transform(
        const Shape& shape,
        Angle angle,
        bool mirror)
{
    Shape shape_new = (mirror)? shape.axial_symmetry_y_axis(): shape;
    return shape_new.rotate(angle);
}

Point transform(
        const Point& point,
        Angle angle,
        bool mirror)
{
    Point point_new = (mirror)? point.axial_symmetry_y_axis(): point;
    return point_new.rotate(angle);
}

/**
 * Number of poles, in order, covering half of the area of the polygons (at
 * least one).
 */
Counter compute_number_of_fail_fast_poles(
        const std::vector<Pole>& poles,
        const std::vector<Polygon>& polygons)
{
    AreaDbl area = 0.0;
    for (const Polygon& polygon: polygons)
        area += polygon_area(polygon.vertices);
    AreaDbl poles_area = 0.0;
    for (Counter pole_pos = 0; pole_pos < (Counter)poles.size(); ++pole_pos) {
        poles_area += M_PI * poles[pole_pos].radius * poles[pole_pos].radius;
        if (poles_area >= 0.5 * area)
            return pole_pos + 1;
    }
    return poles.size();
}

/**
 * Return true iff one of the first 'number_of_poles_1' poles of the first
 * shape overlaps one of the first 'number_of_poles_2' poles of the second
 * shape. The poles being inside the shapes, the shapes then overlap.
 */
bool poles_overlap(
        const std::vector<Pole>& poles_1,
        Counter number_of_poles_1,
        const Point& translation_1,
        const std::vector<Pole>& poles_2,
        Counter number_of_poles_2,
        const Point& translation_2)
{
    for (Counter pole_pos_1 = 0; pole_pos_1 < number_of_poles_1; ++pole_pos_1) {
        const Pole& pole_1 = poles_1[pole_pos_1];
        LengthDbl center_1_x = pole_1.center.x + translation_1.x;
        LengthDbl center_1_y = pole_1.center.y + translation_1.y;
        for (Counter pole_pos_2 = 0; pole_pos_2 < number_of_poles_2; ++pole_pos_2) {
            const Pole& pole_2 = poles_2[pole_pos_2];
            LengthDbl penetration_distance = pole_1.radius + pole_2.radius - tolerance;
            if (penetration_distance <= 0)
                continue;
            LengthDbl delta_x = pole_2.center.x + translation_2.x - center_1_x;
            LengthDbl delta_y = pole_2.center.y + translation_2.y - center_1_y;
            if (delta_x * delta_x + delta_y * delta_y < penetration_distance * penetration_distance)
                return true;
        }
    }
    return false;
}

/**
 * Proxy of the overlap area of two sets of poles.
 *
 * Algorithm 3 of the Sparrow paper: the sum, over all the pairs of poles, of
 * their penetration depth times the smallest radius. Penetration depths
 * smaller than 'epsilon' (in particular negative ones) decay smoothly towards
 * 0, so that the proxy stays positive for colliding shapes whose poles don't
 * overlap.
 */
double overlap_proxy(
        const std::vector<Pole>& poles_1,
        const Point& translation_1,
        const std::vector<Pole>& poles_2,
        const Point& translation_2,
        double epsilon,
        double maximum_overlap)
{
    // The poles of the second shape, translated relatively to the first
    // shape, in separate single precision arrays, so that the inner loop can
    // be vectorized (the proxy only needs to be consistent for comparisons).
    static thread_local std::vector<float> centers_x;
    static thread_local std::vector<float> centers_y;
    static thread_local std::vector<float> radii;
    Counter number_of_poles_2 = poles_2.size();
    centers_x.resize(number_of_poles_2);
    centers_y.resize(number_of_poles_2);
    radii.resize(number_of_poles_2);
    LengthDbl translation_x = translation_2.x - translation_1.x;
    LengthDbl translation_y = translation_2.y - translation_1.y;
    for (Counter pole_pos = 0; pole_pos < number_of_poles_2; ++pole_pos) {
        centers_x[pole_pos] = (float)(poles_2[pole_pos].center.x + translation_x);
        centers_y[pole_pos] = (float)(poles_2[pole_pos].center.y + translation_y);
        radii[pole_pos] = (float)poles_2[pole_pos].radius;
    }
    const float* centers_x_data = centers_x.data();
    const float* centers_y_data = centers_y.data();
    const float* radii_data = radii.data();

    const double infinity = std::numeric_limits<double>::infinity();
    const float epsilon_float = (float)epsilon;
    const float epsilon_square = epsilon_float * epsilon_float;
    double overlap = 0.0;
    double maximum_sum = maximum_overlap / M_PI;
    for (const Pole& pole_1: poles_1) {
        float center_1_x = (float)pole_1.center.x;
        float center_1_y = (float)pole_1.center.y;
        float radius_1 = (float)pole_1.radius;
        float sum = 0.0;
        for (Counter pole_pos = 0; pole_pos < number_of_poles_2; ++pole_pos) {
            float delta_x = centers_x_data[pole_pos] - center_1_x;
            float delta_y = centers_y_data[pole_pos] - center_1_y;
            float penetration_depth = radius_1 + radii_data[pole_pos]
                - std::sqrt(delta_x * delta_x + delta_y * delta_y);
            // Branchless form, so that the loop can be vectorized: below
            // 'epsilon', the decayed penetration depth is larger than the
            // penetration depth; above, the second term is 'epsilon'.
            float penetration_depth_decay = (std::max)(
                    penetration_depth,
                    epsilon_square / (std::max)(2 * epsilon_float - penetration_depth, epsilon_float));
            float radius_minimum = (std::min)(radii_data[pole_pos], radius_1);
            sum += penetration_depth_decay * radius_minimum;
        }
        overlap += sum;
        if (overlap > maximum_sum)
            return infinity;
    }
    return M_PI * overlap;
}

/**
 * Collision loss of two shapes from their poles.
 *
 * Algorithm 4 of the Sparrow paper.
 *
 * Return infinity as soon as the loss is known to exceed 'maximum_loss'.
 */
double collision_loss_from_poles(
        const std::vector<Pole>& poles_1,
        const Point& translation_1,
        AreaDbl convex_hull_area_1,
        LengthDbl diameter_1,
        const std::vector<Pole>& poles_2,
        const Point& translation_2,
        AreaDbl convex_hull_area_2,
        LengthDbl diameter_2,
        double maximum_loss)
{
    const double infinity = std::numeric_limits<double>::infinity();
    double epsilon = (std::max)(diameter_1, diameter_2) * overlap_proxy_epsilon_diameter_ratio;
    double penalty = std::sqrt(std::sqrt(convex_hull_area_1) * std::sqrt(convex_hull_area_2));
    double maximum_overlap = infinity;
    if (maximum_loss != infinity) {
        double maximum_root = maximum_loss / penalty;
        maximum_overlap = maximum_root * maximum_root - epsilon * epsilon;
        if (maximum_overlap < 0)
            return infinity;
    }
    double overlap = overlap_proxy(
            poles_1,
            translation_1,
            poles_2,
            translation_2,
            epsilon,
            maximum_overlap);
    if (overlap == infinity)
        return infinity;
    return std::sqrt(overlap + epsilon * epsilon) * penalty;
}

}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Constructor //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const Separator::ItemTypes> Separator::build_item_types(
        const Instance& instance,
        const SeparatorParameters& parameters)
{
    const bool item_spacing = instance.parameters().item_item_minimum_spacing > 0;
    SimplifiedInstance simplified_instance = shape_simplification(
            instance,
            parameters.maximum_approximation_ratio);
    std::vector<std::vector<std::vector<ItemTypeRotation>>> rotations
        = compute_item_type_rotations(instance);

    // Item types.
    ItemTypes item_types(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const SimplifiedItemType& simplified_item_type = simplified_instance.item_types[item_type_id];
        SeparatorItemType& separator_item_type = item_types[item_type_id];

        // Poles of the shapes, not transformed.
        std::vector<Polygon> polygons;
        std::vector<Pole> poles;
        for (const SimplifiedShape& simplified_shape: simplified_item_type.shapes) {
            polygons.push_back(build_polygon(simplified_shape.shape.shape));
            std::vector<Pole> shape_poles = compute_poles(polygons.back());
            poles.insert(poles.end(), shape_poles.begin(), shape_poles.end());
        }
        compute_convex_hull_area_and_diameter(
                polygons,
                separator_item_type.convex_hull_area,
                separator_item_type.diameter);
        AxisAlignedBoundingBox aabb;
        for (const Polygon& polygon: polygons)
            aabb = merge(aabb, polygon.aabb);
        separator_item_type.minimum_dimension = (std::min)(
                aabb.x_max - aabb.x_min,
                aabb.y_max - aabb.y_min);
        separator_item_type.number_of_fail_fast_poles = compute_number_of_fail_fast_poles(poles, polygons);

        // Orientations.
        separator_item_type.bin_types_orientation_ids.resize(instance.number_of_bin_types());
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            for (const ItemTypeRotation& rotation: rotations[bin_type_id][item_type_id]) {
                Counter orientation_id = -1;
                for (Counter orientation_id_cur = 0;
                        orientation_id_cur < (Counter)separator_item_type.orientations.size();
                        ++orientation_id_cur) {
                    const Orientation& orientation = separator_item_type.orientations[orientation_id_cur];
                    if (equal(orientation.angle, rotation.angle)
                            && orientation.mirror == rotation.mirror) {
                        orientation_id = orientation_id_cur;
                        break;
                    }
                }
                if (orientation_id == -1) {
                    orientation_id = separator_item_type.orientations.size();
                    Orientation orientation;
                    orientation.angle = rotation.angle;
                    orientation.mirror = rotation.mirror;
                    for (const SimplifiedShape& simplified_shape: simplified_item_type.shapes) {
                        Polygon polygon = build_polygon(transform(
                                    simplified_shape.shape.shape,
                                    rotation.angle,
                                    rotation.mirror));
                        orientation.aabb = merge(orientation.aabb, polygon.aabb);
                        orientation.polygons.push_back(polygon);
                        if (item_spacing) {
                            Polygon polygon_inflated = build_polygon(transform(
                                        simplified_shape.shape_inflated.shape,
                                        rotation.angle,
                                        rotation.mirror));
                            orientation.aabb_inflated = merge(orientation.aabb_inflated, polygon_inflated.aabb);
                            orientation.polygons_inflated.push_back(polygon_inflated);
                        }
                    }
                    if (!item_spacing) {
                        orientation.polygons_inflated = orientation.polygons;
                        orientation.aabb_inflated = orientation.aabb;
                    }
                    for (const Pole& pole: poles) {
                        Pole pole_new = pole;
                        pole_new.center = transform(pole.center, rotation.angle, rotation.mirror);
                        orientation.poles.push_back(pole_new);
                    }
                    separator_item_type.orientations.push_back(orientation);
                }
                separator_item_type.bin_types_orientation_ids[bin_type_id].push_back(orientation_id);
            }
        }
    }
    return std::make_shared<const ItemTypes>(item_types);
}

Separator::Separator(
        const Instance& instance,
        const SeparatorParameters& parameters,
        std::mt19937_64& generator,
        const std::shared_ptr<const ItemTypes>& item_types):
    instance_(instance),
    parameters_(parameters),
    generator_(generator()),
    item_types_pointer_((item_types != nullptr)?
            item_types:
            build_item_types(instance, parameters)),
    item_types_(*item_types_pointer_)
{
    const LengthDbl scale_value = instance.parameters().scale_value;

    // Allowed orientations.
    orientation_ids_.resize(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const SeparatorItemType& item_type = item_types_[item_type_id];
            if (bin_type_id < (BinTypeId)item_type.bin_types_orientation_ids.size()) {
                orientation_ids_[bin_type_id].push_back(
                        item_type.bin_types_orientation_ids[bin_type_id]);
            } else {
                std::vector<Counter> orientation_ids(item_type.orientations.size());
                for (Counter orientation_id = 0;
                        orientation_id < (Counter)item_type.orientations.size();
                        ++orientation_id) {
                    orientation_ids[orientation_id] = orientation_id;
                }
                orientation_ids_[bin_type_id].push_back(orientation_ids);
            }
        }
    }

    // The simplified shapes of the bins are only needed for their borders,
    // defects and fixed items.
    bool bin_hazards = false;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (!bin_type.borders.empty()
                || !bin_type.defects.empty()
                || !bin_type.fixed_items.empty()) {
            bin_hazards = true;
        }
    }
    SimplifiedInstance simplified_instance;
    if (bin_hazards) {
        simplified_instance = shape_simplification(
                instance,
                parameters.maximum_approximation_ratio);
    }

    // Bin types.
    bin_types_.resize(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        SeparatorBinType& separator_bin_type = bin_types_[bin_type_id];
        LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
        separator_bin_type.container = bin_type.aabb_scaled;
        separator_bin_type.container.x_min += bin_spacing;
        separator_bin_type.container.x_max -= bin_spacing;
        separator_bin_type.container.y_min += bin_spacing;
        separator_bin_type.container.y_max -= bin_spacing;

        if (!bin_hazards)
            continue;
        const SimplifiedBinType& simplified_bin_type = simplified_instance.bin_types[bin_type_id];

        // Borders and defects.
        std::vector<const SimplifiedShape*> obstacles;
        for (const SimplifiedShape& border: simplified_bin_type.borders)
            obstacles.push_back(&border);
        for (const SimplifiedShape& defect: simplified_bin_type.defects)
            obstacles.push_back(&defect);
        for (const SimplifiedShape* obstacle: obstacles) {
            Hazard hazard;
            hazard.polygons.push_back(build_polygon(obstacle->shape_inflated.shape));
            hazard.aabb = hazard.polygons.back().aabb;
            hazard.poles = compute_poles(hazard.polygons.back());
            hazard.number_of_fail_fast_poles = compute_number_of_fail_fast_poles(hazard.poles, hazard.polygons);
            compute_convex_hull_area_and_diameter(
                    hazard.polygons,
                    hazard.convex_hull_area,
                    hazard.diameter);
            separator_bin_type.hazards.push_back(hazard);
        }

        // Fixed items.
        for (const FixedItem& fixed_item: bin_type.fixed_items) {
            const SimplifiedItemType& simplified_item_type = simplified_instance.item_types[fixed_item.item_type_id];
            Point translation = {
                scale_value * fixed_item.bl_corner.x,
                scale_value * fixed_item.bl_corner.y};
            Hazard hazard;
            hazard.against_inflated = true;
            for (const SimplifiedShape& simplified_shape: simplified_item_type.shapes) {
                Shape shape = transform(
                        simplified_shape.shape.shape,
                        fixed_item.angle,
                        fixed_item.mirror);
                shape.shift(translation.x, translation.y);
                hazard.polygons.push_back(build_polygon(shape));
                hazard.aabb = merge(hazard.aabb, hazard.polygons.back().aabb);
                std::vector<Pole> shape_poles = compute_poles(hazard.polygons.back());
                hazard.poles.insert(hazard.poles.end(), shape_poles.begin(), shape_poles.end());
            }
            hazard.number_of_fail_fast_poles = compute_number_of_fail_fast_poles(hazard.poles, hazard.polygons);
            compute_convex_hull_area_and_diameter(
                    hazard.polygons,
                    hazard.convex_hull_area,
                    hazard.diameter);
            separator_bin_type.hazards.push_back(hazard);
            SolutionItem solution_item;
            solution_item.item_type_id = fixed_item.item_type_id;
            solution_item.bl_corner = fixed_item.bl_corner;
            solution_item.angle = fixed_item.angle;
            solution_item.mirror = fixed_item.mirror;
            solution_item.is_fixed = true;
            separator_bin_type.fixed_items.push_back(solution_item);
        }

        number_of_hazards_ = (std::max)(
                number_of_hazards_,
                (Counter)separator_bin_type.hazards.size() + 1);
    }

    // Items.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies - item_type.copies_fixed; ++copy)
            items_item_type_id_.push_back(item_type_id);
    }
    ItemPos number_of_items = items_item_type_id_.size();
    items_state_.resize(number_of_items);
    items_bin_item_pos_.resize(number_of_items, -1);
    bins_items_.resize(instance.number_of_bins());
    pair_entries_.resize(number_of_items * (number_of_items + 1) / 2);
    hazard_entries_.resize(number_of_items * number_of_hazards_);
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Collision tracker //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

inline Separator::Entry& Separator::pair_entry(
        ItemPos item_pos_1,
        ItemPos item_pos_2)
{
    ItemPos number_of_items = items_item_type_id_.size();
    ItemPos row = (std::min)(item_pos_1, item_pos_2);
    ItemPos column = (std::max)(item_pos_1, item_pos_2);
    return pair_entries_[row * number_of_items + column - (row * (row + 1)) / 2];
}

inline const Separator::Entry& Separator::pair_entry(
        ItemPos item_pos_1,
        ItemPos item_pos_2) const
{
    ItemPos number_of_items = items_item_type_id_.size();
    ItemPos row = (std::min)(item_pos_1, item_pos_2);
    ItemPos column = (std::max)(item_pos_1, item_pos_2);
    return pair_entries_[row * number_of_items + column - (row * (row + 1)) / 2];
}

AxisAlignedBoundingBox Separator::item_aabb(
        ItemPos item_pos,
        const ItemState& item_state) const
{
    const SeparatorItemType& item_type = item_types_[items_item_type_id_[item_pos]];
    return translate(item_type.orientations[item_state.orientation_id].aabb, item_state.position);
}

bool Separator::collide(
        ItemPos item_pos_1,
        const ItemState& item_state_1,
        ItemPos item_pos_2) const
{
    const ItemState& item_state_2 = items_state_[item_pos_2];
    const Orientation& orientation_1 = item_types_[items_item_type_id_[item_pos_1]].orientations[item_state_1.orientation_id];
    const Orientation& orientation_2 = item_types_[items_item_type_id_[item_pos_2]].orientations[item_state_2.orientation_id];
    if (!aabbs_intersect(
                translate(orientation_1.aabb_inflated, item_state_1.position),
                translate(orientation_2.aabb, item_state_2.position))) {
        return false;
    }
    // Fail fast: overlapping poles.
    if (poles_overlap(
                orientation_1.poles,
                item_types_[items_item_type_id_[item_pos_1]].number_of_fail_fast_poles,
                item_state_1.position,
                orientation_2.poles,
                item_types_[items_item_type_id_[item_pos_2]].number_of_fail_fast_poles,
                item_state_2.position)) {
        return true;
    }
    for (const Polygon& polygon_1: orientation_1.polygons_inflated) {
        for (const Polygon& polygon_2: orientation_2.polygons) {
            if (intersect(polygon_1, item_state_1.position, polygon_2, item_state_2.position))
                return true;
        }
    }
    return false;
}

bool Separator::collide(
        ItemPos item_pos,
        const ItemState& item_state,
        const Hazard& hazard) const
{
    const Orientation& orientation = item_types_[items_item_type_id_[item_pos]].orientations[item_state.orientation_id];
    const std::vector<Polygon>& polygons = (hazard.against_inflated)?
        orientation.polygons_inflated:
        orientation.polygons;
    const AxisAlignedBoundingBox& aabb = (hazard.against_inflated)?
        orientation.aabb_inflated:
        orientation.aabb;
    if (!aabbs_intersect(translate(aabb, item_state.position), hazard.aabb))
        return false;
    const Point origin = {0.0, 0.0};
    // Fail fast: overlapping poles.
    if (poles_overlap(
                orientation.poles,
                item_types_[items_item_type_id_[item_pos]].number_of_fail_fast_poles,
                item_state.position,
                hazard.poles,
                hazard.number_of_fail_fast_poles,
                origin)) {
        return true;
    }
    for (const Polygon& polygon: polygons) {
        for (const Polygon& hazard_polygon: hazard.polygons) {
            if (intersect(polygon, item_state.position, hazard_polygon, origin))
                return true;
        }
    }
    return false;
}

bool Separator::collide_exterior(
        ItemPos item_pos,
        const ItemState& item_state) const
{
    const AxisAlignedBoundingBox& container = bin_types_[instance_.bin_type_id(item_state.bin_pos)].container;
    AxisAlignedBoundingBox aabb = item_aabb(item_pos, item_state);
    return aabb.x_min < container.x_min - tolerance
        || aabb.x_max > container.x_max + tolerance
        || aabb.y_min < container.y_min - tolerance
        || aabb.y_max > container.y_max + tolerance;
}

double Separator::collision_loss(
        ItemPos item_pos_1,
        const ItemState& item_state_1,
        ItemPos item_pos_2,
        double maximum_loss) const
{
    const ItemState& item_state_2 = items_state_[item_pos_2];
    const SeparatorItemType& item_type_1 = item_types_[items_item_type_id_[item_pos_1]];
    const SeparatorItemType& item_type_2 = item_types_[items_item_type_id_[item_pos_2]];
    return collision_loss_from_poles(
            item_type_1.orientations[item_state_1.orientation_id].poles,
            item_state_1.position,
            item_type_1.convex_hull_area,
            item_type_1.diameter,
            item_type_2.orientations[item_state_2.orientation_id].poles,
            item_state_2.position,
            item_type_2.convex_hull_area,
            item_type_2.diameter,
            maximum_loss);
}

double Separator::collision_loss(
        ItemPos item_pos,
        const ItemState& item_state,
        const Hazard& hazard,
        double maximum_loss) const
{
    const SeparatorItemType& item_type = item_types_[items_item_type_id_[item_pos]];
    const Point origin = {0.0, 0.0};
    return collision_loss_from_poles(
            item_type.orientations[item_state.orientation_id].poles,
            item_state.position,
            item_type.convex_hull_area,
            item_type.diameter,
            hazard.poles,
            origin,
            hazard.convex_hull_area,
            hazard.diameter,
            maximum_loss);
}

double Separator::exterior_loss(
        ItemPos item_pos,
        const ItemState& item_state) const
{
    // Quantification of Sparrow: the area of the bounding box of the item
    // outside the container.
    const SeparatorItemType& item_type = item_types_[items_item_type_id_[item_pos]];
    const AxisAlignedBoundingBox& container = bin_types_[instance_.bin_type_id(item_state.bin_pos)].container;
    AxisAlignedBoundingBox aabb = item_aabb(item_pos, item_state);
    AxisAlignedBoundingBox intersection;
    intersection.x_min = (std::max)(aabb.x_min, container.x_min);
    intersection.x_max = (std::min)(aabb.x_max, container.x_max);
    intersection.y_min = (std::max)(aabb.y_min, container.y_min);
    intersection.y_max = (std::min)(aabb.y_max, container.y_max);
    AreaDbl overlap = 0.0;
    if (intersection.x_min < intersection.x_max && intersection.y_min < intersection.y_max) {
        overlap = aabb_area(aabb) - aabb_area(intersection) + 0.0001 * aabb_area(aabb);
    } else {
        Point aabb_center = {(aabb.x_min + aabb.x_max) / 2, (aabb.y_min + aabb.y_max) / 2};
        Point container_center = {(container.x_min + container.x_max) / 2, (container.y_min + container.y_max) / 2};
        overlap = aabb_area(aabb) + shape::distance(aabb_center, container_center);
    }
    return 2 * std::sqrt(overlap) * std::sqrt(item_type.convex_hull_area);
}

void Separator::update_item_losses(ItemPos item_pos)
{
    for (ItemPos item_pos_2 = 0; item_pos_2 < (ItemPos)items_state_.size(); ++item_pos_2)
        pair_entry(item_pos, item_pos_2).loss = 0.0;
    for (Counter hazard_pos = 0; hazard_pos < number_of_hazards_; ++hazard_pos)
        hazard_entry(item_pos, hazard_pos).loss = 0.0;

    const ItemState& item_state = items_state_[item_pos];
    if (item_state.bin_pos == -1)
        return;
    if (collide_exterior(item_pos, item_state))
        hazard_entry(item_pos, 0).loss = exterior_loss(item_pos, item_state);
    const SeparatorBinType& bin_type = bin_types_[instance_.bin_type_id(item_state.bin_pos)];
    for (Counter hazard_pos = 0; hazard_pos < (Counter)bin_type.hazards.size(); ++hazard_pos) {
        const Hazard& hazard = bin_type.hazards[hazard_pos];
        if (collide(item_pos, item_state, hazard))
            hazard_entry(item_pos, hazard_pos + 1).loss = collision_loss(item_pos, item_state, hazard);
    }
    for (ItemPos item_pos_2: bins_items_[item_state.bin_pos]) {
        if (item_pos_2 == item_pos)
            continue;
        if (collide(item_pos, item_state, item_pos_2))
            pair_entry(item_pos, item_pos_2).loss = collision_loss(item_pos, item_state, item_pos_2);
    }
}

void Separator::rebuild_tracker()
{
    for (Entry& entry: pair_entries_)
        entry = Entry();
    for (Entry& entry: hazard_entries_)
        entry = Entry();
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos)
        update_item_losses(item_pos);
}

double Separator::item_loss(ItemPos item_pos) const
{
    double loss = 0.0;
    for (ItemPos item_pos_2 = 0; item_pos_2 < (ItemPos)items_state_.size(); ++item_pos_2)
        loss += pair_entry(item_pos, item_pos_2).loss;
    for (Counter hazard_pos = 0; hazard_pos < number_of_hazards_; ++hazard_pos)
        loss += hazard_entry(item_pos, hazard_pos).loss;
    return loss;
}

double Separator::total_loss() const
{
    double loss = 0.0;
    for (const Entry& entry: pair_entries_)
        loss += entry.loss;
    for (const Entry& entry: hazard_entries_)
        loss += entry.loss;
    return loss;
}

void Separator::update_weights()
{
    double maximum_loss = 0.0;
    for (const Entry& entry: pair_entries_)
        maximum_loss = (std::max)(maximum_loss, entry.loss);
    for (const Entry& entry: hazard_entries_)
        maximum_loss = (std::max)(maximum_loss, entry.loss);
    const LocalSearchWeightingScheme weighting_scheme = parameters_.weighting_scheme;
    auto update = [maximum_loss, weighting_scheme](Entry& entry)
    {
        if (weighting_scheme == LocalSearchWeightingScheme::Additive) {
            // Equation (29) of Umetani et al. (2009).
            if (entry.loss > 0.0)
                entry.weight += entry.loss / maximum_loss;
            return;
        }
        // Algorithm 8 of the Sparrow paper.
        double multiplier = (entry.loss == 0.0)?
            weight_decay:
            weight_minimum_increase_ratio
            + (weight_maximum_increase_ratio - weight_minimum_increase_ratio) * entry.loss / maximum_loss;
        entry.weight = (std::max)(1.0, entry.weight * multiplier);
    };
    for (Entry& entry: pair_entries_)
        update(entry);
    for (Entry& entry: hazard_entries_)
        update(entry);
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Layout ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Separator::remove_item(ItemPos item_pos)
{
    ItemState& item_state = items_state_[item_pos];
    if (item_state.bin_pos == -1)
        return;
    std::vector<ItemPos>& bin_items = bins_items_[item_state.bin_pos];
    ItemPos bin_item_pos = items_bin_item_pos_[item_pos];
    bin_items[bin_item_pos] = bin_items.back();
    items_bin_item_pos_[bin_items[bin_item_pos]] = bin_item_pos;
    bin_items.pop_back();
    items_bin_item_pos_[item_pos] = -1;
    item_state.bin_pos = -1;
}

void Separator::add_item(
        ItemPos item_pos,
        const ItemState& item_state)
{
    items_state_[item_pos] = item_state;
    items_bin_item_pos_[item_pos] = bins_items_[item_state.bin_pos].size();
    bins_items_[item_state.bin_pos].push_back(item_pos);
    update_item_losses(item_pos);
}

void Separator::set_layout(const std::vector<ItemState>& layout)
{
    for (std::vector<ItemPos>& bin_items: bins_items_)
        bin_items.clear();
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
        items_state_[item_pos] = layout[item_pos];
        items_bin_item_pos_[item_pos] = -1;
        if (layout[item_pos].bin_pos != -1) {
            items_bin_item_pos_[item_pos] = bins_items_[layout[item_pos].bin_pos].size();
            bins_items_[layout[item_pos].bin_pos].push_back(item_pos);
        }
    }
    rebuild_tracker();
}

Counter Separator::distance(
        const std::vector<ItemState>& layout_1,
        const std::vector<ItemState>& layout_2) const
{
    struct Description
    {
        ItemTypeId item_type_id;
        BinPos bin_pos;
        Counter orientation_id;
        int64_t cell_x;
        int64_t cell_y;

        bool operator<(const Description& description) const
        {
            if (item_type_id != description.item_type_id)
                return item_type_id < description.item_type_id;
            if (bin_pos != description.bin_pos)
                return bin_pos < description.bin_pos;
            if (orientation_id != description.orientation_id)
                return orientation_id < description.orientation_id;
            if (cell_x != description.cell_x)
                return cell_x < description.cell_x;
            return cell_y < description.cell_y;
        }
    };
    auto describe = [this](const std::vector<ItemState>& layout)
    {
        std::vector<Description> descriptions;
        for (ItemPos item_pos = 0; item_pos < (ItemPos)layout.size(); ++item_pos) {
            const ItemState& item_state = layout[item_pos];
            Description description;
            description.item_type_id = items_item_type_id_[item_pos];
            description.bin_pos = item_state.bin_pos;
            description.orientation_id = item_state.orientation_id;
            description.cell_x = 0;
            description.cell_y = 0;
            if (item_state.bin_pos != -1) {
                AxisAlignedBoundingBox aabb = item_aabb(item_pos, item_state);
                LengthDbl cell_size = item_types_[description.item_type_id].minimum_dimension;
                description.cell_x = (int64_t)std::floor((aabb.x_min + aabb.x_max) / 2 / cell_size);
                description.cell_y = (int64_t)std::floor((aabb.y_min + aabb.y_max) / 2 / cell_size);
            }
            descriptions.push_back(description);
        }
        std::sort(descriptions.begin(), descriptions.end());
        return descriptions;
    };
    std::vector<Description> descriptions_1 = describe(layout_1);
    std::vector<Description> descriptions_2 = describe(layout_2);

    // Number of matching descriptions.
    Counter number_of_matches = 0;
    auto description_1_it = descriptions_1.begin();
    auto description_2_it = descriptions_2.begin();
    while (description_1_it != descriptions_1.end() && description_2_it != descriptions_2.end()) {
        if (*description_1_it < *description_2_it) {
            ++description_1_it;
        } else if (*description_2_it < *description_1_it) {
            ++description_2_it;
        } else {
            number_of_matches++;
            ++description_1_it;
            ++description_2_it;
        }
    }
    return (Counter)descriptions_1.size() - number_of_matches;
}

void Separator::set_solution(
        const Solution& solution,
        const optimizationtools::Timer& timer)
{
    const LengthDbl scale_value = instance_.parameters().scale_value;
    std::vector<ItemState> layout(items_state_.size());

    // Items of each item type.
    std::vector<std::vector<ItemPos>> item_types_items(instance_.number_of_item_types());
    for (ItemPos item_pos = (ItemPos)items_item_type_id_.size() - 1; item_pos >= 0; --item_pos)
        item_types_items[items_item_type_id_[item_pos]].push_back(item_pos);

    // Bins of each bin type, in order.
    std::vector<std::vector<BinPos>> bin_types_bins(instance_.number_of_bin_types());
    for (BinPos bin_pos = instance_.number_of_bins() - 1; bin_pos >= 0; --bin_pos)
        bin_types_bins[instance_.bin_type_id(bin_pos)].push_back(bin_pos);

    for (BinPos solution_bin_pos = 0;
            solution_bin_pos < solution.number_of_different_bins();
            ++solution_bin_pos) {
        const SolutionBin& solution_bin = solution.bin(solution_bin_pos);
        for (BinPos copy = 0; copy < solution_bin.copies; ++copy) {
            std::vector<BinPos>& bins = bin_types_bins[solution_bin.bin_type_id];
            if (bins.empty())
                break;
            BinPos bin_pos = bins.back();
            bins.pop_back();
            for (const SolutionItem& solution_item: solution_bin.items) {
                if (solution_item.is_fixed)
                    continue;
                std::vector<ItemPos>& items = item_types_items[solution_item.item_type_id];
                if (items.empty())
                    continue;
                // Orientation.
                const SeparatorItemType& item_type = item_types_[solution_item.item_type_id];
                Counter orientation_id = -1;
                for (Counter orientation_id_cur: orientation_ids_[solution_bin.bin_type_id][solution_item.item_type_id]) {
                    const Orientation& orientation = item_type.orientations[orientation_id_cur];
                    if (equal(orientation.angle, solution_item.angle)
                            && orientation.mirror == solution_item.mirror) {
                        orientation_id = orientation_id_cur;
                        break;
                    }
                }
                // If the orientation isn't one of the orientations considered,
                // the item is inserted again.
                if (orientation_id == -1)
                    continue;
                ItemPos item_pos = items.back();
                items.pop_back();
                layout[item_pos].bin_pos = bin_pos;
                layout[item_pos].orientation_id = orientation_id;
                layout[item_pos].position = {
                    scale_value * solution_item.bl_corner.x,
                    scale_value * solution_item.bl_corner.y};
            }
        }
    }

    items_state_ = layout;
    shift_overflowing_items();
    layout = items_state_;
    set_layout(layout);
    insert_missing_items(timer);
}

void Separator::shift_overflowing_items()
{
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        const AxisAlignedBoundingBox& container = bin_types_[instance_.bin_type_id(bin_pos)].container;
        AxisAlignedBoundingBox items_aabb;
        bool empty = true;
        for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
            if (items_state_[item_pos].bin_pos != bin_pos)
                continue;
            items_aabb = merge(items_aabb, item_aabb(item_pos, items_state_[item_pos]));
            empty = false;
        }
        if (empty)
            continue;

        // Remove a vertical slice of the overflowing width at a random
        // position: the items whose center is after it are shifted.
        LengthDbl overflow_x = items_aabb.x_max - container.x_max;
        if (overflow_x > tolerance && container.x_max > container.x_min) {
            std::uniform_real_distribution<LengthDbl> distribution(container.x_min, container.x_max);
            LengthDbl split = distribution(generator_);
            for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
                if (items_state_[item_pos].bin_pos != bin_pos)
                    continue;
                AxisAlignedBoundingBox aabb = item_aabb(item_pos, items_state_[item_pos]);
                if ((aabb.x_min + aabb.x_max) / 2 > split)
                    items_state_[item_pos].position.x -= overflow_x;
            }
        }

        // Same with a horizontal slice.
        LengthDbl overflow_y = items_aabb.y_max - container.y_max;
        if (overflow_y > tolerance && container.y_max > container.y_min) {
            std::uniform_real_distribution<LengthDbl> distribution(container.y_min, container.y_max);
            LengthDbl split = distribution(generator_);
            for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
                if (items_state_[item_pos].bin_pos != bin_pos)
                    continue;
                AxisAlignedBoundingBox aabb = item_aabb(item_pos, items_state_[item_pos]);
                if ((aabb.y_min + aabb.y_max) / 2 > split)
                    items_state_[item_pos].position.y -= overflow_y;
            }
        }
    }
}

void Separator::insert_missing_items(const optimizationtools::Timer& timer)
{
    // Largest items first.
    std::vector<ItemPos> missing_items;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos)
        if (items_state_[item_pos].bin_pos == -1)
            missing_items.push_back(item_pos);
    std::stable_sort(
            missing_items.begin(),
            missing_items.end(),
            [this](ItemPos item_pos_1, ItemPos item_pos_2)
            {
                const SeparatorItemType& item_type_1 = item_types_[items_item_type_id_[item_pos_1]];
                const SeparatorItemType& item_type_2 = item_types_[items_item_type_id_[item_pos_2]];
                return item_type_1.convex_hull_area * item_type_1.diameter
                    > item_type_2.convex_hull_area * item_type_2.diameter;
            });
    for (ItemPos item_pos: missing_items) {
        if (timer.needs_to_end())
            return;
        Candidate candidate = search_placement(item_pos, true);
        if (candidate.state.bin_pos == -1)
            continue;
        add_item(item_pos, candidate.state);
    }
}

Solution Separator::solution() const
{
    const LengthDbl scale_value = instance_.parameters().scale_value;
    SolutionBuilder solution_builder(instance_);
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
        const SeparatorBinType& bin_type = bin_types_[bin_type_id];
        // The bins with fixed items are always used, since the copies of
        // their fixed items are counted.
        if (bins_items_[bin_pos].empty() && bin_type.fixed_items.empty())
            continue;
        BinPos solution_bin_pos = solution_builder.add_bin(bin_type_id, 1);
        for (const SolutionItem& fixed_item: bin_type.fixed_items) {
            solution_builder.add_item(
                    solution_bin_pos,
                    fixed_item.item_type_id,
                    fixed_item.bl_corner,
                    fixed_item.angle,
                    fixed_item.mirror,
                    true);
        }
        std::vector<ItemPos> bin_items = bins_items_[bin_pos];
        std::sort(bin_items.begin(), bin_items.end());
        for (ItemPos item_pos: bin_items) {
            const ItemState& item_state = items_state_[item_pos];
            ItemTypeId item_type_id = items_item_type_id_[item_pos];
            const Orientation& orientation = item_types_[item_type_id].orientations[item_state.orientation_id];
            solution_builder.add_item(
                    solution_bin_pos,
                    item_type_id,
                    {item_state.position.x / scale_value, item_state.position.y / scale_value},
                    orientation.angle,
                    orientation.mirror);
        }
    }
    return solution_builder.build();
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Moves ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool Separator::Evaluation::operator<(const Evaluation& evaluation) const
{
    if (status != evaluation.status)
        return (int)status < (int)evaluation.status;
    if (status == Status::Invalid)
        return false;
    return loss < evaluation.loss;
}

Separator::Evaluation Separator::evaluate(
        ItemPos item_pos,
        const ItemState& item_state,
        const Evaluation& bound,
        bool insertion)
{
    // Algorithm 7 of the Sparrow paper: the evaluation stops as soon as the
    // loss exceeds the one of the bound.
    number_of_evaluations_++;
    double loss_bound = std::numeric_limits<double>::infinity();
    if (bound.status == Evaluation::Status::Collision)
        loss_bound = bound.loss;
    if (bound.status == Evaluation::Status::Clear)
        loss_bound = 0.0;

    Evaluation evaluation;
    double loss = 0.0;
    bool collision = false;
    if (collide_exterior(item_pos, item_state)) {
        collision = true;
        loss += hazard_entry(item_pos, 0).weight * exterior_loss(item_pos, item_state);
        if (loss > loss_bound)
            return evaluation;
    }
    const SeparatorBinType& bin_type = bin_types_[instance_.bin_type_id(item_state.bin_pos)];
    for (Counter hazard_pos = 0; hazard_pos < (Counter)bin_type.hazards.size(); ++hazard_pos) {
        const Hazard& hazard = bin_type.hazards[hazard_pos];
        if (!collide(item_pos, item_state, hazard))
            continue;
        collision = true;
        double weight = hazard_entry(item_pos, hazard_pos + 1).weight;
        double hazard_loss = collision_loss(
                item_pos,
                item_state,
                hazard,
                (loss_bound - loss) / weight);
        if (hazard_loss == std::numeric_limits<double>::infinity())
            return evaluation;
        loss += weight * hazard_loss;
        if (loss > loss_bound)
            return evaluation;
    }
    for (ItemPos item_pos_2: bins_items_[item_state.bin_pos]) {
        if (item_pos_2 == item_pos)
            continue;
        if (!collide(item_pos, item_state, item_pos_2))
            continue;
        collision = true;
        double weight = pair_entry(item_pos, item_pos_2).weight;
        double pair_loss = collision_loss(
                item_pos,
                item_state,
                item_pos_2,
                (loss_bound - loss) / weight);
        if (pair_loss == std::numeric_limits<double>::infinity())
            return evaluation;
        loss += weight * pair_loss;
        if (loss > loss_bound)
            return evaluation;
    }

    if (collision) {
        evaluation.status = Evaluation::Status::Collision;
        evaluation.loss = loss;
    } else {
        evaluation.status = Evaluation::Status::Clear;
        evaluation.loss = 0.0;
        if (insertion) {
            // Favor the bottom-left of the first bins (left-bottom-fill
            // constructor of Sparrow).
            AxisAlignedBoundingBox aabb = item_aabb(item_pos, item_state);
            evaluation.loss = 1e7 * item_state.bin_pos
                + 10 * (aabb.x_min + (aabb.x_min + aabb.x_max) / 2)
                + (aabb.y_min + (aabb.y_min + aabb.y_max) / 2);
        }
    }
    return evaluation;
}

bool Separator::sample(
        ItemPos item_pos,
        BinPos bin_pos,
        const AxisAlignedBoundingBox* focus,
        ItemState& item_state)
{
    BinTypeId bin_type_id = instance_.bin_type_id(bin_pos);
    const SeparatorItemType& item_type = item_types_[items_item_type_id_[item_pos]];
    const AxisAlignedBoundingBox& container = bin_types_[bin_type_id].container;

    // Orientations for which the item fits in the container (and whose center
    // can be in the focus box).
    struct Range
    {
        Counter orientation_id;
        LengthDbl x_min;
        LengthDbl x_max;
        LengthDbl y_min;
        LengthDbl y_max;
    };
    std::vector<Range> ranges;
    for (Counter orientation_id: orientation_ids_[bin_type_id][items_item_type_id_[item_pos]]) {
        const AxisAlignedBoundingBox& aabb = item_type.orientations[orientation_id].aabb;
        Range range;
        range.orientation_id = orientation_id;
        range.x_min = container.x_min - aabb.x_min;
        range.x_max = container.x_max - aabb.x_max;
        range.y_min = container.y_min - aabb.y_min;
        range.y_max = container.y_max - aabb.y_max;
        if (focus != nullptr) {
            LengthDbl center_x = (aabb.x_min + aabb.x_max) / 2;
            LengthDbl center_y = (aabb.y_min + aabb.y_max) / 2;
            range.x_min = (std::max)(range.x_min, focus->x_min - center_x);
            range.x_max = (std::min)(range.x_max, focus->x_max - center_x);
            range.y_min = (std::max)(range.y_min, focus->y_min - center_y);
            range.y_max = (std::min)(range.y_max, focus->y_max - center_y);
        }
        if (range.x_min <= range.x_max && range.y_min <= range.y_max)
            ranges.push_back(range);
    }
    if (ranges.empty())
        return false;

    std::uniform_int_distribution<Counter> range_distribution(0, ranges.size() - 1);
    const Range& range = ranges[range_distribution(generator_)];
    std::uniform_real_distribution<LengthDbl> distribution(0.0, 1.0);
    item_state.bin_pos = bin_pos;
    item_state.orientation_id = range.orientation_id;
    item_state.position.x = range.x_min + distribution(generator_) * (range.x_max - range.x_min);
    item_state.position.y = range.y_min + distribution(generator_) * (range.y_max - range.y_min);
    // Sometimes, snap the item against a side of the container: the contacts
    // with the container can then be exact.
    if (distribution(generator_) < snap_probability) {
        item_state.position.x = (distribution(generator_) < 0.5)?
            range.x_min:
            range.x_max;
    }
    if (distribution(generator_) < snap_probability) {
        item_state.position.y = (distribution(generator_) < 0.5)?
            range.y_min:
            range.y_max;
    }
    return true;
}

Separator::Candidate Separator::refine(
        ItemPos item_pos,
        const Candidate& candidate,
        LengthDbl step_initial,
        LengthDbl step_limit,
        bool insertion)
{
    // Coordinate descent of the Sparrow paper, on the translations only.
    enum class Axis { Horizontal, Vertical, ForwardDiagonal, BackwardDiagonal };
    std::uniform_int_distribution<int> axis_distribution(0, 3);
    Candidate current = candidate;
    LengthDbl step_x = step_initial;
    LengthDbl step_y = step_initial;

    // Positions for which the item is inside its container.
    const AxisAlignedBoundingBox& container = bin_types_[instance_.bin_type_id(candidate.state.bin_pos)].container;
    const AxisAlignedBoundingBox& aabb = item_types_[items_item_type_id_[item_pos]].orientations[candidate.state.orientation_id].aabb;
    LengthDbl range_x_min = container.x_min - aabb.x_min;
    LengthDbl range_x_max = container.x_max - aabb.x_max;
    LengthDbl range_y_min = container.y_min - aabb.y_min;
    LengthDbl range_y_max = container.y_max - aabb.y_max;
    Axis axis = (Axis)axis_distribution(generator_);
    for (Counter iteration = 0;
            iteration < 1000
            && (step_x >= step_limit || step_y >= step_limit);
            ++iteration) {
        LengthDbl delta_x = 0.0;
        LengthDbl delta_y = 0.0;
        switch (axis) {
        case Axis::Horizontal: delta_x = step_x; break;
        case Axis::Vertical: delta_y = step_y; break;
        case Axis::ForwardDiagonal: delta_x = step_x; delta_y = step_y; break;
        case Axis::BackwardDiagonal: delta_x = -step_x; delta_y = step_y; break;
        }
        Candidate best;
        for (int direction = -1; direction <= 1; direction += 2) {
            Candidate neighbor;
            neighbor.state = current.state;
            neighbor.state.position.x += direction * delta_x;
            neighbor.state.position.y += direction * delta_y;
            // Stop at the sides of the container: the contacts with the
            // container can then be exact.
            if (range_x_min <= range_x_max) {
                neighbor.state.position.x = (std::max)(range_x_min, (std::min)(range_x_max, neighbor.state.position.x));
            }
            if (range_y_min <= range_y_max) {
                neighbor.state.position.y = (std::max)(range_y_min, (std::min)(range_y_max, neighbor.state.position.y));
            }
            neighbor.evaluation = evaluate(item_pos, neighbor.state, current.evaluation, insertion);
            if (direction == -1 || neighbor.evaluation < best.evaluation)
                best = neighbor;
        }
        bool better = best.evaluation < current.evaluation;
        bool worse = current.evaluation < best.evaluation;
        if (!worse)
            current = best;
        double multiplier = (better)?
            coordinate_descent_step_success:
            coordinate_descent_step_failure;
        switch (axis) {
        case Axis::Horizontal: step_x *= multiplier; break;
        case Axis::Vertical: step_y *= multiplier; break;
        case Axis::ForwardDiagonal: case Axis::BackwardDiagonal:
            step_x *= std::sqrt(multiplier);
            step_y *= std::sqrt(multiplier);
            break;
        }
        if (!better)
            axis = (Axis)axis_distribution(generator_);
    }
    return current;
}

Separator::Candidate Separator::search_placement(
        ItemPos item_pos,
        bool insertion)
{
    // Algorithm 6 of the Sparrow paper.
    const SeparatorItemType& item_type = item_types_[items_item_type_id_[item_pos]];
    LengthDbl minimum_dimension = item_type.minimum_dimension;
    LengthDbl unique_threshold = minimum_dimension * unique_sample_threshold_ratio;
    Counter number_of_best_candidates = parameters_.number_of_coordinate_descents;

    // Best candidates, sorted, pairwise distinct.
    std::vector<Candidate> best_candidates;
    auto upper_bound = [&best_candidates, number_of_best_candidates]()
    {
        return ((Counter)best_candidates.size() < number_of_best_candidates)?
            Evaluation():
            best_candidates.back().evaluation;
    };
    auto similar = [unique_threshold](const ItemState& item_state_1, const ItemState& item_state_2)
    {
        return item_state_1.bin_pos == item_state_2.bin_pos
            && item_state_1.orientation_id == item_state_2.orientation_id
            && std::abs(item_state_1.position.x - item_state_2.position.x) < unique_threshold
            && std::abs(item_state_1.position.y - item_state_2.position.y) < unique_threshold;
    };
    auto report = [&best_candidates, &upper_bound, &similar, number_of_best_candidates](
            const Candidate& candidate)
    {
        if (candidate.evaluation.status == Evaluation::Status::Invalid)
            return;
        if ((Counter)best_candidates.size() >= number_of_best_candidates
                && !(candidate.evaluation < upper_bound())) {
            return;
        }
        bool any_similar = false;
        bool better_than_all_similar = true;
        for (const Candidate& best_candidate: best_candidates) {
            if (!similar(best_candidate.state, candidate.state))
                continue;
            any_similar = true;
            if (!(candidate.evaluation < best_candidate.evaluation))
                better_than_all_similar = false;
        }
        if (any_similar) {
            if (!better_than_all_similar)
                return;
            best_candidates.erase(
                    std::remove_if(
                        best_candidates.begin(),
                        best_candidates.end(),
                        [&similar, &candidate](const Candidate& best_candidate)
                        {
                            return similar(best_candidate.state, candidate.state);
                        }),
                    best_candidates.end());
        } else if ((Counter)best_candidates.size() >= number_of_best_candidates) {
            best_candidates.pop_back();
        }
        best_candidates.push_back(candidate);
        std::stable_sort(
                best_candidates.begin(),
                best_candidates.end(),
                [](const Candidate& candidate_1, const Candidate& candidate_2)
                {
                    return candidate_1.evaluation < candidate_2.evaluation;
                });
    };

    std::uniform_int_distribution<BinPos> bin_distribution(0, instance_.number_of_bins() - 1);
    if (!insertion) {
        // Current placement, and samples around it.
        Candidate current;
        current.state = items_state_[item_pos];
        current.evaluation = evaluate(item_pos, current.state, upper_bound(), insertion);
        report(current);
        AxisAlignedBoundingBox focus = item_aabb(item_pos, current.state);
        for (Counter sample_pos = 0;
                sample_pos < parameters_.number_of_focused_samples;
                ++sample_pos) {
            Candidate candidate;
            if (!sample(item_pos, current.state.bin_pos, &focus, candidate.state))
                break;
            candidate.evaluation = evaluate(item_pos, candidate.state, upper_bound(), insertion);
            report(candidate);
        }
    }

    // Samples in the whole bins.
    Counter number_of_samples = (insertion)?
        parameters_.number_of_insertion_samples:
        parameters_.number_of_container_samples;
    for (Counter sample_pos = 0; sample_pos < number_of_samples; ++sample_pos) {
        Candidate candidate;
        if (!sample(item_pos, bin_distribution(generator_), nullptr, candidate.state))
            continue;
        candidate.evaluation = evaluate(item_pos, candidate.state, upper_bound(), insertion);
        report(candidate);
    }

    if (best_candidates.empty()) {
        Candidate candidate;
        if (!insertion)
            candidate.state = items_state_[item_pos];
        return candidate;
    }

    // Refine the best samples, then refine the best one further.
    std::vector<Candidate> starts = best_candidates;
    for (const Candidate& start: starts) {
        report(refine(
                    item_pos,
                    start,
                    first_refinement_step_initial_ratio * minimum_dimension,
                    first_refinement_step_limit_ratio * minimum_dimension,
                    insertion));
    }
    return refine(
            item_pos,
            best_candidates.front(),
            final_refinement_step_initial_ratio * minimum_dimension,
            final_refinement_step_limit_ratio * minimum_dimension,
            insertion);
}

void Separator::move_items(const optimizationtools::Timer& timer)
{
    // Algorithm 5 of the Sparrow paper.
    std::vector<ItemPos> colliding_items;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos)
        if (items_state_[item_pos].bin_pos != -1 && item_loss(item_pos) > 0)
            colliding_items.push_back(item_pos);
    std::shuffle(colliding_items.begin(), colliding_items.end(), generator_);
    for (ItemPos item_pos: colliding_items) {
        if (timer.needs_to_end())
            return;
        if (item_loss(item_pos) == 0)
            continue;
        Candidate candidate = search_placement(item_pos, false);
        remove_item(item_pos);
        add_item(item_pos, candidate.state);
        number_of_moves_++;
    }
}

double Separator::total_weighted_loss() const
{
    double loss = 0.0;
    for (const Entry& entry: pair_entries_)
        loss += entry.weight * entry.loss;
    for (const Entry& entry: hazard_entries_)
        loss += entry.weight * entry.loss;
    return loss;
}

void Separator::copy_state(const Separator& separator)
{
    items_state_ = separator.items_state_;
    bins_items_ = separator.bins_items_;
    items_bin_item_pos_ = separator.items_bin_item_pos_;
    pair_entries_ = separator.pair_entries_;
    hazard_entries_ = separator.hazard_entries_;
}

void Separator::move_items_workers(const optimizationtools::Timer& timer)
{
    // Algorithm 10 of the Sparrow paper.
    if (workers_.empty()) {
        for (Counter worker_pos = 0;
                worker_pos < parameters_.number_of_workers;
                ++worker_pos) {
            std::shared_ptr<Separator> worker(new Separator(*this));
            worker->workers_.clear();
            worker->generator_.seed(generator_());
            workers_.push_back(worker);
        }
    }

    std::vector<std::thread> threads;
    for (const std::shared_ptr<Separator>& worker: workers_) {
        worker->copy_state(*this);
        Separator* worker_pointer = worker.get();
        threads.push_back(std::thread(
                    [worker_pointer, &timer]()
                    {
                        worker_pointer->move_items(timer);
                    }));
    }
    for (std::thread& thread: threads)
        thread.join();

    // Keep the layout of the worker with the smallest weighted loss.
    Counter best_worker_pos = 0;
    double best_weighted_loss = std::numeric_limits<double>::infinity();
    for (Counter worker_pos = 0; worker_pos < (Counter)workers_.size(); ++worker_pos) {
        Separator& worker = *workers_[worker_pos];
        number_of_evaluations_ += worker.number_of_evaluations_;
        number_of_moves_ += worker.number_of_moves_;
        worker.number_of_evaluations_ = 0;
        worker.number_of_moves_ = 0;
        double weighted_loss = worker.total_weighted_loss();
        if (weighted_loss < best_weighted_loss) {
            best_weighted_loss = weighted_loss;
            best_worker_pos = worker_pos;
        }
    }
    copy_state(*workers_[best_worker_pos]);
}

bool Separator::separate_initial(const optimizationtools::Timer& timer)
{
    SeparatorParameters parameters = parameters_;
    if (initial_maximum_number_of_iterations_without_improvement_ != -1) {
        parameters_.maximum_number_of_iterations_without_improvement
            = initial_maximum_number_of_iterations_without_improvement_;
    }
    if (initial_maximum_number_of_strikes_ != -1)
        parameters_.maximum_number_of_strikes = initial_maximum_number_of_strikes_;
    bool separated = separate(timer);
    parameters_ = parameters;
    return separated;
}

bool Separator::separate(
        const optimizationtools::Timer& timer,
        double reference_loss)
{
    // Algorithm 9 of the Sparrow paper.
    struct Snapshot
    {
        std::vector<ItemState> items_state;
        std::vector<double> pair_losses;
        std::vector<double> hazard_losses;
    };
    auto save = [this]()
    {
        Snapshot snapshot;
        snapshot.items_state = items_state_;
        for (const Entry& entry: pair_entries_)
            snapshot.pair_losses.push_back(entry.loss);
        for (const Entry& entry: hazard_entries_)
            snapshot.hazard_losses.push_back(entry.loss);
        return snapshot;
    };
    // Restore a snapshot, keeping the weights.
    auto restore = [this](const Snapshot& snapshot)
    {
        for (std::vector<ItemPos>& bin_items: bins_items_)
            bin_items.clear();
        for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
            items_state_[item_pos] = snapshot.items_state[item_pos];
            items_bin_item_pos_[item_pos] = -1;
            BinPos bin_pos = items_state_[item_pos].bin_pos;
            if (bin_pos != -1) {
                items_bin_item_pos_[item_pos] = bins_items_[bin_pos].size();
                bins_items_[bin_pos].push_back(item_pos);
            }
        }
        for (Counter entry_pos = 0; entry_pos < (Counter)pair_entries_.size(); ++entry_pos)
            pair_entries_[entry_pos].loss = snapshot.pair_losses[entry_pos];
        for (Counter entry_pos = 0; entry_pos < (Counter)hazard_entries_.size(); ++entry_pos)
            hazard_entries_[entry_pos].loss = snapshot.hazard_losses[entry_pos];
    };

    double minimum_loss = total_loss();
    if (minimum_loss == 0)
        return true;
    Snapshot best = save();
    Counter number_of_strikes = 0;
    for (;;) {
        Counter maximum_number_of_strikes = (minimum_loss < reference_loss)?
            parameters_.maximum_number_of_strikes:
            parameters_.maximum_number_of_strikes_above_reference;
        if (number_of_strikes >= maximum_number_of_strikes
                || timer.needs_to_end()) {
            break;
        }
        Counter number_of_iterations_without_improvement = 0;
        double initial_loss = total_loss();
        while (number_of_iterations_without_improvement
                < parameters_.maximum_number_of_iterations_without_improvement) {
            if (timer.needs_to_end())
                break;
            if (parameters_.number_of_workers <= 1) {
                move_items(timer);
            } else {
                move_items_workers(timer);
            }
            number_of_iterations_++;
            count_moves();
            double loss = total_loss();
            if (loss == 0) {
                return true;
            } else if (loss < minimum_loss) {
                if (loss < 0.98 * minimum_loss)
                    number_of_iterations_without_improvement = 0;
                best = save();
                minimum_loss = loss;
            } else {
                number_of_iterations_without_improvement++;
            }
            update_weights();
        }
        if (initial_loss * 0.98 <= minimum_loss) {
            number_of_strikes++;
        } else {
            number_of_strikes = 0;
        }
        restore(best);
    }
    return false;
}

void Separator::disrupt()
{
    std::vector<ItemPos> placed_items;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos)
        if (items_state_[item_pos].bin_pos != -1)
            placed_items.push_back(item_pos);
    if (placed_items.size() < 2)
        return;

    // Large items: the largest ones covering a given proportion of the total
    // convex hull area.
    std::vector<ItemPos> sorted_items = placed_items;
    std::sort(
            sorted_items.begin(),
            sorted_items.end(),
            [this](ItemPos item_pos_1, ItemPos item_pos_2)
            {
                return item_types_[items_item_type_id_[item_pos_1]].convex_hull_area
                    > item_types_[items_item_type_id_[item_pos_2]].convex_hull_area;
            });
    AreaDbl total_convex_hull_area = 0.0;
    for (ItemPos item_pos: sorted_items)
        total_convex_hull_area += item_types_[items_item_type_id_[item_pos]].convex_hull_area;
    AreaDbl convex_hull_area_cutoff = 0.0;
    AreaDbl cumulated_convex_hull_area = 0.0;
    for (ItemPos item_pos: sorted_items) {
        AreaDbl convex_hull_area = item_types_[items_item_type_id_[item_pos]].convex_hull_area;
        cumulated_convex_hull_area += convex_hull_area;
        if (cumulated_convex_hull_area > large_item_convex_hull_area_ratio * total_convex_hull_area) {
            convex_hull_area_cutoff = convex_hull_area;
            break;
        }
    }
    std::vector<ItemPos> large_items;
    for (ItemPos item_pos: placed_items)
        if (item_types_[items_item_type_id_[item_pos]].convex_hull_area >= convex_hull_area_cutoff)
            large_items.push_back(item_pos);

    // Two different large items.
    std::uniform_int_distribution<ItemPos> large_distribution(0, large_items.size() - 1);
    ItemPos item_pos_1 = large_items[large_distribution(generator_)];
    const SeparatorItemType& item_type_1 = item_types_[items_item_type_id_[item_pos_1]];
    std::vector<ItemPos> candidates_2;
    for (ItemPos item_pos: large_items) {
        const SeparatorItemType& item_type = item_types_[items_item_type_id_[item_pos]];
        if (std::abs(item_type.convex_hull_area - item_type_1.convex_hull_area) > 0.01 * item_type_1.convex_hull_area
                && std::abs(item_type.diameter - item_type_1.diameter) > 0.01 * item_type_1.diameter) {
            candidates_2.push_back(item_pos);
        }
    }
    if (candidates_2.empty())
        for (ItemPos item_pos: placed_items)
            if (item_pos != item_pos_1)
                candidates_2.push_back(item_pos);
    std::uniform_int_distribution<ItemPos> distribution_2(0, candidates_2.size() - 1);
    ItemPos item_pos_2 = candidates_2[distribution_2(generator_)];

    // Swap them: each one takes the center of the bounding box of the other.
    std::vector<ItemState> layout = items_state_;
    ItemPos swapped_items[2] = {item_pos_1, item_pos_2};
    for (int swap_pos = 0; swap_pos < 2; ++swap_pos) {
        ItemPos item_pos = swapped_items[swap_pos];
        ItemPos item_pos_other = swapped_items[1 - swap_pos];
        const SeparatorItemType& item_type = item_types_[items_item_type_id_[item_pos]];
        const ItemState& item_state_old = items_state_[item_pos];
        const ItemState& item_state_other = items_state_[item_pos_other];
        BinTypeId bin_type_id_new = instance_.bin_type_id(item_state_other.bin_pos);
        const std::vector<Counter>& orientation_ids = orientation_ids_[bin_type_id_new][items_item_type_id_[item_pos]];
        if (orientation_ids.empty())
            return;
        ItemState item_state_new;
        item_state_new.bin_pos = item_state_other.bin_pos;
        item_state_new.orientation_id = (std::find(
                    orientation_ids.begin(),
                    orientation_ids.end(),
                    item_state_old.orientation_id) != orientation_ids.end())?
            item_state_old.orientation_id:
            orientation_ids.front();
        AxisAlignedBoundingBox aabb_other = item_aabb(item_pos_other, item_state_other);
        const AxisAlignedBoundingBox& aabb = item_type.orientations[item_state_new.orientation_id].aabb;
        item_state_new.position = {
            (aabb_other.x_min + aabb_other.x_max) / 2 - (aabb.x_min + aabb.x_max) / 2,
            (aabb_other.y_min + aabb_other.y_max) / 2 - (aabb.y_min + aabb.y_max) / 2};
        layout[item_pos] = item_state_new;

        // The items covered by the moved item (whose interior point is inside
        // it) are moved to the space it left.
        AxisAlignedBoundingBox aabb_old = item_aabb(item_pos, item_state_old);
        AxisAlignedBoundingBox aabb_new = item_aabb(item_pos, item_state_new);
        Point translation = {
            (aabb_old.x_min + aabb_old.x_max) / 2 - (aabb_new.x_min + aabb_new.x_max) / 2,
            (aabb_old.y_min + aabb_old.y_max) / 2 - (aabb_new.y_min + aabb_new.y_max) / 2};
        const Orientation& orientation_new = item_type.orientations[item_state_new.orientation_id];
        for (ItemPos item_pos_covered: bins_items_[item_state_new.bin_pos]) {
            if (item_pos_covered == item_pos_1 || item_pos_covered == item_pos_2)
                continue;
            const ItemState& item_state_covered = items_state_[item_pos_covered];
            const Orientation& orientation_covered = item_types_[items_item_type_id_[item_pos_covered]].orientations[item_state_covered.orientation_id];
            Point interior_point = orientation_covered.polygons.front().interior_point;
            interior_point.x += item_state_covered.position.x - item_state_new.position.x;
            interior_point.y += item_state_covered.position.y - item_state_new.position.y;
            bool covered = false;
            for (const Polygon& polygon: orientation_new.polygons)
                if (locate_point(interior_point, polygon) == PointLocation::Inside)
                    covered = true;
            if (!covered)
                continue;
            const std::vector<Counter>& orientation_ids_old = orientation_ids_[instance_.bin_type_id(item_state_old.bin_pos)][items_item_type_id_[item_pos_covered]];
            if (std::find(
                        orientation_ids_old.begin(),
                        orientation_ids_old.end(),
                        item_state_covered.orientation_id) == orientation_ids_old.end()) {
                continue;
            }
            layout[item_pos_covered].bin_pos = item_state_old.bin_pos;
            layout[item_pos_covered].position.x = item_state_covered.position.x + translation.x;
            layout[item_pos_covered].position.y = item_state_covered.position.y + translation.y;
        }
    }
    set_layout(layout);
}


void Separator::count_moves()
{
    if (moves_limit_ == nullptr)
        return;
    moves_limit_->number_of_moves += number_of_moves_ - number_of_moves_counted_;
    number_of_moves_counted_ = number_of_moves_;
    Counter number_of_workers = (std::max)((Counter)1, parameters_.number_of_workers);
    if (moves_limit_->maximum_number_of_moves >= 0
            && moves_limit_->number_of_moves
            >= moves_limit_->maximum_number_of_moves * number_of_workers) {
        moves_limit_->reached = true;
    }
}
