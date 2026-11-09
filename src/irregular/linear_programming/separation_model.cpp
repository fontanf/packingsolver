#include "irregular/linear_programming/separation_model.hpp"

#include "irregular/nonlinear_programming/separating_line.hpp"
#include "irregular/utils.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef HIGHS_FOUND
#include "mathoptsolverscmake/mathopt_highs.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <limits>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/** Largest angle between two consecutive points sampled on an arc. */
const Angle approximation_maximum_angle = 2 * M_PI / 64;

/**
 * Return true iff the direction 'direction' is in the angle 'angle' measured
 * anticlockwise from 'start_direction'.
 */
bool in_angle(
        const Point& start_direction,
        Angle angle,
        const Point& direction)
{
    if (angle >= 2 * M_PI)
        return true;
    Angle direction_angle = std::atan2(
            start_direction.x * direction.y - start_direction.y * direction.x,
            start_direction.x * direction.x + start_direction.y * direction.y);
    if (direction_angle < 0)
        direction_angle += 2 * M_PI;
    return direction_angle <= angle;
}

}

BinSeparationModel::BinSeparationModel(
        const Instance& instance,
        const InstanceBasicShapeDecomposition& decomposition,
        BinTypeId bin_type_id,
        const std::vector<SeparationModelItem>& items):
    instance_(instance),
    bin_type_id_(bin_type_id)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const LengthDbl scale_value = instance.parameters().scale_value;
    item_spacing_ = instance.item_spacing_scaled();
    bin_spacing_ = instance.bin_spacing_scaled(bin_type_id);
    maximum_spacing_ = (std::max)(item_spacing_, bin_spacing_);

    // Return false if the shape isn't convex from the material's side.
    auto add_part = [this](
            ItemPos item_pos,
            const Shape& shape,
            const Point& shift,
            LengthDbl spacing)
    {
        Part part;
        part.item_pos = item_pos;
        part.spacing = spacing;
        for (const ShapeElement& element: shape.elements) {
            Point start = {element.start.x + shift.x, element.start.y + shift.y};
            Point end = {element.end.x + shift.x, element.end.y + shift.y};
            part.vertices.push_back(start);
            part.aabb.x_min = (std::min)(part.aabb.x_min, start.x);
            part.aabb.x_max = (std::max)(part.aabb.x_max, start.x);
            part.aabb.y_min = (std::min)(part.aabb.y_min, start.y);
            part.aabb.y_max = (std::max)(part.aabb.y_max, start.y);
            part.approximation.push_back(start);
            if (element.type == ShapeElementType::LineSegment) {
                part.edges.push_back({start, end});
                continue;
            }
            if (element.orientation == ShapeElementOrientation::Clockwise)
                return false;
            Arc arc;
            arc.center = {element.center.x + shift.x, element.center.y + shift.y};
            arc.radius = distance(element.start, element.center);
            if (arc.radius == 0.0)
                continue;
            arc.start_direction = {
                (start.x - arc.center.x) / arc.radius,
                (start.y - arc.center.y) / arc.radius};
            if (element.orientation == ShapeElementOrientation::Full) {
                arc.angle = 2 * M_PI;
            } else {
                Point end_direction = {
                    (end.x - arc.center.x) / arc.radius,
                    (end.y - arc.center.y) / arc.radius};
                arc.angle = std::atan2(
                        arc.start_direction.x * end_direction.y
                        - arc.start_direction.y * end_direction.x,
                        arc.start_direction.x * end_direction.x
                        + arc.start_direction.y * end_direction.y);
                if (arc.angle <= 0)
                    arc.angle += 2 * M_PI;
            }
            part.arcs.push_back(arc);
            // Extreme points of the arc in the directions of the axes.
            if (in_angle(arc.start_direction, arc.angle, {1, 0}))
                part.aabb.x_max = (std::max)(part.aabb.x_max, arc.center.x + arc.radius);
            if (in_angle(arc.start_direction, arc.angle, {-1, 0}))
                part.aabb.x_min = (std::min)(part.aabb.x_min, arc.center.x - arc.radius);
            if (in_angle(arc.start_direction, arc.angle, {0, 1}))
                part.aabb.y_max = (std::max)(part.aabb.y_max, arc.center.y + arc.radius);
            if (in_angle(arc.start_direction, arc.angle, {0, -1}))
                part.aabb.y_min = (std::min)(part.aabb.y_min, arc.center.y - arc.radius);
            // Points sampled on the arc, its end excluded.
            Counter number_of_pieces = (Counter)std::ceil(arc.angle / approximation_maximum_angle);
            Angle start_angle = std::atan2(arc.start_direction.y, arc.start_direction.x);
            for (Counter piece_pos = 1; piece_pos < number_of_pieces; ++piece_pos) {
                Angle angle = start_angle + arc.angle * piece_pos / number_of_pieces;
                part.approximation.push_back({
                        arc.center.x + arc.radius * std::cos(angle),
                        arc.center.y + arc.radius * std::sin(angle)});
            }
        }
        if (item_pos != -1) {
            for (const Point& vertex: part.approximation) {
                part.center.x += vertex.x / part.approximation.size();
                part.center.y += vertex.y / part.approximation.size();
            }
        }
        parts_.push_back(part);
        return true;
    };

    const Point origin = {0.0, 0.0};
    items_aabbs_.resize(items.size());
    items_centers_aabbs_.resize(items.size());
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items.size(); ++item_pos) {
        const SeparationModelItem& item = items[item_pos];
        const ItemType& item_type = instance.item_type(item.item_type_id);
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            for (const BasicShape& basic_shape: decomposition.item_types[item.item_type_id][item_shape_pos]) {
                BasicShape placed_shape = apply_placement(basic_shape, item.mirror, item.angle);
                if (item.is_fixed()) {
                    if (!add_part(-1, placed_shape.shape, item.position, item_spacing_)) {
                        supported_ = false;
                        return;
                    }
                } else {
                    if (!add_part(item_pos, placed_shape.shape, origin, 0.0)) {
                        supported_ = false;
                        return;
                    }
                    items_aabbs_[item_pos] = merge(items_aabbs_[item_pos], parts_.back().aabb);
                    const Point& center = parts_.back().center;
                    AxisAlignedBoundingBox& centers_aabb = items_centers_aabbs_[item_pos];
                    centers_aabb.x_min = (std::min)(centers_aabb.x_min, center.x);
                    centers_aabb.x_max = (std::max)(centers_aabb.x_max, center.x);
                    centers_aabb.y_min = (std::min)(centers_aabb.y_min, center.y);
                    centers_aabb.y_max = (std::max)(centers_aabb.y_max, center.y);
                }
            }
        }
    }
    for (const BasicShape& basic_shape: decomposition.bin_types_borders[bin_type_id]) {
        if (!basic_shape.shape.is_polygon()) {
            supported_ = false;
            return;
        }
        add_part(-1, basic_shape.shape, origin, bin_spacing_);
    }
    // The defect of each part of the decomposition of the defects isn't
    // known: use the largest item-defect spacing of the bin.
    LengthDbl defect_spacing = 0.0;
    for (const auto& defect: bin_type.defects)
        defect_spacing = (std::max)(defect_spacing, scale_value * defect.item_defect_minimum_spacing);
    maximum_spacing_ = (std::max)(maximum_spacing_, defect_spacing);
    for (const BasicShape& basic_shape: decomposition.bin_types_defects[bin_type_id]) {
        if (!add_part(-1, basic_shape.shape, origin, defect_spacing)) {
            supported_ = false;
            return;
        }
    }
}

