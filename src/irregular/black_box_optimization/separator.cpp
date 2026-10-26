#include "irregular/black_box_optimization/separator.hpp"

#include "irregular/shape_simplification.hpp"

#include "mathoptsolverscmake/mathopt.hpp"
#ifdef KNITRO_FOUND
#include "mathoptsolverscmake/mathopt_knitro.hpp"
#endif

#include <algorithm>
#include <cmath>

using namespace packingsolver;
using namespace packingsolver::irregular;
using namespace mathoptsolverscmake;

namespace
{

/** Guided local search weights (as for the direct trigonometry engine). */
const double weight_minimum_increase_ratio = 1.2;
const double weight_maximum_increase_ratio = 2.0;
const double weight_decay = 0.95;

/**
 * Relative improvement of the loss below which an iteration or a strike
 * doesn't count as an improvement.
 */
const double substantial_improvement_ratio = 0.98;

ShapeWithHoles transform(
        const ShapeWithHoles& shape,
        Angle angle,
        bool mirror)
{
    ShapeWithHoles shape_new = (mirror)? shape.axial_symmetry_y_axis(): shape;
    return shape_new.rotate(angle);
}

AxisAlignedBoundingBox translate(
        const AxisAlignedBoundingBox& aabb,
        const Point& translation)
{
    AxisAlignedBoundingBox aabb_new = aabb;
    aabb_new.shift(translation);
    return aabb_new;
}

/** Return true iff two bounding boxes, the first one inflated by 'margin', intersect. */
bool aabbs_intersect(
        const AxisAlignedBoundingBox& aabb_1,
        const AxisAlignedBoundingBox& aabb_2,
        LengthDbl margin)
{
    return aabb_1.x_min - margin < aabb_2.x_max
        && aabb_2.x_min < aabb_1.x_max + margin
        && aabb_1.y_min - margin < aabb_2.y_max
        && aabb_2.y_min < aabb_1.y_max + margin;
}

}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// No-fit polygons //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ItemNoFitPolygons::ItemNoFitPolygons(
        const Instance& instance,
        const std::shared_ptr<const Separator::ItemTypes>& item_types,
        double maximum_approximation_ratio)
{
    const bool item_spacing = instance.parameters().item_item_minimum_spacing > 0;
    SimplifiedInstance simplified_instance = shape_simplification(
            instance,
            maximum_approximation_ratio);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const SimplifiedItemType& simplified_item_type = simplified_instance.item_types[item_type_id];
        const Separator::SeparatorItemType& item_type = (*item_types)[item_type_id];
        item_types_first_orientation_index_.push_back(orientations_shapes_.size());
        for (const Separator::Orientation& orientation: item_type.orientations) {
            std::vector<ShapeWithHoles> shapes;
            std::vector<ShapeWithHoles> shapes_inflated;
            for (const SimplifiedShape& simplified_shape: simplified_item_type.shapes) {
                shapes.push_back(transform(
                            simplified_shape.shape,
                            orientation.angle,
                            orientation.mirror));
                shapes_inflated.push_back((item_spacing)?
                        transform(
                            simplified_shape.shape_inflated,
                            orientation.angle,
                            orientation.mirror):
                        shapes.back());
            }
            orientations_shapes_.push_back(shapes);
            orientations_shapes_inflated_.push_back(shapes_inflated);
        }
    }
}

const NoFitPolygon& ItemNoFitPolygons::no_fit_polygon(
        ItemTypeId item_type_id_1,
        Counter orientation_id_1,
        ItemTypeId item_type_id_2,
        Counter orientation_id_2,
        bool& reversed)
{
    Counter orientation_index_1 = orientation_index(item_type_id_1, orientation_id_1);
    Counter orientation_index_2 = orientation_index(item_type_id_2, orientation_id_2);
    reversed = (orientation_index_1 > orientation_index_2);
    Counter fixed_orientation_index = (reversed)? orientation_index_2: orientation_index_1;
    Counter orbiting_orientation_index = (reversed)? orientation_index_1: orientation_index_2;
    int64_t key = (int64_t)fixed_orientation_index * (int64_t)orientations_shapes_.size()
        + orbiting_orientation_index;
    auto it = no_fit_polygons_.find(key);
    if (it != no_fit_polygons_.end())
        return it->second;
    return no_fit_polygons_.emplace(
            key,
            build_no_fit_polygon(
                orientations_shapes_inflated_[fixed_orientation_index],
                orientations_shapes_[orbiting_orientation_index])).first->second;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Constructor //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

std::shared_ptr<const Separator::ItemTypes> build_item_types_if_needed(
        const Instance& instance,
        const BlackBoxOptimizationSeparatorParameters& parameters,
        const std::shared_ptr<const Separator::ItemTypes>& item_types)
{
    if (item_types != nullptr)
        return item_types;
    return Separator::build_item_types(
            instance,
            parameters.direct_trigonometry_parameters);
}

}

