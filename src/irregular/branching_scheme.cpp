#include "irregular/branching_scheme.hpp"

#include "irregular/shape_simplification.hpp"

#include "shape/convex_hull.hpp"
#include "shape/clean.hpp"
#include "shape/trapezoidation.hpp"
#include "shape/supports.hpp"
#include "shape/shapes_intersections.hpp"

#include <iostream>

using namespace packingsolver;
using namespace packingsolver::irregular;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void TrapezoidSet::write_svg(
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

    LengthDbl width = (x_max - x_min);
    LengthDbl height = (y_max - y_min);

    std::string s = "<svg viewBox=\""
        + std::to_string(x_min)
        + " " + std::to_string(-y_min - height)
        + " " + std::to_string(width)
        + " " + std::to_string(height)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    // Loop through trapezoids of the trapezoid set.
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)shapes.size();
            ++item_shape_pos) {
        const auto& item_shape_trapezoids = shapes[item_shape_pos];
        for (TrapezoidPos item_shape_trapezoid_pos = 0;
                item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                ++item_shape_trapezoid_pos) {
            GeneralizedTrapezoid trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];
            file << "<g>" << std::endl;
            file << trapezoid.to_svg("blue");
            LengthDbl x = (trapezoid.x_max() + trapezoid.x_min()) / 2;
            LengthDbl y = (trapezoid.y_top() + trapezoid.y_bottom()) / 2;
            file << "<text x=\"" << std::to_string(x)
                << "\" y=\"" << std::to_string(-y)
                << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">"
                << std::to_string(item_shape_pos) << "," << std::to_string(item_shape_trapezoid_pos)
                << "</text>" << std::endl;
            file << "</g>" << std::endl;
        }
    }

    file << "</svg>" << std::endl;
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
    item_types_allowed_rotations_
        = std::vector<std::vector<Angle>>(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        std::vector<Angle> angles;
        for (const auto& range: item_type.allowed_rotations) {
            angles.push_back(range.first);
            angles.push_back(range.second);
        }
        for (const ItemShape& item_shape: item_type.shapes) {
            // Find all the angles that make a side of the shape parallel
            // to the x-axis or to the y-axis.
            for (const ShapeElement& element: item_shape.shape_scaled.shape.elements) {
                LengthDbl dx = element.end.x - element.start.x;
                LengthDbl dy = element.end.y - element.start.y;
                Angle phi = std::atan2(dx, dy) * 180 / M_PI;
                for (Angle a: {
                        phi,
                        90 - phi,
                        90 + phi,
                        180 - phi,
                        180 + phi,
                        180 + 90 - phi,
                        180 + 90 + phi,
                        180 + 180 - phi}) {
                    while (a < 0)
                        a += 360;
                    while (a >= 360)
                        a -= 360;
                    bool angle_ok = false;
                    for (auto range: item_type.allowed_rotations)
                        if (range.first <= a && a <= range.second)
                            angle_ok = true;
                    if (angle_ok)
                        angles.push_back(a);
                }
            }
            // Find all the angles that make a side of the convex hull
            // parallel to the x-axis or to the y-axis.
            Shape convex_hull = shape::convex_hull(item_shape.shape_scaled.shape);
            for (const ShapeElement& element: convex_hull.elements) {
                LengthDbl dx = element.end.x - element.start.x;
                LengthDbl dy = element.end.y - element.start.y;
                Angle phi = std::atan2(dx, dy) * 180 / M_PI;
                for (Angle a: {
                        phi,
                        90 - phi,
                        90 + phi,
                        180 - phi,
                        180 + phi,
                        180 + 90 - phi,
                        180 + 90 + phi,
                        180 + 180 - phi}) {
                    while (a < 0)
                        a += 360;
                    while (a >= 360)
                        a -= 360;
                    bool angle_ok = false;
                    for (auto range: item_type.allowed_rotations)
                        if (range.first <= a && a <= range.second)
                            angle_ok = true;
                    if (angle_ok)
                        angles.push_back(a);
                }
            }
        }
        std::sort(angles.begin(), angles.end());
        for (Angle angle: angles) {
            if (item_types_allowed_rotations_[item_type_id].empty()
                    || !equal(angle, item_types_allowed_rotations_[item_type_id].back())) {
                item_types_allowed_rotations_[item_type_id].push_back(angle);
            }
        }
    }

    //std::cout << "BranchingScheme" << std::endl;
    bool write_shapes = false;

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

        direction_data.bin_types = std::vector<BranchingSchemeBinType>(instance.number_of_bin_types());
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            //std::cout << "bin_type_id " << bin_type_id << std::endl;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            const SimplifiedBinType& simplified_bin_type = simplified_instance_.bin_types[bin_type_id];
            BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
            Shape shape = convert_shape(bin_type.shape_scaled, direction);

            shape = shape::clean_extreme_slopes_inner(shape).front();

            auto mm = shape.compute_min_max();
            bb_bin_type.x_min = mm.first.x;
            bb_bin_type.x_max = mm.second.x;
            bb_bin_type.y_min = mm.first.y;
            bb_bin_type.y_max = mm.second.y;

            LengthDbl y_min = bb_bin_type.y_min
                - (bb_bin_type.y_max - bb_bin_type.y_min)
                - instance_.parameters().scale_value * instance_.parameters().item_item_minimum_spacing;
            LengthDbl y_max = bb_bin_type.y_max
                + (bb_bin_type.y_max - bb_bin_type.y_min)
                + instance_.parameters().scale_value * instance_.parameters().item_item_minimum_spacing;
            LengthDbl x_min = bb_bin_type.x_min
                - (bb_bin_type.x_max - bb_bin_type.x_min)
                - instance_.parameters().scale_value * instance_.parameters().item_item_minimum_spacing;
            LengthDbl x_max = bb_bin_type.x_max
                + (bb_bin_type.x_max - bb_bin_type.x_min)
                + instance_.parameters().scale_value * instance_.parameters().item_item_minimum_spacing;

            GeneralizedTrapezoid trapezoid_bottom(
                    y_min,
                    bb_bin_type.y_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing,
                    bb_bin_type.x_min,
                    bb_bin_type.x_max,
                    bb_bin_type.x_min,
                    bb_bin_type.x_max);
            UncoveredTrapezoid defect_bottom(-1, trapezoid_bottom);
            bb_bin_type.defects.push_back(defect_bottom);

            GeneralizedTrapezoid trapezoid_top(
                    bb_bin_type.y_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing,
                    y_max,
                    x_min,
                    x_max,
                    x_min,
                    x_max);
            UncoveredTrapezoid defect_top(-1, trapezoid_top);
            bb_bin_type.defects.push_back(defect_top);

            // Supports.
            shape::ShapeSupports supports = shape::compute_shape_supports(shape, true);
            {
                ShapeElement element;
                element.type = ShapeElementType::LineSegment;
                element.start.x = bb_bin_type.x_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing;
                element.start.y = bb_bin_type.y_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing;
                element.end.x = bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing;
                element.end.y = bb_bin_type.y_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing;
                Shape supporting_shape;
                supporting_shape.elements.push_back(element);

                ShapePos supporting_part_pos = direction_data.supporting_parts.size();
                Support support;
                support.bin_type_id = bin_type_id;
                support.shape = supporting_shape;
                direction_data.supporting_parts.push_back(support);
                bb_bin_type.supporting_parts.push_back(supporting_part_pos);
            }

            for (DefectId border_pos = 0;
                    border_pos < (DefectId)simplified_bin_type.borders.size();
                    ++border_pos) {
                const Shape& simplified_inflated_shape = simplified_bin_type.borders[border_pos].shape_inflated.shape;

                ShapeWithHoles shape_inflated;
                shape_inflated.shape = convert_shape(simplified_inflated_shape, direction);
                shape_inflated.shape = shape::clean_extreme_slopes_outer(shape_inflated.shape).shape;

                // Supports.
                shape::ShapeSupports supports = shape::compute_shape_supports(shape_inflated.shape, false);
                for (const Shape& supporting_part: supports.supporting_parts) {
                    ShapePos supporting_part_pos = direction_data.supporting_parts.size();
                    Support support;
                    support.bin_type_id = bin_type_id;
                    support.defect_id = -2;
                    support.shape = supporting_part;
                    direction_data.supporting_parts.push_back(support);
                    bb_bin_type.supporting_parts.push_back(supporting_part_pos);
                }

                // Trapezoidation.
                auto trapezoids = trapezoidation(shape_inflated);
                for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                    UncoveredTrapezoid defect(
                            -1,
                            trapezoid);
                    bb_bin_type.defects.push_back(defect);
                }
            }

            // Bin defects.
            for (DefectId defect_id = 0;
                    defect_id < (DefectId)simplified_bin_type.defects.size();
                    ++defect_id) {
                //std::cout << "defect_id " << defect_id << std::endl;
                const Shape& simplified_inflated_shape = simplified_bin_type.defects[defect_id].shape_inflated.shape;
                ShapeWithHoles shape_inflated;
                shape_inflated.shape = convert_shape(simplified_inflated_shape, direction);
                shape_inflated = shape::clean_extreme_slopes_outer(shape_inflated.shape);
                for (ShapePos hole_pos = 0;
                        hole_pos < (ShapePos)simplified_bin_type.defects[defect_id].shape_inflated.holes.size();
                        ++hole_pos) {
                    const Shape& simplified_deflated_hole = simplified_bin_type.defects[defect_id].shape_inflated.holes[hole_pos];

                    Shape shape_deflated = convert_shape(simplified_deflated_hole, direction);

                    auto res = shape::clean_extreme_slopes_inner(shape_deflated);

                    for (const auto& hole: res)
                        shape_inflated.holes.push_back(hole);
                }

                // Supports.
                shape::ShapeSupports supports = shape::compute_shape_supports(shape_inflated.shape, false);
                for (const Shape& supporting_part: supports.supporting_parts) {
                    ShapePos supporting_part_pos = direction_data.supporting_parts.size();
                    Support support;
                    support.bin_type_id = bin_type_id;
                    support.defect_id = defect_id;
                    support.shape = supporting_part;
                    direction_data.supporting_parts.push_back(support);
                    bb_bin_type.supporting_parts.push_back(supporting_part_pos);
                }

                for (ShapePos hole_pos = 0;
                        hole_pos < (ShapePos)shape_inflated.holes.size();
                        ++hole_pos) {
                    const Shape& hole_deflated = shape_inflated.holes[hole_pos];

                    // Supports.
                    shape::ShapeSupports supports = shape::compute_shape_supports(hole_deflated, true);
                    for (const Shape& supporting_part: supports.supporting_parts) {
                        ShapePos supporting_part_pos = direction_data.supporting_parts.size();
                        Support support;
                        support.bin_type_id = bin_type_id;
                        support.defect_id = defect_id;
                        support.hole_pos = hole_pos;
                        support.shape = supporting_part;
                        direction_data.supporting_parts.push_back(support);
                        bb_bin_type.supporting_parts.push_back(supporting_part_pos);
                    }
                }

                // Trapezoidation.
                auto trapezoids = trapezoidation(shape_inflated);
                for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                    UncoveredTrapezoid defect(
                            defect_id,
                            trapezoid);
                    bb_bin_type.defects.push_back(defect);
                }
            }
        }

        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            //std::cout << "item_type_id " << item_type_id << std::endl;
            const ItemType& item_type = instance.item_type(item_type_id);
            const SimplifiedItemType& simplified_item_type = simplified_instance_.item_types[item_type_id];

            // Write item type.
            if (write_shapes) {
                item_type.write_svg(
                        "item_type_" + std::to_string(item_type_id)
                        + ".svg");
            }

            for (bool mirror: {false, true}) {
                if (mirror && !item_type.allow_mirroring)
                    continue;

                for (Angle angle: item_types_allowed_rotations_[item_type_id]) {
                    //std::cout << "angle " << angle;
                    TrapezoidSetId trapezoid_set_id = direction_data.trapezoid_sets.size();
                    TrapezoidSet trapezoid_set;
                    trapezoid_set.item_type_id = item_type_id;
                    trapezoid_set.angle = angle;
                    trapezoid_set.mirror = mirror;

                    for (ItemShapePos item_shape_pos = 0;
                            item_shape_pos < (ItemShapePos)item_type.shapes.size();
                            ++item_shape_pos) {
                        const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                        const Shape& simplified_shape = simplified_item_type.shapes[item_shape_pos].shape.shape;
                        if (write_shapes) {
                            simplified_shape.write_svg(
                                    "item_type_" + std::to_string(item_type_id)
                                    + "_shape_pos_" + std::to_string(item_shape_pos)
                                    + "_simplified.svg");
                        }

                        ShapeWithHoles shape;
                        shape.shape = convert_shape(simplified_shape, angle, mirror, direction);
                        shape = shape::clean_extreme_slopes_outer(shape.shape);
                        for (ShapePos hole_pos = 0;
                                hole_pos < (ShapePos)simplified_item_type.shapes[item_shape_pos].shape.holes.size();
                                ++hole_pos) {
                            const Shape& simplified_hole = simplified_item_type.shapes[item_shape_pos].shape.holes[hole_pos];

                            Shape hole = convert_shape(simplified_hole, angle, mirror, direction);

                            auto res = shape::clean_extreme_slopes_inner(hole);

                            for (const auto& hole: res)
                                shape.holes.push_back(hole);
                        }

                        // Supports.
                        std::vector<Shape> supported_parts
                            = shape::compute_shape_supports(shape.shape, false).supported_parts;
                        for (const Shape& supported_part: supported_parts) {
                            ShapePos supported_part_pos = direction_data.supported_parts.size();
                            Support support;
                            support.trapezoid_set_id = trapezoid_set_id;
                            support.item_shape_pos = item_shape_pos;
                            support.shape = supported_part;
                            direction_data.supported_parts.push_back(support);
                            trapezoid_set.supported_parts.push_back(supported_part_pos);
                        }

                        for (ShapePos hole_pos = 0;
                                hole_pos < (ShapePos)shape.holes.size();
                                ++hole_pos) {
                            const Shape& hole = shape.holes[hole_pos];

                            // Supports.
                            std::vector<Shape> supported_parts
                                = shape::compute_shape_supports(hole, true).supported_parts;
                            for (const Shape& supported_part: supported_parts) {
                                ShapePos supported_part_pos = direction_data.supported_parts.size();
                                Support support;
                                support.trapezoid_set_id = trapezoid_set_id;
                                support.item_shape_pos = item_shape_pos;
                                support.hole_pos = hole_pos;
                                support.shape = supported_part;
                                direction_data.supported_parts.push_back(support);
                                trapezoid_set.supported_parts.push_back(supported_part_pos);
                            }
                        }

                        // Write rotated item shape.
                        if (write_shapes) {
                            std::string name = "item_type_" + std::to_string(item_type_id)
                                + "_x"
                                + "_" + std::to_string(item_shape_pos)
                                + "_mirror_" + std::to_string(mirror)
                                + "_rotated_" + std::to_string(angle)
                                + ".svg";
                            shape.write_svg(name);
                        }

                        // Trapezoidation.
                        auto trapezoids = trapezoidation(shape);
                        trapezoid_set.shapes.push_back({});
                        for (const GeneralizedTrapezoid& trapezoid: trapezoids)
                            trapezoid_set.shapes.back().push_back(trapezoid);
                    }

                    for (ItemShapePos item_shape_pos = 0;
                            item_shape_pos < (ItemShapePos)item_type.shapes.size();
                            ++item_shape_pos) {
                        const Shape& simplified_inflated_shape = simplified_item_type.shapes[item_shape_pos].shape_inflated.shape;
                        //std::cout << "item_type_id " << item_type_id
                        //    << " item_shape_pos " << item_shape_pos
                        //    << " angle " << angle
                        //    << " mirror " << mirror
                        //    //<< " simplified_shape " << simplified_shape.to_string(2)
                        //    << std::endl;

                        if (write_shapes) {
                            simplified_inflated_shape.write_svg(
                                    "item_type_" + std::to_string(item_type_id)
                                    + "_shape_pos_" + std::to_string(item_shape_pos)
                                    + "_inflated_simplified.svg");
                        }

                        ShapeWithHoles shape_inflated;
                        shape_inflated.shape = convert_shape(simplified_inflated_shape, angle, mirror, direction);
                        shape_inflated = shape::clean_extreme_slopes_outer(shape_inflated.shape);
                        for (ShapePos hole_pos = 0;
                                hole_pos < (ShapePos)simplified_item_type.shapes[item_shape_pos].shape_inflated.holes.size();
                                ++hole_pos) {
                            const Shape& simplified_deflated_hole = simplified_item_type.shapes[item_shape_pos].shape_inflated.holes[hole_pos];

                            Shape hole_deflated = convert_shape(simplified_deflated_hole, angle, mirror, direction);

                            auto res = shape::clean_extreme_slopes_inner(hole_deflated);

                            for (const auto& hole: res)
                                shape_inflated.holes.push_back(hole);
                        }

                        if (write_shapes) {
                            simplified_inflated_shape.write_svg(
                                    "item_type_" + std::to_string(item_type_id)
                                    + "_shape_pos_" + std::to_string(item_shape_pos)
                                    + "_inflated_simplified_cleaned.svg");
                        }

                        // Supports.
                        // Supporting parts are computed on inflated shapes.
                        // Supported parts are computed on (original) non-inflated shapes.
                        std::vector<Shape> supporting_parts
                            = shape::compute_shape_supports(shape_inflated.shape, false).supporting_parts;
                        for (const Shape& supporting_part: supporting_parts) {
                            ShapePos supporting_part_pos = direction_data.supporting_parts.size();
                            Support support;
                            support.trapezoid_set_id = trapezoid_set_id;
                            support.item_shape_pos = item_shape_pos;
                            support.shape = supporting_part;
                            //std::cout << "trapezoid_set_id " << trapezoid_set_id
                            //    << " item_shape_pos " << item_shape_pos
                            //    << std::endl;
                            //std::cout << support.shape.to_string(2) << std::endl;
                            direction_data.supporting_parts.push_back(support);
                            trapezoid_set.supporting_parts.push_back(supporting_part_pos);
                        }

                        for (ShapePos hole_pos = 0;
                                hole_pos < (ShapePos)shape_inflated.holes.size();
                                ++hole_pos) {
                            const Shape& hole_deflated = shape_inflated.holes[hole_pos];

                            // Supports.
                            std::vector<Shape> supporting_parts
                                = shape::compute_shape_supports(hole_deflated, true).supporting_parts;
                            for (const Shape& supporting_part: supporting_parts) {
                                ShapePos supporting_part_pos = direction_data.supporting_parts.size();
                                Support support;
                                support.trapezoid_set_id = trapezoid_set_id;
                                support.item_shape_pos = item_shape_pos;
                                support.hole_pos = hole_pos;
                                support.shape = supporting_part;
                                direction_data.supporting_parts.push_back(support);
                                trapezoid_set.supporting_parts.push_back(supporting_part_pos);
                            }
                        }

                        // Trapezoidation.
                        auto trapezoids_inflated = trapezoidation(shape_inflated);
                        trapezoid_set.shapes_inflated.push_back({});
                        for (const GeneralizedTrapezoid& trapezoid: trapezoids_inflated)
                            trapezoid_set.shapes_inflated.back().push_back(trapezoid);
                    }

                    for (std::vector<GeneralizedTrapezoid>& item_shape_trapezoids: trapezoid_set.shapes) {
                        std::sort(
                                item_shape_trapezoids.begin(),
                                item_shape_trapezoids.end(),
                                [](
                                    const GeneralizedTrapezoid& trapezoid_1,
                                    const GeneralizedTrapezoid& trapezoid_2)
                                {
                                    return trapezoid_1.x_min() < trapezoid_2.x_min();
                                });
                    }

                    trapezoid_set.x_min = std::numeric_limits<LengthDbl>::infinity();
                    trapezoid_set.x_max = -std::numeric_limits<LengthDbl>::infinity();
                    trapezoid_set.y_min = std::numeric_limits<LengthDbl>::infinity();
                    trapezoid_set.y_max = -std::numeric_limits<LengthDbl>::infinity();
                    for (const auto& item_shape_trapezoids: trapezoid_set.shapes) {
                        for (const GeneralizedTrapezoid& trapezoid: item_shape_trapezoids) {
                            if (trapezoid_set.x_min > trapezoid.x_bottom_left())
                                trapezoid_set.x_min = trapezoid.x_bottom_left();
                            if (trapezoid_set.x_min > trapezoid.x_top_left())
                                trapezoid_set.x_min = trapezoid.x_top_left();
                            if (trapezoid_set.x_max < trapezoid.x_bottom_right())
                                trapezoid_set.x_max = trapezoid.x_bottom_right();
                            if (trapezoid_set.x_max < trapezoid.x_top_right())
                                trapezoid_set.x_max = trapezoid.x_top_right();

                            if (trapezoid_set.y_min > trapezoid.y_bottom())
                                trapezoid_set.y_min = trapezoid.y_bottom();
                            if (trapezoid_set.y_max < trapezoid.y_top())
                                trapezoid_set.y_max = trapezoid.y_top();
                        }
                    }

                    direction_data.trapezoid_sets.push_back(trapezoid_set);
                }
            }
        }

        for (Support& support: direction_data.supporting_parts) {
            auto mm = support.shape.compute_min_max();
            support.min = mm.first;
            support.max = mm.second;
        }
        for (Support& support: direction_data.supported_parts) {
            auto mm = support.shape.compute_min_max();
            support.min = mm.first;
            support.max = mm.second;
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
    //        << " x_min " << item_type.compute_min_max(trapezoid_set.angle).first.x
    //        << " x_max " << item_type.compute_min_max(trapezoid_set.angle).second.x
    //        << " y_min " << item_type.compute_min_max(trapezoid_set.angle).first.y
    //        << " y_max " << item_type.compute_min_max(trapezoid_set.angle).second.y
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
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)item_type.shapes.size();
                ++item_shape_pos) {
            const auto& item_shape = item_type.shapes[item_shape_pos];
            Shape convex_hull = shape::convex_hull(item_shape.shape_scaled.shape);
            AreaDbl convex_hull_area = convex_hull.compute_area();
            item_types_convex_hull_area_[item_type_id] += convex_hull_area;
        }
        //std::cout << "item_type_id " << item_type_id
        //    << " area " << item_type.area
        //    << " convex_hull_area " << item_types_convex_hull_area_[item_type_id]
        //    << std::endl;
    }
}

