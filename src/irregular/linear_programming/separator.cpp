#include "irregular/linear_programming/separator.hpp"

#include "irregular/utils.hpp"
#include "irregular/solution_builder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace packingsolver;
using namespace packingsolver::irregular;

namespace
{

/** Scale below which an item is considered shrunk. */
const double full_scale = 1.0 - 1e-6;

}

LinearProgrammingSeparator::LinearProgrammingSeparator(
        const Instance& instance,
        const LinearProgrammingSeparatorParameters& parameters,
        std::mt19937_64& generator):
    instance_(instance),
    parameters_(parameters),
    generator_(generator()),
    decomposition_(std::make_shared<InstanceBasicShapeDecomposition>(
                compute_instance_basic_shape_decomposition(instance)))
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemPos copy = 0; copy < item_type.copies - item_type.copies_fixed; ++copy) {
            items_item_type_id_.push_back(item_type_id);
            items_area_.push_back(item_type.area_scaled);
        }
    }
    items_state_.resize(items_item_type_id_.size());
    items_weight_.resize(items_item_type_id_.size(), 1.0);
}

void LinearProgrammingSeparator::random_orientation(
        ItemTypeId item_type_id,
        Angle& angle,
        bool& mirror)
{
    const ItemType& item_type = instance_.item_type(item_type_id);
    std::uniform_int_distribution<Counter> distribution(0, item_type.allowed_rotations.size() - 1);
    const AllowedRotation& rotation = item_type.allowed_rotations[distribution(generator_)];
    angle = rotation.start_angle;
    mirror = rotation.mirror;
}

void LinearProgrammingSeparator::random_placement(
        ItemPos item_pos,
        ItemState& item_state)
{
    std::uniform_int_distribution<BinPos> bin_distribution(0, instance_.number_of_bins() - 1);
    item_state.bin_pos = bin_distribution(generator_);
    random_orientation(items_item_type_id_[item_pos], item_state.angle, item_state.mirror);
    const BinType& bin_type = instance_.bin_type(instance_.bin_type_id(item_state.bin_pos));
    std::uniform_real_distribution<LengthDbl> distribution_x(
            bin_type.aabb_scaled.x_min,
            bin_type.aabb_scaled.x_max);
    std::uniform_real_distribution<LengthDbl> distribution_y(
            bin_type.aabb_scaled.y_min,
            bin_type.aabb_scaled.y_max);
    item_state.position = {distribution_x(generator_), distribution_y(generator_)};
    item_state.lambda = 0.0;
}

void LinearProgrammingSeparator::set_solution(
        const Solution& solution,
        const optimizationtools::Timer& timer)
{
    (void)timer;
    const LengthDbl scale_value = instance_.parameters().scale_value;
    Layout layout(items_item_type_id_.size());

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
                ItemPos item_pos = items.back();
                items.pop_back();
                ItemState& item_state = layout[item_pos];
                item_state.bin_pos = bin_pos;
                item_state.angle = solution_item.angle;
                item_state.mirror = solution_item.mirror;
                item_state.position = {
                    scale_value * solution_item.bl_corner.x,
                    scale_value * solution_item.bl_corner.y};
                item_state.lambda = 1.0;
            }
        }
    }

    // Items sticking out of their bin: remove a slice of the bin at a random
    // position, and shift the items after it.
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        const BinType& bin_type = instance_.bin_type(instance_.bin_type_id(bin_pos));
        AxisAlignedBoundingBox items_aabb;
        bool empty = true;
        std::vector<AxisAlignedBoundingBox> aabbs(layout.size());
        for (ItemPos item_pos = 0; item_pos < (ItemPos)layout.size(); ++item_pos) {
            const ItemState& item_state = layout[item_pos];
            if (item_state.bin_pos != bin_pos)
                continue;
            AxisAlignedBoundingBox aabb = instance_.item_type(items_item_type_id_[item_pos]).compute_min_max(
                    item_state.angle,
                    item_state.mirror,
                    1);
            aabb.shift(item_state.position);
            aabbs[item_pos] = aabb;
            items_aabb = merge(items_aabb, aabb);
            empty = false;
        }
        if (empty)
            continue;
        LengthDbl overflow_x = items_aabb.x_max - bin_type.aabb_scaled.x_max;
        if (overflow_x > 0) {
            std::uniform_real_distribution<LengthDbl> distribution(
                    bin_type.aabb_scaled.x_min,
                    bin_type.aabb_scaled.x_max);
            LengthDbl split = distribution(generator_);
            for (ItemPos item_pos = 0; item_pos < (ItemPos)layout.size(); ++item_pos) {
                if (layout[item_pos].bin_pos != bin_pos)
                    continue;
                if ((aabbs[item_pos].x_min + aabbs[item_pos].x_max) / 2 > split)
                    layout[item_pos].position.x -= overflow_x;
            }
        }
        LengthDbl overflow_y = items_aabb.y_max - bin_type.aabb_scaled.y_max;
        if (overflow_y > 0) {
            std::uniform_real_distribution<LengthDbl> distribution(
                    bin_type.aabb_scaled.y_min,
                    bin_type.aabb_scaled.y_max);
            LengthDbl split = distribution(generator_);
            for (ItemPos item_pos = 0; item_pos < (ItemPos)layout.size(); ++item_pos) {
                if (layout[item_pos].bin_pos != bin_pos)
                    continue;
                if ((aabbs[item_pos].y_min + aabbs[item_pos].y_max) / 2 > split)
                    layout[item_pos].position.y -= overflow_y;
            }
        }
    }

    // Missing items: random placement.
    if (instance_.number_of_bins() > 0)
        for (ItemPos item_pos = 0; item_pos < (ItemPos)layout.size(); ++item_pos)
            if (layout[item_pos].bin_pos == -1)
                random_placement(item_pos, layout[item_pos]);

    items_state_ = layout;
}