LengthDbl BinSeparationModel::support_value(
        const Part& part,
        const Point& normal)
{
    LengthDbl value = -std::numeric_limits<LengthDbl>::infinity();
    for (const Point& vertex: part.vertices)
        value = (std::max)(value, normal.x * vertex.x + normal.y * vertex.y);
    for (const Arc& arc: part.arcs) {
        if (in_angle(arc.start_direction, arc.angle, normal)) {
            value = (std::max)(
                    value,
                    normal.x * arc.center.x + normal.y * arc.center.y + arc.radius);
        }
    }
    return value;
}

BinSeparationModel::Constraint BinSeparationModel::build_constraint(
        Counter edge_part_pos,
        Counter support_part_pos,
        const Point& normal,
        LengthDbl edge_support_value) const
{
    const Part& edge_part = parts_[edge_part_pos];
    const Part& support_part = parts_[support_part_pos];
    Constraint constraint;
    constraint.edge_part_pos = edge_part_pos;
    constraint.support_part_pos = support_part_pos;
    constraint.normal = normal;
    constraint.edge_constant = normal.x * edge_part.center.x
        + normal.y * edge_part.center.y;
    constraint.edge_coefficient = edge_support_value - constraint.edge_constant;
    constraint.support_constant = normal.x * support_part.center.x
        + normal.y * support_part.center.y;
    constraint.support_coefficient = -support_value(support_part, {-normal.x, -normal.y})
        - constraint.support_constant;
    constraint.spacing = (edge_part.item_pos == -1)? edge_part.spacing:
        (support_part.item_pos == -1)? support_part.spacing: item_spacing_;
    return constraint;
}