BlackBoxOptimizationSeparator::BlackBoxOptimizationSeparator(
        const Instance& instance,
        const BlackBoxOptimizationSeparatorParameters& parameters,
        std::mt19937_64& generator,
        const std::shared_ptr<const Separator::ItemTypes>& item_types,
        const std::shared_ptr<ItemNoFitPolygons>& no_fit_polygons):
    instance_(instance),
    parameters_(parameters),
    direct_trigonometry_separator_(
            instance,
            parameters.direct_trigonometry_parameters,
            generator,
            build_item_types_if_needed(instance, parameters, item_types)),
    item_types_(build_item_types_if_needed(instance, parameters, item_types)),
    no_fit_polygons_((no_fit_polygons != nullptr)?
            no_fit_polygons:
            std::make_shared<ItemNoFitPolygons>(
                instance,
                item_types_,
                parameters.direct_trigonometry_parameters.maximum_approximation_ratio))
{
    const LengthDbl scale_value = instance.parameters().scale_value;
    const Separator::ItemTypes& separator_item_types = *item_types_;

    // The simplified shapes of the bins are only needed for their borders,
    // defects and fixed items.
    bool bin_obstacles = false;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (!bin_type.borders.empty()
                || !bin_type.defects.empty()
                || !bin_type.fixed_items.empty()) {
            bin_obstacles = true;
        }
    }
    SimplifiedInstance simplified_instance;
    if (bin_obstacles) {
        simplified_instance = shape_simplification(
                instance,
                parameters.direct_trigonometry_parameters.maximum_approximation_ratio);
    }

    // Bin types.
    bin_types_.resize(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BinTypeData& bin_type_data = bin_types_[bin_type_id];
        LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
        bin_type_data.container = bin_type.aabb_scaled;
        bin_type_data.container.x_min += bin_spacing;
        bin_type_data.container.x_max -= bin_spacing;
        bin_type_data.container.y_min += bin_spacing;
        bin_type_data.container.y_max -= bin_spacing;

        if (!bin_obstacles)
            continue;
        const SimplifiedBinType& simplified_bin_type = simplified_instance.bin_types[bin_type_id];

        // Borders and defects.
        std::vector<const SimplifiedShape*> simplified_shapes;
        for (const SimplifiedShape& border: simplified_bin_type.borders)
            simplified_shapes.push_back(&border);
        for (const SimplifiedShape& defect: simplified_bin_type.defects)
            simplified_shapes.push_back(&defect);
        for (const SimplifiedShape* simplified_shape: simplified_shapes) {
            Obstacle obstacle;
            obstacle.shapes.push_back(simplified_shape->shape_inflated);
            bin_type_data.obstacles.push_back(obstacle);
        }

        // Fixed items.
        for (const FixedItem& fixed_item: bin_type.fixed_items) {
            const SimplifiedItemType& simplified_item_type = simplified_instance.item_types[fixed_item.item_type_id];
            Obstacle obstacle;
            obstacle.against_inflated = true;
            for (const SimplifiedShape& simplified_shape: simplified_item_type.shapes) {
                ShapeWithHoles shape = transform(
                        simplified_shape.shape,
                        fixed_item.angle,
                        fixed_item.mirror);
                shape.shift(
                        scale_value * fixed_item.bl_corner.x,
                        scale_value * fixed_item.bl_corner.y);
                obstacle.shapes.push_back(shape);
            }
            bin_type_data.obstacles.push_back(obstacle);
        }

        // Bounding boxes of the obstacles.
        for (Obstacle& obstacle: bin_type_data.obstacles) {
            for (const ShapeWithHoles& shape: obstacle.shapes)
                obstacle.aabb = merge(obstacle.aabb, shape.shape.compute_min_max());
        }

        number_of_hazards_ = (std::max)(
                number_of_hazards_,
                (Counter)bin_type_data.obstacles.size() + 1);
    }

    // Items.
    LengthDbl total_minimum_dimension = 0.0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies - item_type.copies_fixed; ++copy) {
            items_item_type_id_.push_back(item_type_id);
            total_minimum_dimension += separator_item_types[item_type_id].minimum_dimension;
        }
    }
    ItemPos number_of_items = items_item_type_id_.size();
    if (number_of_items > 0 && total_minimum_dimension > 0)
        length_scale_ = total_minimum_dimension / number_of_items;
    margin_ = parameters_.margin_ratio * length_scale_;
    items_state_.resize(number_of_items);
    pair_entries_.resize(number_of_items * (number_of_items + 1) / 2);
    hazard_entries_.resize(number_of_items * number_of_hazards_);
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Losses ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BlackBoxOptimizationSeparator::Entry& BlackBoxOptimizationSeparator::pair_entry(
        ItemPos item_pos_1,
        ItemPos item_pos_2)
{
    ItemPos number_of_items = items_item_type_id_.size();
    ItemPos row = (std::min)(item_pos_1, item_pos_2);
    ItemPos column = (std::max)(item_pos_1, item_pos_2);
    return pair_entries_[row * number_of_items + column - (row * (row + 1)) / 2];
}

