#include "irregular/tree_search.hpp"
#include "irregular/sequential_feasibility.hpp"
#include "irregular/solution_builder.hpp"

#include "packingsolver/irregular/algorithm_formatter.hpp"
#include "algorithms/thread_pool.hpp"
#include "treesearchsolver/iterative_beam_search_2.hpp"

#include "irregular/rotations.hpp"
#include "irregular/shape_simplification.hpp"

#include "shape/convex_hull.hpp"
#include "shape/clean.hpp"
#include "shape/trapezoidation.hpp"
#include "shape/supports.hpp"
#include "shape/shapes_intersections.hpp"

#include <algorithm>
#include <iostream>
#include <numeric>

using namespace packingsolver;
using namespace packingsolver::irregular;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

/**
 * Return false only if two boxes are far enough apart for the elements they
 * contain to not intersect.
 *
 * 'shape::intersect' considers that two elements intersect if they are
 * within its tolerance (1e-6) of each other; the margin is much larger, so
 * that no intersection is missed.
 */
bool aabbs_may_intersect(
        const AxisAlignedBoundingBox& aabb_1,
        const AxisAlignedBoundingBox& aabb_2)
{
    const LengthDbl margin = 1e-4;
    return aabb_1.x_max + margin >= aabb_2.x_min
        && aabb_2.x_max + margin >= aabb_1.x_min
        && aabb_1.y_max + margin >= aabb_2.y_min
        && aabb_2.y_max + margin >= aabb_1.y_min;
}

Direction default_direction(
        const Instance& instance,
        BinTypeId bin_type_id)
{
    const BinType& bin_type = instance.bin_type(bin_type_id);
    bool lengthwise = (bin_type.aabb_scaled.x_max - bin_type.aabb_scaled.x_min) >= (bin_type.aabb_scaled.y_max - bin_type.aabb_scaled.y_min);
    switch (instance.parameters().leftover_mode) {
    case LeftoverMode::BottomLeft: {
        return (lengthwise)?
            Direction::LeftToRightThenBottomToTop:
            Direction::BottomToTopThenLeftToRight;
    } case LeftoverMode::BottomRight: {
        return (lengthwise)?
            Direction::RightToLeftThenBottomToTop:
            Direction::BottomToTopThenRightToLeft;
    } case LeftoverMode::TopLeft: {
        return (lengthwise)?
            Direction::LeftToRightThenTopToBottom:
            Direction::TopToBottomThenLeftToRight;
    } case LeftoverMode::TopRight: {
        return (lengthwise)?
            Direction::RightToLeftThenTopToBottom:
            Direction::TopToBottomThenRightToLeft;
    } case LeftoverMode::Left: {
        return Direction::LeftToRightThenBottomToTop;
    } case LeftoverMode::Right: {
        return Direction::RightToLeftThenBottomToTop;
    } case LeftoverMode::Bottom: {
        return Direction::BottomToTopThenLeftToRight;
    } case LeftoverMode::Top: {
        return Direction::TopToBottomThenLeftToRight;
    }
    }
    return Direction::LeftToRightThenBottomToTop;
}

}

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    simplified_instance_(shape_simplification(
            instance,
            parameters.maximum_approximation_ratio)),
    parameters_(parameters)
{
    // Compute bin_type_ids_ and previous_bins_area_.
    //
    // For the Knapsack objective, bins are considered by increasing space,
    // regardless of the order in which they appear in the instance.
    // Otherwise, bins are considered in the order in which they appear in
    // the instance.
    {
        std::vector<BinTypeId> bin_type_id_order(instance.number_of_bin_types());
        std::iota(bin_type_id_order.begin(), bin_type_id_order.end(), 0);
        if (instance.objective() == Objective::Knapsack) {
            std::stable_sort(
                    bin_type_id_order.begin(),
                    bin_type_id_order.end(),
                    [&instance](BinTypeId bin_type_id_1, BinTypeId bin_type_id_2)
                    {
                        return instance.bin_type(bin_type_id_1).space()
                            < instance.bin_type(bin_type_id_2).space();
                    });
        }
        AreaDbl previous_bin_area = 0;
        for (BinTypeId bin_type_id: bin_type_id_order) {
            const BinType& bin_type = instance.bin_type(bin_type_id);
            for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
                bin_type_ids_.push_back(bin_type_id);
                previous_bins_area_.push_back(previous_bin_area);
                previous_bin_area += bin_type.area_orig;
            }
        }
    }

    item_types_allowed_rotations_ = compute_item_type_rotations(instance);

    //std::cout << "BranchingScheme" << std::endl;

    for (Direction direction: {
            Direction::LeftToRightThenBottomToTop,
            Direction::LeftToRightThenTopToBottom,
            Direction::RightToLeftThenBottomToTop,
            Direction::RightToLeftThenTopToBottom,
            Direction::BottomToTopThenLeftToRight,
            Direction::BottomToTopThenRightToLeft,
            Direction::TopToBottomThenLeftToRight,
            Direction::TopToBottomThenRightToLeft}) {
        DirectionData& direction_data = directions_data_[(int)direction];

        if (parameters_.direction != Direction::Any && parameters_.direction != direction)
            continue;

        direction_data.bin_types.clear();
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            direction_data.bin_types.push_back(build_trapezoid_bin_type(
                        instance,
                        simplified_instance_,
                        item_types_allowed_rotations_[bin_type_id],
                        bin_type_id,
                        direction));
        }
    }

    //for (TrapezoidSetId trapezoid_set_id = 0;
    //        trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_[(int)Direction::LeftToRightThenBottomToTop].size();
    //        ++trapezoid_set_id) {
    //    const TrapezoidSet& trapezoid_set = trapezoid_sets_[(int)Direction::LeftToRightThenBottomToTop][trapezoid_set_id];
    //    const ItemType& item_type = instance.item_type(trapezoid_set.item_type_id);
    //    std::cout << "item_type_id " << trapezoid_set.item_type_id
    //        << " angle " << trapezoid_set.angle
    //        << " mirror " << trapezoid_set.mirror
    //        << " x_min " << item_type.compute_min_max(trapezoid_set.angle).x_min
    //        << " x_max " << item_type.compute_min_max(trapezoid_set.angle).x_max
    //        << " y_min " << item_type.compute_min_max(trapezoid_set.angle).y_min
    //        << " y_max " << item_type.compute_min_max(trapezoid_set.angle).y_max
    //        << std::endl;
    //    std::cout << "trapezoid_set_id " << trapezoid_set_id << std::endl;
    //    std::cout << "item_type_id " << trapezoid_set.item_type_id
    //        << " angle " << trapezoid_set.angle << std::endl;
    //    std::cout << "x_min " << trapezoid_set.x_min
    //        << " x_max " << trapezoid_set.x_max << std::endl;
    //    std::cout << "y_min " << trapezoid_set.y_min
    //        << " y_max " << trapezoid_set.y_max << std::endl;

    //    // Loop through rectangles of the rectangle set.
    //    for (ItemShapePos item_shape_pos = 0;
    //            item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
    //            ++item_shape_pos) {
    //        const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
    //        std::cout << "shape " << item_shape_pos << " # trapezoids " << item_shape_trapezoids.size() << std::endl;
    //        for (TrapezoidPos item_shape_trapezoid_pos = 0;
    //                item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
    //                ++item_shape_trapezoid_pos) {
    //            std::cout << " - " << item_shape_trapezoids[item_shape_trapezoid_pos] << std::endl;
    //        }
    //    }
    //}

    item_types_convex_hull_area_
        = std::vector<AreaDbl>(instance.number_of_item_types(), 0.0);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        const SimplifiedItemType& simplified_item_type = simplified_instance_.item_types[item_type_id];
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            // 'convex_hull' only supports polygons: use the already-computed
            // 'simplified_instance_' shape instead of the original 'instance'
            // shape, which is always a polygon (any circular arc already
            // approximated by line segments - see 'shape_simplification()'),
            // rather than approximating it again here, and consistent with
            // the geometry the rest of the search (e.g. the trapezoid sets
            // below) actually branches over.
            const Shape& item_shape_polygon = simplified_item_type.shapes[item_shape_pos].shape.shape;
            Shape convex_hull = shape::convex_hull(item_shape_polygon);
            AreaDbl convex_hull_area = convex_hull.compute_area();
            item_types_convex_hull_area_[item_type_id] += convex_hull_area;
        }
        //std::cout << "item_type_id " << item_type_id
        //    << " area " << item_type.area
        //    << " convex_hull_area " << item_types_convex_hull_area_[item_type_id]
        //    << std::endl;
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::vector<BranchingScheme::UncoveredTrapezoid> BranchingScheme::add_trapezoid_to_skyline(
        const std::vector<UncoveredTrapezoid>& uncovered_trapezoids,
        ItemTypeId item_type_id,
        ItemShapePos item_shape_pos,
        ItemShapeTrapezoidPos item_shape_trapezoid_pos,
        const GeneralizedTrapezoid& new_trapezoid) const
{
    //std::cout << "add_trapezoid_to_skyline" << std::endl;
    //std::cout << "new_trapezoid " << new_trapezoid << std::endl;
    //std::cout << "uncovered_trapezoids_cur:" << std::endl;
    //for (const UncoveredTrapezoid& uncovered_trapezoid: uncovered_trapezoids)
    //    std::cout << "* " << uncovered_trapezoid << std::endl;

    std::vector<UncoveredTrapezoid> new_uncovered_trapezoids;

    // Reserve for uncovered_trapezoids and extra_trapezoids.
    new_uncovered_trapezoids.reserve(uncovered_trapezoids.size() + 3);
    LengthDbl ys = std::max(new_trapezoid.y_bottom(), uncovered_trapezoids.front().trapezoid.y_bottom());
    LengthDbl ye = std::min(new_trapezoid.y_top(), uncovered_trapezoids.back().trapezoid.y_top());
    //std::cout << "ys " << ys << " ye " << ye << std::endl;

    bool current_new_trapezoid = false;
    LengthDbl y_cur = 0.0;

    // Update uncovered_trapezoids.
    for (const BranchingScheme::UncoveredTrapezoid& uncovered_trapezoid: uncovered_trapezoids) {
        //std::cout << "uncovered_trapezoid " << uncovered_trapezoid << std::endl;
        if (uncovered_trapezoid.trapezoid.y_top() <= ys) {
            new_uncovered_trapezoids.push_back(uncovered_trapezoid);
        } else if (uncovered_trapezoid.trapezoid.y_bottom() >= ye) {
            new_uncovered_trapezoids.push_back(uncovered_trapezoid);
        } else {

            bool right_sides_intersect = false;
            LengthDbl y_intersection = 0.0;
            LengthDbl yb = (std::max)(uncovered_trapezoid.trapezoid.y_bottom(), new_trapezoid.y_bottom());
            LengthDbl yt = (std::min)(uncovered_trapezoid.trapezoid.y_top(), new_trapezoid.y_top());
            if (equal(uncovered_trapezoid.trapezoid.a_right(), new_trapezoid.a_right())) {
                right_sides_intersect = false;
            } else {
                double b1 = uncovered_trapezoid.trapezoid.x_top_right()
                    - uncovered_trapezoid.trapezoid.a_right()
                    * uncovered_trapezoid.trapezoid.y_top();
                double b2 = new_trapezoid.x_top_right()
                    - new_trapezoid.a_right()
                    * new_trapezoid.y_top();
                y_intersection = -(b1 - b2)
                    / (uncovered_trapezoid.trapezoid.a_right()
                            - new_trapezoid.a_right());
                if (strictly_greater(y_intersection, yb)
                        && strictly_lesser(y_intersection, yt)) {
                    right_sides_intersect = true;
                }
            }
            //std::cout << "right_sides_intersect " << right_sides_intersect << std::endl;
            //std::cout << "y_intersection " << y_intersection << std::endl;

            LengthDbl y1 = yb;
            LengthDbl y2 = yt;
            //std::cout << "uncovered_trapezoid.trapezoid.x_bottom_right() " << uncovered_trapezoid.trapezoid.x_bottom_right() << std::endl;
            //std::cout << "new_trapezoid.x_right(uncovered_trapezoid.trapezoid.y_bottom() " << new_trapezoid.x_right(uncovered_trapezoid.trapezoid.y_bottom()) << std::endl;
            if (strictly_greater(
                        uncovered_trapezoid.trapezoid.x_right(yb),
                        new_trapezoid.x_right(yb))
                    && (right_sides_intersect
                        || strictly_greater(
                            uncovered_trapezoid.trapezoid.x_right(yt),
                            new_trapezoid.x_right(yt)))) {
                if (!right_sides_intersect) {
                    y1 = yt;
                    if (y2 < y1)
                        y2 = y1;
                } else {
                    y1 = y_intersection;
                }
            }
            //std::cout << "uncovered_trapezoid.trapezoid.x_top_right() " << uncovered_trapezoid.trapezoid.x_top_right() << std::endl;
            //std::cout << "new_trapezoid.x_right(uncovered_trapezoid.trapezoid.y_top()) " << new_trapezoid.x_right(uncovered_trapezoid.trapezoid.y_top()) << std::endl;
            if (strictly_greater(
                        uncovered_trapezoid.trapezoid.x_right(yt),
                        new_trapezoid.x_right(yt))
                    && (right_sides_intersect
                        || strictly_greater(
                            uncovered_trapezoid.trapezoid.x_right(yb),
                            new_trapezoid.x_right(yb)))) {
                if (!right_sides_intersect) {
                    y2 = yb;
                    if (y1 > y2)
                        y1 = y2;
                } else {
                    y2 = y_intersection;
                }
            }
            //std::cout << "y1 " << y1 << " y2 " << y2 << std::endl;

            if (uncovered_trapezoid.trapezoid.y_bottom() < y1) {

                if (current_new_trapezoid) {
                    LengthDbl y_curr_top = uncovered_trapezoid.trapezoid.y_bottom();
                    UncoveredTrapezoid new_uncovered_trapezoid(
                            item_type_id,
                            item_shape_pos,
                            item_shape_trapezoid_pos,
                            GeneralizedTrapezoid(
                                y_cur,
                                y_curr_top,
                                new_trapezoid.x_left(y_cur),
                                new_trapezoid.x_right(y_cur),
                                new_trapezoid.x_left(y_curr_top),
                                new_trapezoid.x_right(y_curr_top)));
                    new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
                    current_new_trapezoid = false;
                }

                UncoveredTrapezoid new_uncovered_trapezoid(
                        uncovered_trapezoid.item_type_id,
                        uncovered_trapezoid.item_shape_pos,
                        uncovered_trapezoid.item_shape_trapezoid_pos,
                        GeneralizedTrapezoid(
                            uncovered_trapezoid.trapezoid.y_bottom(),
                            y1,
                            uncovered_trapezoid.trapezoid.x_bottom_left(),
                            uncovered_trapezoid.trapezoid.x_bottom_right(),
                            uncovered_trapezoid.trapezoid.x_left(y1),
                            uncovered_trapezoid.trapezoid.x_right(y1)));
                new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
            }

            if (y1 < y2) {
                if (!current_new_trapezoid)
                    y_cur = y1;
                current_new_trapezoid = true;

                if (y2 == new_trapezoid.y_top()) {
                    LengthDbl y_curr_top = y2;
                    UncoveredTrapezoid new_uncovered_trapezoid(
                            item_type_id,
                            item_shape_pos,
                            item_shape_trapezoid_pos,
                            GeneralizedTrapezoid(
                                y_cur,
                                y_curr_top,
                                new_trapezoid.x_left(y_cur),
                                new_trapezoid.x_right(y_cur),
                                new_trapezoid.x_left(y_curr_top),
                                new_trapezoid.x_right(y_curr_top)));
                    new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
                    current_new_trapezoid = false;
                }
            }

            if (y2 < uncovered_trapezoid.trapezoid.y_top()) {

                if (current_new_trapezoid) {
                    LengthDbl y_curr_top = y2;
                    UncoveredTrapezoid new_uncovered_trapezoid(
                            item_type_id,
                            item_shape_pos,
                            item_shape_trapezoid_pos,
                            GeneralizedTrapezoid(
                                y_cur,
                                y_curr_top,
                                new_trapezoid.x_left(y_cur),
                                new_trapezoid.x_right(y_cur),
                                new_trapezoid.x_left(y_curr_top),
                                new_trapezoid.x_right(y_curr_top)));
                    new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
                    current_new_trapezoid = false;
                }

                UncoveredTrapezoid new_uncovered_trapezoid(
                        uncovered_trapezoid.item_type_id,
                        uncovered_trapezoid.item_shape_pos,
                        uncovered_trapezoid.item_shape_trapezoid_pos,
                        GeneralizedTrapezoid(
                            y2,
                            uncovered_trapezoid.trapezoid.y_top(),
                            uncovered_trapezoid.trapezoid.x_left(y2),
                            uncovered_trapezoid.trapezoid.x_right(y2),
                            uncovered_trapezoid.trapezoid.x_top_left(),
                            uncovered_trapezoid.trapezoid.x_top_right()));
                new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
            }
        }
    }

    if (current_new_trapezoid) {
        LengthDbl y_curr_top = ye;
        UncoveredTrapezoid new_uncovered_trapezoid(
                item_type_id,
                item_shape_pos,
                item_shape_trapezoid_pos,
                GeneralizedTrapezoid(
                    y_cur,
                    y_curr_top,
                    new_trapezoid.x_left(y_cur),
                    new_trapezoid.x_right(y_cur),
                    new_trapezoid.x_left(y_curr_top),
                    new_trapezoid.x_right(y_curr_top)));
        new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
        current_new_trapezoid = false;
    }

    //std::cout << "uncovered_trapezoids:" << std::endl;
    //for (const UncoveredTrapezoid& uncovered_trapezoid: new_uncovered_trapezoids)
    //    std::cout << "* " << uncovered_trapezoid << std::endl;

    check_skyline(new_uncovered_trapezoids);
    return new_uncovered_trapezoids;
}

