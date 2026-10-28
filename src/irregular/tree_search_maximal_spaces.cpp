#include "irregular/tree_search_maximal_spaces.hpp"
#include "irregular/sequential_feasibility.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"

#include "irregular/rotations.hpp"
#include "irregular/solution_builder.hpp"
#include "irregular/tree_search_periodic_packing.hpp"
#include "algorithms/thread_pool.hpp"
#include "treesearchsolver/iterative_beam_search.hpp"

#include "shape/rectangular_approximation.hpp"

#include <thread>
#include <forward_list>
#include <algorithm>
#include <cmath>

using namespace packingsolver;
using namespace packingsolver::irregular;

////////////////////////////////////////////////////////////////////////////////
////////////////////////// BranchingSchemeMaximalSpaces ////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

/** Convert a rectangle from original coordinates to direction coordinates. */
AxisAlignedBoundingBox convert_rectangle(
        const AxisAlignedBoundingBox& rectangle,
        Direction direction)
{
    Point point_1 = convert_point({rectangle.x_min, rectangle.y_min}, direction);
    Point point_2 = convert_point({rectangle.x_max, rectangle.y_max}, direction);
    AxisAlignedBoundingBox converted_rectangle;
    converted_rectangle.x_min = (std::min)(point_1.x, point_2.x);
    converted_rectangle.x_max = (std::max)(point_1.x, point_2.x);
    converted_rectangle.y_min = (std::min)(point_1.y, point_2.y);
    converted_rectangle.y_max = (std::max)(point_1.y, point_2.y);
    return converted_rectangle;
}

/** Shift a trapezoid. */
GeneralizedTrapezoid shifted(
        GeneralizedTrapezoid trapezoid,
        LengthDbl x,
        LengthDbl y)
{
    trapezoid.shift_right(x);
    trapezoid.shift_top(y);
    return trapezoid;
}

}

BranchingSchemeMaximalSpaces::BranchingSchemeMaximalSpaces(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters),
    simplified_instance_(shape_simplification(
            instance,
            parameters.maximum_approximation_ratio))
{
    BinTypeId bin_type_id = instance.bin_type_id(0);
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Direction direction = parameters_.direction;

    bin_type_ = build_trapezoid_bin_type(
            instance,
            simplified_instance_,
            compute_item_type_rotations(instance)[bin_type_id],
            bin_type_id,
            direction);
    trapezoid_sets_ = bin_type_.trapezoid_sets;
    supporting_parts_ = bin_type_.supporting_parts;
    supported_parts_ = bin_type_.supported_parts;
    for (const TrapezoidSet& trapezoid_set: trapezoid_sets_)
        trapezoid_sets_rectangles_.push_back(compute_rectangles(trapezoid_set));

    LengthDbl bin_spacing = instance.bin_spacing_scaled(bin_type_id);
    bin_aabb_.x_min = bin_type_.x_min + bin_spacing;
    bin_aabb_.x_max = bin_type_.x_max - bin_spacing;
    bin_aabb_.y_min = bin_type_.y_min + bin_spacing;
    bin_aabb_.y_max = bin_type_.y_max - bin_spacing;

    // Approximate the obstacles by rectangles, in the original frame, then
    // convert them. The empty spaces span the bounding box of the bin: the
    // parts of it outside of the shape of the bin, the borders, are obstacles
    // too.
    std::vector<shape::OuterApproximationByRectanglesInputShape> obstacles;
    for (const std::vector<Defect>& defects: {bin_type.defects, bin_type.borders}) {
        for (const Defect& defect: defects) {
            shape::OuterApproximationByRectanglesInputShape obstacle;
            obstacle.shape = defect.shape_inflated;
            obstacles.push_back(obstacle);
        }
    }
    for (const FixedItem& fixed_item: bin_type.fixed_items) {
        const ItemType& item_type = instance.item_type(fixed_item.item_type_id);
        Point bl_corner = instance.parameters().scale_value * fixed_item.bl_corner;
        for (const ItemShape& item_shape: item_type.shapes) {
            shape::OuterApproximationByRectanglesInputShape obstacle;
            obstacle.shape = item_shape.shape_inflated;
            if (fixed_item.mirror)
                obstacle.shape = obstacle.shape.axial_symmetry_y_axis();
            obstacle.shape = obstacle.shape.rotate(fixed_item.angle);
            obstacle.shape.shift(bl_corner.x, bl_corner.y);
            obstacles.push_back(obstacle);
        }
    }
    if (!obstacles.empty()) {
        AreaDbl total_item_area = 0.0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance.item_type(item_type_id);
            total_item_area += item_type.copies * item_type.area_scaled;
        }
        AreaDbl maximum_waste = parameters_.rectangle_approximation_ratio
            * (std::min)(bin_type.area_scaled, total_item_area);
        for (const std::vector<AxisAlignedBoundingBox>& rectangles:
                shape::outer_approximation_by_rectangles(obstacles, maximum_waste)) {
            for (const AxisAlignedBoundingBox& rectangle: rectangles)
                obstacles_rectangles_.push_back(convert_rectangle(rectangle, direction));
        }
    }

    compute_blocks();
}