const BlackBoxOptimizationSeparator::Entry& BlackBoxOptimizationSeparator::pair_entry(
        ItemPos item_pos_1,
        ItemPos item_pos_2) const
{
    ItemPos number_of_items = items_item_type_id_.size();
    ItemPos row = (std::min)(item_pos_1, item_pos_2);
    ItemPos column = (std::max)(item_pos_1, item_pos_2);
    return pair_entries_[row * number_of_items + column - (row * (row + 1)) / 2];
}

bool BlackBoxOptimizationSeparator::pair_signed_distance(
        ItemPos item_pos_1,
        const Point& position_1,
        ItemPos item_pos_2,
        const Point& position_2,
        SignedDistance& output)
{
    const Separator::ItemTypes& item_types = *item_types_;
    ItemTypeId item_type_id_1 = items_item_type_id_[item_pos_1];
    ItemTypeId item_type_id_2 = items_item_type_id_[item_pos_2];
    Counter orientation_id_1 = items_state_[item_pos_1].orientation_id;
    Counter orientation_id_2 = items_state_[item_pos_2].orientation_id;
    const Separator::Orientation& orientation_1 = item_types[item_type_id_1].orientations[orientation_id_1];
    const Separator::Orientation& orientation_2 = item_types[item_type_id_2].orientations[orientation_id_2];
    if (!aabbs_intersect(
                translate(orientation_1.aabb_inflated, position_1),
                translate(orientation_2.aabb, position_2),
                margin_)
            && !aabbs_intersect(
                translate(orientation_2.aabb_inflated, position_2),
                translate(orientation_1.aabb, position_1),
                margin_)) {
        return false;
    }
    bool reversed = false;
    const NoFitPolygon& no_fit_polygon = no_fit_polygons_->no_fit_polygon(
            item_type_id_1,
            orientation_id_1,
            item_type_id_2,
            orientation_id_2,
            reversed);
    // Position of the orbiting item relatively to the fixed item.
    Point relative_position = (reversed)?
        Point{position_1.x - position_2.x, position_1.y - position_2.y}:
        Point{position_2.x - position_1.x, position_2.y - position_1.y};
    if (!signed_distance(no_fit_polygon, relative_position, margin_, output))
        return false;
    // Gradient with respect to the position of the first item.
    if (!reversed) {
        output.gradient.x = -output.gradient.x;
        output.gradient.y = -output.gradient.y;
    }
    return true;
}