std::vector<Point> BinSeparationModel::state_approximation(
        const std::vector<SeparationModelItem>& items,
        const Part& part) const
{
    if (part.item_pos == -1)
        return part.approximation;
    const SeparationModelItem& item = items[part.item_pos];
    std::vector<Point> vertices;
    for (const Point& vertex: part.approximation) {
        vertices.push_back({
                item.position.x + part.center.x + item.lambda * (vertex.x - part.center.x),
                item.position.y + part.center.y + item.lambda * (vertex.y - part.center.y)});
    }
    return vertices;
}

AxisAlignedBoundingBox BinSeparationModel::part_aabb(
        const std::vector<SeparationModelItem>& items,
        Counter part_pos) const
{
    const Part& part = parts_[part_pos];
    if (part.item_pos == -1)
        return part.aabb;
    const SeparationModelItem& item = items[part.item_pos];
    AxisAlignedBoundingBox aabb;
    aabb.x_min = item.position.x + part.center.x + item.lambda * (part.aabb.x_min - part.center.x);
    aabb.x_max = item.position.x + part.center.x + item.lambda * (part.aabb.x_max - part.center.x);
    aabb.y_min = item.position.y + part.center.y + item.lambda * (part.aabb.y_min - part.center.y);
    aabb.y_max = item.position.y + part.center.y + item.lambda * (part.aabb.y_max - part.center.y);
    return aabb;
}

LengthDbl BinSeparationModel::value(
        const std::vector<SeparationModelItem>& items,
        const Constraint& constraint) const
{
    ItemPos edge_item_pos = parts_[constraint.edge_part_pos].item_pos;
    ItemPos support_item_pos = parts_[constraint.support_part_pos].item_pos;
    Point edge_position = {0.0, 0.0};
    Point support_position = {0.0, 0.0};
    double edge_lambda = 1.0;
    double support_lambda = 1.0;
    if (edge_item_pos != -1) {
        edge_position = items[edge_item_pos].position;
        edge_lambda = items[edge_item_pos].lambda;
    }
    if (support_item_pos != -1) {
        support_position = items[support_item_pos].position;
        support_lambda = items[support_item_pos].lambda;
    }
    return constraint.normal.x * (support_position.x - edge_position.x)
        + constraint.normal.y * (support_position.y - edge_position.y)
        + constraint.support_constant + constraint.support_coefficient * support_lambda
        - constraint.edge_constant - constraint.edge_coefficient * edge_lambda
        - constraint.spacing;
}