std::vector<SeparationModelItem> LinearProgrammingSeparator::bin_items(
        BinPos bin_pos,
        std::vector<ItemPos>& item_positions) const
{
    const LengthDbl scale_value = instance_.parameters().scale_value;
    std::vector<SeparationModelItem> items;
    item_positions.clear();
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
        const ItemState& item_state = items_state_[item_pos];
        if (item_state.bin_pos != bin_pos)
            continue;
        SeparationModelItem item;
        item.item_type_id = items_item_type_id_[item_pos];
        item.angle = item_state.angle;
        item.mirror = item_state.mirror;
        item.position = item_state.position;
        item.lambda = item_state.lambda;
        item.weight = items_weight_[item_pos] * items_area_[item_pos];
        items.push_back(item);
        item_positions.push_back(item_pos);
    }
    const BinType& bin_type = instance_.bin_type(instance_.bin_type_id(bin_pos));
    for (const auto& fixed_item: bin_type.fixed_items) {
        SeparationModelItem item;
        item.item_type_id = fixed_item.item_type_id;
        item.angle = fixed_item.angle;
        item.mirror = fixed_item.mirror;
        item.is_fixed = true;
        item.position = {scale_value * fixed_item.bl_corner.x, scale_value * fixed_item.bl_corner.y};
        items.push_back(item);
        item_positions.push_back(-1);
    }
    return items;
}

LengthDbl LinearProgrammingSeparator::separation() const
{
    LengthDbl separation = std::numeric_limits<LengthDbl>::infinity();
    std::vector<ItemPos> item_positions;
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        std::vector<SeparationModelItem> items = bin_items(bin_pos, item_positions);
        if (items.empty())
            continue;
        BinSeparationModel model(instance_, *decomposition_, instance_.bin_type_id(bin_pos), items);
        if (!model.supported()) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "the shapes must be made of line segments.");
        }
        separation = (std::min)(separation, model.separation(items));
    }
    return separation;
}

bool LinearProgrammingSeparator::step(
        const optimizationtools::Timer& timer,
        double movement_ratio)
{
    std::vector<ItemPos> item_positions;
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        std::vector<SeparationModelItem> items = bin_items(bin_pos, item_positions);
        bool movable = false;
        for (const SeparationModelItem& item: items)
            if (!item.is_fixed)
                movable = true;
        if (!movable)
            continue;
        BinSeparationModel model(instance_, *decomposition_, instance_.bin_type_id(bin_pos), items);
        if (!model.supported()) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "the shapes must be made of line segments.");
        }
        BinSeparationModel::StepParameters step_parameters;
        step_parameters.movement_ratio = movement_ratio;
        step_parameters.shrink = true;
        step_parameters.target_separation = 0.0;
        // Eta first: its weight is larger than the largest possible gain on
        // the scales.
        double weights_sum = 0.0;
        for (const SeparationModelItem& item: items)
            weights_sum += item.weight;
        step_parameters.eta_weight = 1e6 * (1.0 + weights_sum);
        if (!model.step(items, step_parameters, timer))
            return false;
        for (Counter pos = 0; pos < (Counter)items.size(); ++pos) {
            ItemPos item_pos = item_positions[pos];
            if (item_pos == -1)
                continue;
            items_state_[item_pos].position = items[pos].position;
            items_state_[item_pos].lambda = items[pos].lambda;
        }
    }

    // Guided local search: the weights of the shrunk items increase.
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos)
        if (items_state_[item_pos].lambda < full_scale)
            items_weight_[item_pos] += 1.0;
    return true;
}

double LinearProgrammingSeparator::total_loss() const
{
    double loss = 0.0;
    double total_area = 0.0;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
        const ItemState& item_state = items_state_[item_pos];
        total_area += items_area_[item_pos];
        double lambda = (item_state.bin_pos == -1)? 0.0: item_state.lambda;
        loss += items_area_[item_pos] * (1.0 - lambda);
    }
    LengthDbl separation = this->separation();
    if (separation < -parameters_.tolerance)
        loss = total_area - separation;
    return loss;
}