bool BlackBoxOptimizationSeparator::obstacle_signed_distance(
        ItemPos item_pos,
        const Point& position,
        Obstacle& obstacle,
        SignedDistance& output)
{
    const Separator::ItemTypes& item_types = *item_types_;
    ItemTypeId item_type_id = items_item_type_id_[item_pos];
    Counter orientation_id = items_state_[item_pos].orientation_id;
    const Separator::Orientation& orientation = item_types[item_type_id].orientations[orientation_id];
    const AxisAlignedBoundingBox& item_aabb = (obstacle.against_inflated)?
        orientation.aabb_inflated:
        orientation.aabb;
    if (!aabbs_intersect(obstacle.aabb, translate(item_aabb, position), margin_))
        return false;
    int64_t key = (int64_t)item_type_id * 1024 * 1024 + orientation_id;
    auto it = obstacle.no_fit_polygons.find(key);
    if (it == obstacle.no_fit_polygons.end()) {
        const std::vector<ShapeWithHoles>& item_shapes = (obstacle.against_inflated)?
            no_fit_polygons_->shapes_inflated(item_type_id, orientation_id):
            no_fit_polygons_->shapes(item_type_id, orientation_id);
        it = obstacle.no_fit_polygons.emplace(
                key,
                build_no_fit_polygon(obstacle.shapes, item_shapes)).first;
    }
    return signed_distance(it->second, position, margin_, output);
}

AxisAlignedBoundingBox BlackBoxOptimizationSeparator::position_bounds(
        ItemPos item_pos,
        const ItemState& item_state) const
{
    const Separator::Orientation& orientation
        = (*item_types_)[items_item_type_id_[item_pos]].orientations[item_state.orientation_id];
    const AxisAlignedBoundingBox& container
        = bin_types_[instance_.bin_type_id(item_state.bin_pos)].container;
    AxisAlignedBoundingBox bounds;
    bounds.x_min = container.x_min - orientation.aabb.x_min;
    bounds.x_max = container.x_max - orientation.aabb.x_max;
    bounds.y_min = container.y_min - orientation.aabb.y_min;
    bounds.y_max = container.y_max - orientation.aabb.y_max;
    if (bounds.x_min > bounds.x_max) {
        bounds.x_min = (bounds.x_min + bounds.x_max) / 2;
        bounds.x_max = bounds.x_min;
    }
    if (bounds.y_min > bounds.y_max) {
        bounds.y_min = (bounds.y_min + bounds.y_max) / 2;
        bounds.y_max = bounds.y_min;
    }
    return bounds;
}

void BlackBoxOptimizationSeparator::compute_losses()
{
    for (Entry& entry: pair_entries_)
        entry.loss = 0.0;
    for (Entry& entry: hazard_entries_)
        entry.loss = 0.0;

    std::vector<std::vector<ItemPos>> bins_items(instance_.number_of_bins());
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos)
        if (items_state_[item_pos].bin_pos != -1)
            bins_items[items_state_[item_pos].bin_pos].push_back(item_pos);

    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        const std::vector<ItemPos>& bin_items = bins_items[bin_pos];
        BinTypeData& bin_type = bin_types_[instance_.bin_type_id(bin_pos)];
        for (ItemPos bin_item_pos_1 = 0;
                bin_item_pos_1 < (ItemPos)bin_items.size();
                ++bin_item_pos_1) {
            ItemPos item_pos_1 = bin_items[bin_item_pos_1];
            const ItemState& item_state_1 = items_state_[item_pos_1];
            SignedDistance output;

            // Other items.
            for (ItemPos bin_item_pos_2 = bin_item_pos_1 + 1;
                    bin_item_pos_2 < (ItemPos)bin_items.size();
                    ++bin_item_pos_2) {
                ItemPos item_pos_2 = bin_items[bin_item_pos_2];
                if (pair_signed_distance(
                            item_pos_1,
                            item_state_1.position,
                            item_pos_2,
                            items_state_[item_pos_2].position,
                            output)
                        && output.distance > 0) {
                    pair_entry(item_pos_1, item_pos_2).loss = output.distance * output.distance;
                }
            }

            // Exterior of the bin.
            const Separator::Orientation& orientation
                = (*item_types_)[items_item_type_id_[item_pos_1]].orientations[item_state_1.orientation_id];
            LengthDbl overflow_x = (std::max)({
                    bin_type.container.x_min - (item_state_1.position.x + orientation.aabb.x_min),
                    (item_state_1.position.x + orientation.aabb.x_max) - bin_type.container.x_max,
                    0.0});
            LengthDbl overflow_y = (std::max)({
                    bin_type.container.y_min - (item_state_1.position.y + orientation.aabb.y_min),
                    (item_state_1.position.y + orientation.aabb.y_max) - bin_type.container.y_max,
                    0.0});
            hazard_entry(item_pos_1, 0).loss = overflow_x * overflow_x + overflow_y * overflow_y;

            // Obstacles.
            for (Counter obstacle_pos = 0;
                    obstacle_pos < (Counter)bin_type.obstacles.size();
                    ++obstacle_pos) {
                if (obstacle_signed_distance(
                            item_pos_1,
                            item_state_1.position,
                            bin_type.obstacles[obstacle_pos],
                            output)
                        && output.distance > 0) {
                    hazard_entry(item_pos_1, obstacle_pos + 1).loss = output.distance * output.distance;
                }
            }
        }
    }
}

