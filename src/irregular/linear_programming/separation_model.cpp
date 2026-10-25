#include "irregular/linear_programming/separation_model.hpp"

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
            Point vertex = {element.start.x + shift.x, element.start.y + shift.y};
            part.vertices.push_back(vertex);
            part.aabb.x_min = (std::min)(part.aabb.x_min, vertex.x);
            part.aabb.x_max = (std::max)(part.aabb.x_max, vertex.x);
            part.aabb.y_min = (std::min)(part.aabb.y_min, vertex.y);
            part.aabb.y_max = (std::max)(part.aabb.y_max, vertex.y);
        }
        if (item_pos != -1) {
            for (const Point& vertex: part.vertices) {
                part.center.x += vertex.x / part.vertices.size();
                part.center.y += vertex.y / part.vertices.size();
            }
        }
        parts_.push_back(part);
        return part.aabb;
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
                if (basic_shape.type != BasicShapeType::ConvexPolygon
                        || !basic_shape.shape.is_polygon()) {
                    supported_ = false;
                    return;
                }
                BasicShape placed_shape = apply_placement(basic_shape, item.mirror, item.angle);
                if (item.is_fixed) {
                    add_part(-1, placed_shape.shape, item.position, item_spacing_);
                } else {
                    items_aabbs_[item_pos] = merge(
                            items_aabbs_[item_pos],
                            add_part(item_pos, placed_shape.shape, origin, 0.0));
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
        if (!basic_shape.shape.is_polygon()) {
            supported_ = false;
            return;
        }
        add_part(-1, basic_shape.shape, origin, defect_spacing);
    }
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
        LengthDbl& best_value) const
{
    const LengthDbl infinity = std::numeric_limits<LengthDbl>::infinity();
    const Part& part_1 = parts_[part_pos_1];
    const Part& part_2 = parts_[part_pos_2];

    // Best separating edge, among the edges of both parts whose constraints
    // are satisfied with null scales, and among all the edges of both parts.
    Constraint best_constraint;
    best_value = -infinity;
    Constraint best_feasible_constraint;
    LengthDbl best_feasible_value = -infinity;
    bool feasible_constraint_found = false;
    for (int side = 0; side < 2; ++side) {
        Counter edge_part_pos = (side == 0)? part_pos_1: part_pos_2;
        Counter support_part_pos = (side == 0)? part_pos_2: part_pos_1;
        const Part& edge_part = parts_[edge_part_pos];
        const Part& support_part = parts_[support_part_pos];
        ElementPos number_of_vertices = edge_part.vertices.size();
        for (ElementPos vertex_pos = 0; vertex_pos < number_of_vertices; ++vertex_pos) {
            const Point& edge_start = edge_part.vertices[vertex_pos];
            const Point& edge_end = edge_part.vertices[(vertex_pos + 1) % number_of_vertices];
            LengthDbl delta_x = edge_end.x - edge_start.x;
            LengthDbl delta_y = edge_end.y - edge_start.y;
            LengthDbl length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
            if (length == 0.0)
                continue;
            Constraint constraint;
            constraint.edge_part_pos = edge_part_pos;
            constraint.support_part_pos = support_part_pos;
            // The parts are anticlockwise: the outward normal is on the right
            // of the edge.
            constraint.normal = {delta_y / length, -delta_x / length};
            constraint.edge_constant = constraint.normal.x * edge_part.center.x
                + constraint.normal.y * edge_part.center.y;
            constraint.edge_coefficient = constraint.normal.x * edge_start.x
                + constraint.normal.y * edge_start.y
                - constraint.edge_constant;
            constraint.support_constant = constraint.normal.x * support_part.center.x
                + constraint.normal.y * support_part.center.y;
            constraint.support_coefficient = infinity;
            for (const Point& vertex: support_part.vertices) {
                constraint.support_coefficient = (std::min)(
                        constraint.support_coefficient,
                        constraint.normal.x * vertex.x + constraint.normal.y * vertex.y);
            }
            constraint.support_coefficient -= constraint.support_constant;
            constraint.spacing = (part_1.item_pos == -1)? part_1.spacing:
                (part_2.item_pos == -1)? part_2.spacing: item_spacing_;
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
        }
    }
    return (feasible_constraint_found)? best_feasible_constraint: best_constraint;
}

LengthDbl BinSeparationModel::compute_constraints(
        const std::vector<SeparationModelItem>& items,
        const std::vector<LengthDbl>& movements,
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
            constraints.push_back(best_constraint(items, part_pos_1, part_pos_2, best_value));
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
    return compute_constraints(items, movements, constraints);
}