Shape BranchingScheme::convert_shape(
        Shape shape,
        Direction direction) const
{
    if (direction == Direction::LeftToRightThenTopToBottom) {
        shape = shape.axial_symmetry_x_axis();
    } else if (direction == Direction::RightToLeftThenBottomToTop) {
        shape = shape.axial_symmetry_y_axis();
    } else if (direction == Direction::RightToLeftThenTopToBottom) {
        shape = shape.axial_symmetry_x_axis();
        shape = shape.axial_symmetry_y_axis();
    } else if (direction == Direction::BottomToTopThenLeftToRight) {
        shape = shape.axial_symmetry_identity_line();
    } else if (direction == Direction::BottomToTopThenRightToLeft) {
        shape = shape.axial_symmetry_identity_line();
        shape = shape.axial_symmetry_x_axis();
    } else if (direction == Direction::TopToBottomThenLeftToRight) {
        shape = shape.axial_symmetry_identity_line();
        shape = shape.axial_symmetry_y_axis();
    } else if (direction == Direction::TopToBottomThenRightToLeft) {
        shape = shape.axial_symmetry_identity_line();
        shape = shape.axial_symmetry_y_axis();
        shape = shape.axial_symmetry_x_axis();
    }
    return shape;
}

Point BranchingScheme::convert_point_back(
        const Point& point,
        Direction direction) const
{
    if (direction == Direction::LeftToRightThenBottomToTop) {
        return Point{point.x, point.y};
    } else if (direction == Direction::LeftToRightThenTopToBottom) {
        return Point{point.x, -point.y};
    } else if (direction == Direction::RightToLeftThenBottomToTop) {
        return Point{-point.x, point.y};
    } else if (direction == Direction::RightToLeftThenTopToBottom) {
        return Point{-point.x, -point.y};
    } else if (direction == Direction::BottomToTopThenLeftToRight) {
        return Point{point.y, point.x};
    } else if (direction == Direction::BottomToTopThenRightToLeft) {
        return Point{-point.y, point.x};
    } else if (direction == Direction::TopToBottomThenLeftToRight) {
        return Point{point.y, -point.x};
    } else if (direction == Direction::TopToBottomThenRightToLeft) {
        return Point{-point.y, -point.x};
    }
    throw std::runtime_error(FUNC_SIGNATURE);
    return Point();
}