std::vector<AxisAlignedBoundingBox> BranchingSchemeMaximalSpaces::compute_rectangles(
        const TrapezoidSet& trapezoid_set) const
{
    const ItemType& item_type = instance().item_type(trapezoid_set.item_type_id);
    const SimplifiedItemType& simplified_item_type = simplified_instance_.item_types[trapezoid_set.item_type_id];
    std::vector<shape::OuterApproximationByRectanglesInputShape> shapes;
    AreaDbl area = 0.0;
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)item_type.shapes.size();
            ++item_shape_pos) {
        const ShapeWithHoles& simplified_shape = simplified_item_type.shapes[item_shape_pos].shape_inflated;
        shape::OuterApproximationByRectanglesInputShape input_shape;
        input_shape.shape.shape = convert_shape(
                simplified_shape.shape,
                trapezoid_set.angle,
                trapezoid_set.mirror,
                parameters_.direction);
        for (const Shape& hole: simplified_shape.holes) {
            input_shape.shape.holes.push_back(convert_shape(
                        hole,
                        trapezoid_set.angle,
                        trapezoid_set.mirror,
                        parameters_.direction));
        }
        area += input_shape.shape.compute_area();
        shapes.push_back(input_shape);
    }
    // Increase the maximum waste until there are few enough rectangles: the
    // number of empty spaces grows quickly with the number of rectangles.
    std::vector<AxisAlignedBoundingBox> rectangles;
    for (double ratio = parameters_.rectangle_approximation_ratio;; ratio *= 2) {
        rectangles.clear();
        for (const std::vector<AxisAlignedBoundingBox>& shape_rectangles:
                shape::outer_approximation_by_rectangles(shapes, ratio * area)) {
            rectangles.insert(
                    rectangles.end(),
                    shape_rectangles.begin(),
                    shape_rectangles.end());
        }
        if ((ElementPos)rectangles.size() <= parameters_.maximum_number_of_rectangles
                || (ElementPos)rectangles.size() <= (ElementPos)shapes.size()) {
            break;
        }
    }
    return rectangles;
}

TrapezoidSetId BranchingSchemeMaximalSpaces::trapezoid_set_id(
        ItemTypeId item_type_id,
        Angle angle,
        bool mirror)
{
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_.size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set = trapezoid_sets_[trapezoid_set_id];
        if (trapezoid_set.item_type_id == item_type_id
                && shape::equal(trapezoid_set.angle, angle)
                && trapezoid_set.mirror == mirror) {
            return trapezoid_set_id;
        }
    }
    TrapezoidSetId trapezoid_set_id = trapezoid_sets_.size();
    ItemTypeRotation rotation;
    rotation.angle = angle;
    rotation.mirror = mirror;
    trapezoid_sets_.push_back(build_trapezoid_set(
                instance(),
                simplified_instance_,
                item_type_id,
                rotation,
                parameters_.direction,
                trapezoid_set_id,
                supporting_parts_,
                supported_parts_));
    trapezoid_sets_rectangles_.push_back(compute_rectangles(trapezoid_sets_.back()));
    return trapezoid_set_id;
}