void BranchingScheme::check_skyline(
        const std::vector<UncoveredTrapezoid>& uncovered_trapezoids) const
{
    LengthDbl y_cur = uncovered_trapezoids.front().trapezoid.y_top();
    for (TrapezoidPos uncovered_trapezoid_pos = 1;
            uncovered_trapezoid_pos < (TrapezoidPos)uncovered_trapezoids.size();
            ++uncovered_trapezoid_pos) {
        const UncoveredTrapezoid& uncovered_trapezoid = uncovered_trapezoids[uncovered_trapezoid_pos];
        if (uncovered_trapezoid.trapezoid.y_bottom() != y_cur) {
            for (const UncoveredTrapezoid& uncovered_trapezoid: uncovered_trapezoids)
                std::cout << "* " << uncovered_trapezoid << std::endl;
            throw std::logic_error(
                    FUNC_SIGNATURE + "; "
                    " y_cur: " + std::to_string(y_cur)
                    + "; uncovered_trapezoid.trapezoid.y_bottom(): "
                    + std::to_string(uncovered_trapezoid.trapezoid.y_bottom())
                    + ".");
        }
        y_cur = uncovered_trapezoid.trapezoid.y_top();
    }
}

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    //std::cout << std::endl;
    //std::cout << "child_tmp"
    //    << " parent " << pparent->id
    //    << " insertion " << insertion << std::endl;

    const Node& parent = *pparent;
    Node node;

    node.parent = pparent;

    node.trapezoid_set_id = insertion.trapezoid_set_id;
    node.x = insertion.x;
    node.y = insertion.y;
    node.ys = insertion.ys;
    node.ye = insertion.ye;

    // Update number_of_bins and last_bin_direction.
    if (insertion.new_bin_direction != Direction::Any) {  // New bin.
        node.number_of_bins = insertion.new_bin_pos + 1;
        node.last_bin_direction = (Direction)insertion.new_bin_direction;
    } else {  // Same bin.
        node.number_of_bins = parent.number_of_bins;
        node.last_bin_direction = parent.last_bin_direction;
    }

    BinPos bin_pos = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = bin_type_ids_[bin_pos];
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const DirectionData& direction_data = directions_data_[(int)node.last_bin_direction];
    const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
    const TrapezoidSet& trapezoid_set = bb_bin_type.trapezoid_sets[insertion.trapezoid_set_id];
    const Support& supporting_part = bb_bin_type.supporting_parts[insertion.supporting_part_pos];
    Shape supporting_shape = supporting_part.shape;
    supporting_shape.shift(insertion.supporting_part_x, insertion.supporting_part_y);
    // Bounding boxes of the supporting shape and of its elements, to skip
    // the intersection tests which can't succeed.
    std::vector<AxisAlignedBoundingBox> supporting_shape_elements_aabbs;
    supporting_shape_elements_aabbs.reserve(supporting_shape.elements.size());
    AxisAlignedBoundingBox supporting_shape_aabb;
    for (const ShapeElement& supporting_shape_element: supporting_shape.elements) {
        supporting_shape_elements_aabbs.push_back(supporting_shape_element.min_max());
        supporting_shape_aabb = merge(
                supporting_shape_aabb,
                supporting_shape_elements_aabbs.back());
    }

    //std::cout << "shape_trapezoid " << shape_trapezoid << std::endl;

    // Reserve for uncovered_trapezoids and extra_trapezoids.
    ItemPos p = -1;
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)trapezoid_set.shapes_inflated.size();
            ++item_shape_pos) {
        const auto& item_shape_trapezoids = trapezoid_set.shapes_inflated[item_shape_pos];
        p += item_shape_trapezoids.size();
    }
    node.extra_trapezoids.reserve(parent.extra_trapezoids.size() + p);

    // Update uncovered_trapezoids.
    if (insertion.new_bin_direction != Direction::Any) {  // New bin.
        UncoveredTrapezoid uncovered_trapezoid(GeneralizedTrapezoid(
                    bb_bin_type.y_min,
                    bb_bin_type.y_max,
                    bb_bin_type.x_min,
                    bb_bin_type.x_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing,
                    bb_bin_type.x_min,
                    bb_bin_type.x_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing));
        node.uncovered_trapezoids.push_back(uncovered_trapezoid);
        node.all_trapezoids_skyline.push_back(uncovered_trapezoid);

        // Add defect trapezoids (including fixed items) for this bin type.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bb_bin_type.defects.size();
                ++defect_id) {
            //std::cout << "defect_id " << defect_id << " " << bb_bin_type.defects[defect_id] << std::endl;
            const DefectTrapezoid& defect = bb_bin_type.defects[defect_id];
            node.extra_trapezoids.push_back(UncoveredTrapezoid(defect.defect_id, defect.trapezoid));
        }

    } else {  // Same bin.

        node.uncovered_trapezoids = parent.uncovered_trapezoids;
        node.all_trapezoids_skyline = parent.all_trapezoids_skyline;

    }

    // Add trapezoids of inserted trapezoid set to uncovered and extra
    // trapezoids.
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)trapezoid_set.shapes_inflated.size();
            ++item_shape_pos) {
        const auto& item_shape_trapezoids = trapezoid_set.shapes_inflated[item_shape_pos];
        for (TrapezoidPos item_shape_trapezoid_pos = 0;
                item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                ++item_shape_trapezoid_pos) {
            //std::cout << "insertion " << insertion << std::endl;
            //std::cout << "add extra_trapezoid " << item_shape_trapezoids[item_shape_trapezoid_pos] << std::endl;
            GeneralizedTrapezoid trapezoid
                = bb_bin_type.trapezoid_sets[node.trapezoid_set_id].shapes_inflated[item_shape_pos][item_shape_trapezoid_pos];
            //std::cout << "inflate " << trapezoid << std::endl;
            trapezoid.shift_right(insertion.x);
            trapezoid.shift_top(insertion.y);
            //std::cout << "shifted " << trapezoid << std::endl;

            node.all_trapezoids_skyline = add_trapezoid_to_skyline(
                    node.all_trapezoids_skyline,
                    trapezoid_set.item_type_id,
                    item_shape_pos,
                    item_shape_trapezoid_pos,
                    trapezoid);

            if (!trapezoid.left_side_increasing_not_vertical()) {
                ShapeElement element_1;
                element_1.type = ShapeElementType::LineSegment;
                element_1.start = {trapezoid.x_top_left(), trapezoid.y_top()};
                element_1.end = {trapezoid.x_bottom_left(), trapezoid.y_bottom()};
                ShapeElement element_2;
                element_2.type = ShapeElementType::LineSegment;
                element_2.start = {trapezoid.x_bottom_left(), trapezoid.y_bottom()};
                element_2.end = {trapezoid.x_bottom_right(), trapezoid.y_bottom()};
                ShapeElement element_3;
                element_3.type = ShapeElementType::LineSegment;
                element_3.start = {trapezoid.x_bottom_right(), trapezoid.y_bottom()};
                element_3.end = {trapezoid.x_top_right(), trapezoid.y_top()};

                // Bounding box of the three elements.
                AxisAlignedBoundingBox trapezoid_aabb;
                trapezoid_aabb.x_min = (std::min)(trapezoid.x_bottom_left(), trapezoid.x_top_left());
                trapezoid_aabb.x_max = (std::max)(trapezoid.x_bottom_right(), trapezoid.x_top_right());
                trapezoid_aabb.y_min = trapezoid.y_bottom();
                trapezoid_aabb.y_max = trapezoid.y_top();

                bool add_to_skyline = false;
                bool may_intersect_supporting_shape = aabbs_may_intersect(
                        supporting_shape_aabb,
                        trapezoid_aabb);
                for (ElementPos supporting_shape_element_pos = 0;
                        may_intersect_supporting_shape
                        && supporting_shape_element_pos < supporting_shape.elements.size();
                        ++supporting_shape_element_pos) {
                    if (!aabbs_may_intersect(
                                supporting_shape_elements_aabbs[supporting_shape_element_pos],
                                trapezoid_aabb)) {
                        continue;
                    }
                    const ShapeElement& supporting_shape_element = supporting_shape.elements[supporting_shape_element_pos];
                    if (shape::intersect(supporting_shape_element, element_1)) {
                        //std::cout << "supporting_shape_element " << supporting_shape_element.to_string() << std::endl;
                        //std::cout << "element_1 " << element_1.to_string() << std::endl;
                        add_to_skyline = true;
                        break;
                    }
                    if (shape::intersect(supporting_shape_element, element_2)) {
                        //std::cout << "supporting_shape_element " << supporting_shape_element.to_string() << std::endl;
                        //std::cout << "element_2 " << element_2.to_string() << std::endl;
                        add_to_skyline = true;
                        break;
                    }
                    if (shape::intersect(supporting_shape_element, element_3)) {
                        //std::cout << "supporting_shape_element " << supporting_shape_element.to_string() << std::endl;
                        //std::cout << "element_3 " << element_3.to_string() << std::endl;
                        add_to_skyline = true;
                        break;
                    }
                }
                if (add_to_skyline) {
                    //std::cout << "add_trapezoid_to_skyline tata " << trapezoid << std::endl;
                    node.uncovered_trapezoids = add_trapezoid_to_skyline(
                            node.uncovered_trapezoids,
                            trapezoid_set.item_type_id,
                            item_shape_pos,
                            item_shape_trapezoid_pos,
                            trapezoid);
                    continue;
                }
            }

            LengthDbl touching_height = 0;
            for (const UncoveredTrapezoid& uncovered_trapezoid: node.uncovered_trapezoids) {
                if (uncovered_trapezoid.trapezoid.y_bottom() >= trapezoid.y_top())
                    continue;
                if (uncovered_trapezoid.trapezoid.y_top() <= trapezoid.y_bottom())
                    continue;
                //std::cout << "uncovered_trapezoid " << uncovered_trapezoid << std::endl;
                LengthDbl yb = std::max(
                        trapezoid.y_bottom(),
                        uncovered_trapezoid.trapezoid.y_bottom());
                LengthDbl yt = std::min(
                        trapezoid.y_top(),
                        uncovered_trapezoid.trapezoid.y_top());
                LengthDbl xb_t = trapezoid.x_left(yb);
                LengthDbl xt_t = trapezoid.x_left(yt);
                LengthDbl xb_ut = uncovered_trapezoid.trapezoid.x_right(yb);
                LengthDbl xt_ut = uncovered_trapezoid.trapezoid.x_right(yt);
                if (!strictly_greater(xb_t, xb_ut)
                        && !strictly_greater(xt_t, xt_ut)) {
                    touching_height += (yt - yb);
                }
            }
            //std::cout << "touching_height " << touching_height << " " << trapezoid.height() << std::endl;
            if (equal(touching_height, trapezoid.height())) {
                //std::cout << "add_trapezoid_to_skyline tete " << trapezoid << std::endl;
                node.uncovered_trapezoids = add_trapezoid_to_skyline(
                        node.uncovered_trapezoids,
                        trapezoid_set.item_type_id,
                        item_shape_pos,
                        item_shape_trapezoid_pos,
                        trapezoid);
                continue;
            }

            // Otherwise, add the trapezoid to the extra trapezoids.
            UncoveredTrapezoid extra_trapezoid(
                    trapezoid_set.item_type_id,
                    item_shape_pos,
                    item_shape_trapezoid_pos,
                    trapezoid);
            node.extra_trapezoids.push_back(extra_trapezoid);
        }
    }

    //std::cout << "extra_trapezoids:" << std::endl;
    //for (const UncoveredTrapezoid& extra_trapezoid: node.extra_trapezoids)
    //    std::cout << "* " << extra_trapezoid << std::endl;

    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_area and profit.
    node.item_number_of_copies = parent.item_number_of_copies;
    const ItemType& item_type = instance_.item_type(trapezoid_set.item_type_id);
    node.item_number_of_copies[trapezoid_set.item_type_id]++;
    node.number_of_items = parent.number_of_items + 1;
    node.item_area = parent.item_area + item_type.area_scaled;
    //std::cout << "parent.item_area " << parent.item_area << " item_area " << item_type.area << std::endl;
    node.profit = parent.profit + item_type.profit;
    node.item_convex_hull_area = parent.item_convex_hull_area
        + item_types_convex_hull_area_[trapezoid_set.item_type_id];

    // Update last_bin_item_number_of_copies and last_bin_resource_consumption.
    // 'node.profit' is further adjusted below by any 'penalize' resource
    // whose consumption crosses its capacity for the first time as a result
    // of this insertion (see 'Resource::penalize'), mirroring 'Solution::
    // update_indicators' so the search optimizes the same objective the
    // final solution will report.
    // Skipped entirely when the bin type has no resources at all, so
    // instances that don't use resources don't pay for allocating/copying
    // these per-node vectors.
    if (bin_type.number_of_resources() > 0) {
        if (insertion.new_bin_direction != Direction::Any) {  // New bin.
            node.last_bin_item_number_of_copies.assign(instance_.number_of_item_types(), 0);
            node.last_bin_resource_consumption.assign(bin_type.number_of_resources(), 0.0);
            for (const ItemResourceConsumption& item_resource_consumption: item_type.resources[bin_type_id]) {
                ResourceId resource_id = item_resource_consumption.resource_id;
                const Resource& resource = bin_type.resource(resource_id);
                const std::vector<double>& schedule
                    = resource.item_consumptions[item_resource_consumption.consumption_pos].second;
                double consumption = schedule_consumption(schedule, 0);
                node.last_bin_resource_consumption[resource_id] = consumption;
                // The bin is empty before this insertion, so any crossing here
                // is necessarily the first one.
                if (resource.penalize && consumption > resource.capacity)
                    node.profit -= resource.penalty;
            }
            node.last_bin_item_number_of_copies[trapezoid_set.item_type_id] = 1;
        } else {  // Same bin.
            node.last_bin_item_number_of_copies = parent.last_bin_item_number_of_copies;
            ItemPos item_copy = node.last_bin_item_number_of_copies[trapezoid_set.item_type_id];
            node.last_bin_resource_consumption = parent.last_bin_resource_consumption;
            for (const ItemResourceConsumption& item_resource_consumption: item_type.resources[bin_type_id]) {
                ResourceId resource_id = item_resource_consumption.resource_id;
                const Resource& resource = bin_type.resource(resource_id);
                const std::vector<double>& schedule
                    = resource.item_consumptions[item_resource_consumption.consumption_pos].second;
                double previous_consumption = node.last_bin_resource_consumption[resource_id];
                node.last_bin_resource_consumption[resource_id]
                    += schedule_consumption(schedule, item_copy);
                if (resource.penalize
                        && node.last_bin_resource_consumption[resource_id] > resource.capacity
                        && previous_consumption <= resource.capacity) {
                    node.profit -= resource.penalty;
                }
            }
            node.last_bin_item_number_of_copies[trapezoid_set.item_type_id]++;
        }
    }

    // Compute, guide_area and width using uncovered_trapezoids.
    node.xs_max = (insertion.new_bin_direction == Direction::Any)?  // Same bin
        std::max(parent.xs_max, insertion.x + trapezoid_set.x_min):
        insertion.x + trapezoid_set.x_min;
    node.guide_area = previous_bins_area_[bin_pos]
        + (node.xs_max - bb_bin_type.x_min)
        * (bb_bin_type.y_max - bb_bin_type.y_min);
    for (auto it = node.all_trapezoids_skyline.rbegin(); it != node.all_trapezoids_skyline.rend(); ++it) {
        const GeneralizedTrapezoid& trapezoid = it->trapezoid;
        //std::cout << trapezoid << std::endl;
        //std::cout << "* " << trapezoid.area() << " " << trapezoid.area(0.0) << std::endl;
        //std::cout << "current_area: " << node.current_area << std::endl;
        if (trapezoid.x_max() > node.xs_max)
            node.guide_area += trapezoid.area(node.xs_max);
    }

    // Compute node.x_max and node.y_max.
    Point xy = convert_point_back({node.x, node.y}, node.last_bin_direction);
    xy = (1.0 / instance().parameters().scale_value) * xy;
    const AxisAlignedBoundingBox& aabb = trapezoid_set.item_type_aabb_orig;
    if (insertion.new_bin_direction == Direction::Any) {  // Same bin
        node.x_min = std::min(parent.x_min, xy.x + aabb.x_min);
        node.x_max = std::max(parent.x_max, xy.x + aabb.x_max);
        node.y_min = std::min(parent.y_min, xy.y + aabb.y_min);
        node.y_max = std::max(parent.y_max, xy.y + aabb.y_max);
    } else {
        node.x_min = xy.x + aabb.x_min;
        node.x_max = xy.x + aabb.x_max;
        node.y_min = xy.y + aabb.y_min;
        node.y_max = xy.y + aabb.y_max;
    }

    AxisAlignedBoundingBox items_aabb;
    items_aabb.x_min = node.x_min;
    items_aabb.y_min = node.y_min;
    items_aabb.x_max = node.x_max;
    items_aabb.y_max = node.y_max;
    node.leftover_value = leftover_value(
            instance().parameters().leftover_mode,
            bin_type.aabb_orig,
            items_aabb);

    // Update the staircase of the last bin.
    if (insertion.new_bin_direction == Direction::Any) {  // Same bin
        node.last_bin_staircase = parent.last_bin_staircase;
        node.previous_bins_leftover_value
            = parent.previous_bins_leftover_value;
        node.previous_bins_staircase_leftover_value
            = parent.previous_bins_staircase_leftover_value;
    } else if (parent.number_of_bins > 0) {
        node.previous_bins_leftover_value
            = parent.previous_bins_leftover_value
            + parent.leftover_value;
        node.previous_bins_staircase_leftover_value
            = parent.previous_bins_staircase_leftover_value
            + parent.last_bin_staircase_leftover_value;
    }
    AxisAlignedBoundingBox item_aabb;
    item_aabb.x_min = xy.x + aabb.x_min;
    item_aabb.y_min = xy.y + aabb.y_min;
    item_aabb.x_max = xy.x + aabb.x_max;
    item_aabb.y_max = xy.y + aabb.y_max;
    node.last_bin_staircase.add_item(
            instance().parameters().leftover_mode,
            bin_type.aabb_orig,
            item_aabb);
    node.last_bin_staircase_leftover_value
        = node.last_bin_staircase.leftover_value(bin_type.aabb_orig);

    node.id = node_id_++;
    //std::cout << "node.id " << node.id << std::endl;
    return node;
}