LengthDbl BinSeparationModel::null_scales_value(
        const std::vector<SeparationModelItem>& items,
        const Constraint& constraint) const
{
    // The parts of the obstacles keep their full scale.
    ItemPos edge_item_pos = parts_[constraint.edge_part_pos].item_pos;
    ItemPos support_item_pos = parts_[constraint.support_part_pos].item_pos;
    Point edge_position = {0.0, 0.0};
    Point support_position = {0.0, 0.0};
    double edge_lambda = 1.0;
    double support_lambda = 1.0;
    if (edge_item_pos != -1) {
        edge_position = items[edge_item_pos].position;
        edge_lambda = 0.0;
    }
    if (support_item_pos != -1) {
        support_position = items[support_item_pos].position;
        support_lambda = 0.0;
    }
    return constraint.normal.x * (support_position.x - edge_position.x)
        + constraint.normal.y * (support_position.y - edge_position.y)
        + constraint.support_constant + constraint.support_coefficient * support_lambda
        - constraint.edge_constant - constraint.edge_coefficient * edge_lambda
        - constraint.spacing;
}

BinSeparationModel::Constraint BinSeparationModel::best_constraint(
        const std::vector<SeparationModelItem>& items,
        Counter part_pos_1,
        Counter part_pos_2,
        bool fixed_scales,
        LengthDbl& best_value) const
{
    const LengthDbl infinity = std::numeric_limits<LengthDbl>::infinity();
    const Part& part_1 = parts_[part_pos_1];
    const Part& part_2 = parts_[part_pos_2];

    // Best separating direction, among the candidates whose constraints are
    // satisfied with null scales, and among all the candidates.
    Constraint best_constraint;
    best_value = -infinity;
    Constraint best_feasible_constraint;
    LengthDbl best_feasible_value = -infinity;
    bool feasible_constraint_found = false;
    auto consider = [&](const Constraint& constraint)
    {
        LengthDbl value_cur = value(items, constraint);
        if (value_cur > best_value) {
            best_value = value_cur;
            best_constraint = constraint;
        }
        if (null_scales_value(items, constraint) >= 0.0
                && value_cur > best_feasible_value) {
            best_feasible_value = value_cur;
            best_feasible_constraint = constraint;
            feasible_constraint_found = true;
        }
    };

    // Outward normals of the edges.
    for (int side = 0; side < 2; ++side) {
        Counter edge_part_pos = (side == 0)? part_pos_1: part_pos_2;
        Counter support_part_pos = (side == 0)? part_pos_2: part_pos_1;
        const Part& edge_part = parts_[edge_part_pos];
        for (const auto& edge: edge_part.edges) {
            const Point& edge_start = edge.first;
            const Point& edge_end = edge.second;
            LengthDbl delta_x = edge_end.x - edge_start.x;
            LengthDbl delta_y = edge_end.y - edge_start.y;
            LengthDbl length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
            if (length == 0.0)
                continue;
            // The parts are anticlockwise: the outward normal is on the right
            // of the edge.
            Point normal = {delta_y / length, -delta_x / length};
            consider(build_constraint(
                        edge_part_pos,
                        support_part_pos,
                        normal,
                        normal.x * edge_start.x + normal.y * edge_start.y));
        }
    }

    bool has_arcs = !part_1.arcs.empty() || !part_2.arcs.empty();

    if (has_arcs) {
        // Direction between the centers of the parts; for two circles, the
        // direction which separates them the best.
        Point center_1 = part_1.center;
        Point center_2 = part_2.center;
        if (part_1.item_pos != -1) {
            center_1.x += items[part_1.item_pos].position.x;
            center_1.y += items[part_1.item_pos].position.y;
        }
        if (part_2.item_pos != -1) {
            center_2.x += items[part_2.item_pos].position.x;
            center_2.y += items[part_2.item_pos].position.y;
        }
        LengthDbl delta_x = center_2.x - center_1.x;
        LengthDbl delta_y = center_2.y - center_1.y;
        LengthDbl length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
        if (length > 0.0) {
            Point normal = {delta_x / length, delta_y / length};
            consider(build_constraint(
                        part_pos_1,
                        part_pos_2,
                        normal,
                        support_value(part_1, normal)));
        }
    }

    if (fixed_scales || has_arcs) {
        // The separating line maximizing the gap between the approximations
        // of the parts. It isn't an edge normal when the closest points of
        // the parts are two vertices, for example two corners facing each
        // other diagonally: then the gap along each edge normal may be
        // smaller than the spacing although the distance between the parts
        // isn't.
        const SeparatingLine separating_line = compute_separating_line(
                state_approximation(items, part_1),
                state_approximation(items, part_2));
        // The approximation of a part with a null scale is a point.
        if (std::isfinite(separating_line.normal.x)
                && std::isfinite(separating_line.normal.y)) {
            Point normal = separating_line.normal;
            if (has_arcs) {
                // The approximations of the arcs are inscribed polygons, so
                // the direction may be off by up to the angle between two
                // consecutive points of the approximations: refine it by
                // maximizing the gap between the parts themselves.
                auto direction_value = [&](Angle angle)
                {
                    Point direction = {std::cos(angle), std::sin(angle)};
                    return value(items, build_constraint(
                                part_pos_1,
                                part_pos_2,
                                direction,
                                support_value(part_1, direction)));
                };
                Angle normal_angle = std::atan2(normal.y, normal.x);
                Angle angle_min = normal_angle - approximation_maximum_angle;
                Angle angle_max = normal_angle + approximation_maximum_angle;
                for (Counter iteration = 0; iteration < 40; ++iteration) {
                    Angle angle_1 = (2 * angle_min + angle_max) / 3;
                    Angle angle_2 = (angle_min + 2 * angle_max) / 3;
                    if (direction_value(angle_1) < direction_value(angle_2)) {
                        angle_min = angle_1;
                    } else {
                        angle_max = angle_2;
                    }
                }
                Angle refined_angle = (angle_min + angle_max) / 2;
                consider(build_constraint(
                            part_pos_1,
                            part_pos_2,
                            {std::cos(refined_angle), std::sin(refined_angle)},
                            support_value(part_1, {std::cos(refined_angle), std::sin(refined_angle)})));
            }
            consider(build_constraint(
                        part_pos_1,
                        part_pos_2,
                        normal,
                        support_value(part_1, normal)));
        }
    }

    if (fixed_scales || !feasible_constraint_found)
        return best_constraint;
    return best_feasible_constraint;
}