double BlackBoxOptimizationSeparator::total_loss() const
{
    double loss = 0.0;
    for (const Entry& entry: pair_entries_)
        loss += entry.loss;
    for (const Entry& entry: hazard_entries_)
        loss += entry.loss;
    return loss;
}

void BlackBoxOptimizationSeparator::update_weights()
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
            if (entry.loss > 0.0)
                entry.weight += entry.loss / maximum_loss;
            return;
        }
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
////////////////////////////// Nonlinear programs //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void BlackBoxOptimizationSeparator::solve_bin(
        BinPos bin_pos,
        const std::vector<ItemPos>& bin_items,
        const optimizationtools::Timer& timer)
{
    BinTypeData& bin_type = bin_types_[instance_.bin_type_id(bin_pos)];
    ItemPos number_of_bin_items = bin_items.size();

    // Variables: the positions of the items, divided by the length scale.
    MathOptModel model(2 * number_of_bin_items);
    model.objective_direction = ObjectiveDirection::Minimize;
    model.variables_initial_values.resize(2 * number_of_bin_items);
    for (ItemPos bin_item_pos = 0; bin_item_pos < number_of_bin_items; ++bin_item_pos) {
        ItemPos item_pos = bin_items[bin_item_pos];
        const ItemState& item_state = items_state_[item_pos];
        AxisAlignedBoundingBox bounds = position_bounds(item_pos, item_state);
        model.variables_lower_bounds[2 * bin_item_pos] = bounds.x_min / length_scale_;
        model.variables_upper_bounds[2 * bin_item_pos] = bounds.x_max / length_scale_;
        model.variables_lower_bounds[2 * bin_item_pos + 1] = bounds.y_min / length_scale_;
        model.variables_upper_bounds[2 * bin_item_pos + 1] = bounds.y_max / length_scale_;
        model.variables_initial_values[2 * bin_item_pos] = (std::max)(bounds.x_min, (std::min)(bounds.x_max, item_state.position.x)) / length_scale_;
        model.variables_initial_values[2 * bin_item_pos + 1] = (std::max)(bounds.y_min, (std::min)(bounds.y_max, item_state.position.y)) / length_scale_;
    }

    // Objective: the weighted squared penetration depths, increased by the
    // margin.
    model.objective_function = [this, &bin_items, &bin_type, number_of_bin_items](
            const std::vector<double>& variables)
    {
        BlackBoxFunctionOutput output;
        output.gradient.resize(variables.size(), 0.0);
        std::vector<Point> positions(number_of_bin_items);
        for (ItemPos bin_item_pos = 0; bin_item_pos < number_of_bin_items; ++bin_item_pos) {
            positions[bin_item_pos] = {
                variables[2 * bin_item_pos] * length_scale_,
                variables[2 * bin_item_pos + 1] * length_scale_};
        }
        double value = 0.0;
        // Add the penalty of a signed distance, whose gradient is with
        // respect to the position of the item at 'bin_item_pos'.
        auto add = [&output, &value](
                const SignedDistance& signed_distance,
                LengthDbl margin,
                double weight,
                ItemPos bin_item_pos,
                ItemPos other_bin_item_pos)
        {
            LengthDbl penetration = signed_distance.distance + margin;
            if (penetration <= 0)
                return;
            value += weight * penetration * penetration;
            double gradient_x = 2 * weight * penetration * signed_distance.gradient.x;
            double gradient_y = 2 * weight * penetration * signed_distance.gradient.y;
            output.gradient[2 * bin_item_pos] += gradient_x;
            output.gradient[2 * bin_item_pos + 1] += gradient_y;
            if (other_bin_item_pos != -1) {
                output.gradient[2 * other_bin_item_pos] -= gradient_x;
                output.gradient[2 * other_bin_item_pos + 1] -= gradient_y;
            }
        };
        SignedDistance signed_distance;
        for (ItemPos bin_item_pos_1 = 0; bin_item_pos_1 < number_of_bin_items; ++bin_item_pos_1) {
            ItemPos item_pos_1 = bin_items[bin_item_pos_1];
            for (ItemPos bin_item_pos_2 = bin_item_pos_1 + 1;
                    bin_item_pos_2 < number_of_bin_items;
                    ++bin_item_pos_2) {
                ItemPos item_pos_2 = bin_items[bin_item_pos_2];
                if (pair_signed_distance(
                            item_pos_1,
                            positions[bin_item_pos_1],
                            item_pos_2,
                            positions[bin_item_pos_2],
                            signed_distance)) {
                    add(
                            signed_distance,
                            margin_,
                            pair_entry(item_pos_1, item_pos_2).weight,
                            bin_item_pos_1,
                            bin_item_pos_2);
                }
            }
            for (Counter obstacle_pos = 0;
                    obstacle_pos < (Counter)bin_type.obstacles.size();
                    ++obstacle_pos) {
                if (obstacle_signed_distance(
                            item_pos_1,
                            positions[bin_item_pos_1],
                            bin_type.obstacles[obstacle_pos],
                            signed_distance)) {
                    add(
                            signed_distance,
                            margin_,
                            hazard_entry(item_pos_1, obstacle_pos + 1).weight,
                            bin_item_pos_1,
                            -1);
                }
            }
        }
        // Scaling of the variables.
        output.objective_value = value / (length_scale_ * length_scale_);
        for (double& gradient: output.gradient)
            gradient /= length_scale_;
        return output;
    };

    // Solve.
    std::vector<double> solution;
#ifdef KNITRO_FOUND
    KnitroContext knitro;
    reduce_printout(knitro);
    set_time_limit(knitro, timer.remaining_time());
    load(knitro, model);
    KN_set_int_param(knitro.kc, KN_PARAM_NUMTHREADS, 1);
    KN_set_int_param(knitro.kc, KN_PARAM_HESSOPT, KN_HESSOPT_LBFGS);
    KN_set_int_param(knitro.kc, KN_PARAM_MAXIT, parameters_.maximum_number_of_solver_iterations);
    solve(knitro);
    solution = get_solution(knitro);
#else
    (void)timer;
    throw std::invalid_argument(FUNC_SIGNATURE + ": Knitro not available.");
#endif
    number_of_nonlinear_programs_++;

    // New positions, clamped to their bounds.
    for (ItemPos bin_item_pos = 0; bin_item_pos < number_of_bin_items; ++bin_item_pos) {
        ItemPos item_pos = bin_items[bin_item_pos];
        ItemState& item_state = items_state_[item_pos];
        AxisAlignedBoundingBox bounds = position_bounds(item_pos, item_state);
        item_state.position.x = (std::max)(bounds.x_min, (std::min)(bounds.x_max,
                    solution[2 * bin_item_pos] * length_scale_));
        item_state.position.y = (std::max)(bounds.y_min, (std::min)(bounds.y_max,
                    solution[2 * bin_item_pos + 1] * length_scale_));
    }
}