void BranchingScheme::update_extra_trapezoids(Node& node) const
{
    if (node.parent != nullptr
            && node.number_of_bins == node.parent->number_of_bins) {  // Same bin
        const Node& parent = *node.parent;
        const DirectionData& direction_data = directions_data_[(int)node.last_bin_direction];
        BinPos bin_pos = node.number_of_bins - 1;
        BinTypeId bin_type_id = bin_type_ids_[bin_pos];
        const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];

        // Don't add extra rectangles which are behind the skyline.
        //std::cout << "check previous extra trapezoids:" << std::endl;
        for (const UncoveredTrapezoid& extra_trapezoid: parent.extra_trapezoids) {
            if (extra_trapezoid.trapezoid.y_top() >= bb_bin_type.y_max + (bb_bin_type.y_max - bb_bin_type.y_min)
                    || extra_trapezoid.trapezoid.y_bottom() <= bb_bin_type.y_min - (bb_bin_type.y_max - bb_bin_type.y_min)) {
                node.extra_trapezoids.push_back(extra_trapezoid);
                continue;
            }
            //std::cout << "extra_trapezoid " << extra_trapezoid << std::endl;
            LengthDbl covered_length = 0.0;
            for (const UncoveredTrapezoid& uncovered_trapezoid: node.uncovered_trapezoids) {
                if (uncovered_trapezoid.trapezoid.y_bottom() >= extra_trapezoid.trapezoid.y_top())
                    continue;
                if (uncovered_trapezoid.trapezoid.y_top() <= extra_trapezoid.trapezoid.y_bottom())
                    continue;
                //std::cout << "uncovered_trapezoid " << uncovered_trapezoid << std::endl;
                LengthDbl yb = std::max(
                        extra_trapezoid.trapezoid.y_bottom(),
                        uncovered_trapezoid.trapezoid.y_bottom());
                LengthDbl yt = std::min(
                        extra_trapezoid.trapezoid.y_top(),
                        uncovered_trapezoid.trapezoid.y_top());
                LengthDbl y = (yb + yt) / 2;
                LengthDbl x_extra = extra_trapezoid.trapezoid.x_right(y);
                LengthDbl x_uncov = uncovered_trapezoid.trapezoid.x_right(y);
                //std::cout << "yb " << yb << " yt " << yt << " y " << y << std::endl;
                //std::cout << "x_extra " << x_extra << " x_uncov " << x_uncov << std::endl;
                if (!strictly_greater(x_extra, x_uncov)) {
                    //std::cout << "covers" << std::endl;
                    covered_length += (yt - yb);
                }
            }
            //std::cout << "length " << extra_trapezoid.trapezoid.height()
            //    << " covered " << covered_length
            //    << std::endl;
            if (strictly_lesser(covered_length, extra_trapezoid.trapezoid.height())) {
                //std::cout << "add " << extra_trapezoid << std::endl;
                node.extra_trapezoids.push_back(extra_trapezoid);
            }
        }
    }

    // Sort extra_trapezoids.
    std::sort(
            node.extra_trapezoids.begin(),
            node.extra_trapezoids.end(),
            [](
                const UncoveredTrapezoid& uncovered_trapezoid_1,
                const UncoveredTrapezoid& uncovered_trapezoid_2)
            {
                const GeneralizedTrapezoid& trapezoid_1 = uncovered_trapezoid_1.trapezoid;
                const GeneralizedTrapezoid& trapezoid_2 = uncovered_trapezoid_2.trapezoid;
                if (trapezoid_1.y_bottom() != trapezoid_2.y_bottom())
                    return trapezoid_1.y_bottom() < trapezoid_2.y_bottom();
                if (trapezoid_1.y_top() != trapezoid_2.y_top())
                    return trapezoid_1.y_top() < trapezoid_2.y_top();
                return trapezoid_1.x_bottom_left() < trapezoid_2.x_bottom_left();
            });

}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& parent) const
{
    //if (parent->number_of_items == 20)
    //    exit(0);
    update_extra_trapezoids(*parent);
    insertions(parent);
    std::vector<std::shared_ptr<Node>> cs;
    if (parameters_.timer.needs_to_end())
        return cs;
    cs.reserve(parent->children_insertions.size());
    for (Counter i = 0; i < (Counter)parent->children_insertions.size(); ++i) {
        if (time_limit_reached(cs.size()))
            break;
        const Insertion& insertion = parent->children_insertions[i];

        //std::cout << "- insertion " << insertion << std::endl;
        cs.push_back(std::make_shared<Node>(child_tmp(parent, insertion)));
        //BinPos bin_pos = cs.back()->number_of_bins - 1;
        //Direction o = cs.back()->last_bin_direction;
        //const DirectionData& direction_data = directions_data_[(int)o];
        //BinTypeId bin_type_id = bin_type_ids_[bin_pos];
        //const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
        //std::cout << cs.back()->id
        //    << " insertion " << parent->children_insertions[i]
        //    << " xs_max " << cs.back()->xs_max
        //    << " xi_min " << bb_bin_type.x_min
        //    << " yi_min " << bb_bin_type.y_min
        //    << " yi_max " << bb_bin_type.y_max
        //    << " guide_area " << cs.back()->guide_area
        //    << std::endl;
        //write_svg(cs.back(), "node_" + std::to_string(cs.back()->id) + ".svg");
    }
    return cs;
}

void BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    uncovered_trapezoids_cur_.clear();
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = bin_type_ids_[parent->number_of_bins - 1];
        const DirectionData& direction_data = directions_data_[(int)parent->last_bin_direction];
        const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
        for (ItemPos uncovered_trapezoid_pos_cur = 0;
                uncovered_trapezoid_pos_cur < (ItemPos)parent->uncovered_trapezoids.size();
                ++uncovered_trapezoid_pos_cur) {
            GeneralizedTrapezoid uncovered_trapezoid = parent->uncovered_trapezoids[uncovered_trapezoid_pos_cur].trapezoid;
            uncovered_trapezoid.extend_left(bb_bin_type.x_min);
            uncovered_trapezoids_cur_.push_back(uncovered_trapezoid);
        }
    }

    //std::cout << std::endl;
    //std::cout << "insertions"
    //    << " id " << parent->id
    //    << " ts " << parent->trapezoid_set_id
    //    << " x " << parent->x
    //    << " y " << parent->y
    //    << " ys " << parent->ys
    //    << " ye " << parent->ye
    //    << " n " << parent->number_of_items
    //    << " item_area " << parent->item_area
    //    << " xs_max " << parent->xs_max
    //    << " guide_area " << parent->guide_area
    //    << " profit " << parent->profit
    //    << " leftover_value " << parent->leftover_value
    //    << " item_convex_hull_area " << parent->item_convex_hull_area
    //    << " guide " << parent->guide_area / parent->item_convex_hull_area
    //    << std::endl;
    //std::cout << "uncovered_trapezoids" << std::endl;
    //for (const UncoveredTrapezoid& uncovered_trapezoid: parent->uncovered_trapezoids)
    //    std::cout << "* " << uncovered_trapezoid << std::endl;
    //std::cout << "extra_trapezoid" << std::endl;
    //for (const UncoveredTrapezoid& extra_trapezoid: parent->extra_trapezoids)
    //    std::cout << "* " << extra_trapezoid << std::endl;
    //write_svg(parent, "node_" + std::to_string(parent->id) + ".svg");

    // Insert in the current bin.
    if (parent->number_of_bins > 0) {

        BinTypeId bin_type_id = bin_type_ids_[parent->number_of_bins - 1];
        const BinType& bin_type = instance().bin_type(bin_type_id);
        const DirectionData& direction_data = directions_data_[(int)parent->last_bin_direction];
        const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
        const std::vector<TrapezoidSet>& trapezoid_sets = bb_bin_type.trapezoid_sets;

        // Valid trapezoid sets for this bin type.
        std::vector<TrapezoidSetId> valid_trapezoid_set_ids;
        for (TrapezoidSetId trapezoid_set_id = 0;
                trapezoid_set_id < (TrapezoidSetId)trapezoid_sets.size();
                ++trapezoid_set_id) {
            const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
            if (parent->item_number_of_copies[trapezoid_set.item_type_id] + 1
                    <= instance().item_type(trapezoid_set.item_type_id).copies) {
                valid_trapezoid_set_ids.push_back(trapezoid_set_id);
            }
        }

        // Check all parents insertions.
        for (const Insertion& insertion: parent->parent->children_insertions) {
            if (time_limit_reached(parent->children_insertions.size()))
                return;
            // Check item quantity.
            ItemTypeId item_type_id = trapezoid_sets[insertion.trapezoid_set_id].item_type_id;
            const ItemType& item_type = instance().item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            // Check intersections.
            update_insertion(
                    parent,
                    insertion);
        }

        // Create insertions on the supporting parts of the last inserted item.
        // Loop through each supporting part of the last inserted item.
        for (ShapePos supporting_part_pos: bb_bin_type.trapezoid_sets[parent->trapezoid_set_id].supporting_parts) {
            const Support& supporting_part = bb_bin_type.supporting_parts[supporting_part_pos];
            //std::cout << "supporting_part " << supporting_part.shape.to_string(0) << std::endl;

            // Loop through each supported part of each remaining item.
            for (TrapezoidSetId supported_trapezoid_set_id: valid_trapezoid_set_ids) {
                //std::cout << "trapezoid_set_id " << trapezoid_set_id << std::endl;
                const TrapezoidSet& supported_trapezoid_set = trapezoid_sets[supported_trapezoid_set_id];
                for (ShapePos supported_part_pos: supported_trapezoid_set.supported_parts) {
                    if (time_limit_reached(parent->children_insertions.size()))
                        return;
                    const Support& supported_part = bb_bin_type.supported_parts[supported_part_pos];

                    insertion_trapezoid_set(
                            parent,
                            supported_trapezoid_set_id,
                            supporting_part_pos,
                            parent->x,
                            parent->y,
                            supported_part_pos,
                            Direction::Any);
                }
            }
        }
    }

    // Insert in a new bin.
    // Bins that can't fit any item are skipped: if a bin position doesn't
    // yield any insertion, the next bin position is tried instead.
    for (BinPos new_bin_pos = parent->number_of_bins;
            parent->children_insertions.empty()
            && new_bin_pos < instance().number_of_bins();
            ++new_bin_pos) {
        //std::cout << "insert in a new bin " << new_bin_pos << std::endl;
        BinTypeId bin_type_id = bin_type_ids_[new_bin_pos];
        const BinType& bin_type = instance().bin_type(bin_type_id);

        Direction new_bin_direction = parameters_.direction;
        if (new_bin_direction == Direction::Any)
            new_bin_direction = default_direction(instance(), bin_type_id);

        const DirectionData& direction_data = directions_data_[(int)new_bin_direction];
        const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
        const std::vector<TrapezoidSet>& trapezoid_sets = bb_bin_type.trapezoid_sets;

        // Valid trapezoid sets for this bin type.
        std::vector<TrapezoidSetId> valid_trapezoid_set_ids;
        for (TrapezoidSetId trapezoid_set_id = 0;
                trapezoid_set_id < (TrapezoidSetId)trapezoid_sets.size();
                ++trapezoid_set_id) {
            const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
            if (parent->item_number_of_copies[trapezoid_set.item_type_id] + 1
                    <= instance().item_type(trapezoid_set.item_type_id).copies) {
                valid_trapezoid_set_ids.push_back(trapezoid_set_id);
            }
        }

        // Create insertions on the supporting parts of the bin.
        for (ShapePos supporting_part_pos: bb_bin_type.bin_supporting_part_ids) {
            const Support& supporting_part = bb_bin_type.supporting_parts[supporting_part_pos];

            // Loop through each supported part of each remaining item.
            for (TrapezoidSetId supported_trapezoid_set_id: valid_trapezoid_set_ids) {
                const TrapezoidSet& supported_trapezoid_set = trapezoid_sets[supported_trapezoid_set_id];
                //std::cout << "trapezoid_set_id " << trapezoid_set_id << std::endl;

                // If inserting above defect, check if it is impacting.
                ItemTypeId supported_item_type_id = supported_trapezoid_set.item_type_id;
                const ItemType& supported_item_type = instance().item_type(supported_item_type_id);
                ItemShapePos item_shape_pos = -1;

                for (ShapePos supported_part_pos: bb_bin_type.trapezoid_sets[supported_trapezoid_set_id].supported_parts) {
                    if (time_limit_reached(parent->children_insertions.size()))
                        return;
                    const Support& supported_part = bb_bin_type.supported_parts[supported_part_pos];

                    // If inserting on a defect, check if it is impacting.
                    ItemShapePos supported_item_shape_pos = supported_part.item_shape_pos;
                    const ItemShape& item_shape = supported_item_type.shapes[supported_item_shape_pos];
                    if (supporting_part.defect_id >= 0) {
                        if (instance().can_contain(
                                    item_shape.quality_rule,
                                    bin_type.defects[supporting_part.defect_id].type)) {
                            return;
                        }
                    }

                    insertion_trapezoid_set(
                            parent,
                            supported_trapezoid_set_id,
                            supporting_part_pos,
                            0.0,
                            0.0,
                            supported_part_pos,
                            new_bin_direction,
                            new_bin_pos);
                }
            }
        }
    }
}