BranchingSchemeMaximalSpaces::Block BranchingSchemeMaximalSpaces::build_block(
        const std::vector<BlockItem>& items) const
{
    Block block;
    block.items = items;
    TrapezoidSet& block_trapezoid_set = block.trapezoid_set;
    block_trapezoid_set.item_type_id = -1;
    block_trapezoid_set.angle = 0.0;
    block_trapezoid_set.mirror = false;
    block_trapezoid_set.x_min = +std::numeric_limits<LengthDbl>::infinity();
    block_trapezoid_set.x_max = -std::numeric_limits<LengthDbl>::infinity();
    block_trapezoid_set.y_min = +std::numeric_limits<LengthDbl>::infinity();
    block_trapezoid_set.y_max = -std::numeric_limits<LengthDbl>::infinity();

    // For each trapezoid shape of the block, the item it belongs to.
    std::vector<ItemPos> shapes_item_pos;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items.size(); ++item_pos) {
        const BlockItem& item = items[item_pos];
        const TrapezoidSet& trapezoid_set = trapezoid_sets_[item.trapezoid_set_id];
        const ItemType& item_type = instance().item_type(trapezoid_set.item_type_id);
        for (const auto& item_shape_trapezoids: trapezoid_set.shapes) {
            block_trapezoid_set.shapes.push_back({});
            for (const GeneralizedTrapezoid& trapezoid: item_shape_trapezoids)
                block_trapezoid_set.shapes.back().push_back(shifted(trapezoid, item.x, item.y));
            shapes_item_pos.push_back(item_pos);
        }
        for (const auto& item_shape_trapezoids: trapezoid_set.shapes_inflated) {
            block_trapezoid_set.shapes_inflated.push_back({});
            for (const GeneralizedTrapezoid& trapezoid: item_shape_trapezoids)
                block_trapezoid_set.shapes_inflated.back().push_back(shifted(trapezoid, item.x, item.y));
        }
        block_trapezoid_set.x_min = (std::min)(block_trapezoid_set.x_min, item.x + trapezoid_set.x_min);
        block_trapezoid_set.x_max = (std::max)(block_trapezoid_set.x_max, item.x + trapezoid_set.x_max);
        block_trapezoid_set.y_min = (std::min)(block_trapezoid_set.y_min, item.y + trapezoid_set.y_min);
        block_trapezoid_set.y_max = (std::max)(block_trapezoid_set.y_max, item.y + trapezoid_set.y_max);
        for (AxisAlignedBoundingBox rectangle: trapezoid_sets_rectangles_[item.trapezoid_set_id])
            block.rectangles.push_back(rectangle.shift(item.x, item.y));

        block.number_of_items++;
        block.item_area += item_type.area_scaled;
        block.profit += item_type.profit;
        bool found = false;
        for (std::pair<ItemTypeId, ItemPos>& item_copies: block.item_copies) {
            if (item_copies.first == trapezoid_set.item_type_id) {
                item_copies.second++;
                found = true;
                break;
            }
        }
        if (!found)
            block.item_copies.push_back({trapezoid_set.item_type_id, 1});
    }
    std::sort(block.item_copies.begin(), block.item_copies.end());

    AreaDbl core_rectangle_area = -1.0;
    for (const AxisAlignedBoundingBox& rectangle: block.rectangles) {
        AreaDbl area = (rectangle.x_max - rectangle.x_min) * (rectangle.y_max - rectangle.y_min);
        if (area > core_rectangle_area) {
            core_rectangle_area = area;
            block.core_rectangle = rectangle;
        }
    }

    // Return 'true' iff an item of the block other than 'item_pos' has a
    // trapezoid overlapping 'aabb' horizontally and above (or below) it.
    auto covered = [&block_trapezoid_set, &shapes_item_pos](
            ItemPos item_pos,
            const AxisAlignedBoundingBox& aabb,
            bool above)
    {
        for (ItemShapePos shape_pos = 0;
                shape_pos < (ItemShapePos)block_trapezoid_set.shapes.size();
                ++shape_pos) {
            if (shapes_item_pos[shape_pos] == item_pos)
                continue;
            for (const GeneralizedTrapezoid& trapezoid: block_trapezoid_set.shapes[shape_pos]) {
                if (!shape::strictly_lesser(trapezoid.x_min(), aabb.x_max)
                        || !shape::strictly_greater(trapezoid.x_max(), aabb.x_min)) {
                    continue;
                }
                if (above && !shape::strictly_lesser(trapezoid.y_bottom(), aabb.y_min))
                    return true;
                if (!above && !shape::strictly_greater(trapezoid.y_top(), aabb.y_max))
                    return true;
            }
        }
        return false;
    };

    // Supporting parts: the ones not covered by another item of the block.
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items.size(); ++item_pos) {
        const BlockItem& item = items[item_pos];
        const TrapezoidSet& trapezoid_set = trapezoid_sets_[item.trapezoid_set_id];
        for (ShapePos supporting_part_pos: trapezoid_set.supporting_parts) {
            Support support = supporting_parts_[supporting_part_pos];
            support.shape.shift(item.x, item.y);
            support.aabb = support.shape.compute_min_max();
            if (covered(item_pos, support.aabb, true))
                continue;
            block_trapezoid_set.supporting_parts.push_back(block.supporting_parts.size());
            block.supporting_parts.push_back(support);
        }
    }

    // Supported parts: the ones of the lowest item which are not covered by
    // another item of the block. Resting the block on the supported parts of
    // its other items would mostly lead to the same positions.
    ItemPos lowest_item_pos = -1;
    for (ItemPos item_pos = 0; item_pos < (ItemPos)items.size(); ++item_pos) {
        const BlockItem& item = items[item_pos];
        const TrapezoidSet& trapezoid_set = trapezoid_sets_[item.trapezoid_set_id];
        if (lowest_item_pos == -1) {
            lowest_item_pos = item_pos;
            continue;
        }
        const BlockItem& lowest_item = items[lowest_item_pos];
        const TrapezoidSet& lowest_trapezoid_set = trapezoid_sets_[lowest_item.trapezoid_set_id];
        LengthDbl y = item.y + trapezoid_set.y_min;
        LengthDbl y_lowest = lowest_item.y + lowest_trapezoid_set.y_min;
        if (shape::strictly_lesser(y, y_lowest)
                || (shape::equal(y, y_lowest)
                    && shape::strictly_lesser(
                        item.x + trapezoid_set.x_min,
                        lowest_item.x + lowest_trapezoid_set.x_min))) {
            lowest_item_pos = item_pos;
        }
    }
    {
        const BlockItem& item = items[lowest_item_pos];
        const TrapezoidSet& trapezoid_set = trapezoid_sets_[item.trapezoid_set_id];
        for (ShapePos supported_part_pos: trapezoid_set.supported_parts) {
            Support support = supported_parts_[supported_part_pos];
            support.shape.shift(item.x, item.y);
            support.aabb = support.shape.compute_min_max();
            if (covered(lowest_item_pos, support.aabb, false))
                continue;
            block_trapezoid_set.supported_parts.push_back(block.supported_parts.size());
            block.supported_parts.push_back(support);
        }
    }

    return block;
}

