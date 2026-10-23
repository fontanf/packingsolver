#include "irregular/nonlinear_programming/separation_model.hpp"

#include "irregular/nonlinear_programming/separating_line.hpp"
#include "irregular/utils.hpp"

#include "mathoptsolverscmake/mathopt_nonlinear.hpp"
#ifdef KNITRO_FOUND
#include "mathoptsolverscmake/mathopt_knitro.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <limits>

using namespace packingsolver;
using namespace packingsolver::irregular;
using namespace mathoptsolverscmake;

namespace
{

/** Convex hull of a set of points, anticlockwise (monotone chain). */
std::vector<Point> convex_hull_of_points(std::vector<Point> points)
{
    std::sort(
            points.begin(),
            points.end(),
            [](const Point& point_1, const Point& point_2)
            {
                return point_1.x < point_2.x
                    || (point_1.x == point_2.x && point_1.y < point_2.y);
            });
    if (points.size() < 3)
        return points;
    std::vector<Point> hull(2 * points.size());
    ElementPos hull_size = 0;
    for (int pass = 0; pass < 2; ++pass) {
        ElementPos start_size = hull_size;
        for (ElementPos point_pos = 0; point_pos < (ElementPos)points.size(); ++point_pos) {
            const Point& point = (pass == 0)?
                points[point_pos]:
                points[points.size() - 1 - point_pos];
            while (hull_size >= start_size + 2
                    && cross_product(
                        hull[hull_size - 1] - hull[hull_size - 2],
                        point - hull[hull_size - 2]) <= 0) {
                hull_size--;
            }
            hull[hull_size++] = point;
        }
        hull_size--;
    }
    hull.resize(hull_size);
    return hull;
}

/**
 * Projection of a vertex of an item on the normal of a separating line:
 * cos(alpha) * x + sin(alpha) * y
 * + lambda * (vertex_x * cos(alpha - theta) + vertex_y * sin(alpha - theta)).
 */
NonlinearExpression item_vertex_projection(
        int alpha,
        int x_variable,
        int y_variable,
        int theta,
        int lambda,
        const Point& vertex)
{
    NonlinearExpression angle = nonlinear_subtract(
            nonlinear_variable(alpha),
            nonlinear_variable(theta));
    return nonlinear_sum({
            nonlinear_product({nonlinear_cos(nonlinear_variable(alpha)), nonlinear_variable(x_variable)}),
            nonlinear_product({nonlinear_sin(nonlinear_variable(alpha)), nonlinear_variable(y_variable)}),
            nonlinear_product({
                nonlinear_variable(lambda),
                nonlinear_sum({
                    nonlinear_product({nonlinear_constant(vertex.x), nonlinear_cos(angle)}),
                    nonlinear_product({nonlinear_constant(vertex.y), nonlinear_sin(angle)})})})});
}

/** Projection of a fixed vertex on the normal of a separating line. */
NonlinearExpression fixed_vertex_projection(
        int alpha,
        const Point& vertex)
{
    return nonlinear_sum({
            nonlinear_product({nonlinear_constant(vertex.x), nonlinear_cos(nonlinear_variable(alpha))}),
            nonlinear_product({nonlinear_constant(vertex.y), nonlinear_sin(nonlinear_variable(alpha))})});
}

}