bool BinSeparationModel::step(
        std::vector<SeparationModelItem>& items,
        const StepParameters& parameters,
        const optimizationtools::Timer& timer) const
{
    const BinType& bin_type = instance_.bin_type(bin_type_id_);
    ItemPos number_of_items = items.size();

    // Maximum move of each item.
    std::vector<LengthDbl> movements(number_of_items, 0.0);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        if (items[item_pos].is_fixed)
            continue;
        const AxisAlignedBoundingBox& aabb = items_aabbs_[item_pos];
        movements[item_pos] = parameters.movement_ratio
            * (std::max)(aabb.x_max - aabb.x_min, aabb.y_max - aabb.y_min);
    }
    std::vector<Constraint> constraints;
    compute_constraints(items, movements, constraints);

    // Variables.
    mathoptsolverscmake::MathOptModel model;
    model.objective_direction = mathoptsolverscmake::ObjectiveDirection::Maximize;
    auto add_variable = [&model](double lower_bound, double upper_bound, double objective_coefficient)
    {
        int variable_id = model.variables_lower_bounds.size();
        model.variables_lower_bounds.push_back(lower_bound);
        model.variables_upper_bounds.push_back(upper_bound);
        model.variables_types.push_back(mathoptsolverscmake::VariableType::Continuous);
        model.objective_coefficients.push_back(objective_coefficient);
        return variable_id;
    };
    std::vector<int> x(number_of_items, -1);
    std::vector<int> y(number_of_items, -1);
    std::vector<int> lambda(number_of_items, -1);
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        const SeparationModelItem& item = items[item_pos];
        if (item.is_fixed)
            continue;
        // The centers of the parts stay in the bin, so that null scales are
        // always feasible.
        const AxisAlignedBoundingBox& centers_aabb = items_centers_aabbs_[item_pos];
        x[item_pos] = add_variable(
                (std::min)(item.position.x, (std::max)(
                        item.position.x - movements[item_pos],
                        bin_type.aabb_scaled.x_min + bin_spacing_ - centers_aabb.x_min)),
                (std::max)(item.position.x, (std::min)(
                        item.position.x + movements[item_pos],
                        bin_type.aabb_scaled.x_max - bin_spacing_ - centers_aabb.x_max)),
                0.0);
        y[item_pos] = add_variable(
                (std::min)(item.position.y, (std::max)(
                        item.position.y - movements[item_pos],
                        bin_type.aabb_scaled.y_min + bin_spacing_ - centers_aabb.y_min)),
                (std::max)(item.position.y, (std::min)(
                        item.position.y + movements[item_pos],
                        bin_type.aabb_scaled.y_max - bin_spacing_ - centers_aabb.y_max)),
                0.0);
        lambda[item_pos] = add_variable(0.0, 1.0, item.weight);
    }

    // Terms of an item in a row: position and scale; the terms of an
    // obstacle are constant and go to the lower bound.
    struct Row
    {
        std::vector<int> variables;
        std::vector<double> coefficients;
        double lower_bound = 0.0;
    };
    auto add_item_terms = [&x, &y, &lambda](
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
        row.variables.push_back(lambda[item_pos]);
        row.coefficients.push_back(sign * lambda_coefficient);
    };
    auto add_row = [&model](const Row& row)
    {
        model.constraints_starts.push_back(model.elements_variables.size());
        model.constraints_lower_bounds.push_back(row.lower_bound);
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
    // x + center_x + lambda * (x_min - center_x) >= bin_x_min + spacing, etc.
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
            bin_type.aabb_scaled.x_min + bin_spacing_,
            -(bin_type.aabb_scaled.x_max - bin_spacing_),
            bin_type.aabb_scaled.y_min + bin_spacing_,
            -(bin_type.aabb_scaled.y_max - bin_spacing_)};
        for (int side = 0; side < 4; ++side) {
            Row row;
            row.lower_bound = limits[side] - signs[side] * centers[side];
            row.variables.push_back(position_variables[side]);
            row.coefficients.push_back(signs[side]);
            row.variables.push_back(lambda[item_pos]);
            row.coefficients.push_back(signs[side] * (extents[side] - centers[side]));
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
        if (item.is_fixed)
            continue;
        item.position.x = (std::max)(item.position.x - movements[item_pos],
                (std::min)(item.position.x + movements[item_pos], solution[x[item_pos]]));
        item.position.y = (std::max)(item.position.y - movements[item_pos],
                (std::min)(item.position.y + movements[item_pos], solution[y[item_pos]]));
        item.lambda = (std::max)(0.0, (std::min)(1.0, solution[lambda[item_pos]]));
    }
    return true;
}