void BranchingSchemeMaximalSpaces::compute_blocks()
{
    // Single items.
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)bin_type_.trapezoid_sets.size();
            ++trapezoid_set_id) {
        BlockItem item;
        item.trapezoid_set_id = trapezoid_set_id;
        blocks_.push_back(build_block({item}));
    }

    if (!parameters_.use_periodic_blocks)
        return;

    // Periodic blocks. Only the periodic packings already computed in the
    // instance are used. The blocks aligning the bounding boxes of the items
    // on a grid are skipped: on the actual geometry, packing the items one
    // by one does at least as well.
    BinTypeId bin_type_id = instance().bin_type_id(0);
    std::vector<PeriodicBlock> periodic_blocks = compute_periodic_blocks(
            instance(),
            instance().periodic_packings(),
            compute_item_type_rotations(instance())[bin_type_id]);
    for (const PeriodicBlock& periodic_block: periodic_blocks) {
        if (periodic_block.packing_id < 0)
            continue;
        if (periodic_block.number_of_items <= 1)
            continue;
        std::vector<BlockItem> items;
        for (const SolutionItem& solution_item: periodic_block.items) {
            BlockItem item;
            item.trapezoid_set_id = trapezoid_set_id(
                    solution_item.item_type_id,
                    solution_item.angle,
                    solution_item.mirror);
            Point position = convert_point(solution_item.bl_corner, parameters_.direction);
            item.x = position.x;
            item.y = position.y;
            items.push_back(item);
        }
        Block block = build_block(items);
        if (block.supported_parts.empty())
            continue;
        blocks_.push_back(block);
    }
}

const std::shared_ptr<BranchingSchemeMaximalSpaces::Node> BranchingSchemeMaximalSpaces::root() const
{
    auto node = std::make_shared<Node>();
    node->id = node_id_++;
    node->item_number_of_copies.assign(instance().number_of_item_types(), 0);

    for (const DefectTrapezoid& defect: bin_type_.defects)
        node->trapezoids.push_back(defect.trapezoid);

    EmptySpace space;
    space.bl_corner = {bin_aabb_.x_min, bin_aabb_.y_min};
    space.bx = bin_aabb_.x_max - bin_aabb_.x_min;
    space.by = bin_aabb_.y_max - bin_aabb_.y_min;
    node->empty_spaces.push_back(space);
    for (const AxisAlignedBoundingBox& rectangle: obstacles_rectangles_) {
        cut_empty_spaces(
                node->empty_spaces,
                {rectangle.x_min, rectangle.y_min},
                rectangle.x_max - rectangle.x_min,
                rectangle.y_max - rectangle.y_min);
    }

    // The fixed items are part of every solution: they consume copies of
    // their item types.
    const BinType& bin_type = instance().bin_type(instance().bin_type_id(0));
    for (const FixedItem& fixed_item: bin_type.fixed_items) {
        const ItemType& item_type = instance().item_type(fixed_item.item_type_id);
        node->item_number_of_copies[fixed_item.item_type_id]++;
        node->item_area += item_type.area_scaled;
        node->number_of_items++;
        node->profit += item_type.profit;
    }

    for (ItemPos block_id = 0; block_id < (ItemPos)blocks_.size(); ++block_id) {
        bool feasible = true;
        for (const std::pair<ItemTypeId, ItemPos>& item_copies: blocks_[block_id].item_copies) {
            if (node->item_number_of_copies[item_copies.first] + item_copies.second
                    > instance().item_type(item_copies.first).copies) {
                feasible = false;
                break;
            }
        }
        if (feasible)
            node->valid_block_ids.push_back(block_id);
    }
    remove_unusable_spaces(*node);
    node->greedy_value = node->profit;
    return node;
}

ItemPos BranchingSchemeMaximalSpaces::select_empty_space(const Node& node) const
{
    ItemPos best_space_pos = -1;
    LengthDbl best_distance = std::numeric_limits<LengthDbl>::infinity();
    AreaDbl best_area = 0.0;
    for (ItemPos space_pos = 0;
            space_pos < (ItemPos)node.empty_spaces.size();
            ++space_pos) {
        const EmptySpace& space = node.empty_spaces[space_pos];
        LengthDbl distance = (space.xs() - bin_aabb_.x_min) + (space.ys() - bin_aabb_.y_min);
        AreaDbl area = space.area();
        if (shape::strictly_lesser(distance, best_distance)
                || (shape::equal(distance, best_distance) && shape::strictly_greater(area, best_area))) {
            best_space_pos = space_pos;
            best_distance = distance;
            best_area = area;
        }
    }
    return best_space_pos;
}

bool BranchingSchemeMaximalSpaces::may_fit(
        const Block& block,
        const EmptySpace& space) const
{
    // The block may stick out of the empty space, so this is only a rough
    // necessary condition: its largest rectangle must fit in the empty
    // space, up to the approximation of the rectangles.
    double factor = 1.0 + parameters_.rectangle_approximation_ratio;
    return !shape::strictly_greater(
            block.core_rectangle.x_max - block.core_rectangle.x_min,
            space.bx * factor)
        && !shape::strictly_greater(
            block.core_rectangle.y_max - block.core_rectangle.y_min,
            space.by * factor)
        && !shape::strictly_greater(
            block.trapezoid_set.x_max - block.trapezoid_set.x_min,
            bin_aabb_.x_max - bin_aabb_.x_min)
        && !shape::strictly_greater(
            block.trapezoid_set.y_max - block.trapezoid_set.y_min,
            bin_aabb_.y_max - bin_aabb_.y_min);
}