void BlackBoxOptimizationSeparator::step(const optimizationtools::Timer& timer)
{
    std::vector<std::vector<ItemPos>> bins_items(instance_.number_of_bins());
    std::vector<bool> bins_overlap(instance_.number_of_bins(), false);
    ItemPos number_of_items = items_state_.size();
    for (ItemPos item_pos = 0; item_pos < number_of_items; ++item_pos) {
        BinPos bin_pos = items_state_[item_pos].bin_pos;
        if (bin_pos == -1)
            continue;
        bins_items[bin_pos].push_back(item_pos);
        for (Counter hazard_pos = 0; hazard_pos < number_of_hazards_; ++hazard_pos)
            if (hazard_entry(item_pos, hazard_pos).loss > 0)
                bins_overlap[bin_pos] = true;
        for (ItemPos item_pos_2 = item_pos + 1; item_pos_2 < number_of_items; ++item_pos_2)
            if (pair_entry(item_pos, item_pos_2).loss > 0)
                bins_overlap[bin_pos] = true;
    }
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        if (timer.needs_to_end())
            break;
        if (bins_overlap[bin_pos])
            solve_bin(bin_pos, bins_items[bin_pos], timer);
    }
    compute_losses();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Separation //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BlackBoxOptimizationSeparator::separate_initial(const optimizationtools::Timer& timer)
{
    return separate(
            timer,
            std::numeric_limits<double>::infinity(),
            (initial_maximum_number_of_iterations_without_improvement_ != -1)?
                initial_maximum_number_of_iterations_without_improvement_:
                parameters_.maximum_number_of_iterations_without_improvement,
            (initial_maximum_number_of_strikes_ != -1)?
                initial_maximum_number_of_strikes_:
                parameters_.maximum_number_of_strikes,
            parameters_.maximum_number_of_strikes_above_reference);
}

