#include "irregular/trapezoid_set.hpp"

#include "shape/clean.hpp"
#include "shape/trapezoidation.hpp"
#include "shape/supports.hpp"

#include <fstream>

using namespace packingsolver;
using namespace packingsolver::irregular;

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

Shape packingsolver::irregular::convert_shape(
        Shape shape,
        Direction direction)
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

Point packingsolver::irregular::convert_point(
        const Point& point,
        Direction direction)
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
        return Point{point.y, -point.x};
    } else if (direction == Direction::TopToBottomThenLeftToRight) {
        return Point{-point.y, point.x};
    } else if (direction == Direction::TopToBottomThenRightToLeft) {
        return Point{-point.y, -point.x};
    }
    throw std::runtime_error(FUNC_SIGNATURE);
    return Point();
}

Point packingsolver::irregular::convert_point_back(
        const Point& point,
        Direction direction)
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

Shape packingsolver::irregular::convert_shape(
        const Shape& shape,
        Angle angle,
        bool mirror)
{
    Shape shape_new = (!mirror)?
        shape:
        shape.axial_symmetry_y_axis();
    shape_new = shape_new.rotate(angle);
    return shape_new;
}

Shape packingsolver::irregular::convert_shape(
        const Shape& shape,
        Angle angle,
        bool mirror,
        Direction direction)
{
    Shape shape_new = convert_shape(shape, angle, mirror);
    shape_new = convert_shape(shape_new, direction);
    return shape_new;
}