std::vector<BranchingSchemeMaximalSpaces::Insertion> BranchingSchemeMaximalSpaces::insertions(
        Node& node,
        bool only_best) const
{
    std::vector<Insertion> insertions;
    while (insertions.empty() && !node.empty_spaces.empty()) {
        ItemPos space_pos = select_empty_space(node);
        EmptySpace space = node.empty_spaces[space_pos];

        // Blocks which may fit in the empty space.
        std::vector<ItemPos> block_ids;
        LengthDbl blocks_width = 0.0;
        LengthDbl blocks_height = 0.0;
        for (ItemPos block_id: node.valid_block_ids) {
            const Block& block = blocks_[block_id];
            if (!may_fit(block, space))
                continue;
            block_ids.push_back(block_id);
            blocks_width = (std::max)(blocks_width, block.trapezoid_set.x_max - block.trapezoid_set.x_min);
            blocks_height = (std::max)(blocks_height, block.trapezoid_set.y_max - block.trapezoid_set.y_min);
        }

        // Supporting parts touching the bottom side of the empty space.
        std::vector<Shape> supporting_shapes;
        auto touches_bottom = [&space](const AxisAlignedBoundingBox& aabb)
        {
            return shape::strictly_lesser(aabb.x_min, space.xe())
                && shape::strictly_greater(aabb.x_max, space.xs())
                && !shape::strictly_greater(aabb.y_min, space.ys())
                && !shape::strictly_lesser(aabb.y_max, space.ys());
        };
        if (!block_ids.empty()) {
            for (ShapePos supporting_part_pos: bin_type_.bin_supporting_part_ids) {
                const Support& support = bin_type_.supporting_parts[supporting_part_pos];
                if (touches_bottom(support.aabb))
                    supporting_shapes.push_back(support.shape);
            }
            for (const Node::PlacedBlock& placed_block: node.placed_blocks) {
                const Block& block = blocks_[placed_block.block_id];
                AxisAlignedBoundingBox block_aabb;
                block_aabb.x_min = placed_block.x + block.trapezoid_set.x_min;
                block_aabb.x_max = placed_block.x + block.trapezoid_set.x_max;
                block_aabb.y_min = placed_block.y + block.trapezoid_set.y_min;
                block_aabb.y_max = placed_block.y + block.trapezoid_set.y_max;
                if (!touches_bottom(block_aabb))
                    continue;
                for (const Support& support: block.supporting_parts) {
                    AxisAlignedBoundingBox support_aabb = support.aabb;
                    support_aabb.shift(placed_block.x, placed_block.y);
                    if (!touches_bottom(support_aabb))
                        continue;
                    supporting_shapes.push_back(support.shape);
                    supporting_shapes.back().shift(placed_block.x, placed_block.y);
                }
            }
        }

        // For each supporting part, the trapezoids a block resting on it
        // may overlap.
        LengthDbl x_wall_min = (std::max)(bin_aabb_.x_min, space.xs() - blocks_width);
        std::vector<std::vector<GeneralizedTrapezoid>> supporting_shapes_obstacles;
        for (const Shape& supporting_shape: supporting_shapes) {
            AxisAlignedBoundingBox aabb = supporting_shape.compute_min_max();
            supporting_shapes_obstacles.push_back({});
            for (const GeneralizedTrapezoid& trapezoid: node.trapezoids) {
                if (!shape::strictly_greater(trapezoid.x_max(), x_wall_min))
                    continue;
                if (!shape::strictly_greater(trapezoid.y_top(), aabb.y_min - blocks_height))
                    continue;
                if (!shape::strictly_lesser(trapezoid.y_bottom(), aabb.y_max + blocks_height))
                    continue;
                supporting_shapes_obstacles.back().push_back(trapezoid);
            }
        }

        LengthDbl bin_width = bin_type_.x_max - bin_type_.x_min;
        for (ItemPos block_id: block_ids) {
            if (parameters_.timer.needs_to_end())
                break;
            const Block& block = blocks_[block_id];
            const TrapezoidSet& block_trapezoid_set = block.trapezoid_set;
            LengthDbl block_width = block_trapezoid_set.x_max - block_trapezoid_set.x_min;
            LengthDbl block_height = block_trapezoid_set.y_max - block_trapezoid_set.y_min;
            LengthDbl x_wall = (std::max)(bin_aabb_.x_min, space.xs() - block_width);

            Insertion best_insertion;
            for (ShapePos supporting_shape_pos = 0;
                    supporting_shape_pos < (ShapePos)supporting_shapes.size();
                    ++supporting_shape_pos) {
                const Shape& supporting_shape = supporting_shapes[supporting_shape_pos];
                for (const Support& supported_part: block.supported_parts) {
                    TrapezoidSetSlider slider(
                            block_trapezoid_set,
                            supporting_shape,
                            supported_part.shape,
                            bin_aabb_.x_max);
                    SlidePosition position = slider.initial_position();
                    if (slider.out_of_bounds(position))
                        continue;
                    if (!slider.slide_right_of(
                                position,
                                x_wall,
                                x_wall - bin_width,
                                bin_type_.y_min,
                                bin_type_.y_max)) {
                        continue;
                    }
                    if (!slider.slide(
                                position,
                                supporting_shapes_obstacles[supporting_shape_pos])) {
                        continue;
                    }
                    if (slider.out_of_bounds(position))
                        continue;

                    // The block must overlap the empty space.
                    Point bl_corner = {
                        position.x + block_trapezoid_set.x_min,
                        position.y + block_trapezoid_set.y_min};
                    if (!overlaps(space, bl_corner, block_width, block_height))
                        continue;

                    // Score.
                    LengthDbl x_min = (std::min)(space.xs(), bl_corner.x);
                    LengthDbl y_min = (std::min)(space.ys(), bl_corner.y);
                    AreaDbl area = (bl_corner.x + block_width - x_min)
                        * (bl_corner.y + block_height - y_min);
                    double density = (std::min)(1.0, block.item_area / area);
                    double guide = block.profit
                        * std::pow(density, parameters_.beta)
                        * std::pow(block.number_of_items, -parameters_.gamma);
                    if (best_insertion.block_id == -1
                            || guide > best_insertion.guide) {
                        best_insertion.block_id = block_id;
                        best_insertion.x = position.x;
                        best_insertion.y = position.y;
                        best_insertion.guide = guide;
                    }
                }
            }
            if (best_insertion.block_id == -1)
                continue;
            if (only_best && !insertions.empty()) {
                if (best_insertion.guide > insertions.front().guide)
                    insertions.front() = best_insertion;
            } else {
                insertions.push_back(best_insertion);
            }
        }

        if (parameters_.timer.needs_to_end())
            break;

        // No block can be inserted in this empty space now, so none can be
        // later either: remove it.
        if (insertions.empty()) {
            node.empty_spaces[space_pos] = node.empty_spaces.back();
            node.empty_spaces.pop_back();
        }
    }

    std::stable_sort(
            insertions.begin(),
            insertions.end(),
            [](const Insertion& insertion_1, const Insertion& insertion_2)
            {
                return insertion_1.guide > insertion_2.guide;
            });
    return insertions;
}