void BranchingScheme::update_insertion(
        const std::shared_ptr<Node>& parent,
        const Insertion& insertion_orig) const
{
    //std::cout << "update_insertion " << insertion_orig << std::endl;

    BinTypeId bin_type_id = bin_type_ids_[parent->number_of_bins - 1];
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction direction = parent->last_bin_direction;

    const DirectionData& direction_data = directions_data_[(int)direction];
    const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
    const std::vector<TrapezoidSet>& trapezoid_sets = bb_bin_type.trapezoid_sets;
    const Support& supporting_part = bb_bin_type.supporting_parts[insertion_orig.supporting_part_pos];
    Shape supporting_shape = supporting_part.shape;
    supporting_shape.shift(insertion_orig.supporting_part_x, insertion_orig.supporting_part_y);
    const Support& supported_part = bb_bin_type.supported_parts[insertion_orig.supported_part_pos];

    const TrapezoidSet& trapezoid_set = trapezoid_sets[insertion_orig.trapezoid_set_id];

    Insertion insertion = insertion_orig;
    insertion.new_bin_direction = Direction::Any;

    LengthDbl ys_tmp = supporting_part.aabb.y_min + insertion.supporting_part_y
        - (supported_part.aabb.y_max - trapezoid_set.y_min)
        - instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    LengthDbl ye_tmp = supporting_part.aabb.y_max + insertion.supporting_part_y
        + (trapezoid_set.y_max - supported_part.aabb.y_min)
        + instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    if (strictly_greater(ys_tmp, parent->ye)
            || strictly_lesser(ye_tmp, parent->ys)) {
        parent->children_insertions.push_back(insertion);
        return;
    }

    TrapezoidSetSlider slider(
            trapezoid_set,
            supporting_shape,
            supported_part.shape,
            bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing);
    if (!slider.slide(
                insertion,
                uncovered_trapezoids_cur_,
                parent->extra_trapezoids)) {
        return;
    }

    // Check bin width.
    if (slider.out_of_bounds(insertion))
        return;

    parent->children_insertions.push_back(insertion);

    //std::cout << "- insertion " << insertion
    //    << " -> ok" << std::endl;
}

void BranchingScheme::insertion_trapezoid_set(
        const std::shared_ptr<Node>& parent,
        TrapezoidSetId trapezoid_set_id,
        ShapePos supporting_part_pos,
        LengthDbl supporting_part_x,
        LengthDbl supporting_part_y,
        ShapePos supported_part_pos,
        Direction new_bin_direction,
        BinPos new_bin_pos) const
{
    //std::cout << "insertion_trapezoid_set " << trapezoid_set_id
    //    << " new_bin_direction " << (int)new_bin_direction
    //    << " spting " << supporting_part_pos
    //    << " " << supporting_part_x
    //    << " " << supporting_part_y
    //    << " spted " << supported_part_pos
    //    << std::endl;

    BinTypeId bin_type_id = (new_bin_direction == Direction::Any)?
        bin_type_ids_[parent->number_of_bins - 1]:
        bin_type_ids_[new_bin_pos];
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction direction = (new_bin_direction == Direction::Any)?
        parent->last_bin_direction:
        new_bin_direction;

    const DirectionData& direction_data = directions_data_[(int)direction];
    const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
    const std::vector<TrapezoidSet>& trapezoid_sets = bb_bin_type.trapezoid_sets;
    const Support& supporting_part = bb_bin_type.supporting_parts[supporting_part_pos];
    Shape supporting_shape = supporting_part.shape;
    supporting_shape.shift(supporting_part_x, supporting_part_y);
    const Support& supported_part = bb_bin_type.supported_parts[supported_part_pos];
    const Shape& supported_shape = supported_part.shape;
    //std::cout << "supporting_shape" << std::endl;
    //std::cout << supporting_shape.to_string(0) << std::endl;
    //std::cout << "supported_shape" << std::endl;
    //std::cout << supported_shape.to_string(0) << std::endl;

    const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
    const ItemType& item_type = instance().item_type(trapezoid_set.item_type_id);

    TrapezoidSetSlider slider(
            trapezoid_set,
            supporting_shape,
            supported_shape,
            bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing);

    Insertion insertion;
    static_cast<SlidePosition&>(insertion) = slider.initial_position();
    insertion.trapezoid_set_id = trapezoid_set_id;
    insertion.supporting_part_pos = supporting_part_pos;
    insertion.supporting_part_x = supporting_part_x;
    insertion.supporting_part_y = supporting_part_y;
    insertion.supported_part_pos = supported_part_pos;
    insertion.new_bin_direction = new_bin_direction;
    insertion.new_bin_pos = new_bin_pos;

    // Check resource capacity. 'penalize' resources never block an
    // insertion (see 'Resource::penalize'); only their non-'penalize'
    // counterparts do. Skipped entirely when the bin type has no resources
    // (see 'child_tmp').
    if (bin_type.number_of_resources() > 0) {
        if (new_bin_direction == Direction::Any) {  // Same bin.
            ItemPos item_copy = parent->last_bin_item_number_of_copies[trapezoid_set.item_type_id];
            for (const ItemResourceConsumption& item_resource_consumption: item_type.resources[bin_type_id]) {
                ResourceId resource_id = item_resource_consumption.resource_id;
                const Resource& resource = bin_type.resource(resource_id);
                if (resource.penalize)
                    continue;
                const std::vector<double>& schedule
                    = resource.item_consumptions[item_resource_consumption.consumption_pos].second;
                double consumption = parent->last_bin_resource_consumption[resource_id]
                    + schedule_consumption(schedule, item_copy);
                if (consumption > resource.capacity * PSTOL)
                    return;
            }
        } else {  // New bin.
            for (const ItemResourceConsumption& item_resource_consumption: item_type.resources[bin_type_id]) {
                ResourceId resource_id = item_resource_consumption.resource_id;
                const Resource& resource = bin_type.resource(resource_id);
                if (resource.penalize)
                    continue;
                const std::vector<double>& schedule
                    = resource.item_consumptions[item_resource_consumption.consumption_pos].second;
                double consumption = schedule_consumption(schedule, 0);
                if (consumption > resource.capacity * PSTOL)
                    return;
            }
        }
    }

    if (slider.out_of_bounds(insertion))
        return;

    // Check that the item can fit between the bottom and the top of the bin.
    // While sliding, the y-coordinate of the trapezoid set remains in the
    // range given by the bounding boxes of the supporting and supported parts.
    LengthDbl y_lowest = supporting_part.aabb.y_min + supporting_part_y
        - supported_part.aabb.y_max;
    LengthDbl y_highest = supporting_part.aabb.y_max + supporting_part_y
        - supported_part.aabb.y_min;
    if (strictly_greater(y_lowest + trapezoid_set.y_max, bb_bin_type.y_max)
            || strictly_lesser(y_highest + trapezoid_set.y_min, bb_bin_type.y_min)) {
        return;
    }

    // Move the item to the right of the left side of the bin.
    if (!slider.slide_right_of(
                insertion,
                bb_bin_type.x_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing,
                bb_bin_type.x_min - (bb_bin_type.x_max - bb_bin_type.x_min),
                bb_bin_type.y_min,
                bb_bin_type.y_max)) {
        return;
    }

    // Move the item to the right of the items already packed.
    if (new_bin_direction == Direction::Any) {  // Same bin.
        if (!slider.slide(
                    insertion,
                    uncovered_trapezoids_cur_,
                    parent->extra_trapezoids)) {
            return;
        }
    } else {  // New bin.
        if (!slider.slide(
                    insertion,
                    bb_bin_type.defects)) {
            return;
        }
    }

    // Check bin width.
    if (slider.out_of_bounds(insertion))
        return;

    insertion.ys = supporting_part.aabb.y_min + supporting_part_y
        - (trapezoid_set.y_max - trapezoid_set.y_min)
        - instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    insertion.ye = supporting_part.aabb.y_max + supporting_part_y
        + (trapezoid_set.y_max - trapezoid_set.y_min)
        + instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    parent->children_insertions.push_back(insertion);
    //std::cout << "- insertion " << insertion
    //    << " -> ok" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    BranchingScheme::Node node;
    node.item_number_of_copies = std::vector<ItemPos>(instance().number_of_item_types(), 0);

    // Update node attributes to reflect already-fixed items.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance().number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance().bin_type(bin_type_id);
        for (const FixedItem& fixed_item: bin_type.fixed_items) {
            const ItemType& item_type = instance().item_type(fixed_item.item_type_id);
            node.item_number_of_copies[fixed_item.item_type_id] += bin_type.copies;
            node.number_of_items += bin_type.copies;
            node.item_area += bin_type.copies * item_type.area_scaled;
            node.item_convex_hull_area += bin_type.copies * item_types_convex_hull_area_[fixed_item.item_type_id];
            node.profit += bin_type.copies * item_type.profit;
        }
    }

    node.id = node_id_++;
    return std::make_shared<Node>(node);
}