bool BlackBoxOptimizationSeparator::separate(
        const optimizationtools::Timer& timer,
        double reference_loss)
{
    return separate(
            timer,
            reference_loss,
            parameters_.maximum_number_of_iterations_without_improvement,
            parameters_.maximum_number_of_strikes,
            parameters_.maximum_number_of_strikes_above_reference);
}

bool BlackBoxOptimizationSeparator::separate(
        const optimizationtools::Timer& timer,
        double reference_loss,
        Counter maximum_number_of_iterations_without_improvement,
        Counter maximum_number_of_strikes,
        Counter maximum_number_of_strikes_above_reference)
{
    compute_losses();
    double minimum_loss = total_loss();
    if (minimum_loss == 0)
        return true;
    Layout best_layout = items_state_;
    Counter number_of_strikes = 0;
    for (;;) {
        Counter maximum_number_of_strikes_cur = (minimum_loss < reference_loss)?
            maximum_number_of_strikes:
            maximum_number_of_strikes_above_reference;
        if (number_of_strikes >= maximum_number_of_strikes_cur
                || timer.needs_to_end()) {
            break;
        }
        Counter number_of_iterations_without_improvement = 0;
        double initial_loss = total_loss();
        while (number_of_iterations_without_improvement
                < maximum_number_of_iterations_without_improvement) {
            if (timer.needs_to_end())
                break;
            step(timer);
            double loss = total_loss();
            if (loss == 0) {
                return true;
            } else if (loss < minimum_loss) {
                if (loss < substantial_improvement_ratio * minimum_loss)
                    number_of_iterations_without_improvement = 0;
                best_layout = items_state_;
                minimum_loss = loss;
            } else {
                number_of_iterations_without_improvement++;
            }
            update_weights();
        }
        if (initial_loss * substantial_improvement_ratio <= minimum_loss) {
            number_of_strikes++;
        } else {
            number_of_strikes = 0;
        }
        // Restore the best layout, keeping the weights.
        items_state_ = best_layout;
        compute_losses();
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Layout ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void BlackBoxOptimizationSeparator::set_solution(
        const Solution& solution,
        const optimizationtools::Timer& timer)
{
    direct_trigonometry_separator_.set_solution(solution, timer);
    set_layout(direct_trigonometry_separator_.layout());
}

void BlackBoxOptimizationSeparator::set_layout(const Layout& layout)
{
    items_state_ = layout;
    for (Entry& entry: pair_entries_)
        entry = Entry();
    for (Entry& entry: hazard_entries_)
        entry = Entry();
    compute_losses();
}

void BlackBoxOptimizationSeparator::disrupt()
{
    direct_trigonometry_separator_.set_layout(items_state_);
    direct_trigonometry_separator_.disrupt();
    items_state_ = direct_trigonometry_separator_.layout();
    compute_losses();
}

Profit BlackBoxOptimizationSeparator::profit() const
{
    Profit profit = 0;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos)
        if (items_state_[item_pos].bin_pos != -1)
            profit += instance_.item_type(items_item_type_id_[item_pos]).profit;
    return profit;
}

bool BlackBoxOptimizationSeparator::insert_items_above_profit_threshold()
{
    direct_trigonometry_separator_.set_layout(items_state_);
    bool inserted = direct_trigonometry_separator_.insert_items_above_profit_threshold();
    set_layout(direct_trigonometry_separator_.layout());
    return inserted;
}

Counter BlackBoxOptimizationSeparator::distance(
        const Layout& layout_1,
        const Layout& layout_2) const
{
    return direct_trigonometry_separator_.distance(layout_1, layout_2);
}

Solution BlackBoxOptimizationSeparator::solution() const
{
    direct_trigonometry_separator_.set_layout(items_state_);
    return direct_trigonometry_separator_.solution();
}