void BranchingSchemeMaximalSpaces::apply_insertion(
        Node& node,
        const Insertion& insertion) const
{
    node.id = node_id_++;
    node.cached_insertions.clear();
    node.next_child_pos = 0;

    const Block& block = blocks_[insertion.block_id];

    Node::PlacedBlock placed_block;
    placed_block.block_id = insertion.block_id;
    placed_block.x = insertion.x;
    placed_block.y = insertion.y;
    node.placed_blocks.push_back(placed_block);

    for (const auto& item_shape_trapezoids: block.trapezoid_set.shapes_inflated)
        for (const GeneralizedTrapezoid& trapezoid: item_shape_trapezoids)
            node.trapezoids.push_back(shifted(trapezoid, insertion.x, insertion.y));

    // Update the number of copies and the valid blocks.
    for (const std::pair<ItemTypeId, ItemPos>& item_copies: block.item_copies)
        node.item_number_of_copies[item_copies.first] += item_copies.second;
    for (ItemPos block_pos = 0; block_pos < (ItemPos)node.valid_block_ids.size(); ) {
        const Block& candidate_block = blocks_[node.valid_block_ids[block_pos]];
        bool feasible = true;
        for (const std::pair<ItemTypeId, ItemPos>& item_copies: candidate_block.item_copies) {
            if (node.item_number_of_copies[item_copies.first] + item_copies.second
                    > instance().item_type(item_copies.first).copies) {
                feasible = false;
                break;
            }
        }
        if (feasible) {
            ++block_pos;
        } else {
            node.valid_block_ids[block_pos] = node.valid_block_ids.back();
            node.valid_block_ids.pop_back();
        }
    }

    node.item_area += block.item_area;
    node.number_of_items += block.number_of_items;
    node.number_of_blocks++;
    node.profit += block.profit;

    // Update the empty spaces. The new ones too small for all the valid
    // blocks are discarded (see 'may_fit').
    std::vector<AxisAlignedBoundingBox> rectangles = block.rectangles;
    for (AxisAlignedBoundingBox& rectangle: rectangles)
        rectangle.shift(insertion.x, insertion.y);
    LengthDbl minimum_width = std::numeric_limits<LengthDbl>::infinity();
    LengthDbl minimum_height = std::numeric_limits<LengthDbl>::infinity();
    for (ItemPos block_id: node.valid_block_ids) {
        const AxisAlignedBoundingBox& core_rectangle = blocks_[block_id].core_rectangle;
        minimum_width = (std::min)(minimum_width, core_rectangle.x_max - core_rectangle.x_min);
        minimum_height = (std::min)(minimum_height, core_rectangle.y_max - core_rectangle.y_min);
    }
    double factor = 1.0 + parameters_.rectangle_approximation_ratio;
    cut_empty_spaces(
            node.empty_spaces,
            rectangles,
            minimum_width / factor,
            minimum_height / factor);
    remove_unusable_spaces(node);
}