bool BranchingScheme::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->number_of_bins > node_1->number_of_bins;
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        if (node_2->number_of_bins != node_1->number_of_bins)
            return node_2->number_of_bins > node_1->number_of_bins;
        // Same comparison as 'Solution::operator<'.
        if (shape::strictly_greater(
                    std::sqrt(node_1->leftover_value),
                    std::sqrt(node_2->leftover_value))) {
            return true;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_2->leftover_value),
                    std::sqrt(node_1->leftover_value))) {
            return false;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_1->last_bin_staircase_leftover_value),
                    std::sqrt(node_2->last_bin_staircase_leftover_value))) {
            return true;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_2->last_bin_staircase_leftover_value),
                    std::sqrt(node_1->last_bin_staircase_leftover_value))) {
            return false;
        }
        if (!instance().parameters().leftover_all_bins)
            return false;
        if (shape::strictly_greater(
                    std::sqrt(node_1->previous_bins_leftover_value + node_1->leftover_value),
                    std::sqrt(node_2->previous_bins_leftover_value + node_2->leftover_value))) {
            return true;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_2->previous_bins_leftover_value + node_2->leftover_value),
                    std::sqrt(node_1->previous_bins_leftover_value + node_1->leftover_value))) {
            return false;
        }
        return shape::strictly_greater(
                std::sqrt(node_1->previous_bins_staircase_leftover_value + node_1->last_bin_staircase_leftover_value),
                std::sqrt(node_2->previous_bins_staircase_leftover_value + node_2->last_bin_staircase_leftover_value));
    } case Objective::OpenDimensionX: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->x_max > node_1->x_max;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->y_max > node_1->y_max;
    } case Objective::Knapsack: {
        return node_2->profit < node_1->profit;
    } case Objective::Feasibility: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "branching scheme 'irregular::BranchingScheme' "
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingScheme::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance().objective()) {
    case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_2))
            return false;
        return (node_1->number_of_bins >= node_2->number_of_bins);
    } case Objective::BinPackingWithLeftovers: {
        if (!leaf(node_2))
            return false;
        if (node_1->number_of_bins > node_2->number_of_bins) {
            return true;
        } else if (node_1->number_of_bins < node_2->number_of_bins) {
            return false;
        }
        // The leftover values and the staircase leftover values of a node
        // are upper bounds of the ones of its descendants in the same bin.
        if (shape::strictly_greater(
                    std::sqrt(node_1->leftover_value),
                    std::sqrt(node_2->leftover_value))) {
            return false;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_2->leftover_value),
                    std::sqrt(node_1->leftover_value))) {
            return true;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_1->last_bin_staircase_leftover_value),
                    std::sqrt(node_2->last_bin_staircase_leftover_value))) {
            return false;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_2->last_bin_staircase_leftover_value),
                    std::sqrt(node_1->last_bin_staircase_leftover_value))) {
            return true;
        }
        if (!instance().parameters().leftover_all_bins)
            return true;
        if (shape::strictly_greater(
                    std::sqrt(node_1->previous_bins_leftover_value + node_1->leftover_value),
                    std::sqrt(node_2->previous_bins_leftover_value + node_2->leftover_value))) {
            return false;
        }
        if (shape::strictly_greater(
                    std::sqrt(node_2->previous_bins_leftover_value + node_2->leftover_value),
                    std::sqrt(node_1->previous_bins_leftover_value + node_1->leftover_value))) {
            return true;
        }
        return !shape::strictly_greater(
                std::sqrt(node_1->previous_bins_staircase_leftover_value + node_1->last_bin_staircase_leftover_value),
                std::sqrt(node_2->previous_bins_staircase_leftover_value + node_2->last_bin_staircase_leftover_value));
    } case Objective::Knapsack: {
        if (leaf(node_2))
            return true;
        return false;
    } case Objective::Feasibility: {
        if (leaf(node_2))
            return true;
        return false;
    } case Objective::OpenDimensionX: {
        if (!leaf(node_2))
            return false;
        return node_1->x_max >= node_2->x_max;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return node_1->y_max >= node_2->y_max;
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        return false;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "branching scheme 'irregular::BranchingScheme' "
            << "does not support objective '" << instance().objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// export ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Solution BranchingScheme::to_solution(
        const std::shared_ptr<Node>& node) const
{
    //std::cout << "to_solution" << std::endl;
    std::vector<std::shared_ptr<Node>> descendents;
    for (std::shared_ptr<Node> current_node = node;
            current_node->parent != nullptr;
            current_node = current_node->parent) {
        descendents.push_back(current_node);
    }
    std::reverse(descendents.begin(), descendents.end());

    SolutionBuilder solution_builder(instance());
    BinPos bin_pos = -1;
    BinPos number_of_bins = 0;
    for (auto current_node: descendents) {
        // Bins that were skipped because no item fit in them are still
        // added to the solution, as empty bins.
        while (number_of_bins < current_node->number_of_bins) {
            bin_pos = solution_builder.add_bin(
                    bin_type_ids_[number_of_bins],
                    1);
            number_of_bins++;
        }

        BinTypeId bin_type_id_cur = bin_type_ids_[current_node->number_of_bins - 1];
        const DirectionData& direction_data = directions_data_[(int)current_node->last_bin_direction];
        const TrapezoidSet& trapezoid_set = direction_data.bin_types[bin_type_id_cur].trapezoid_sets[current_node->trapezoid_set_id];
        Point bl_corner = convert_point_back({current_node->x, current_node->y}, current_node->last_bin_direction);
        bl_corner = (1.0 / instance().parameters().scale_value) * bl_corner;
        //std::cout << "bin_pos " << bin_pos
        //    << " item_type_id " << trapezoid_set.item_type_id
        //    << " bl_corner " << bl_corner.to_string()
        //    << " angle " << trapezoid_set.angle
        //    << std::endl;
        solution_builder.add_item(
                bin_pos,
                trapezoid_set.item_type_id,
                bl_corner,
                trapezoid_set.angle,
                trapezoid_set.mirror);
    }

    // Add fixed items to each bin, extending the solution up to the last bin
    // that has a bin type with fixed items.
    // (The main loop above already created bins 0..number_of_bins-1.)
    for (BinPos bin_pos = 0;
            bin_pos <= instance().last_bin_with_fixed_items();
            ++bin_pos) {
        if (bin_pos >= number_of_bins) {
            BinTypeId bin_type_id = bin_type_ids_[bin_pos];
            solution_builder.add_bin(bin_type_id, 1);
            number_of_bins++;
        }
        BinTypeId bin_type_id = bin_type_ids_[bin_pos];
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
    }
    return solution_builder.build();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void BranchingScheme::json_export_setup() const
{
    nlohmann::json json_init;

    // Bins.
    json_bins_init_ids_ = std::vector<Counter>(instance().number_of_bin_types(), -1);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance().number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance().bin_type(bin_type_id);
        json_bins_init_ids_[bin_type_id] = json_counter_;
        json_counter_++;
    }

    // Items.
    // Collect unique (item_type, mirror, angle) triples across all bin types
    // and assign each a JSON counter.  Different bin types may eventually have
    // different rotation sets, so we loop over all bin types and skip
    // rotations that have already been assigned an ID.
    json_items_init_ids_ = std::vector<std::vector<std::unordered_map<double, Counter>>>(instance().number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance().number_of_item_types();
            ++item_type_id) {
        json_items_init_ids_[item_type_id] = std::vector<std::unordered_map<double, Counter>>(2);
    }
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance().number_of_bin_types();
            ++bin_type_id) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance().number_of_item_types();
                ++item_type_id) {
            for (const ItemTypeRotation& rotation: item_types_allowed_rotations_[bin_type_id][item_type_id]) {
                auto& id_map = json_items_init_ids_[item_type_id][(int)rotation.mirror];
                if (id_map.count(rotation.angle) == 0) {
                    id_map[rotation.angle] = json_counter_;
                    json_counter_++;
                }
            }
        }
    }
}

nlohmann::json BranchingScheme::json_export_init() const
{
    nlohmann::json json_init;

    // Bins.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance().number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance().bin_type(bin_type_id);

        Counter json_counter = json_bins_init_ids_[bin_type_id];
        json_init[json_counter][0] = {
            {"Shape", bin_type.shape_orig.to_json()["elements"]},
            {"FillColor", ""},
        };
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const Defect& defect = bin_type.defects[defect_id];
            json_init[json_counter][defect_id + 1] = {
                {"Shape", bin_type.shape_orig.to_json()["elements"]},
                {"FillColor", "red"},
            };
            for (Counter hole_pos = 0;
                    hole_pos < (Counter)defect.shape_orig.holes.size();
                    ++hole_pos) {
                const Shape& hole = defect.shape_orig.holes[hole_pos];
                json_init[json_counter][hole_pos]["Holes"][hole_pos] = hole.to_json()["elements"];
            }
        }
    }

    // Items.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance().number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance().item_type(item_type_id);

        for (int mirror_int = 0; mirror_int < 2; ++mirror_int) {
        const bool mirror = (bool)mirror_int;
        for (const auto& angle_counter: json_items_init_ids_[item_type_id][mirror_int]) {
            const Angle angle = angle_counter.first;
            {
                Counter json_counter = angle_counter.second;

                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                    Shape shape = convert_shape(item_shape.shape_orig.shape, angle, mirror);
                    json_init[json_counter][item_shape_pos] = {
                        {"Shape", shape.to_json()["elements"]},
                        {"FillColor", "blue"},
                    };
                    for (Counter hole_pos = 0;
                            hole_pos < (Counter)item_shape.shape_orig.holes.size();
                            ++hole_pos) {
                        const Shape& hole = item_shape.shape_orig.holes[hole_pos];
                        json_init[json_counter][item_shape_pos]["Holes"][hole_pos] = hole.to_json()["elements"];
                    }
                }
            }
        }
        }  // mirror_int
    }

    return json_init;
}