TrapezoidBinType packingsolver::irregular::build_trapezoid_bin_type(
        const Instance& instance,
        const SimplifiedInstance& simplified_instance,
        const std::vector<std::vector<ItemTypeRotation>>& item_types_rotations,
        BinTypeId bin_type_id,
        Direction direction)
{
    bool write_shapes = false;
    TrapezoidBinType trapezoid_bin_type;
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const SimplifiedBinType& simplified_bin_type = simplified_instance.bin_types[bin_type_id];
    Shape shape = convert_shape(bin_type.shape_scaled, direction);

    shape = shape::clean_extreme_slopes_inner(shape).front();

    AxisAlignedBoundingBox aabb = shape.compute_min_max();
    trapezoid_bin_type.x_min = aabb.x_min;
    trapezoid_bin_type.x_max = aabb.x_max;
    trapezoid_bin_type.y_min = aabb.y_min;
    trapezoid_bin_type.y_max = aabb.y_max;

    LengthDbl y_min = trapezoid_bin_type.y_min
        - (trapezoid_bin_type.y_max - trapezoid_bin_type.y_min)
        - instance.parameters().scale_value * instance.parameters().item_item_minimum_spacing;
    LengthDbl y_max = trapezoid_bin_type.y_max
        + (trapezoid_bin_type.y_max - trapezoid_bin_type.y_min)
        + instance.parameters().scale_value * instance.parameters().item_item_minimum_spacing;
    LengthDbl x_min = trapezoid_bin_type.x_min
        - (trapezoid_bin_type.x_max - trapezoid_bin_type.x_min)
        - instance.parameters().scale_value * instance.parameters().item_item_minimum_spacing;
    LengthDbl x_max = trapezoid_bin_type.x_max
        + (trapezoid_bin_type.x_max - trapezoid_bin_type.x_min)
        + instance.parameters().scale_value * instance.parameters().item_item_minimum_spacing;

    GeneralizedTrapezoid trapezoid_bottom(
            y_min,
            trapezoid_bin_type.y_min + instance.parameters().scale_value * bin_type.item_bin_minimum_spacing,
            trapezoid_bin_type.x_min,
            trapezoid_bin_type.x_max,
            trapezoid_bin_type.x_min,
            trapezoid_bin_type.x_max);
    DefectTrapezoid defect_bottom(-1, trapezoid_bottom);
    trapezoid_bin_type.defects.push_back(defect_bottom);

    GeneralizedTrapezoid trapezoid_top(
            trapezoid_bin_type.y_max - instance.parameters().scale_value * bin_type.item_bin_minimum_spacing,
            y_max,
            x_min,
            x_max,
            x_min,
            x_max);
    DefectTrapezoid defect_top(-1, trapezoid_top);
    trapezoid_bin_type.defects.push_back(defect_top);

    // Supports.
    {
        ShapeElement element;
        element.type = ShapeElementType::LineSegment;
        element.start.x = trapezoid_bin_type.x_min + instance.parameters().scale_value * bin_type.item_bin_minimum_spacing;
        element.start.y = trapezoid_bin_type.y_min + instance.parameters().scale_value * bin_type.item_bin_minimum_spacing;
        element.end.x = trapezoid_bin_type.x_max - instance.parameters().scale_value * bin_type.item_bin_minimum_spacing;
        element.end.y = trapezoid_bin_type.y_min + instance.parameters().scale_value * bin_type.item_bin_minimum_spacing;
        Shape supporting_shape;
        supporting_shape.elements.push_back(element);

        ShapePos supporting_part_pos = trapezoid_bin_type.supporting_parts.size();
        Support support;
        support.bin_type_id = bin_type_id;
        support.shape = supporting_shape;
        trapezoid_bin_type.supporting_parts.push_back(support);
        trapezoid_bin_type.bin_supporting_part_ids.push_back(supporting_part_pos);
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
            ShapePos supporting_part_pos = trapezoid_bin_type.supporting_parts.size();
            Support support;
            support.bin_type_id = bin_type_id;
            support.defect_id = -2;
            support.shape = supporting_part;
            trapezoid_bin_type.supporting_parts.push_back(support);
            trapezoid_bin_type.bin_supporting_part_ids.push_back(supporting_part_pos);
        }

        // Trapezoidation.
        auto trapezoids = trapezoidation(shape_inflated);
        for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
            DefectTrapezoid defect(
                    -1,
                    trapezoid);
            trapezoid_bin_type.defects.push_back(defect);
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
            ShapePos supporting_part_pos = trapezoid_bin_type.supporting_parts.size();
            Support support;
            support.bin_type_id = bin_type_id;
            support.defect_id = defect_id;
            support.shape = supporting_part;
            trapezoid_bin_type.supporting_parts.push_back(support);
            trapezoid_bin_type.bin_supporting_part_ids.push_back(supporting_part_pos);
        }

        for (ShapePos hole_pos = 0;
                hole_pos < (ShapePos)shape_inflated.holes.size();
                ++hole_pos) {
            const Shape& hole_deflated = shape_inflated.holes[hole_pos];

            // Supports.
            shape::ShapeSupports supports = shape::compute_shape_supports(hole_deflated, true);
            for (const Shape& supporting_part: supports.supporting_parts) {
                ShapePos supporting_part_pos = trapezoid_bin_type.supporting_parts.size();
                Support support;
                support.bin_type_id = bin_type_id;
                support.defect_id = defect_id;
                support.hole_pos = hole_pos;
                support.shape = supporting_part;
                trapezoid_bin_type.supporting_parts.push_back(support);
                trapezoid_bin_type.bin_supporting_part_ids.push_back(supporting_part_pos);
            }
        }

        // Trapezoidation.
        auto trapezoids = trapezoidation(shape_inflated);
        for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
            DefectTrapezoid defect(
                    defect_id,
                    trapezoid);
            trapezoid_bin_type.defects.push_back(defect);
        }
    }

    // Append fixed-item inflated trapezoids into each bin type's defects,
    // and add the fixed items' supporting surfaces so that items can be
    // placed on top of fixed items when opening a new bin.
    for (const FixedItem& fixed_item: bin_type.fixed_items) {
        Point bl_corner = convert_point(
                instance.parameters().scale_value * fixed_item.bl_corner,
                direction);
        const ItemType& fixed_item_type = instance.item_type(fixed_item.item_type_id);
        const SimplifiedItemType& simplified_item_type
            = simplified_instance.item_types[fixed_item.item_type_id];
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)fixed_item_type.shapes.size();
                ++item_shape_pos) {
            const Shape& simplified_inflated_shape
                = simplified_item_type.shapes[item_shape_pos].shape_inflated.shape;
            ShapeWithHoles shape_inflated;
            shape_inflated.shape = convert_shape(
                    simplified_inflated_shape,
                    fixed_item.angle,
                    fixed_item.mirror,
                    direction);
            shape_inflated = shape::clean_extreme_slopes_outer(shape_inflated.shape);
            for (ShapePos hole_pos = 0;
                    hole_pos < (ShapePos)simplified_item_type.shapes[item_shape_pos].shape_inflated.holes.size();
                    ++hole_pos) {
                const Shape& simplified_deflated_hole
                    = simplified_item_type.shapes[item_shape_pos].shape_inflated.holes[hole_pos];
                Shape hole_deflated = convert_shape(
                        simplified_deflated_hole,
                        fixed_item.angle,
                        fixed_item.mirror,
                        direction);
                auto res = shape::clean_extreme_slopes_inner(hole_deflated);
                for (const auto& hole: res)
                    shape_inflated.holes.push_back(hole);
            }
            // Add defect trapezoids (inflated shape shifted to bl_corner).
            auto trapezoids_inflated = trapezoidation(shape_inflated);
            for (const GeneralizedTrapezoid& trapezoid: trapezoids_inflated) {
                GeneralizedTrapezoid shifted = trapezoid;
                shifted.shift_right(bl_corner.x);
                shifted.shift_top(bl_corner.y);
                trapezoid_bin_type.defects.push_back(DefectTrapezoid(-1, shifted));
            }
            // Add supporting surfaces of the outer inflated shape,
            // shifted to bl_corner, so that items can be placed on
            // top of this fixed item when opening a new bin.
            std::vector<Shape> supporting_parts
                = shape::compute_shape_supports(shape_inflated.shape, false).supporting_parts;
            for (const Shape& supporting_part: supporting_parts) {
                ShapePos new_pos = (ShapePos)trapezoid_bin_type.supporting_parts.size();
                Support support;
                support.bin_type_id = bin_type_id;
                support.item_shape_pos = item_shape_pos;
                support.defect_id = -1;
                support.shape = supporting_part;
                support.shape.shift(bl_corner.x, bl_corner.y);
                support.aabb = support.shape.compute_min_max();
                trapezoid_bin_type.supporting_parts.push_back(support);
                trapezoid_bin_type.bin_supporting_part_ids.push_back(new_pos);
            }
            // Add supporting surfaces from holes of the inflated shape.
            for (ShapePos hole_pos = 0;
                    hole_pos < (ShapePos)shape_inflated.holes.size();
                    ++hole_pos) {
                const Shape& hole = shape_inflated.holes[hole_pos];
                std::vector<Shape> hole_supporting_parts
                    = shape::compute_shape_supports(hole, true).supporting_parts;
                for (const Shape& supporting_part: hole_supporting_parts) {
                    ShapePos new_pos = (ShapePos)trapezoid_bin_type.supporting_parts.size();
                    Support support;
                    support.bin_type_id = bin_type_id;
                    support.item_shape_pos = item_shape_pos;
                    support.hole_pos = hole_pos;
                    support.defect_id = -1;
                    support.shape = supporting_part;
                    support.shape.shift(bl_corner.x, bl_corner.y);
                    support.aabb = support.shape.compute_min_max();
                    trapezoid_bin_type.supporting_parts.push_back(support);
                    trapezoid_bin_type.bin_supporting_part_ids.push_back(new_pos);
                }
            }
        }
    }

    // Item types: build one trapezoid set per (item_type, rotation)
    // for this bin type.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        //std::cout << "item_type_id " << item_type_id << std::endl;
        const ItemType& item_type = instance.item_type(item_type_id);
        const SimplifiedItemType& simplified_item_type = simplified_instance.item_types[item_type_id];

        // Write item type.
        if (write_shapes) {
            item_type.write_svg(
                    "item_type_" + std::to_string(item_type_id)
                    + ".svg");
        }

        for (const ItemTypeRotation& rotation: item_types_rotations[item_type_id]) {
            //std::cout << "angle " << angle;
            TrapezoidSetId trapezoid_set_id = trapezoid_bin_type.trapezoid_sets.size();
            TrapezoidSet trapezoid_set;
            trapezoid_set.item_type_id = item_type_id;
            trapezoid_set.angle = rotation.angle;
            trapezoid_set.mirror = rotation.mirror;

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
                shape.shape = convert_shape(simplified_shape, rotation.angle, rotation.mirror, direction);
                shape = shape::clean_extreme_slopes_outer(shape.shape);
                for (ShapePos hole_pos = 0;
                        hole_pos < (ShapePos)simplified_item_type.shapes[item_shape_pos].shape.holes.size();
                        ++hole_pos) {
                    const Shape& simplified_hole = simplified_item_type.shapes[item_shape_pos].shape.holes[hole_pos];

                    Shape hole = convert_shape(simplified_hole, rotation.angle, rotation.mirror, direction);

                    auto res = shape::clean_extreme_slopes_inner(hole);

                    for (const auto& hole: res)
                        shape.holes.push_back(hole);
                }

                // Supports.
                std::vector<Shape> supported_parts
                    = shape::compute_shape_supports(shape.shape, false).supported_parts;
                for (const Shape& supported_part: supported_parts) {
                    ShapePos supported_part_pos = trapezoid_bin_type.supported_parts.size();
                    Support support;
                    support.trapezoid_set_id = trapezoid_set_id;
                    support.item_shape_pos = item_shape_pos;
                    support.shape = supported_part;
                    trapezoid_bin_type.supported_parts.push_back(support);
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
                        ShapePos supported_part_pos = trapezoid_bin_type.supported_parts.size();
                        Support support;
                        support.trapezoid_set_id = trapezoid_set_id;
                        support.item_shape_pos = item_shape_pos;
                        support.hole_pos = hole_pos;
                        support.shape = supported_part;
                        trapezoid_bin_type.supported_parts.push_back(support);
                        trapezoid_set.supported_parts.push_back(supported_part_pos);
                    }
                }

                // Write rotated item shape.
                if (write_shapes) {
                    std::string name = "item_type_" + std::to_string(item_type_id)
                        + "_x"
                        + "_" + std::to_string(item_shape_pos)
                        + "_mirror_" + std::to_string(rotation.mirror)
                        + "_rotated_" + std::to_string(rotation.angle)
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
                shape_inflated.shape = convert_shape(simplified_inflated_shape, rotation.angle, rotation.mirror, direction);
                shape_inflated = shape::clean_extreme_slopes_outer(shape_inflated.shape);
                for (ShapePos hole_pos = 0;
                        hole_pos < (ShapePos)simplified_item_type.shapes[item_shape_pos].shape_inflated.holes.size();
                        ++hole_pos) {
                    const Shape& simplified_deflated_hole = simplified_item_type.shapes[item_shape_pos].shape_inflated.holes[hole_pos];

                    Shape hole_deflated = convert_shape(simplified_deflated_hole, rotation.angle, rotation.mirror, direction);

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
                    ShapePos supporting_part_pos = trapezoid_bin_type.supporting_parts.size();
                    Support support;
                    support.trapezoid_set_id = trapezoid_set_id;
                    support.item_shape_pos = item_shape_pos;
                    support.shape = supporting_part;
                    //std::cout << "trapezoid_set_id " << trapezoid_set_id
                    //    << " item_shape_pos " << item_shape_pos
                    //    << std::endl;
                    //std::cout << support.shape.to_string(2) << std::endl;
                    trapezoid_bin_type.supporting_parts.push_back(support);
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
                        ShapePos supporting_part_pos = trapezoid_bin_type.supporting_parts.size();
                        Support support;
                        support.trapezoid_set_id = trapezoid_set_id;
                        support.item_shape_pos = item_shape_pos;
                        support.hole_pos = hole_pos;
                        support.shape = supporting_part;
                        trapezoid_bin_type.supporting_parts.push_back(support);
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

            trapezoid_bin_type.trapezoid_sets.push_back(trapezoid_set);
        }
    }

    // Compute axis-aligned bounding boxes for all supports of this bin type.
    for (Support& support: trapezoid_bin_type.supporting_parts)
        support.aabb = support.shape.compute_min_max();
    for (Support& support: trapezoid_bin_type.supported_parts)
        support.aabb = support.shape.compute_min_max();
    return trapezoid_bin_type;
}