void BranchingSchemeMaximalSpaces::remove_unusable_spaces(Node& node) const
{
    for (ItemPos space_pos = 0; space_pos < (ItemPos)node.empty_spaces.size(); ) {
        const EmptySpace& space = node.empty_spaces[space_pos];
        bool usable = false;
        for (ItemPos block_id: node.valid_block_ids) {
            if (may_fit(blocks_[block_id], space)) {
                usable = true;
                break;
            }
        }
        if (usable) {
            ++space_pos;
        } else {
            node.empty_spaces[space_pos] = node.empty_spaces.back();
            node.empty_spaces.pop_back();
        }
    }
}

void BranchingSchemeMaximalSpaces::complete_greedily(Node& node) const
{
    for (;;) {
        if (parameters_.timer.needs_to_end())
            break;
        std::vector<Insertion> best_insertions = insertions(node, true);
        if (best_insertions.empty())
            break;
        apply_insertion(node, best_insertions.front());
    }
}

std::shared_ptr<BranchingSchemeMaximalSpaces::Node> BranchingSchemeMaximalSpaces::next_child(
        const std::shared_ptr<Node>& parent) const
{
    if (parent->next_child_pos == 0)
        parent->cached_insertions = insertions(*parent, false);
    NodeId pos = parent->next_child_pos;
    parent->next_child_pos++;
    if (pos >= (NodeId)parent->cached_insertions.size())
        return nullptr;
    Insertion insertion = parent->cached_insertions[pos];

    // Don't copy the insertions of the parent.
    std::vector<Insertion> saved_insertions;
    std::swap(parent->cached_insertions, saved_insertions);
    auto node = std::make_shared<Node>(*parent);
    std::swap(parent->cached_insertions, saved_insertions);

    apply_insertion(*node, insertion);
    Node greedy_node = *node;
    complete_greedily(greedy_node);
    node->greedy_value = greedy_node.profit;
    return node;
}