LengthDbl BinSeparationModel::compute_constraints(
        const std::vector<SeparationModelItem>& items,
        const std::vector<LengthDbl>& movements,
        bool fixed_scales,
        std::vector<Constraint>& constraints) const
{
    const LengthDbl infinity = std::numeric_limits<LengthDbl>::infinity();
    constraints.clear();

    // Boxes of the parts, those of the items expanded by their maximum move
    // (at full scale) and the spacing.
    std::vector<AxisAlignedBoundingBox> boxes(parts_.size());
    std::vector<Counter> sorted_parts(parts_.size());
    for (Counter part_pos = 0; part_pos < (Counter)parts_.size(); ++part_pos) {
        const Part& part = parts_[part_pos];
        AxisAlignedBoundingBox box = part.aabb;
        if (part.item_pos != -1) {
            const SeparationModelItem& item = items[part.item_pos];
            LengthDbl expansion = movements[part.item_pos] + maximum_spacing_;
            box.x_min = item.position.x + part.aabb.x_min - expansion;
            box.x_max = item.position.x + part.aabb.x_max + expansion;
            box.y_min = item.position.y + part.aabb.y_min - expansion;
            box.y_max = item.position.y + part.aabb.y_max + expansion;
        }
        boxes[part_pos] = box;
        sorted_parts[part_pos] = part_pos;
    }
    std::sort(
            sorted_parts.begin(),
            sorted_parts.end(),
            [&boxes](Counter part_pos_1, Counter part_pos_2)
            {
                return boxes[part_pos_1].x_min < boxes[part_pos_2].x_min;
            });

    LengthDbl separation = infinity;
    for (Counter sorted_pos_1 = 0; sorted_pos_1 < (Counter)sorted_parts.size(); ++sorted_pos_1) {
        Counter part_pos_1 = sorted_parts[sorted_pos_1];
        for (Counter sorted_pos_2 = sorted_pos_1 + 1;
                sorted_pos_2 < (Counter)sorted_parts.size()
                && boxes[sorted_parts[sorted_pos_2]].x_min <= boxes[part_pos_1].x_max;
                ++sorted_pos_2) {
            Counter part_pos_2 = sorted_parts[sorted_pos_2];
            const Part& part_1 = parts_[part_pos_1];
            const Part& part_2 = parts_[part_pos_2];
            if (part_1.item_pos == -1 && part_2.item_pos == -1)
                continue;
            if (part_1.item_pos == part_2.item_pos)
                continue;
            if (boxes[part_pos_2].y_min > boxes[part_pos_1].y_max
                    || boxes[part_pos_1].y_min > boxes[part_pos_2].y_max) {
                continue;
            }

            LengthDbl best_value = -infinity;
            constraints.push_back(best_constraint(
                        items,
                        part_pos_1,
                        part_pos_2,
                        fixed_scales,
                        best_value));
            separation = (std::min)(separation, best_value);
        }
    }

    // Containment in the bounding box of the bin.
    const BinType& bin_type = instance_.bin_type(bin_type_id_);
    for (Counter part_pos = 0; part_pos < (Counter)parts_.size(); ++part_pos) {
        if (parts_[part_pos].item_pos == -1)
            continue;
        AxisAlignedBoundingBox aabb = part_aabb(items, part_pos);
        separation = (std::min)(separation,
                aabb.x_min - (bin_type.aabb_scaled.x_min + bin_spacing_));
        separation = (std::min)(separation,
                bin_type.aabb_scaled.x_max - bin_spacing_ - aabb.x_max);
        separation = (std::min)(separation,
                aabb.y_min - (bin_type.aabb_scaled.y_min + bin_spacing_));
        separation = (std::min)(separation,
                bin_type.aabb_scaled.y_max - bin_spacing_ - aabb.y_max);
    }
    return separation;
}