BinNonlinearSeparationModel::BinNonlinearSeparationModel(
        const Instance& instance,
        const InstanceBasicShapeDecomposition& decomposition,
        BinTypeId bin_type_id,
        const std::vector<NonlinearSeparationModelItem>& items):
    instance_(instance),
    bin_type_id_(bin_type_id)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const LengthDbl scale_value = instance.parameters().scale_value;
    item_spacing_ = instance.item_spacing_scaled();
    bin_spacing_ = instance.bin_spacing_scaled(bin_type_id);
    maximum_spacing_ = (std::max)(item_spacing_, bin_spacing_);

    auto add_part = [this](
            ItemPos item_pos,
            const Shape& shape,
            const Point& shift,
            LengthDbl spacing)
    {
        Part part;
        part.item_pos = item_pos;
        part.spacing = spacing;
        for (const ShapeElement& element: shape.elements)
            part.vertices.push_back({element.start.x + shift.x, element.start.y + shift.y});
        parts_.push_back(part);
    };

    const Point origin = {0.0, 0.0};
    items_hulls_.resize(items.size());
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items.size(); ++item_pos) {
        const NonlinearSeparationModelItem& item = items[item_pos];
        const ItemType& item_type = instance.item_type(item.item_type_id);
        std::vector<Point> item_vertices;
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            for (const BasicShape& basic_shape: decomposition.item_types[item.item_type_id][item_shape_pos]) {
                if (basic_shape.type != BasicShapeType::ConvexPolygon
                        || !basic_shape.shape.is_polygon()) {
                    supported_ = false;
                    return;
                }
                BasicShape placed_shape = apply_placement(basic_shape, item.mirror, item.angle);
                if (item.is_fixed) {
                    add_part(-1, placed_shape.shape, item.position, item_spacing_);
                } else {
                    add_part(item_pos, placed_shape.shape, origin, 0.0);
                    for (const ShapeElement& element: placed_shape.shape.elements)
                        item_vertices.push_back(element.start);
                }
            }
        }
        if (!item.is_fixed)
            items_hulls_[item_pos] = convex_hull_of_points(item_vertices);
    }
    for (const BasicShape& basic_shape: decomposition.bin_types_borders[bin_type_id]) {
        if (!basic_shape.shape.is_polygon()) {
            supported_ = false;
            return;
        }
        add_part(-1, basic_shape.shape, origin, bin_spacing_);
    }
    LengthDbl defect_spacing = 0.0;
    for (const auto& defect: bin_type.defects)
        defect_spacing = (std::max)(defect_spacing, scale_value * defect.item_defect_minimum_spacing);
    maximum_spacing_ = (std::max)(maximum_spacing_, defect_spacing);
    for (const BasicShape& basic_shape: decomposition.bin_types_defects[bin_type_id]) {
        if (!basic_shape.shape.is_polygon()) {
            supported_ = false;
            return;
        }
        add_part(-1, basic_shape.shape, origin, defect_spacing);
    }
}

std::vector<Point> BinNonlinearSeparationModel::world_vertices(
        const std::vector<NonlinearSeparationModelItem>& items,
        Counter part_pos) const
{
    const Part& part = parts_[part_pos];
    if (part.item_pos == -1)
        return part.vertices;
    const NonlinearSeparationModelItem& item = items[part.item_pos];
    std::vector<Point> vertices;
    for (const Point& vertex: part.vertices) {
        vertices.push_back({
                item.position.x + item.lambda * vertex.x,
                item.position.y + item.lambda * vertex.y});
    }
    return vertices;
}