nlohmann::json BranchingScheme::json_export(
        const std::shared_ptr<Node>& node) const
{
    if (!json_is_setup_) {
        json_export_setup();
        json_is_setup_ = true;
    }

    if (node->number_of_items == 0) {
        nlohmann::json json = {
            {"Id", node->id},
            {"ParentId", (node->parent == nullptr)? -1: node->parent->id},
        };
        return json;
    }

    BinTypeId bin_type_id = bin_type_ids_[node->number_of_bins - 1];
    const DirectionData& direction_data = directions_data_[(int)node->last_bin_direction];
    const TrapezoidSet& trapezoid_set = direction_data.bin_types[bin_type_id].trapezoid_sets[node->trapezoid_set_id];
    Point bl_orig = convert_point_back({node->x, node->y}, node->last_bin_direction);
    bl_orig = (1.0 / instance().parameters().scale_value) * bl_orig;
    nlohmann::json json = {
        {"Id", node->id},
        {"ParentId", (node->parent == nullptr)? -1: node->parent->id},
        {"TrapezoidSetId", node->trapezoid_set_id},
        {"ItemTypeId", trapezoid_set.item_type_id},
        {"Angle", trapezoid_set.angle},
        {"Mirror", trapezoid_set.mirror},
        {"X", node->x},
        {"Y", node->y},
        {"XOrig", bl_orig.x},
        {"YOrig", bl_orig.y},
        {"NumberOfItems", node->number_of_items},
        {"NumberOfBins", node->number_of_bins},
        {"Profit", node->profit},
        {"ItemArea", node->item_area},
        {"ItemConvexHullArea", node->item_convex_hull_area},
        {"GuideArea", node->guide_area},
    };
    if (node->number_of_items == 0)
        return json;

    nlohmann::json plot;
    Counter i = 0;
    // Plot bin.
    plot[i] = {
        {"Id",json_bins_init_ids_[bin_type_id]},
        {"X", 0},
        {"Y", 0},
    };
    i++;
    // Plot items.
    for (std::shared_ptr<const Node> node_tmp = node;
            node_tmp->number_of_bins == node->number_of_bins;
            node_tmp = node_tmp->parent) {
        BinTypeId bin_type_id_tmp = bin_type_ids_[node_tmp->number_of_bins - 1];
        const DirectionData& direction_data_tmp = directions_data_[(int)node_tmp->last_bin_direction];
        const TrapezoidSet& trapezoid_set = direction_data_tmp.bin_types[bin_type_id_tmp].trapezoid_sets[node_tmp->trapezoid_set_id];
        Point bl_corner = convert_point_back({node_tmp->x, node_tmp->y}, node_tmp->last_bin_direction);
        bl_corner = (1.0 / instance().parameters().scale_value) * bl_corner;
        plot[i] = {
            {"Id", json_items_init_ids_[trapezoid_set.item_type_id][trapezoid_set.mirror].at(trapezoid_set.angle)},
            {"X", bl_corner.x},
            {"Y", bl_corner.y},
        };
        if (node_tmp == node)
            plot[i]["FillColor"] = "green";
        i++;
    }
    json["Plot"] = plot;
    return json;
}

void BranchingScheme::write_svg(
        const std::shared_ptr<Node>& node,
        const std::string& file_path) const
{
    if (file_path.empty())
        return;
    std::ofstream file{file_path};
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + file_path + "\".");
    }

    if (node->number_of_bins == 0)
        return;

    BinPos bin_pos = node->number_of_bins - 1;
    BinTypeId bin_type_id = bin_type_ids_[bin_pos];
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const DirectionData& direction_data = directions_data_[(int)node->last_bin_direction];
    const TrapezoidBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
    LengthDbl width = (bb_bin_type.x_max - bb_bin_type.x_min);
    LengthDbl height = (bb_bin_type.y_max - bb_bin_type.y_min);

    std::string s = "<svg viewBox=\""
        + std::to_string(bb_bin_type.x_min)
        + " " + std::to_string(-bb_bin_type.y_min - height)
        + " " + std::to_string(width)
        + " " + std::to_string(height)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    for (ItemPos extra_trapezoid_pos = 0;
            extra_trapezoid_pos < (ItemPos)node->extra_trapezoids.size();
            ++extra_trapezoid_pos) {
        const GeneralizedTrapezoid& trapezoid = node->extra_trapezoids[extra_trapezoid_pos].trapezoid;

        file << "<g>" << std::endl;
        file << trapezoid.to_svg("red");
        LengthDbl x = (trapezoid.x_max() + trapezoid.x_min()) / 2;
        LengthDbl y = (trapezoid.y_top() + trapezoid.y_bottom()) / 2;
        file << "<text x=\"" << std::to_string(x) << "\" y=\"" << std::to_string(-y) << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">" << std::to_string(extra_trapezoid_pos) << "</text>" << std::endl;
        file << "</g>" << std::endl;
    }
    for (TrapezoidPos uncovered_trapezoid_pos = 0;
            uncovered_trapezoid_pos < (ItemPos)node->uncovered_trapezoids.size();
            ++uncovered_trapezoid_pos) {
        GeneralizedTrapezoid trapezoid = node->uncovered_trapezoids[uncovered_trapezoid_pos].trapezoid;
        trapezoid.extend_left(bb_bin_type.x_min);

        file << "<g>" << std::endl;
        file << trapezoid.to_svg("blue");
        LengthDbl x = (trapezoid.x_max() + trapezoid.x_min()) / 2;
        LengthDbl y = (trapezoid.y_top() + trapezoid.y_bottom()) / 2;
        file << "<text x=\"" << std::to_string(x) << "\" y=\"" << std::to_string(-y) << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">" << std::to_string(uncovered_trapezoid_pos) << "</text>" << std::endl;
        file << "</g>" << std::endl;
    }

    file << "</svg>" << std::endl;
}

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredTrapezoid& uncovered_trapezoid)
{
    os << "item_type_id " << uncovered_trapezoid.item_type_id
        << " defect_id " << uncovered_trapezoid.defect_id
        << " trapezoid " << uncovered_trapezoid.trapezoid
        ;
    return os;
}