LengthDbl BinSeparationModel::separation(
        const std::vector<SeparationModelItem>& items) const
{
    std::vector<LengthDbl> movements(items.size(), 0.0);
    std::vector<Constraint> constraints;
    return compute_constraints(items, movements, false, constraints);
}

bool BinSeparationModel::step(
        std::vector<SeparationModelItem>& items,
        const StepParameters& parameters,
        const optimizationtools::Timer& timer) const
{
    const BinType& bin_type = instance_.bin_type(bin_type_id_);
    AxisAlignedBoundingBox container;
    container.x_min = bin_type.aabb_scaled.x_min + bin_spacing_;
    container.x_max = bin_type.aabb_scaled.x_max - bin_spacing_;
    container.y_min = bin_type.aabb_scaled.y_min + bin_spacing_;
    container.y_max = bin_type.aabb_scaled.y_max - bin_spacing_;
    return solve_step(items, parameters, false, 0.0, 0.0, container, timer);
}

bool BinSeparationModel::anchor_step(
        std::vector<SeparationModelItem>& items,
        double x_weight,
        double y_weight,
        const AxisAlignedBoundingBox& container,
        const StepParameters& parameters,
        const optimizationtools::Timer& timer) const
{
    return solve_step(items, parameters, true, x_weight, y_weight, container, timer);
}