bool BinNonlinearSeparationModel::step(
        std::vector<NonlinearSeparationModelItem>& items,
        const StepParameters& parameters,
        const optimizationtools::Timer& timer) const
{
    const LengthDbl infinity = std::numeric_limits<LengthDbl>::infinity();
    const BinType& bin_type = instance_.bin_type(bin_type_id_);
    ItemPos number_of_items = items.size();

    // Maximum move of each item, and maximum move of its vertices (with the
    // rotation).
    std::vector<LengthDbl> movements(number_of_items, 0.0);
    std::vector<LengthDbl> reaches(number_of_items, 0.0);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        const NonlinearSeparationModelItem& item = items[item_pos];
        if (item.is_fixed)
            continue;
        AxisAlignedBoundingBox aabb;
        LengthDbl radius = 0.0;
        for (const Point& vertex: items_hulls_[item_pos]) {
            aabb.x_min = (std::min)(aabb.x_min, vertex.x);
            aabb.x_max = (std::max)(aabb.x_max, vertex.x);
            aabb.y_min = (std::min)(aabb.y_min, vertex.y);
            aabb.y_max = (std::max)(aabb.y_max, vertex.y);
            radius = (std::max)(radius, std::sqrt(vertex.x * vertex.x + vertex.y * vertex.y));
        }
        movements[item_pos] = parameters.movement_ratio
            * (std::max)(aabb.x_max - aabb.x_min, aabb.y_max - aabb.y_min);
        Angle rotation = (std::max)(-item.minimum_rotation, item.maximum_rotation) * M_PI / 180;
        reaches[item_pos] = movements[item_pos] + rotation * radius;
    }

    // Boxes of the parts: for the parts of the items, the boxes containing
    // them at any scale, expanded by the maximum moves of their vertices and
    // the spacing.
    std::vector<AxisAlignedBoundingBox> boxes(parts_.size());
    for (Counter part_pos = 0; part_pos < (Counter)parts_.size(); ++part_pos) {
        const Part& part = parts_[part_pos];
        AxisAlignedBoundingBox box;
        if (part.item_pos == -1) {
            for (const Point& vertex: part.vertices) {
                box.x_min = (std::min)(box.x_min, vertex.x);
                box.x_max = (std::max)(box.x_max, vertex.x);
                box.y_min = (std::min)(box.y_min, vertex.y);
                box.y_max = (std::max)(box.y_max, vertex.y);
            }
        } else {
            const NonlinearSeparationModelItem& item = items[part.item_pos];
            box.x_min = box.x_max = item.position.x;
            box.y_min = box.y_max = item.position.y;
            for (const Point& vertex: part.vertices) {
                box.x_min = (std::min)(box.x_min, item.position.x + vertex.x);
                box.x_max = (std::max)(box.x_max, item.position.x + vertex.x);
                box.y_min = (std::min)(box.y_min, item.position.y + vertex.y);
                box.y_max = (std::max)(box.y_max, item.position.y + vertex.y);
            }
            LengthDbl expansion = reaches[part.item_pos] + maximum_spacing_;
            box.x_min -= expansion;
            box.x_max += expansion;
            box.y_min -= expansion;
            box.y_max += expansion;
        }
        boxes[part_pos] = box;
    }

    // Variables.
    MathOptModel model;
    model.objective_direction = ObjectiveDirection::Maximize;
    auto add_variable = [&model](
            double lower_bound,
            double upper_bound,
            double objective_coefficient,
            double initial_value)
    {
        int variable_id = model.variables_lower_bounds.size();
        model.variables_lower_bounds.push_back(lower_bound);
        model.variables_upper_bounds.push_back(upper_bound);
        model.variables_types.push_back(VariableType::Continuous);
        model.objective_coefficients.push_back(objective_coefficient);
        model.variables_initial_values.push_back(initial_value);
        return variable_id;
    };
    std::vector<int> x_variables(number_of_items, -1);
    std::vector<int> y_variables(number_of_items, -1);
    std::vector<int> theta_variables(number_of_items, -1);
    std::vector<int> lambda_variables(number_of_items, -1);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        const NonlinearSeparationModelItem& item = items[item_pos];
        if (item.is_fixed)
            continue;
        // The reference point stays in the bin, so that a null scale is
        // always feasible.
        LengthDbl x_lower_bound = (std::min)(item.position.x, (std::max)(
                    item.position.x - movements[item_pos],
                    bin_type.aabb_scaled.x_min + bin_spacing_));
        LengthDbl x_upper_bound = (std::max)(item.position.x, (std::min)(
                    item.position.x + movements[item_pos],
                    bin_type.aabb_scaled.x_max - bin_spacing_));
        LengthDbl y_lower_bound = (std::min)(item.position.y, (std::max)(
                    item.position.y - movements[item_pos],
                    bin_type.aabb_scaled.y_min + bin_spacing_));
        LengthDbl y_upper_bound = (std::max)(item.position.y, (std::min)(
                    item.position.y + movements[item_pos],
                    bin_type.aabb_scaled.y_max - bin_spacing_));
        x_variables[item_pos] = add_variable(x_lower_bound, x_upper_bound, 0.0, item.position.x);
        y_variables[item_pos] = add_variable(y_lower_bound, y_upper_bound, 0.0, item.position.y);
        theta_variables[item_pos] = add_variable(
                item.minimum_rotation * M_PI / 180,
                item.maximum_rotation * M_PI / 180,
                0.0,
                0.0);
        lambda_variables[item_pos] = add_variable(0.0, 1.0, item.weight, item.lambda);
    }

    // Rows: a linear part and a nonlinear term, lower bounded.
    auto add_row = [&model](
            const std::vector<int>& variables,
            const std::vector<double>& coefficients,
            const NonlinearExpression& expression,
            double lower_bound)
    {
        int constraint_id = model.constraints_lower_bounds.size();
        model.constraints_starts.push_back(model.elements_variables.size());
        model.constraints_lower_bounds.push_back(lower_bound);
        model.constraints_upper_bounds.push_back(std::numeric_limits<double>::infinity());
        for (Counter pos = 0; pos < (Counter)variables.size(); ++pos) {
            model.elements_variables.push_back(variables[pos]);
            model.elements_coefficients.push_back(coefficients[pos]);
        }
        add_constraint_nonlinear_expression(model, constraint_id, expression);
    };

    // Separating lines of the pairs of parts whose boxes intersect:
    // c - normal . vertex_1 >= 0;
    // normal . vertex_2 - c >= spacing.
    for (Counter part_pos_1 = 0; part_pos_1 < (Counter)parts_.size(); ++part_pos_1) {
        for (Counter part_pos_2 = part_pos_1 + 1; part_pos_2 < (Counter)parts_.size(); ++part_pos_2) {
            const Part& part_1 = parts_[part_pos_1];
            const Part& part_2 = parts_[part_pos_2];
            if (part_1.item_pos == -1 && part_2.item_pos == -1)
                continue;
            if (part_1.item_pos == part_2.item_pos)
                continue;
            const AxisAlignedBoundingBox& box_1 = boxes[part_pos_1];
            const AxisAlignedBoundingBox& box_2 = boxes[part_pos_2];
            if (box_1.x_min > box_2.x_max || box_2.x_min > box_1.x_max
                    || box_1.y_min > box_2.y_max || box_2.y_min > box_1.y_max) {
                continue;
            }
            LengthDbl spacing = (part_1.item_pos == -1)? part_1.spacing:
                (part_2.item_pos == -1)? part_2.spacing: item_spacing_;

            // Initial separating line, with the spacing split on both sides.
            SeparatingLine separating_line = compute_separating_line(
                    world_vertices(items, part_pos_1),
                    world_vertices(items, part_pos_2));
            double initial_alpha = std::atan2(separating_line.normal.y, separating_line.normal.x);
            int alpha = add_variable(initial_alpha - M_PI, initial_alpha + M_PI, 0.0, initial_alpha);
            int offset = add_variable(-infinity, infinity, 0.0, separating_line.offset - spacing / 2);

            for (int side = 0; side < 2; ++side) {
                const Part& part = (side == 0)? part_1: part_2;
                double sign = (side == 0)? -1.0: 1.0;
                for (const Point& vertex: part.vertices) {
                    NonlinearExpression projection = (part.item_pos == -1)?
                        fixed_vertex_projection(alpha, vertex):
                        item_vertex_projection(
                                alpha,
                                x_variables[part.item_pos],
                                y_variables[part.item_pos],
                                theta_variables[part.item_pos],
                                lambda_variables[part.item_pos],
                                vertex);
                    if (side == 0)
                        projection = nonlinear_negate(projection);
                    add_row({offset}, {-sign}, projection, (side == 0)? 0.0: spacing);
                }
            }
        }
    }

    // Containment: the vertices of the convex hull of each item which may
    // reach the borders of the bounding box of the bin; their coordinates
    // are x + lambda * (v_x * cos(theta) - v_y * sin(theta)) and
    // y + lambda * (v_x * sin(theta) + v_y * cos(theta)).
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        const NonlinearSeparationModelItem& item = items[item_pos];
        if (item.is_fixed)
            continue;
        LengthDbl reach = reaches[item_pos] + maximum_spacing_;
        for (const Point& vertex: items_hulls_[item_pos]) {
            NonlinearExpression cos_theta = nonlinear_cos(nonlinear_variable(theta_variables[item_pos]));
            NonlinearExpression sin_theta = nonlinear_sin(nonlinear_variable(theta_variables[item_pos]));
            NonlinearExpression offset_x = nonlinear_product({
                    nonlinear_variable(lambda_variables[item_pos]),
                    nonlinear_subtract(
                        nonlinear_product({nonlinear_constant(vertex.x), cos_theta}),
                        nonlinear_product({nonlinear_constant(vertex.y), sin_theta}))});
            NonlinearExpression offset_y = nonlinear_product({
                    nonlinear_variable(lambda_variables[item_pos]),
                    nonlinear_sum({
                        nonlinear_product({nonlinear_constant(vertex.x), sin_theta}),
                        nonlinear_product({nonlinear_constant(vertex.y), cos_theta})})});
            LengthDbl lower_x = bin_type.aabb_scaled.x_min + bin_spacing_;
            LengthDbl upper_x = bin_type.aabb_scaled.x_max - bin_spacing_;
            LengthDbl lower_y = bin_type.aabb_scaled.y_min + bin_spacing_;
            LengthDbl upper_y = bin_type.aabb_scaled.y_max - bin_spacing_;
            if ((std::min)(item.position.x, item.position.x + vertex.x) - reach <= lower_x)
                add_row({x_variables[item_pos]}, {1.0}, offset_x, lower_x);
            if ((std::max)(item.position.x, item.position.x + vertex.x) + reach >= upper_x)
                add_row({x_variables[item_pos]}, {-1.0}, nonlinear_negate(offset_x), -upper_x);
            if ((std::min)(item.position.y, item.position.y + vertex.y) - reach <= lower_y)
                add_row({y_variables[item_pos]}, {1.0}, offset_y, lower_y);
            if ((std::max)(item.position.y, item.position.y + vertex.y) + reach >= upper_y)
                add_row({y_variables[item_pos]}, {-1.0}, nonlinear_negate(offset_y), -upper_y);
        }
    }

    // Solve.
    std::vector<double> solution;