bool BranchingScheme::UncoveredTrapezoid::operator==(
        const UncoveredTrapezoid& uncovered_trapezoid) const
{
    return ((item_type_id == uncovered_trapezoid.item_type_id)
            && (defect_id == uncovered_trapezoid.defect_id)
            && (equal(trapezoid.y_bottom(), uncovered_trapezoid.trapezoid.y_bottom()))
            && (equal(trapezoid.y_top(), uncovered_trapezoid.trapezoid.y_top()))
            && (equal(trapezoid.x_bottom_left(), uncovered_trapezoid.trapezoid.x_bottom_left()))
            && (equal(trapezoid.x_bottom_right(), uncovered_trapezoid.trapezoid.x_bottom_right()))
            && (equal(trapezoid.x_top_left(), uncovered_trapezoid.trapezoid.x_top_left()))
            && (equal(trapezoid.x_top_right(), uncovered_trapezoid.trapezoid.x_top_right()))
            );
}

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return ((trapezoid_set_id == insertion.trapezoid_set_id)
            && (supporting_part_pos == insertion.supporting_part_pos)
            && (supporting_part_element_pos == insertion.supporting_part_element_pos)
            && (supported_part_pos == insertion.supported_part_pos)
            && (supported_part_element_pos == insertion.supported_part_element_pos)
            && (new_bin_direction == insertion.new_bin_direction)
            && (new_bin_pos == insertion.new_bin_pos)
            && (equal(x, insertion.x))
            && (equal(y, insertion.y))
            );
}

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "trapezoid_set_id " << insertion.trapezoid_set_id
        << " spting " << insertion.supporting_part_pos
        << " pos " << insertion.supporting_part_element_pos
        << " sx " << insertion.supporting_part_x
        << " sy " << insertion.supporting_part_y
        << " spted " << insertion.supported_part_pos
        << " pos " << insertion.supported_part_element_pos
        << " new_bin_direction " << (int)insertion.new_bin_direction
        << " new_bin_pos " << insertion.new_bin_pos
        << " x " << insertion.x
        << " y " << insertion.y
        << " ys " << insertion.ys
        << " ye " << insertion.ye
        ;
    return os;
}

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node)
{
    os << "number_of_items " << node.number_of_items
        << " number_of_bins " << node.number_of_bins
        << std::endl;
    os << "item_area " << node.item_area
        << std::endl;
    os << " profit " << node.profit
        << std::endl;

    // item_number_of_copies
    os << "item_number_of_copies" << std::flush;
    for (ItemPos j_pos: node.item_number_of_copies)
        os << " " << j_pos;
    os << std::endl;

    return os;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// tree_search ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

namespace
{

void tree_search_worker(
        const packingsolver::irregular::Instance& instance,
        const packingsolver::irregular::TreeSearchParameters& parameters,
        packingsolver::irregular::AlgorithmFormatter& algorithm_formatter,
        packingsolver::irregular::BranchingScheme::Parameters branching_scheme_parameters,
        treesearchsolver::IterativeBeamSearch2Parameters<packingsolver::irregular::BranchingScheme> ibs_parameters,
        std::function<void(const packingsolver::irregular::Solution&, const std::string&)> new_solution_callback)
{
    using namespace packingsolver::irregular;
    Counter queue_size = 1;
    double maximum_approximation_ratio = parameters.initial_maximum_approximation_ratio;
    std::shared_ptr<BranchingScheme::Node> cutoff = nullptr;
    for (Counter iteration = 0;
            ;
            ++iteration) {

        if (parameters.optimization_mode != OptimizationMode::Anytime
                && parameters.optimization_mode != OptimizationMode::Exact) {
            queue_size = parameters.not_anytime_tree_search_queue_size;
            maximum_approximation_ratio = parameters.not_anytime_maximum_approximation_ratio;
        }

        branching_scheme_parameters.maximum_approximation_ratio = maximum_approximation_ratio;
        branching_scheme_parameters.timer = ibs_parameters.timer;
        ibs_parameters.cutoff = cutoff;
        ibs_parameters.minimum_size_of_the_queue = queue_size;
        ibs_parameters.maximum_size_of_the_queue = queue_size;
        if (!parameters.json_search_tree_path.empty()) {
            ibs_parameters.json_search_tree_path = parameters.json_search_tree_path
                + "_guide_" + std::to_string(branching_scheme_parameters.guide_id)
                + "_d_" + std::to_string((int)branching_scheme_parameters.direction);
        }
        BranchingScheme branching_scheme(instance, branching_scheme_parameters);
        ibs_parameters.new_solution_callback =
            [&branching_scheme, &branching_scheme_parameters, &new_solution_callback](
                    const treesearchsolver::Output<BranchingScheme>& tss_output)
            {
                const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>& tssibs_output
                    = static_cast<const treesearchsolver::IterativeBeamSearch2Output<BranchingScheme>&>(tss_output);
                Solution solution = branching_scheme.to_solution(
                        tssibs_output.solution_pool.best());
                std::stringstream ss;
                ss << "g " << branching_scheme_parameters.guide_id
                    << " d " << (int)branching_scheme_parameters.direction
                    << " q " << tssibs_output.maximum_size_of_the_queue;
                new_solution_callback(solution, ss.str());
            };
        auto ibs_output = treesearchsolver::iterative_beam_search_2<BranchingScheme>(
                branching_scheme,
                ibs_parameters);

        if (ibs_output.optimal)
            break;
        if (algorithm_formatter.end_boolean())
            break;
        if (parameters.timer.needs_to_end())
            break;

        if (parameters.optimization_mode != OptimizationMode::Anytime
                && parameters.optimization_mode != OptimizationMode::Exact)
            break;

        if (cutoff != nullptr)
            ibs_output.solution_pool.add(cutoff);
        cutoff = ibs_output.solution_pool.best();

        queue_size = std::max(
                queue_size + 1,
                (NodeId)(queue_size * 1.5));
        maximum_approximation_ratio *= parameters.maximum_approximation_ratio_factor;
    }
}

} // anonymous namespace

const packingsolver::irregular::TreeSearchOutput packingsolver::irregular::tree_search(
        const Instance& instance,
        const TreeSearchParameters& parameters)
{
    if (instance.objective() == Objective::OpenDimensionXY) {
        TreeSearchOutput output(instance);
        AlgorithmFormatter algorithm_formatter(instance, parameters, output);
        algorithm_formatter.start();
        algorithm_formatter.print_header();

        SequentialFeasibilitySolver solver = [&parameters, &algorithm_formatter](
                const Instance& sub_instance,
                const Solution*)
        {
            TreeSearchParameters inner_parameters;
            inner_parameters.verbosity_level = 0;
            inner_parameters.timer = parameters.timer;
            inner_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
            inner_parameters.guides = parameters.guides;
            inner_parameters.optimization_mode = parameters.optimization_mode;
            inner_parameters.not_anytime_tree_search_queue_size
                = parameters.not_anytime_tree_search_queue_size;
            inner_parameters.initial_maximum_approximation_ratio
                = parameters.initial_maximum_approximation_ratio;
            inner_parameters.not_anytime_maximum_approximation_ratio
                = parameters.not_anytime_maximum_approximation_ratio;
            inner_parameters.maximum_approximation_ratio_factor
                = parameters.maximum_approximation_ratio_factor;
            return tree_search(sub_instance, inner_parameters).solution_pool;
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

    TreeSearchOutput output(instance);
    AlgorithmFormatter algorithm_formatter(instance, parameters, output);
    algorithm_formatter.start();
    algorithm_formatter.print_header();

    std::vector<GuideId> guides = parameters.guides;
    if (guides.empty()) {
        if (instance.objective() == Objective::Knapsack) {
            guides = {4, 5};
        } else {
            guides = {0, 1};
        }
    }

    std::vector<Direction> directions;
    if (instance.objective() == Objective::OpenDimensionX) {
        directions = {
            Direction::LeftToRightThenBottomToTop,
            Direction::LeftToRightThenTopToBottom,
        };
    } else if (instance.objective() == Objective::OpenDimensionY) {
        directions = {
            Direction::BottomToTopThenLeftToRight,
            Direction::BottomToTopThenRightToLeft,
        };
    } else if (instance.number_of_bins() == 1) {
        if (instance.objective() == Objective::BinPackingWithLeftovers) {
            switch (instance.parameters().leftover_mode) {
            case LeftoverMode::BottomLeft: {
                directions = {
                    Direction::LeftToRightThenBottomToTop,
                    Direction::BottomToTopThenLeftToRight,
                    //Direction::LeftToRightThenTopToBottom,
                    //Direction::BottomToTopThenRightToLeft,
                };
                break;
            } case LeftoverMode::BottomRight: {
                directions = {
                    Direction::RightToLeftThenBottomToTop,
                    Direction::BottomToTopThenLeftToRight,
                    //Direction::RightToLeftThenTopToBottom,
                    //Direction::BottomToTopThenRightToLeft,
                };
                break;
            } case LeftoverMode::TopLeft: {
                directions = {
                    Direction::LeftToRightThenBottomToTop,
                    Direction::TopToBottomThenLeftToRight,
                    //Direction::LeftToRightThenTopToBottom,
                    //Direction::TopToBottomThenRightToLeft,
                };
                break;
            } case LeftoverMode::TopRight: {
                directions = {
                    Direction::RightToLeftThenBottomToTop,
                    Direction::TopToBottomThenLeftToRight,
                    //Direction::RightToLeftThenTopToBottom,
                    //Direction::TopToBottomThenRightToLeft,
                };
                break;
            } case LeftoverMode::Left: {
                directions = {
                    Direction::LeftToRightThenBottomToTop,
                    Direction::LeftToRightThenTopToBottom,
                };
                break;
            } case LeftoverMode::Right: {
                directions = {
                    Direction::RightToLeftThenBottomToTop,
                    Direction::RightToLeftThenTopToBottom,
                };
                break;
            } case LeftoverMode::Bottom: {
                directions = {
                    Direction::BottomToTopThenLeftToRight,
                    Direction::BottomToTopThenRightToLeft,
                };
                break;
            } case LeftoverMode::Top: {
                directions = {
                    Direction::TopToBottomThenLeftToRight,
                    Direction::TopToBottomThenRightToLeft,
                };
                break;
            }
            }
        } else {
            directions = {
                Direction::LeftToRightThenBottomToTop,
                Direction::BottomToTopThenLeftToRight,
                //Direction::RightToLeftThenTopToBottom,
                //Direction::TopToBottomThenRightToLeft,
            };
        }
    } else {
        switch (instance.parameters().leftover_mode) {
        case LeftoverMode::BottomLeft: {
            directions = {Direction::LeftToRightThenBottomToTop};
            break;
        } case LeftoverMode::BottomRight: {
            directions = {Direction::RightToLeftThenBottomToTop};
            break;
        } case LeftoverMode::TopLeft: {
            directions = {Direction::LeftToRightThenTopToBottom,};
            break;
        } case LeftoverMode::TopRight: {
            directions = {Direction::RightToLeftThenTopToBottom};
            break;
        } case LeftoverMode::Left: {
            directions = {Direction::LeftToRightThenBottomToTop};
            break;
        } case LeftoverMode::Right: {
            directions = {Direction::RightToLeftThenBottomToTop};
            break;
        } case LeftoverMode::Bottom: {
            directions = {Direction::BottomToTopThenLeftToRight};
            break;
        } case LeftoverMode::Top: {
            directions = {Direction::TopToBottomThenLeftToRight};
            break;
        }
        }
    }

    bool all_items_full_rotation = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        if (!item_type.has_full_continuous_rotations()) {
            all_items_full_rotation = false;
            break;
        }
    }
    bool all_bins_squared = true;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance.bin_type(bin_type_id);
        if (!bin_type.shape_scaled.is_square()) {
            all_bins_squared = false;
            break;
        }
    }
    if (all_items_full_rotation && all_bins_squared)
        directions = {Direction::LeftToRightThenBottomToTop};

    std::vector<double> growth_factors = {1.5};
    //if (guides.size() * directions.size() * 2 <= 4)
    //    growth_factors = {1.33, 1.5};
    if (parameters.optimization_mode != OptimizationMode::Anytime
            && parameters.optimization_mode != OptimizationMode::Exact)
        growth_factors = {1.5};

    std::vector<BranchingScheme::Parameters> branching_scheme_parameters_list;
    std::vector<treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme>> ibs_parameters_list;
    // In 'NotAnytimeDeterministic' mode, each scheme reports to its own
    // algorithm formatter, writing into its own output, instead of the shared
    // 'algorithm_formatter', so that the scheme can stop as soon as its own
    // solution is proven optimal while the order in which solutions reach
    // the shared one stays deterministic (they are replayed at the end).
    // These local formatters share silenced parameters (no messages, no JSON
    // output, no new solution callback). A 'unique_ptr' is used since an
    // 'AlgorithmFormatter' is neither copyable nor movable.
    bool deterministic = (parameters.optimization_mode == OptimizationMode::NotAnytimeDeterministic);
    packingsolver::Parameters<Instance, Solution, Output> local_parameters;
    local_parameters.verbosity_level = 0;
    local_parameters.timer = parameters.timer;
    std::vector<std::unique_ptr<Output>> local_outputs;
    std::vector<std::unique_ptr<AlgorithmFormatter>> local_algorithm_formatters;
    for (double growth_factor: growth_factors) {
        for (GuideId guide_id: guides) {
            for (Direction direction: directions) {
                BranchingScheme::Parameters branching_scheme_parameters;
                branching_scheme_parameters.guide_id = guide_id;
                branching_scheme_parameters.direction = direction;
                if (parameters.optimization_mode == OptimizationMode::Anytime
                        || parameters.optimization_mode == OptimizationMode::Exact) {
                    branching_scheme_parameters.maximum_approximation_ratio
                        = parameters.initial_maximum_approximation_ratio;
                } else {
                    branching_scheme_parameters.maximum_approximation_ratio
                        = parameters.not_anytime_maximum_approximation_ratio;
                }
                branching_scheme_parameters_list.push_back(branching_scheme_parameters);
                treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters;
                ibs_parameters.verbosity_level = 0;
                ibs_parameters.timer = parameters.timer;
                ibs_parameters.timer.add_end_boolean(&algorithm_formatter.end_boolean());
                ibs_parameters.growth_factor = growth_factor;
                if (parameters.optimization_mode != OptimizationMode::Anytime
                        && parameters.optimization_mode != OptimizationMode::Exact) {
                    ibs_parameters.minimum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size;
                    ibs_parameters.maximum_size_of_the_queue
                        = parameters.not_anytime_tree_search_queue_size;
                }
                if (deterministic) {
                    local_outputs.push_back(std::make_unique<Output>(instance));
                    local_algorithm_formatters.push_back(std::make_unique<AlgorithmFormatter>(
                                instance,
                                local_parameters,
                                *local_outputs.back()));
                    ibs_parameters.timer.add_end_boolean(
                            &local_algorithm_formatters.back()->end_boolean());
                }
                ibs_parameters_list.push_back(ibs_parameters);
            }
        }
    }

    std::vector<std::function<void()>> tasks;
    std::forward_list<std::exception_ptr> exception_ptr_list;
    for (Counter scheme_idx = 0;
            scheme_idx < (Counter)branching_scheme_parameters_list.size();
            ++scheme_idx) {
        exception_ptr_list.push_front(std::exception_ptr());
        std::exception_ptr& exception_ptr = exception_ptr_list.front();
        BranchingScheme::Parameters bs_parameters = branching_scheme_parameters_list[scheme_idx];
        treesearchsolver::IterativeBeamSearch2Parameters<BranchingScheme> ibs_parameters = ibs_parameters_list[scheme_idx];
        AlgorithmFormatter* scheme_algorithm_formatter = (deterministic)?
            local_algorithm_formatters[(size_t)scheme_idx].get():
            &algorithm_formatter;
        tasks.push_back([&exception_ptr, &instance, &parameters,
                scheme_algorithm_formatter, bs_parameters, ibs_parameters]()
        {
            wrapper<decltype(&tree_search_worker), tree_search_worker>(
                    exception_ptr,
                    instance,
                    parameters,
                    *scheme_algorithm_formatter,
                    bs_parameters,
                    ibs_parameters,
                    [scheme_algorithm_formatter](
                            const Solution& solution,
                            const std::string& label)
                    {
                        scheme_algorithm_formatter->update_solution(solution, label);
                    });
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