bool BinSeparationModel::solve_step(
        std::vector<SeparationModelItem>& items,
        const StepParameters& parameters,
        bool fixed_scales,
        double x_weight,
        double y_weight,
        const AxisAlignedBoundingBox& container,
        const optimizationtools::Timer& timer) const
{
    ItemPos number_of_items = items.size();

    // Maximum move of each item.
    std::vector<LengthDbl> movements(number_of_items, 0.0);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        if (items[item_pos].is_fixed())
            continue;
        const AxisAlignedBoundingBox& aabb = items_aabbs_[item_pos];
        movements[item_pos] = parameters.movement_ratio
            * (std::max)(aabb.x_max - aabb.x_min, aabb.y_max - aabb.y_min);
    }
    std::vector<Constraint> constraints;
    compute_constraints(items, movements, fixed_scales, constraints);

    // Variables, and their values in the current state.
    mathoptsolverscmake::MathOptModel model;
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
    std::vector<double> current_values;
    auto add_variable = [&model, &current_values](
            double lower_bound,
            double upper_bound,
            double objective_coefficient,
            double current_value)
    {
        int variable_id = model.variables_lower_bounds.size();
        model.variables_lower_bounds.push_back(lower_bound);
        model.variables_upper_bounds.push_back(upper_bound);
        model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
        model.objective_coefficients.push_back(objective_coefficient);
        current_values.push_back(current_value);
        return variable_id;
    };
    std::vector<int> x(number_of_items, -1);
    std::vector<int> y(number_of_items, -1);
    std::vector<int> lambda(number_of_items, -1);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        const SeparationModelItem& item = items[item_pos];
        if (item.is_fixed())
            continue;
        if (fixed_scales) {
            // The containment of the parts is enforced by the constraints.
            x[item_pos] = add_variable(
                    item.position.x - movements[item_pos],
                    item.position.x + movements[item_pos],
                    -x_weight,
                    item.position.x);
            y[item_pos] = add_variable(
                    item.position.y - movements[item_pos],
                    item.position.y + movements[item_pos],
                    -y_weight,
                    item.position.y);
            continue;
        }
        // The centers of the parts stay in the bin, so that null scales are
        // always feasible.
        const AxisAlignedBoundingBox& centers_aabb = items_centers_aabbs_[item_pos];
        x[item_pos] = add_variable(
                (std::min)(item.position.x, (std::max)(
                        item.position.x - movements[item_pos],
                        container.x_min - centers_aabb.x_min)),
                (std::max)(item.position.x, (std::min)(
                        item.position.x + movements[item_pos],
                        container.x_max - centers_aabb.x_max)),
                0.0,
                item.position.x);
        y[item_pos] = add_variable(
                (std::min)(item.position.y, (std::max)(
                        item.position.y - movements[item_pos],
                        container.y_min - centers_aabb.y_min)),
                (std::max)(item.position.y, (std::min)(
                        item.position.y + movements[item_pos],
                        container.y_max - centers_aabb.y_max)),
                0.0,
                item.position.y);
        lambda[item_pos] = add_variable(0.0, 1.0, item.weight, item.lambda);
    }

    // Terms of an item in a row: position and scale; the terms of an
    // obstacle, and the scale terms when the scales are fixed, are constant
    // and go to the lower bound.
    struct Row
    {
        std::vector<int> variables;
        std::vector<double> coefficients;
        double lower_bound = 0.0;
    };
    auto add_scale_term = [&items, &lambda](
            Row& row,
            ItemPos item_pos,
            double lambda_coefficient)
    {
        if (lambda[item_pos] == -1) {
            row.lower_bound -= lambda_coefficient * items[item_pos].lambda;
            return;
        }
        row.variables.push_back(lambda[item_pos]);
        row.coefficients.push_back(lambda_coefficient);
    };
    auto add_item_terms = [&x, &y, &add_scale_term](
            Row& row,
            ItemPos item_pos,
            const Point& normal,
            double constant,
            double lambda_coefficient,
            double sign)
    {
        if (item_pos == -1) {
            row.lower_bound -= sign * (constant + lambda_coefficient);
            return;
        }
        row.lower_bound -= sign * constant;
        row.variables.push_back(x[item_pos]);
        row.coefficients.push_back(sign * normal.x);
        row.variables.push_back(y[item_pos]);
        row.coefficients.push_back(sign * normal.y);
        add_scale_term(row, item_pos, sign * lambda_coefficient);
    };
    auto add_row = [&model, &current_values, fixed_scales](const Row& row)
    {
        double lower_bound = row.lower_bound;
        if (fixed_scales) {
            // Keep the current state feasible.
            double current_value = 0.0;
            for (Counter pos = 0; pos < (Counter)row.variables.size(); ++pos)
                current_value += row.coefficients[pos] * current_values[row.variables[pos]];
            lower_bound = (std::min)(lower_bound, current_value);
        }
        model.constraints_starts.push_back(model.elements_variables.size());
        model.constraints_lower_bounds.push_back(lower_bound);
        model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
        for (Counter pos = 0; pos < (Counter)row.variables.size(); ++pos) {
            model.elements_variables.push_back(row.variables[pos]);
            model.elements_coefficients.push_back(row.coefficients[pos]);
        }
    };

    // normal . (p_support - p_edge)
    // + support_constant + support_coefficient * lambda_support
    // - edge_constant - edge_coefficient * lambda_edge >= spacing.
    for (const Constraint& constraint: constraints) {
        Row row;
        row.lower_bound = constraint.spacing;
        add_item_terms(
                row,
                parts_[constraint.support_part_pos].item_pos,
                constraint.normal,
                constraint.support_constant,
                constraint.support_coefficient,
                1.0);
        add_item_terms(
                row,
                parts_[constraint.edge_part_pos].item_pos,
                constraint.normal,
                constraint.edge_constant,
                constraint.edge_coefficient,
                -1.0);
        add_row(row);
    }

    // Containment of each part:
    // x + center_x + lambda * (x_min - center_x) >= container_x_min, etc.
    for (Counter part_pos = 0; part_pos < (Counter)parts_.size(); ++part_pos) {
        const Part& part = parts_[part_pos];
        if (part.item_pos == -1)
            continue;
        ItemPos item_pos = part.item_pos;
        const int position_variables[4] = {x[item_pos], x[item_pos], y[item_pos], y[item_pos]};
        const double signs[4] = {1.0, -1.0, 1.0, -1.0};
        const double centers[4] = {part.center.x, part.center.x, part.center.y, part.center.y};
        const double extents[4] = {part.aabb.x_min, part.aabb.x_max, part.aabb.y_min, part.aabb.y_max};
        const double limits[4] = {
            container.x_min,
            -container.x_max,
            container.y_min,
            -container.y_max};
        for (int side = 0; side < 4; ++side) {
            Row row;
            row.lower_bound = limits[side] - signs[side] * centers[side];
            row.variables.push_back(position_variables[side]);
            row.coefficients.push_back(signs[side]);
            add_scale_term(row, item_pos, signs[side] * (extents[side] - centers[side]));
            add_row(row);
        }
    }
    model.feasibility_tolerance = 1e-9;

    // Solve. No initial solution is given to the solver (see
    // https://github.com/ERGO-Code/HiGHS/issues/3454).
    std::vector<double> solution;
#ifdef HIGHS_FOUND
    Highs highs;
    mathoptsolverscmake::reduce_printout(highs);
    mathoptsolverscmake::set_time_limit(highs, timer.remaining_time());
    highs.setOptionValue("parallel", "off");
    mathoptsolverscmake::load(highs, model);
    mathoptsolverscmake::solve(highs);
    if (highs.getModelStatus() != HighsModelStatus::kOptimal)
        return false;
    solution = mathoptsolverscmake::get_solution(highs);
#else
    (void)timer;
    throw std::invalid_argument(FUNC_SIGNATURE + ": HiGHS not available.");
#endif

    // New positions and scales, clamped to their bounds.
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        SeparationModelItem& item = items[item_pos];
        if (item.is_fixed())
            continue;
        item.position.x = (std::max)(item.position.x - movements[item_pos],
                (std::min)(item.position.x + movements[item_pos], solution[x[item_pos]]));
        item.position.y = (std::max)(item.position.y - movements[item_pos],
                (std::min)(item.position.y + movements[item_pos], solution[y[item_pos]]));
        if (lambda[item_pos] != -1)
            item.lambda = (std::max)(0.0, (std::min)(1.0, solution[lambda[item_pos]]));
    }
    return true;
}