#ifdef KNITRO_FOUND
    KnitroContext knitro;
    reduce_printout(knitro);
    set_time_limit(knitro, timer.remaining_time());
    load(knitro, model);
    KN_set_int_param(knitro.kc, KN_PARAM_NUMTHREADS, 1);
    // Tight feasibility tolerances, so that the solutions pass the exact
    // overlap checks.
    KN_set_double_param(knitro.kc, KN_PARAM_FEASTOL, 1e-10);
    KN_set_double_param(knitro.kc, KN_PARAM_FEASTOLABS, 1e-10);
    solve(knitro);
    if (is_infeasible(knitro))
        return false;
    solution = get_solution(knitro);
#else
    (void)timer;
    throw std::invalid_argument(FUNC_SIGNATURE + ": Knitro not available.");
#endif

    // New positions, angles and scales, clamped to their bounds.
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        NonlinearSeparationModelItem& item = items[item_pos];
        if (item.is_fixed)
            continue;
        item.position.x = (std::max)(model.variables_lower_bounds[x_variables[item_pos]],
                (std::min)(model.variables_upper_bounds[x_variables[item_pos]], solution[x_variables[item_pos]]));
        item.position.y = (std::max)(model.variables_lower_bounds[y_variables[item_pos]],
                (std::min)(model.variables_upper_bounds[y_variables[item_pos]], solution[y_variables[item_pos]]));
        Angle rotation = (std::max)(model.variables_lower_bounds[theta_variables[item_pos]],
                (std::min)(model.variables_upper_bounds[theta_variables[item_pos]], solution[theta_variables[item_pos]]));
        item.angle += rotation * 180 / M_PI;
        item.lambda = (std::max)(0.0, (std::min)(1.0, solution[lambda_variables[item_pos]]));
    }
    return true;
}