Shape BranchingScheme::convert_shape(
        const Shape& shape,
        Angle angle,
        bool mirror) const
{
    Shape shape_new = (!mirror)?
        shape:
        shape.axial_symmetry_y_axis();
    shape_new = shape_new.rotate(angle);
    return shape_new;
}

Shape BranchingScheme::convert_shape(
        const Shape& shape,
        Angle angle,
        bool mirror,
        Direction direction) const
{
    Shape shape_new = convert_shape(shape, angle, mirror);
    shape_new = convert_shape(shape_new, direction);
    return shape_new;
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
        node.number_of_bins = parent.number_of_bins + 1;
        node.last_bin_direction = (Direction)insertion.new_bin_direction;
    } else {  // Same bin.
        node.number_of_bins = parent.number_of_bins;
        node.last_bin_direction = parent.last_bin_direction;
    }

    const DirectionData& direction_data = directions_data_[(int)node.last_bin_direction];
    const TrapezoidSet& trapezoid_set = direction_data.trapezoid_sets[insertion.trapezoid_set_id];
    const Support& supporting_part = direction_data.supporting_parts[insertion.supporting_part_pos];
    Shape supporting_shape = supporting_part.shape;
    supporting_shape.shift(insertion.supporting_part_x, insertion.supporting_part_y);

    //std::cout << "shape_trapezoid " << shape_trapezoid << std::endl;

    BinPos bin_pos = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];

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

        // Add extra trapezoids from defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bb_bin_type.defects.size();
                ++defect_id) {
            //std::cout << "defect_id " << defect_id << " " << bb_bin_type.defects[defect_id] << std::endl;
            node.extra_trapezoids.push_back(bb_bin_type.defects[defect_id]);
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
                = direction_data.trapezoid_sets[node.trapezoid_set_id].shapes_inflated[item_shape_pos][item_shape_trapezoid_pos];
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

                bool add_to_skyline = false;
                for (ElementPos supporting_shape_element_pos = 0;
                        supporting_shape_element_pos < supporting_shape.elements.size();
                        ++supporting_shape_element_pos) {
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

    // Compute, guide_area and width using uncovered_trapezoids.
    node.xs_max = (insertion.new_bin_direction == Direction::Any)?  // Same bin
        std::max(parent.xs_max, insertion.x + trapezoid_set.x_min):
        insertion.x + trapezoid_set.x_min;
    node.guide_area = instance().previous_bin_area(bin_pos)
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

    // Compute node.xe_max and node.ye_max.
    Point xy = convert_point_back({node.x, node.y}, node.last_bin_direction);
    xy = (1.0 / instance().parameters().scale_value) * xy;
    auto mm = item_type.compute_min_max(
            trapezoid_set.angle,
            trapezoid_set.mirror);
    if (insertion.new_bin_direction == Direction::Any) {  // Same bin
        node.xe_max = std::max(parent.xe_max, xy.x + mm.second.x);
        node.ye_max = std::max(parent.ye_max, xy.y + mm.second.y);
    } else {
        node.xe_max = xy.x + mm.second.x;
        node.ye_max = xy.y + mm.second.y;
    }

    node.leftover_value = (bin_type.x_max - bin_type.x_min) * (bin_type.y_max - bin_type.y_min)
        - (node.xe_max - bin_type.x_min) * (node.ye_max - bin_type.y_min);

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
        BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
        const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];

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
    cs.reserve(parent->children_insertions.size());
    for (Counter i = 0; i < (Counter)parent->children_insertions.size(); ++i) {
        const Insertion& insertion = parent->children_insertions[i];

        //std::cout << "- insertion " << insertion << std::endl;
        cs.push_back(std::make_shared<Node>(child_tmp(parent, insertion)));
        //BinPos bin_pos = cs.back()->number_of_bins - 1;
        //Direction o = cs.back()->last_bin_direction;
        //const DirectionData& direction_data = directions_data_[(int)o];
        //BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
        //const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
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
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const DirectionData& direction_data = directions_data_[(int)parent->last_bin_direction];
        const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
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

    // Check number of items for each rectangle set.
    Direction d = (parameters_.direction != Direction::Any)?
        parameters_.direction:
        Direction::LeftToRightThenBottomToTop;
    std::vector<TrapezoidSetId> valid_trapezoid_set_ids;
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)directions_data_[(int)d].trapezoid_sets.size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set = directions_data_[(int)d].trapezoid_sets[trapezoid_set_id];

        if (parent->item_number_of_copies[trapezoid_set.item_type_id] + 1
                <= instance().item_type(trapezoid_set.item_type_id).copies) {
            valid_trapezoid_set_ids.push_back(trapezoid_set_id);
        }
    }

    // Insert in the current bin.
    if (parent->number_of_bins > 0) {

        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        const DirectionData& direction_data = directions_data_[(int)parent->last_bin_direction];
        const std::vector<TrapezoidSet>& trapezoid_sets = direction_data.trapezoid_sets;

        // Check all parents insertions.
        for (const Insertion& insertion: parent->parent->children_insertions) {
            // Check item quantity.
            ItemTypeId item_type_id = directions_data_[(int)d].trapezoid_sets[insertion.trapezoid_set_id].item_type_id;
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
        for (ShapePos supporting_part_pos: direction_data.trapezoid_sets[parent->trapezoid_set_id].supporting_parts) {
            const Support& supporting_part = direction_data.supporting_parts[supporting_part_pos];
            //std::cout << "supporting_part " << supporting_part.shape.to_string(0) << std::endl;

            // Loop through each supported part of each remaining item.
            for (TrapezoidSetId supported_trapezoid_set_id: valid_trapezoid_set_ids) {
                //std::cout << "trapezoid_set_id " << trapezoid_set_id << std::endl;
                const TrapezoidSet& supported_trapezoid_set = trapezoid_sets[supported_trapezoid_set_id];
                for (ShapePos supported_part_pos: supported_trapezoid_set.supported_parts) {
                    const Support& supported_part = direction_data.supported_parts[supported_part_pos];

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
    if (parent->children_insertions.empty() && parent->number_of_bins < instance().number_of_bins()) {
        //std::cout << "insert in a new bin" << std::endl;
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins);
        const BinType& bin_type = instance().bin_type(bin_type_id);

        Direction new_bin_direction = parameters_.direction;
        if (new_bin_direction == Direction::Any)
            new_bin_direction = Direction::LeftToRightThenBottomToTop;

        const DirectionData& direction_data = directions_data_[(int)new_bin_direction];
        const std::vector<TrapezoidSet>& trapezoid_sets = direction_data.trapezoid_sets;
        const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];

        // Create insertions on the supporting parts of the bin.
        for (ShapePos supporting_part_pos: bb_bin_type.supporting_parts) {
            const Support& supporting_part = direction_data.supporting_parts[supporting_part_pos];

            // Loop through each supported part of each remaining item.
            for (TrapezoidSetId supported_trapezoid_set_id: valid_trapezoid_set_ids) {
                const TrapezoidSet& supported_trapezoid_set = trapezoid_sets[supported_trapezoid_set_id];
                //std::cout << "trapezoid_set_id " << trapezoid_set_id << std::endl;

                // If inserting above defect, check if it is impacting.
                ItemTypeId supported_item_type_id = supported_trapezoid_set.item_type_id;
                const ItemType& supported_item_type = instance().item_type(supported_item_type_id);
                ItemShapePos item_shape_pos = -1;

                for (ShapePos supported_part_pos: direction_data.trapezoid_sets[supported_trapezoid_set_id].supported_parts) {
                    const Support& supported_part = direction_data.supported_parts[supported_part_pos];

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
                            new_bin_direction);
                }
            }
        }
    }
}

inline bool BranchingScheme::update_position(
        Insertion& insertion,
        const Shape& supporting_shape,
        const Shape& supported_shape,
        const GeneralizedTrapezoid& trapezoid_to_avoid,
        GeneralizedTrapezoid& current_trapezoid) const
{
    //std::cout << "update_position" << std::endl
    //    << "    insertion " << insertion << std::endl
    //    << "    current_trapezoid " << current_trapezoid << std::endl
    //    << "    trapezoid_to_avoid " << trapezoid_to_avoid << std::endl
    //    ;

    bool updated = false;
    for (;;) {
        if (!current_trapezoid.intersect(trapezoid_to_avoid))
            return updated;

        double direction = 0.0;
        Point point_limit = {0.0, 0.0};
        bool update_supporting_element = false;
        bool update_supported_element = false;
        if (insertion.supported_part_element_pos == supported_shape.elements.size()) {
            const ShapeElement& supporting_element = supporting_shape.elements[insertion.supporting_part_element_pos];
            double supporting_slope
                = (supporting_element.end.y - supporting_element.start.y)
                / (supporting_element.end.x - supporting_element.start.x);
            direction = supporting_slope;
            point_limit = supporting_element.end - supported_shape.elements.back().end;
            update_supporting_element = true;
            update_supported_element = false;
        } else if (insertion.supporting_part_element_pos == supporting_shape.elements.size()) {
            const ShapeElement& supported_element = supported_shape.elements[insertion.supported_part_element_pos];
            double supported_slope
                = (supported_element.end.y - supported_element.start.y)
                / (supported_element.end.x - supported_element.start.x);
            direction = supported_slope;
            point_limit = supporting_shape.elements.back().end - supported_element.end;
            update_supporting_element = false;
            update_supported_element = true;
        } else {
            const ShapeElement& supporting_element = supporting_shape.elements[insertion.supporting_part_element_pos];
            const ShapeElement& supported_element = supported_shape.elements[insertion.supported_part_element_pos];
            ShapeElement supported_element_shifted = supported_element;
            supported_element_shifted.shift(insertion.x, insertion.y);
            //std::cout << "supporting_element " << supporting_element.to_string() << std::endl
            //    << " supported_element " << supported_element.to_string() << std::endl
            //    << " shifted " << supported_element_shifted.to_string() << std::endl;
            double supporting_slope
                = (supporting_element.end.y - supporting_element.start.y)
                / (supporting_element.end.x - supporting_element.start.x);
            double supported_slope
                = (supported_element.end.y - supported_element.start.y)
                / (supported_element.end.x - supported_element.start.x);
            //std::cout << "supporting_slope " << supporting_slope
            //    << " supported_slope " << supported_slope << std::endl;
            if (equal(supporting_slope, supported_slope)) {
                direction = supported_slope;
                point_limit = supporting_element.end - supported_element.end;
                update_supporting_element = true;
                update_supported_element = true;
            } else if (supporting_slope > supported_slope) {
                direction = supporting_slope;
                point_limit = supporting_element.end - supported_element.start;
                update_supporting_element = true;
                update_supported_element = false;
            } else {
                direction = supported_slope;
                point_limit = supporting_element.start - supported_element.end;
                update_supporting_element = false;
                update_supported_element = true;
            }
        }

        LengthDbl lx = (direction == 0.0)?
            current_trapezoid.compute_right_shift_if_intersects(
                trapezoid_to_avoid):
            current_trapezoid.compute_top_right_shift(
                trapezoid_to_avoid,
                direction);
        LengthDbl ly = lx * direction;
        //std::cout << "direction " << direction
        //    << " point_limit " << point_limit.to_string()
        //    << " lx " << lx << " ly " << ly << std::endl;
        if (equal(insertion.x + lx, insertion.x) && equal(insertion.y + ly, insertion.y)) {
            return updated;
        } else if (!strictly_greater(insertion.x + lx, point_limit.x)) {
            // Same element.
            current_trapezoid.shift_right(lx);
            current_trapezoid.shift_top(ly);
            insertion.x += lx;
            insertion.y += ly;
            //std::cout << "     -> insertion " << insertion << std::endl;
            return true;
        } else {
            // Change element.
            if (update_supporting_element)
                insertion.supporting_part_element_pos++;
            if (update_supported_element)
                insertion.supported_part_element_pos++;
            if (insertion.supporting_part_element_pos == supporting_shape.elements.size()
                    && insertion.supported_part_element_pos == supported_shape.elements.size()) {
                return updated;
            }
            current_trapezoid.shift_right(point_limit.x - insertion.x);
            current_trapezoid.shift_top(point_limit.y - insertion.y);
            insertion.x = point_limit.x;
            insertion.y = point_limit.y;
            updated = true;
        }
        //std::cout << "     -> insertion " << insertion << std::endl;
    }
    return true;
}

void BranchingScheme::update_insertion(
        const std::shared_ptr<Node>& parent,
        const Insertion& insertion_orig) const
{
    //std::cout << "update_insertion " << insertion_orig << std::endl;

    BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction direction = parent->last_bin_direction;

    const DirectionData& direction_data = directions_data_[(int)direction];
    const std::vector<TrapezoidSet>& trapezoid_sets = direction_data.trapezoid_sets;
    const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
    const std::vector<UncoveredTrapezoid>& extra_trapezoids = parent->extra_trapezoids;
    const Support& supporting_part = direction_data.supporting_parts[insertion_orig.supporting_part_pos];
    Shape supporting_shape = supporting_part.shape;
    supporting_shape.shift(insertion_orig.supporting_part_x, insertion_orig.supporting_part_y);
    const Support& supported_part = direction_data.supported_parts[insertion_orig.supported_part_pos];

    const TrapezoidSet& trapezoid_set = trapezoid_sets[insertion_orig.trapezoid_set_id];
    const ItemType& item_type = instance().item_type(trapezoid_set.item_type_id);

    Insertion insertion = insertion_orig;
    insertion.new_bin_direction = Direction::Any;

    LengthDbl ys_tmp = supporting_part.min.y + insertion.supporting_part_y
        - (supported_part.max.y - trapezoid_set.y_min)
        - instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    LengthDbl ye_tmp = supporting_part.max.y + insertion.supporting_part_y
        + (trapezoid_set.y_max - supported_part.min.y)
        + instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    if (strictly_greater(ys_tmp, parent->ye)
            || strictly_lesser(ye_tmp, parent->ys)) {
        parent->children_insertions.push_back(insertion);
        return;
    }

    for (;;) {
        bool stop = true;

        // Loop through trapezoids of the trapezoid set.
        for (ItemShapePos item_shape_cur_pos = 0;
                item_shape_cur_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_cur_pos) {
            const auto& item_shape_trapezoids_cur = trapezoid_set.shapes[item_shape_cur_pos];
            for (TrapezoidPos item_shape_trapezoid_cur_pos = 0;
                    item_shape_trapezoid_cur_pos < (TrapezoidPos)item_shape_trapezoids_cur.size();
                    ++item_shape_trapezoid_cur_pos) {
                GeneralizedTrapezoid item_shape_trapezoid_cur = item_shape_trapezoids_cur[item_shape_trapezoid_cur_pos];
                item_shape_trapezoid_cur.shift_right(insertion.x);
                item_shape_trapezoid_cur.shift_top(insertion.y);

                // Skyline.
                for (ItemPos uncovered_trapezoid_pos_cur = 0;
                        uncovered_trapezoid_pos_cur < (ItemPos)parent->uncovered_trapezoids.size();
                        ++uncovered_trapezoid_pos_cur) {

                    bool b = update_position(
                            insertion,
                            supporting_shape,
                            supported_part.shape,
                            uncovered_trapezoids_cur_[uncovered_trapezoid_pos_cur],
                            item_shape_trapezoid_cur);
                    if (insertion.supporting_part_element_pos == supporting_part.shape.elements.size()
                            && insertion.supported_part_element_pos == supported_part.shape.elements.size()) {
                        return;
                    }
                    if (b) {
                        stop = false;
                        if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing))
                            return;
                    }
                }
            }
        }

        // Loop through trapezoids of the trapezoid set.
        for (ItemShapePos item_shape_cur_pos = 0;
                item_shape_cur_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_cur_pos) {
            const ItemShape& item_shape_cur = item_type.shapes[item_shape_cur_pos];
            const auto& item_shape_trapezoids_cur = trapezoid_set.shapes[item_shape_cur_pos];
            for (TrapezoidPos item_shape_trapezoid_cur_pos = 0;
                    item_shape_trapezoid_cur_pos < (TrapezoidPos)item_shape_trapezoids_cur.size();
                    ++item_shape_trapezoid_cur_pos) {
                GeneralizedTrapezoid item_shape_trapezoid_cur = item_shape_trapezoids_cur[item_shape_trapezoid_cur_pos];
                item_shape_trapezoid_cur.shift_right(insertion.x);
                item_shape_trapezoid_cur.shift_top(insertion.y);

                // Extra trapezoids.
                for (const UncoveredTrapezoid& extra_trapezoid: extra_trapezoids) {
                    bool b = update_position(
                            insertion,
                            supporting_shape,
                            supported_part.shape,
                            extra_trapezoid.trapezoid,
                            item_shape_trapezoid_cur);
                    if (insertion.supporting_part_element_pos == supporting_part.shape.elements.size()
                            && insertion.supported_part_element_pos == supported_part.shape.elements.size()) {
                        return;
                    }
                    if (b) {
                        if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing))
                            return;
                        stop = false;
                    }
                }
            }
        }

        if (stop)
            break;
    }

    // Check bin width.
    if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing)) {
        return;
    }

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
        Direction new_bin_direction) const
{
    //std::cout << "insertion_trapezoid_set " << trapezoid_set_id
    //    << " new_bin_direction " << (int)new_bin_direction
    //    << " spting " << supporting_part_pos
    //    << " " << supporting_part_x
    //    << " " << supporting_part_y
    //    << " spted " << supported_part_pos
    //    << std::endl;

    BinTypeId bin_type_id = (new_bin_direction == Direction::Any)?
        instance().bin_type_id(parent->number_of_bins - 1):
        instance().bin_type_id(parent->number_of_bins);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction direction = (new_bin_direction == Direction::Any)?
        parent->last_bin_direction:
        new_bin_direction;

    const DirectionData& direction_data = directions_data_[(int)direction];
    const std::vector<TrapezoidSet>& trapezoid_sets = direction_data.trapezoid_sets;
    const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
    const std::vector<UncoveredTrapezoid>& extra_trapezoids = (new_bin_direction == Direction::Any)?
        parent->extra_trapezoids:
        bb_bin_type.defects;
    const Support& supporting_part = direction_data.supporting_parts[supporting_part_pos];
    Shape supporting_shape = supporting_part.shape;
    supporting_shape.shift(supporting_part_x, supporting_part_y);
    const Support& supported_part = direction_data.supported_parts[supported_part_pos];
    const Shape& supported_shape = supported_part.shape;
    //std::cout << "supporting_shape" << std::endl;
    //std::cout << supporting_shape.to_string(0) << std::endl;
    //std::cout << "supported_shape" << std::endl;
    //std::cout << supported_shape.to_string(0) << std::endl;

    const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
    const ItemType& item_type = instance().item_type(trapezoid_set.item_type_id);

    Insertion insertion;
    insertion.trapezoid_set_id = trapezoid_set_id;
    insertion.supporting_part_pos = supporting_part_pos;
    insertion.supporting_part_element_pos = 0;
    insertion.supporting_part_x = supporting_part_x;
    insertion.supporting_part_y = supporting_part_y;
    insertion.supported_part_pos = supported_part_pos;
    insertion.supported_part_element_pos = 0;
    insertion.x = supporting_shape.elements.front().start.x - supported_shape.elements.front().start.x;
    insertion.y = supporting_shape.elements.front().start.y - supported_shape.elements.front().start.y;
    insertion.new_bin_direction = new_bin_direction;

    //if (parent->parent != nullptr
    //        && new_bin_direction == Direction::Any) {
    //    LengthDbl ys_tmp = supporting_part.min.y + supporting_part_y
    //        - supported_part.max.y
    //        + trapezoid_set.y_min
    //        - instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    //    LengthDbl ye_tmp = supporting_part.max.y + supporting_part_y
    //        - supported_part.min.y
    //        + trapezoid_set.y_max
    //        + instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    //    if (strictly_greater(ys_tmp, parent->ye)
    //            || strictly_lesser(ye_tmp, parent->ys)) {
    //        return;
    //    }
    //}

    //xs = (std::max)(xs, bb_bin_type.x_min - item_shape_trapezoid.x_max());

    if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing))
        return;
    //if (strictly_greater(ys + trapezoid_set.y_max, bb_bin_type.y_max))
    //    return;
    //if (strictly_lesser(ys + trapezoid_set.y_min, bb_bin_type.y_min))
    //    return;

    // Move the item to the right of the left side of the bin.
    for (;;) {
        bool stop = true;

        // Loop through trapezoids of the trapezoid set.
        for (ItemShapePos item_shape_cur_pos = 0;
                item_shape_cur_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_cur_pos) {
            const auto& item_shape_trapezoids_cur = trapezoid_set.shapes[item_shape_cur_pos];
            for (TrapezoidPos item_shape_trapezoid_cur_pos = 0;
                    item_shape_trapezoid_cur_pos < (TrapezoidPos)item_shape_trapezoids_cur.size();
                    ++item_shape_trapezoid_cur_pos) {
                GeneralizedTrapezoid item_shape_trapezoid_cur = item_shape_trapezoids_cur[item_shape_trapezoid_cur_pos];
                item_shape_trapezoid_cur.shift_right(insertion.x);
                item_shape_trapezoid_cur.shift_top(insertion.y);

                GeneralizedTrapezoid trapezoid_to_avoid(
                        std::min(
                            bb_bin_type.y_min,
                            item_shape_trapezoid_cur.y_bottom()),
                        std::max(
                            bb_bin_type.y_max,
                            item_shape_trapezoid_cur.y_top()),
                        std::min(
                            bb_bin_type.x_min - (bb_bin_type.x_max - bb_bin_type.x_min),
                            item_shape_trapezoid_cur.x_min()),
                        bb_bin_type.x_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing,
                        std::min(
                            bb_bin_type.x_min - (bb_bin_type.x_max - bb_bin_type.x_min),
                            item_shape_trapezoid_cur.x_min()),
                        bb_bin_type.x_min + instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing);

                bool b = update_position(
                        insertion,
                        supporting_shape,
                        supported_part.shape,
                        trapezoid_to_avoid,
                        item_shape_trapezoid_cur);
                if (insertion.supporting_part_element_pos == supporting_part.shape.elements.size()
                        && insertion.supported_part_element_pos == supported_part.shape.elements.size()) {
                    return;
                }
                if (b) {
                    stop = false;
                    if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing))
                        return;
                }
            }
        }

        if (stop)
            break;
    }

    for (;;) {
        bool stop = true;

        // Handle intersections with skyline.
        if (new_bin_direction == Direction::Any) {

            // Loop through trapezoids of the trapezoid set.
            for (ItemShapePos item_shape_cur_pos = 0;
                    item_shape_cur_pos < (ItemShapePos)trapezoid_set.shapes.size();
                    ++item_shape_cur_pos) {
                const auto& item_shape_trapezoids_cur = trapezoid_set.shapes[item_shape_cur_pos];
                for (TrapezoidPos item_shape_trapezoid_cur_pos = 0;
                        item_shape_trapezoid_cur_pos < (TrapezoidPos)item_shape_trapezoids_cur.size();
                        ++item_shape_trapezoid_cur_pos) {
                    GeneralizedTrapezoid item_shape_trapezoid_cur = item_shape_trapezoids_cur[item_shape_trapezoid_cur_pos];
                    item_shape_trapezoid_cur.shift_right(insertion.x);
                    item_shape_trapezoid_cur.shift_top(insertion.y);

                    // Skyline.
                    for (ItemPos uncovered_trapezoid_pos_cur = 0;
                            uncovered_trapezoid_pos_cur < (ItemPos)parent->uncovered_trapezoids.size();
                            ++uncovered_trapezoid_pos_cur) {
                        bool b = update_position(
                                insertion,
                                supporting_shape,
                                supported_part.shape,
                                uncovered_trapezoids_cur_[uncovered_trapezoid_pos_cur],
                                item_shape_trapezoid_cur);
                        if (insertion.supporting_part_element_pos == supporting_part.shape.elements.size()
                                && insertion.supported_part_element_pos == supported_part.shape.elements.size()) {
                            return;
                        }
                        if (b) {
                            stop = false;
                            if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing))
                                return;
                        }
                    }
                }
            }
        }


        // Loop through trapezoids of the trapezoid set.
        for (ItemShapePos item_shape_cur_pos = 0;
                item_shape_cur_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_cur_pos) {
            const ItemShape& item_shape_cur = item_type.shapes[item_shape_cur_pos];
            const auto& item_shape_trapezoids_cur = trapezoid_set.shapes[item_shape_cur_pos];
            for (TrapezoidPos item_shape_trapezoid_cur_pos = 0;
                    item_shape_trapezoid_cur_pos < (TrapezoidPos)item_shape_trapezoids_cur.size();
                    ++item_shape_trapezoid_cur_pos) {
                GeneralizedTrapezoid item_shape_trapezoid_cur = item_shape_trapezoids_cur[item_shape_trapezoid_cur_pos];
                item_shape_trapezoid_cur.shift_right(insertion.x);
                item_shape_trapezoid_cur.shift_top(insertion.y);

                // Extra trapezoids.
                for (const UncoveredTrapezoid& extra_trapezoid: extra_trapezoids) {
                    bool b = update_position(
                            insertion,
                            supporting_shape,
                            supported_part.shape,
                            extra_trapezoid.trapezoid,
                            item_shape_trapezoid_cur);
                    if (insertion.supporting_part_element_pos == supporting_part.shape.elements.size()
                            && insertion.supported_part_element_pos == supported_part.shape.elements.size()) {
                        return;
                    }
                    if (b) {
                        if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing))
                            return;
                        stop = false;
                    }
                }
            }
        }

        if (stop)
            break;
    }

    // Check bin width.
    if (strictly_greater(insertion.x + trapezoid_set.x_max, bb_bin_type.x_max - instance_.parameters().scale_value * bin_type.item_bin_minimum_spacing)) {
        return;
    }

    insertion.ys = supporting_part.min.y + supporting_part_y
        - (trapezoid_set.y_max - trapezoid_set.y_min)
        - instance_.parameters().scale_value * instance().parameters().item_item_minimum_spacing;
    insertion.ye = supporting_part.max.y + supporting_part_y
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
        return node_2->leftover_value < node_1->leftover_value;
    } case Objective::OpenDimensionX: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->xe_max > node_1->xe_max;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_1))
            return false;
        if (!leaf(node_2))
            return true;
        return node_2->ye_max > node_1->ye_max;
    } case Objective::Knapsack: {
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
    case Objective::Default: {
        return false;
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
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
        } else {
            return (node_1->leftover_value <= node_2->leftover_value);
        }
    } case Objective::Knapsack: {
        if (leaf(node_2))
            return true;
        return false;
    } case Objective::OpenDimensionX: {
        if (!leaf(node_2))
            return false;
        return node_1->xe_max >= node_2->xe_max;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return node_1->ye_max >= node_2->ye_max;
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

    Solution solution(instance());
    BinPos bin_pos = -1;
    for (auto current_node: descendents) {
        if (current_node->number_of_bins > solution.number_of_bins())
            bin_pos = solution.add_bin(instance().bin_type_id(current_node->number_of_bins - 1), 1);

        const DirectionData& direction_data = directions_data_[(int)current_node->last_bin_direction];
        const TrapezoidSet& trapezoid_set = direction_data.trapezoid_sets[current_node->trapezoid_set_id];
        Point bl_corner = convert_point_back({current_node->x, current_node->y}, current_node->last_bin_direction);
        bl_corner = (1.0 / instance().parameters().scale_value) * bl_corner;
        //std::cout << "bin_pos " << bin_pos
        //    << " item_type_id " << trapezoid_set.item_type_id
        //    << " bl_corner " << bl_corner.to_string()
        //    << " angle " << trapezoid_set.angle
        //    << std::endl;
        solution.add_item(
                bin_pos,
                trapezoid_set.item_type_id,
                bl_corner,
                trapezoid_set.angle,
                trapezoid_set.mirror);
    }

    if (node->number_of_bins > 0) {
        if (node->last_bin_direction == Direction::LeftToRightThenBottomToTop
                || node->last_bin_direction == Direction::BottomToTopThenLeftToRight) {
            if (!equal(node->xe_max, solution.x_max())) {
                solution.write("solution_irregular.json");
                throw std::runtime_error(
                        FUNC_SIGNATURE + "; "
                        "node->xe_max: " + std::to_string(node->xe_max) + "; "
                        "solution.x_max(): " + std::to_string(solution.x_max()) + "; "
                        "d: " + std::to_string((int)node->last_bin_direction) + ".");
            }
            if (!equal(node->ye_max, solution.y_max())) {
                throw std::runtime_error(
                        FUNC_SIGNATURE + "; "
                        "node->ye_max: " + std::to_string(node->ye_max) + "; "
                        "solution.y_max(): " + std::to_string(solution.y_max()) + "; "
                        "d: " + std::to_string((int)node->last_bin_direction) + ".");
            }
        }
    }
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void BranchingScheme::json_export_setup() const
{
    Direction d = (parameters_.direction != Direction::Any)?
        parameters_.direction:
        Direction::LeftToRightThenBottomToTop;
    const auto& trapezoid_sets = directions_data_[(int)d].trapezoid_sets;
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
    json_items_init_ids_ = std::vector<std::vector<std::unordered_map<double, Counter>>>(instance().number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance().number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance().item_type(item_type_id);
        json_items_init_ids_[item_type_id] = std::vector<std::unordered_map<double, Counter>>(2);
        for (bool mirror: {false, true}) {
            for (Angle angle: item_types_allowed_rotations_[item_type_id]) {
                json_items_init_ids_[item_type_id][mirror][angle] = json_counter_;
                json_counter_++;
            }
        }
    }
}

nlohmann::json BranchingScheme::json_export_init() const
{
    Direction d = (parameters_.direction != Direction::Any)?
        parameters_.direction:
        Direction::LeftToRightThenBottomToTop;
    const auto& trapezoid_sets = directions_data_[(int)d].trapezoid_sets;
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

        for (bool mirror: {false, true}) {

            for (Angle angle: item_types_allowed_rotations_[item_type_id]) {

                Counter json_counter = json_items_init_ids_[item_type_id][mirror][angle];

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

    const DirectionData& direction_data = directions_data_[(int)node->last_bin_direction];
    if (node->number_of_items == 0) {
        nlohmann::json json = {
            {"Id", node->id},
            {"ParentId", (node->parent == nullptr)? -1: node->parent->id},
        };
        return json;
    }

    const TrapezoidSet& trapezoid_set = direction_data.trapezoid_sets[node->trapezoid_set_id];
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
    BinTypeId bin_type_id = instance().bin_type_id(node->number_of_bins - 1);
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
        const DirectionData& direction_data = directions_data_[(int)node_tmp->last_bin_direction];
        const TrapezoidSet& trapezoid_set = direction_data.trapezoid_sets[node_tmp->trapezoid_set_id];
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
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const DirectionData& direction_data = directions_data_[(int)node->last_bin_direction];
    const BranchingSchemeBinType& bb_bin_type = direction_data.bin_types[bin_type_id];
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