bool LinearProgrammingSeparator::separate(
        const optimizationtools::Timer& timer,
        double reference_loss,
        Counter maximum_number_of_iterations_without_improvement)
{
    double loss_best = total_loss();
    Layout layout_best = items_state_;
    Counter number_of_iterations_without_improvement = 0;
    // Trust region: halved after each linear program without improvement,
    // the next one starting again from the best layout.
    double movement_ratio = parameters_.movement_ratio;
    for (;;) {
        // Success: all the items have their full size, without overlap.
        bool full = true;
        for (const ItemState& item_state: items_state_)
            if (item_state.bin_pos == -1 || item_state.lambda < full_scale)
                full = false;
        if (full) {
            Layout layout = items_state_;
            for (ItemState& item_state: items_state_)
                item_state.lambda = 1.0;
            if (separation() >= -parameters_.tolerance)
                return true;
            items_state_ = layout;
        }

        Counter maximum_number_of_iterations = (loss_best < reference_loss)?
            maximum_number_of_iterations_without_improvement:
            (std::min)(
                    maximum_number_of_iterations_without_improvement,
                    parameters_.maximum_number_of_iterations_without_improvement_above_reference);
        if (number_of_iterations_without_improvement >= maximum_number_of_iterations
                || timer.needs_to_end()) {
            break;
        }
        if (!step(timer, movement_ratio))
            break;
        double loss = total_loss();
        if (loss < loss_best - 1e-9 * (1.0 + std::abs(loss_best))) {
            loss_best = loss;
            layout_best = items_state_;
            number_of_iterations_without_improvement = 0;
        } else {
            number_of_iterations_without_improvement++;
            items_state_ = layout_best;
            movement_ratio = (std::max)(
                    parameters_.minimum_movement_ratio,
                    movement_ratio / 2);
        }
    }
    items_state_ = layout_best;
    return false;
}

bool LinearProgrammingSeparator::separate(
        const optimizationtools::Timer& timer,
        double reference_loss)
{
    return separate(
            timer,
            reference_loss,
            parameters_.maximum_number_of_iterations_without_improvement);
}

bool LinearProgrammingSeparator::separate_initial(const optimizationtools::Timer& timer)
{
    return separate(
            timer,
            std::numeric_limits<double>::infinity(),
            parameters_.initial_maximum_number_of_iterations_without_improvement);
}

void LinearProgrammingSeparator::disrupt()
{
    // Most shrunk item.
    ItemPos item_pos_shrunk = -1;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
        if (items_state_[item_pos].lambda >= full_scale)
            continue;
        if (item_pos_shrunk == -1
                || items_state_[item_pos].lambda < items_state_[item_pos_shrunk].lambda) {
            item_pos_shrunk = item_pos;
        }
    }
    if (item_pos_shrunk == -1 || instance_.number_of_bins() == 0)
        return;

    random_placement(item_pos_shrunk, items_state_[item_pos_shrunk]);
}

double LinearProgrammingSeparator::distance(
        const Layout& layout_1,
        const Layout& layout_2) const
{
    if (layout_1.empty())
        return 0.0;
    double distance = 0.0;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)layout_1.size(); ++item_pos) {
        const ItemState& item_state_1 = layout_1[item_pos];
        const ItemState& item_state_2 = layout_2[item_pos];
        if (item_state_1.bin_pos != item_state_2.bin_pos
                || item_state_1.angle != item_state_2.angle
                || item_state_1.mirror != item_state_2.mirror) {
            distance += 1.0;
            continue;
        }
        LengthDbl delta_x = item_state_1.position.x - item_state_2.position.x;
        LengthDbl delta_y = item_state_1.position.y - item_state_2.position.y;
        LengthDbl size = std::sqrt(items_area_[item_pos]);
        distance += (std::min)(1.0, std::sqrt(delta_x * delta_x + delta_y * delta_y) / size);
    }
    return distance / layout_1.size();
}

Solution LinearProgrammingSeparator::solution() const
{
    const LengthDbl scale_value = instance_.parameters().scale_value;
    SolutionBuilder solution_builder(instance_);
    for (BinPos bin_pos = 0; bin_pos < instance_.number_of_bins(); ++bin_pos) {
        const BinType& bin_type = instance_.bin_type(instance_.bin_type_id(bin_pos));
        bool empty = bin_type.fixed_items.empty();
        for (const ItemState& item_state: items_state_)
            if (item_state.bin_pos == bin_pos)
                empty = false;
        if (empty)
            continue;
        BinPos solution_bin_pos = solution_builder.add_bin(instance_.bin_type_id(bin_pos), 1);
        for (const auto& fixed_item: bin_type.fixed_items) {
            solution_builder.add_item(
                    solution_bin_pos,
                    fixed_item.item_type_id,
                    fixed_item.bl_corner,
                    fixed_item.angle,
                    fixed_item.mirror,
                    true);
        }
        for (ItemPos item_pos = 0; item_pos < (ItemPos)items_state_.size(); ++item_pos) {
            const ItemState& item_state = items_state_[item_pos];
            if (item_state.bin_pos != bin_pos)
                continue;
            solution_builder.add_item(
                    solution_bin_pos,
                    items_item_type_id_[item_pos],
                    {item_state.position.x / scale_value, item_state.position.y / scale_value},
                    item_state.angle,
                    item_state.mirror);
        }
    }
    return solution_builder.build();
}