bool BranchingSchemeMaximalSpaces::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::Knapsack: case Objective::Feasibility: {
        return node_2->greedy_value < node_1->greedy_value;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "branching scheme 'irregular::BranchingSchemeMaximalSpaces' "
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingSchemeMaximalSpaces::bound(
        const std::shared_ptr<Node>&,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::Knapsack: case Objective::Feasibility: {
        return leaf(node_2);
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "branching scheme 'irregular::BranchingSchemeMaximalSpaces' "
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

Solution BranchingSchemeMaximalSpaces::to_solution(
        const std::shared_ptr<Node>& node) const
{
    Node greedy_node = *node;
    complete_greedily(greedy_node);

    SolutionBuilder solution_builder(instance());
    BinTypeId bin_type_id = instance().bin_type_id(0);
    BinPos bin_pos = solution_builder.add_bin(bin_type_id, 1);
    double scale = 1.0 / instance().parameters().scale_value;
    for (const Node::PlacedBlock& placed_block: greedy_node.placed_blocks) {
        const Block& block = blocks_[placed_block.block_id];
        for (const BlockItem& item: block.items) {
            const TrapezoidSet& trapezoid_set = trapezoid_sets_[item.trapezoid_set_id];
            Point bl_corner = convert_point_back(
                    {placed_block.x + item.x, placed_block.y + item.y},
                    parameters_.direction);
            solution_builder.add_item(
                    bin_pos,
                    trapezoid_set.item_type_id,
                    scale * bl_corner,
                    trapezoid_set.angle,
                    trapezoid_set.mirror);
        }
    }

    const BinType& bin_type = instance().bin_type(bin_type_id);
    for (const FixedItem& fixed_item: bin_type.fixed_items) {
        solution_builder.add_item(
                bin_pos,
                fixed_item.item_type_id,
                fixed_item.bl_corner,
                fixed_item.angle,
                fixed_item.mirror,
                true);
    }

    return solution_builder.build();
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////// tree_search_maximal_spaces /////////////////////////
////////////////////////////////////////////////////////////////////////////////

const packingsolver::irregular::TreeSearchMaximalSpacesOutput packingsolver::irregular::tree_search_maximal_spaces(
        const Instance& instance,
        const TreeSearchMaximalSpacesParameters& parameters)
{
    if (instance.number_of_bins() > 1) {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "algorithm 'irregular::tree_search_maximal_spaces' "
            << "does not support instances with more than one bin.";
        throw std::logic_error(ss.str());
    }
    if (instance.objective() == Objective::VariableSizedBinPacking) {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "algorithm 'irregular::tree_search_maximal_spaces' "
            << "does not support objective '" << instance.objective() << "'.";
        throw std::logic_error(ss.str());
    }

    TreeSearchMaximalSpacesOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    if (instance.objective() == Objective::BinPacking
            || instance.objective() == Objective::BinPackingWithLeftovers
            || instance.objective() == Objective::OpenDimensionX
            || instance.objective() == Objective::OpenDimensionY
            || instance.objective() == Objective::OpenDimensionXY) {
        SequentialFeasibilitySolver solver = [&parameters, &algorithm_formatter](
                const Instance& sub_instance,
                const Solution*)
        {
            TreeSearchMaximalSpacesParameters inner_parameters;
            inner_parameters.verbosity_level = 0;
            inner_parameters.timer = parameters.timer;
            inner_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            inner_parameters.optimization_mode = parameters.optimization_mode;
            inner_parameters.maximum_approximation_ratio = parameters.maximum_approximation_ratio;
            inner_parameters.not_anytime_tree_search_queue_size
                = parameters.not_anytime_tree_search_queue_size;
            return tree_search_maximal_spaces(sub_instance, inner_parameters).solution_pool;
        };

        SequentialFeasibilityParameters sf_parameters;
        sf_parameters.verbosity_level = 0;
        sf_parameters.timer = parameters.timer;
        sf_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
        sf_parameters.new_solution_callback = [&algorithm_formatter](
                const packingsolver::Output<Instance, Solution>& sf_output)
        {
            algorithm_formatter.update_solution(
                    sf_output.solution_pool.best(),
                    sf_output.solution_pool.best_label());
        };

        sequential_feasibility(instance, solver, sf_parameters);
        algorithm_formatter.end();
        return output;
    }

    std::vector<Direction> directions = {
        Direction::LeftToRightThenBottomToTop,
        Direction::BottomToTopThenLeftToRight,
    };

    // In 'NotAnytimeDeterministic' mode, each scheme reports to its own
    // algorithm formatter, writing into its own output, instead of the shared
    // 'algorithm_formatter', so that the order in which solutions reach the
    // shared one stays deterministic (they are replayed at the end).
    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    packingsolver::Parameters<Instance, Solution, Output> local_parameters;
    local_parameters.verbosity_level = 0;
    local_parameters.timer = parameters.timer;
    std::vector<std::unique_ptr<Output>> local_outputs;
    std::vector<std::unique_ptr<AlgorithmFormatter>> local_algorithm_formatters;

    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Direction direction: directions) {
        AlgorithmFormatter* scheme_algorithm_formatter = &algorithm_formatter;
        if (deterministic) {
            local_outputs.push_back(std::unique_ptr<Output>(new Output(instance)));
            local_algorithm_formatters.push_back(std::unique_ptr<AlgorithmFormatter>(new AlgorithmFormatter(
                            instance,
                            local_parameters,
                            *local_outputs.back())));
            scheme_algorithm_formatter = local_algorithm_formatters.back().get();
        }
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        tasks.push_back([&exception_ptr, &instance, &parameters,
                &algorithm_formatter, scheme_algorithm_formatter, direction]()
        {
            try {
                treesearchsolver::IterativeBeamSearchParameters<BranchingSchemeMaximalSpaces> ibs_parameters;
                ibs_parameters.verbosity_level = 0;
                ibs_parameters.timer = parameters.timer;
                ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                ibs_parameters.timer.add_end_boolean(&scheme_algorithm_formatter->end_boolean());
                ibs_parameters.global_history = true;
                if (parameters.optimization_mode != OptimizationMode::Anytime) {
                    ibs_parameters.minimum_size_of_the_queue = 1;
                    ibs_parameters.growth_factor = parameters.not_anytime_tree_search_queue_size;
                    ibs_parameters.maximum_size_of_the_queue = parameters.not_anytime_tree_search_queue_size;
                }

                BranchingSchemeMaximalSpaces::Parameters branching_scheme_parameters;
                branching_scheme_parameters.direction = direction;
                branching_scheme_parameters.maximum_approximation_ratio = parameters.maximum_approximation_ratio;
                branching_scheme_parameters.timer = ibs_parameters.timer;
                BranchingSchemeMaximalSpaces branching_scheme(instance, branching_scheme_parameters);

                ibs_parameters.new_solution_callback = [scheme_algorithm_formatter, &branching_scheme, direction](
                        const treesearchsolver::Output<BranchingSchemeMaximalSpaces>& tss_output)
                {
                    const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemeMaximalSpaces>& tssibs_output
                        = static_cast<const treesearchsolver::IterativeBeamSearchOutput<BranchingSchemeMaximalSpaces>&>(tss_output);
                    Solution solution = branching_scheme.to_solution(
                            tssibs_output.solution_pool.best());
                    std::stringstream ss;
                    ss << "d " << (int)direction
                        << " q " << tssibs_output.maximum_size_of_the_queue;
                    scheme_algorithm_formatter->update_solution(solution, ss.str());
                };
                treesearchsolver::iterative_beam_search<BranchingSchemeMaximalSpaces>(
                        branching_scheme,
                        ibs_parameters);
            } catch (...) {
                exception_ptr = std::current_exception();
            }
        });
    }
    run(tasks, parameters.optimization_mode != OptimizationMode::NotAnytimeSequential);
    for (const std::exception_ptr& exception_ptr: exception_ptr_list)
        if (exception_ptr)
            std::rethrow_exception(exception_ptr);
    if (deterministic) {
        for (const auto& local_output: local_outputs) {
            algorithm_formatter.update_solution(
                    local_output->solution_pool.best(),
                    local_output->solution_pool.best_label());
        }
    }

    algorithm_formatter.end();
    return output;
}
