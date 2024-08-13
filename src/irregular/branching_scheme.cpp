#include "irregular/branching_scheme.hpp"

#include "irregular/polygon_trapezoidation.hpp"
#include "irregular/shape.hpp"

#include <iostream>

using namespace packingsolver;
using namespace packingsolver::irregular;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters)
{
    // Compute branching scheme bin types.
    bin_types_x_ = std::vector<BranchingSchemeBinType>(instance.number_of_bin_types());
    bin_types_y_ = std::vector<BranchingSchemeBinType>(instance.number_of_bin_types());
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        //std::cout << "bin_type_id " << bin_type_id << std::endl;
        const BinType& bin_type = instance.bin_type(bin_type_id);

        bin_types_x_[bin_type_id].x_min = bin_type.x_min;
        bin_types_x_[bin_type_id].x_max = bin_type.x_max;
        bin_types_x_[bin_type_id].y_min = bin_type.y_min;
        bin_types_x_[bin_type_id].y_max = bin_type.y_max;
        bin_types_y_[bin_type_id].x_min = bin_type.y_min;
        bin_types_y_[bin_type_id].x_max = bin_type.y_max;
        bin_types_y_[bin_type_id].y_min = bin_type.x_min;
        bin_types_y_[bin_type_id].y_max = bin_type.x_max;

        for (BranchingSchemeBinType* bb_bin_type: {&bin_types_x_[bin_type_id], &bin_types_y_[bin_type_id]}) {
            LengthDbl y_min = bb_bin_type->y_min - (bb_bin_type->y_max - bb_bin_type->y_min);
            LengthDbl y_max = bb_bin_type->y_max + (bb_bin_type->y_max - bb_bin_type->y_min);
            LengthDbl x_min = bb_bin_type->x_min - (bb_bin_type->x_max - bb_bin_type->x_min);
            LengthDbl x_max = bb_bin_type->x_max + (bb_bin_type->x_max - bb_bin_type->x_min);

            GeneralizedTrapezoid trapezoid_bottom(
                    y_min,
                    bb_bin_type->y_min,
                    bb_bin_type->x_min,
                    bb_bin_type->x_max,
                    bb_bin_type->x_min,
                    bb_bin_type->x_max);
            UncoveredTrapezoid defect_bottom(-1, trapezoid_bottom);
            bb_bin_type->defects.push_back(defect_bottom);

            GeneralizedTrapezoid trapezoid_left(
                    y_min,
                    y_max,
                    x_min,
                    bb_bin_type->x_min,
                    x_min,
                    bb_bin_type->x_min);
            UncoveredTrapezoid defect_left(-1, trapezoid_left);
            bb_bin_type->defects.push_back(defect_left);

            GeneralizedTrapezoid trapezoid_top(
                    bb_bin_type->y_max,
                    y_max,
                    x_min,
                    x_max,
                    x_min,
                    x_max);
            UncoveredTrapezoid defect_top(-1, trapezoid_top);
            bb_bin_type->defects.push_back(defect_top);
        }

        // Bin borders.
        Shape shape_border;
        ElementPos element_0_pos = 0;
        for (ElementPos element_pos = 0;
                element_pos < bin_type.shape.elements.size();
                ++element_pos) {
            const ShapeElement& shape_element = bin_type.shape.elements[element_pos];
            if (shape_element.start.x == bin_type.x_min) {
                element_0_pos = element_pos;
                break;
            }
        }
        // 0: left; 1: bottom; 2: right; 3: top.
        const ShapeElement& element_0 = bin_type.shape.elements[element_0_pos];
        int start_border = (element_0.start.y == bin_type.y_min)? 1: 0;
        LengthDbl start_coordinate = element_0.start.y;
        for (ElementPos element_pos = 0;
                element_pos < bin_type.shape.elements.size();
                ++element_pos) {
            const ShapeElement& element = bin_type.shape.elements[(element_0_pos + element_pos) % bin_type.shape.elements.size()];
            //std::cout << "element_pos " << ((element_0_pos + element_pos) % bin_type.shape.elements.size()) << " / " << bin_type.shape.elements.size() << ": " << element.to_string() << std::endl;
            shape_border.elements.push_back(element);
            bool close = false;
            if (start_border == 0) {
                if (equal(element.end.x, bin_type.x_min)) {
                    ShapeElement new_element;
                    new_element.type = ShapeElementType::LineSegment;
                    new_element.start = element.end;
                    new_element.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element);
                    close = true;
                    start_border = 0;
                } else if (equal(element.end.y, bin_type.y_min)) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {bin_type.x_min, bin_type.y_min};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                    close = true;
                    start_border = 1;
                }
            } else if (start_border == 1) {
                if (equal(element.end.y, bin_type.y_min)) {
                    ShapeElement new_element;
                    new_element.type = ShapeElementType::LineSegment;
                    new_element.start = element.end;
                    new_element.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element);
                    close = true;
                    start_border = 1;
                } else if (equal(element.end.x, bin_type.x_max)) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {bin_type.x_max, bin_type.y_min};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                    close = true;
                    start_border = 2;
                }
            } else if (start_border == 2) {
                if (equal(element.end.x, bin_type.x_max)) {
                    ShapeElement new_element;
                    new_element.type = ShapeElementType::LineSegment;
                    new_element.start = element.end;
                    new_element.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element);
                    close = true;
                    start_border = 2;
                } else if (equal(element.end.y, bin_type.y_max)) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {bin_type.x_max, bin_type.y_max};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                    close = true;
                    start_border = 3;
                }
            } else if (start_border == 3) {
                if (equal(element.end.y, bin_type.y_max)) {
                    ShapeElement new_element;
                    new_element.type = ShapeElementType::LineSegment;
                    new_element.start = element.end;
                    new_element.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element);
                    close = true;
                    start_border = 3;
                } else if (equal(element.end.x, bin_type.x_min)) {
                    ShapeElement new_element_1;
                    new_element_1.type = ShapeElementType::LineSegment;
                    new_element_1.start = element.end;
                    new_element_1.end = {bin_type.x_min, bin_type.y_max};
                    shape_border.elements.push_back(new_element_1);
                    ShapeElement new_element_2;
                    new_element_2.type = ShapeElementType::LineSegment;
                    new_element_2.start = new_element_1.end;
                    new_element_2.end = shape_border.elements[0].start;
                    shape_border.elements.push_back(new_element_2);
                    close = true;
                    start_border = 0;
                }
            }
            // New shape.
            if (close) {
                Shape reversed_shape = shape_border.reverse();
                {
                    Shape cleaned_shape = clean_shape(reversed_shape);
                    //std::cout << cleaned_shape.to_string(0) << std::endl;
                    if (cleaned_shape.elements.size() > 2) {
                        auto trapezoids = polygon_trapezoidation(cleaned_shape);
                        for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                            UncoveredTrapezoid defect(-1, trapezoid);
                            bin_types_x_[bin_type_id].defects.push_back(defect);
                        }
                    }
                }
                {
                    Shape sym_shape = reversed_shape.identity_line_axial_symmetry();
                    Shape cleaned_shape = clean_shape(reversed_shape);
                    if (cleaned_shape.elements.size() > 2) {
                        auto trapezoids = polygon_trapezoidation(cleaned_shape);
                        for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                            UncoveredTrapezoid defect(-1, trapezoid);
                            bin_types_y_[bin_type_id].defects.push_back(defect);
                        }
                    }
                }
                shape_border.elements.clear();
            }
        }

        // Bin defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            //std::cout << "defect_id " << defect_id << std::endl;
            const Defect& defect = bin_type.defects[defect_id];
            {
                Shape cleaned_shape = clean_shape(defect.shape);
                std::vector<Shape> cleaned_holes;
                for (const Shape& hole: defect.holes)
                    if (!striclty_lesser(hole.compute_area(), instance.smallest_item_area()))
                        cleaned_holes.push_back(clean_shape(hole));
                {
                    auto trapezoids = polygon_trapezoidation(
                            cleaned_shape,
                            cleaned_holes);
                    for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                        UncoveredTrapezoid defect(defect_id, trapezoid);
                        bin_types_x_[bin_type_id].defects.push_back(defect);
                    }
                }
            }
            {
                Shape sym_shape = defect.shape.identity_line_axial_symmetry();
                std::vector<Shape> sym_holes;
                for (const Shape& hole: defect.holes)
                    if (!striclty_lesser(hole.compute_area(), instance.smallest_item_area()))
                        sym_holes.push_back(hole.identity_line_axial_symmetry());

                Shape cleaned_shape = clean_shape(sym_shape);
                std::vector<Shape> cleaned_holes;
                for (const Shape& hole: sym_holes)
                    cleaned_holes.push_back(clean_shape(hole));

                auto trapezoids = polygon_trapezoidation(
                        cleaned_shape,
                        cleaned_holes);
                for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                    UncoveredTrapezoid defect(defect_id, trapezoid);
                    bin_types_y_[bin_type_id].defects.push_back(defect);
                }
            }
        }
    }

    // Compute item_types_rectangles_.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        //std::cout << "item_type_id " << item_type_id << std::endl;
        const ItemType& item_type = instance.item_type(item_type_id);
        for (const auto& angle_range: item_type.allowed_rotations) {
            //std::cout << "angle " << angle_range.first;
            TrapezoidSet trapezoid_set_x;
            trapezoid_set_x.item_type_id = item_type_id;
            trapezoid_set_x.angle = angle_range.first;
            for (const ItemShape& item_shape: item_type.shapes) {
                //std::cout << "item_shape " << item_shape.to_string(0) << std::endl;

                Shape rotated_shape = item_shape.shape.rotate(angle_range.first);
                std::vector<Shape> rotated_holes;
                for (const Shape& hole: item_shape.holes)
                    if (!striclty_lesser(hole.compute_area(), instance.smallest_item_area()))
                        rotated_holes.push_back(hole.rotate(angle_range.first));

                Shape cleaned_shape = clean_shape(rotated_shape);
                std::vector<Shape> cleaned_holes;
                for (const Shape& hole: rotated_holes)
                    cleaned_holes.push_back(clean_shape(hole));

                auto trapezoids = polygon_trapezoidation(
                        cleaned_shape,
                        cleaned_holes);
                std::sort(
                        trapezoids.begin(),
                        trapezoids.end(),
                        [](
                            const GeneralizedTrapezoid& trapezoid_1,
                            const GeneralizedTrapezoid& trapezoid_2)
                        {
                            return trapezoid_1.x_min() < trapezoid_2.x_min();
                        });
                trapezoid_set_x.shapes.push_back(trapezoids);
            }
            trapezoid_set_x.x_min = std::numeric_limits<LengthDbl>::infinity();
            trapezoid_set_x.x_max = -std::numeric_limits<LengthDbl>::infinity();
            trapezoid_set_x.y_min = std::numeric_limits<LengthDbl>::infinity();
            trapezoid_set_x.y_max = -std::numeric_limits<LengthDbl>::infinity();
            for (const auto& item_shape_trapezoids: trapezoid_set_x.shapes) {
                for (const GeneralizedTrapezoid& trapezoid: item_shape_trapezoids) {
                    if (trapezoid_set_x.x_min > trapezoid.x_bottom_left())
                        trapezoid_set_x.x_min = trapezoid.x_bottom_left();
                    if (trapezoid_set_x.x_min > trapezoid.x_top_left())
                        trapezoid_set_x.x_min = trapezoid.x_top_left();
                    if (trapezoid_set_x.x_max < trapezoid.x_bottom_right())
                        trapezoid_set_x.x_max = trapezoid.x_bottom_right();
                    if (trapezoid_set_x.x_max < trapezoid.x_top_right())
                        trapezoid_set_x.x_max = trapezoid.x_top_right();

                    if (trapezoid_set_x.y_min > trapezoid.y_bottom())
                        trapezoid_set_x.y_min = trapezoid.y_bottom();
                    if (trapezoid_set_x.y_max < trapezoid.y_top())
                        trapezoid_set_x.y_max = trapezoid.y_top();
                }
            }
            trapezoid_sets_x_.push_back(trapezoid_set_x);

            TrapezoidSet trapezoid_set_y;
            trapezoid_set_y.item_type_id = item_type_id;
            trapezoid_set_y.angle = angle_range.first;
            for (const ItemShape& item_shape: item_type.shapes) {

                Shape sym_shape = item_shape.shape.identity_line_axial_symmetry();
                std::vector<Shape> sym_holes;
                for (const Shape& hole: item_shape.holes)
                    if (!striclty_lesser(hole.compute_area(), instance.smallest_item_area()))
                        sym_holes.push_back(hole.identity_line_axial_symmetry());

                Shape rotated_shape = sym_shape.rotate(angle_range.first);
                std::vector<Shape> rotated_holes;
                for (const Shape& hole: sym_holes)
                    rotated_holes.push_back(hole.rotate(angle_range.first));

                Shape cleaned_shape = clean_shape(rotated_shape);
                std::vector<Shape> cleaned_holes;
                for (const Shape& hole: rotated_holes)
                    cleaned_holes.push_back(clean_shape(hole));

                auto trapezoids = polygon_trapezoidation(
                        cleaned_shape,
                        cleaned_holes);
                std::sort(
                        trapezoids.begin(),
                        trapezoids.end(),
                        [](
                            const GeneralizedTrapezoid& trapezoid_1,
                            const GeneralizedTrapezoid& trapezoid_2)
                        {
                            return trapezoid_1.x_min() < trapezoid_2.x_min();
                        });
                trapezoid_set_y.shapes.push_back(trapezoids);
            }
            trapezoid_set_y.x_min = std::numeric_limits<LengthDbl>::infinity();
            trapezoid_set_y.x_max = -std::numeric_limits<LengthDbl>::infinity();
            trapezoid_set_y.y_min = std::numeric_limits<LengthDbl>::infinity();
            trapezoid_set_y.y_max = -std::numeric_limits<LengthDbl>::infinity();
            for (const auto& item_shape_trapezoids: trapezoid_set_y.shapes) {
                for (const GeneralizedTrapezoid& trapezoid: item_shape_trapezoids) {
                    if (trapezoid_set_y.x_min > trapezoid.x_bottom_left())
                        trapezoid_set_y.x_min = trapezoid.x_bottom_left();
                    if (trapezoid_set_y.x_min > trapezoid.x_top_left())
                        trapezoid_set_y.x_min = trapezoid.x_top_left();
                    if (trapezoid_set_y.x_max < trapezoid.x_bottom_right())
                        trapezoid_set_y.x_max = trapezoid.x_bottom_right();
                    if (trapezoid_set_y.x_max < trapezoid.x_top_right())
                        trapezoid_set_y.x_max = trapezoid.x_top_right();

                    if (trapezoid_set_y.y_min > trapezoid.y_bottom())
                        trapezoid_set_y.y_min = trapezoid.y_bottom();
                    if (trapezoid_set_y.y_max < trapezoid.y_top())
                        trapezoid_set_y.y_max = trapezoid.y_top();
                }
            }
            trapezoid_sets_y_.push_back(trapezoid_set_y);
        }
    }

    //for (TrapezoidSetId trapezoid_set_id = 0;
    //        trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_x_.size();
    //        ++trapezoid_set_id) {
    //    const TrapezoidSet& trapezoid_set = trapezoid_sets_x_[trapezoid_set_id];
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
    //std::cout << "uncovered_trapezoids_cur:" << std::endl;
    //for (const UncoveredTrapezoid& uncovered_trapezoid: uncovered_trapezoids)
    //    std::cout << "* " << uncovered_trapezoid << std::endl;

    std::vector<UncoveredTrapezoid> new_uncovered_trapezoids;

    // Reserve for uncovered_trapezoids and extra_trapezoids.
    new_uncovered_trapezoids.reserve(uncovered_trapezoids.size() + 3);
    LengthDbl ys = new_trapezoid.y_bottom();
    LengthDbl ye = new_trapezoid.y_top();

    // Update uncovered_trapezoids.
    for (const BranchingScheme::UncoveredTrapezoid& uncovered_trapezoid: uncovered_trapezoids) {
        if (!striclty_greater(uncovered_trapezoid.trapezoid.y_top(), ys)) {
            UncoveredTrapezoid new_uncovered_trapezoid = uncovered_trapezoid;
            new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
        } else if (!striclty_greater(uncovered_trapezoid.trapezoid.y_bottom(), ys)) {
            if (striclty_lesser(uncovered_trapezoid.trapezoid.y_bottom(), ys)) {
                UncoveredTrapezoid new_uncovered_trapezoid(
                        uncovered_trapezoid.item_type_id,
                        uncovered_trapezoid.item_shape_pos,
                        uncovered_trapezoid.item_shape_trapezoid_pos,
                        GeneralizedTrapezoid(
                            uncovered_trapezoid.trapezoid.y_bottom(),
                            ys,
                            uncovered_trapezoid.trapezoid.x_bottom_left(),
                            uncovered_trapezoid.trapezoid.x_bottom_right(),
                            uncovered_trapezoid.trapezoid.x_left(ys),
                            uncovered_trapezoid.trapezoid.x_right(ys)));
                new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
            }

            UncoveredTrapezoid new_uncovered_trapezoid(
                    item_type_id,
                    item_shape_pos,
                    item_shape_trapezoid_pos,
                    new_trapezoid);
            new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);

            if (striclty_greater(uncovered_trapezoid.trapezoid.y_top(), ye)) {
                UncoveredTrapezoid new_uncovered_trapezoid(
                        uncovered_trapezoid.item_type_id,
                        uncovered_trapezoid.item_shape_pos,
                        uncovered_trapezoid.item_shape_trapezoid_pos,
                        GeneralizedTrapezoid(
                            ye,
                            uncovered_trapezoid.trapezoid.y_top(),
                            uncovered_trapezoid.trapezoid.x_left(ye),
                            uncovered_trapezoid.trapezoid.x_right(ye),
                            uncovered_trapezoid.trapezoid.x_top_left(),
                            uncovered_trapezoid.trapezoid.x_top_right()));
                new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
            }
        } else if (!striclty_lesser(uncovered_trapezoid.trapezoid.y_bottom(), ye)) {
            UncoveredTrapezoid new_uncovered_trapezoid = uncovered_trapezoid;
            new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
        } else {
            if (striclty_greater(uncovered_trapezoid.trapezoid.y_top(), ye)) {
                UncoveredTrapezoid new_uncovered_trapezoid(
                        uncovered_trapezoid.item_type_id,
                        uncovered_trapezoid.item_shape_pos,
                        uncovered_trapezoid.item_shape_trapezoid_pos,
                        GeneralizedTrapezoid(
                            ye,
                            uncovered_trapezoid.trapezoid.y_top(),
                            uncovered_trapezoid.trapezoid.x_left(ye),
                            uncovered_trapezoid.trapezoid.x_right(ye),
                            uncovered_trapezoid.trapezoid.x_top_left(),
                            uncovered_trapezoid.trapezoid.x_top_right()));
                new_uncovered_trapezoids.push_back(new_uncovered_trapezoid);
            }
        }
    }

    //std::cout << "uncovered_trapezoids:" << std::endl;
    //for (const UncoveredTrapezoid& uncovered_trapezoid: new_uncovered_trapezoids)
    //    std::cout << "* " << uncovered_trapezoid << std::endl;

    return new_uncovered_trapezoids;
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
    if (insertion.new_bin > 0) {  // New bin.
        node.number_of_bins = parent.number_of_bins + 1;
        node.last_bin_direction = (insertion.new_bin == 1)?
            Direction::X:
            Direction::Y;
    } else {  // Same bin.
        node.number_of_bins = parent.number_of_bins;
        node.last_bin_direction = parent.last_bin_direction;
    }

    const TrapezoidSet& trapezoid_set = (node.last_bin_direction == Direction::X)?
        trapezoid_sets_x_[insertion.trapezoid_set_id]:
        trapezoid_sets_y_[insertion.trapezoid_set_id];
    const auto& item_shape_trapezoids = trapezoid_set.shapes[insertion.item_shape_pos];
    const GeneralizedTrapezoid& shape_trapezoid = item_shape_trapezoids[insertion.item_shape_trapezoid_pos];
    //std::cout << "shape_trapezoid " << shape_trapezoid << std::endl;

    BinPos bin_pos = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const BranchingSchemeBinType& bb_bin_type = (o == Direction::X)?
        bin_types_x_[bin_type_id]:
        bin_types_y_[bin_type_id];

    // Reserve for uncovered_trapezoids and extra_trapezoids.
    ItemPos p = -1;
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
            ++item_shape_pos) {
        const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
        p += item_shape_trapezoids.size();
    }
    node.extra_trapezoids.reserve(parent.extra_trapezoids.size() + p);

    // Update uncovered_trapezoids.
    if (insertion.new_bin > 0) {  // New bin.
        UncoveredTrapezoid uncovered_trapezoid(GeneralizedTrapezoid(
                    bb_bin_type.y_min,
                    bb_bin_type.y_max,
                    bb_bin_type.x_min,
                    bb_bin_type.x_min,
                    bb_bin_type.x_min,
                    bb_bin_type.x_min));
        node.uncovered_trapezoids.push_back(uncovered_trapezoid);

        // Add extra trapezoids from defects.
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bb_bin_type.defects.size();
                ++defect_id) {
            //std::cout << "defect_id " << defect_id << " " << bb_bin_type.defects[defect_id] << std::endl;
            node.extra_trapezoids.push_back(bb_bin_type.defects[defect_id]);
        }

    } else {  // Same bin.

        node.uncovered_trapezoids = parent.uncovered_trapezoids;

    }

    // Add trapezoids of inserted trapezoid set to uncovered and extra
    // trapezoids.
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
            ++item_shape_pos) {
        const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
        for (TrapezoidPos item_shape_trapezoid_pos = 0;
                item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                ++item_shape_trapezoid_pos) {
            GeneralizedTrapezoid trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];
            //std::cout << "trapezoid " << trapezoid << std::endl;
            trapezoid.shift_right(insertion.x);
            trapezoid.shift_top(insertion.y);
            //std::cout << "shifted trapezoid " << trapezoid << std::endl;

            // Add the item to the skyline.
            if (item_shape_pos == insertion.item_shape_pos
                    && item_shape_trapezoid_pos == insertion.item_shape_trapezoid_pos
                    //&& !trapezoid.left_side_increasing_not_vertical()
                    ) {
                node.uncovered_trapezoids = add_trapezoid_to_skyline(
                        node.uncovered_trapezoids,
                        trapezoid_set.item_type_id,
                        item_shape_pos,
                        item_shape_trapezoid_pos,
                        trapezoid);
                continue;
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
                if (equal(xb_t, xb_ut) && equal(xt_t, xt_ut)) {
                    touching_height += (yt - yb);
                }
            }
            if (equal(touching_height, trapezoid.height())) {
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
            //std::cout << "add extra_trapezoid " << item_shape_trapezoids[item_shape_trapezoid_pos] << std::endl;
            //std::cout << "shifted " << extra_trapezoid << std::endl;
            node.extra_trapezoids.push_back(extra_trapezoid);
        }
    }

    // Update extra rectangles.
    if (insertion.new_bin == 0) {

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
                if (!striclty_greater(x_extra, x_uncov)) {
                    //std::cout << "covers" << std::endl;
                    covered_length += (yt - yb);
                }
            }
            //std::cout << "length " << extra_trapezoid.trapezoid.height()
            //    << " covered " << covered_length
            //    << std::endl;
            if (striclty_lesser(covered_length, extra_trapezoid.trapezoid.height())) {
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

    //std::cout << "extra_trapezoids:" << std::endl;
    //for (const UncoveredTrapezoid& extra_trapezoid: node.extra_trapezoids)
    //    std::cout << "* " << extra_trapezoid << std::endl;

    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_area and profit.
    node.item_number_of_copies = parent.item_number_of_copies;
    const ItemType& item_type = instance().item_type(trapezoid_set.item_type_id);
    node.item_number_of_copies[trapezoid_set.item_type_id]++;
    node.number_of_items = parent.number_of_items + 1;
    node.item_area = parent.item_area + item_type.area;
    //std::cout << "parent.item_area " << parent.item_area << " item_area " << item_type.area << std::endl;
    node.profit = parent.profit + item_type.profit;

    // Compute current_area, guide_area and width using uncovered_trapezoids.
    node.xs_max = (insertion.new_bin == 0)?
        std::max(parent.xs_max, insertion.x):
        insertion.x;
    node.current_area = instance_.previous_bin_area(bin_pos);
    node.guide_area = instance_.previous_bin_area(bin_pos) + node.xs_max * (bb_bin_type.y_max - bb_bin_type.y_min);
    for (auto it = node.uncovered_trapezoids.rbegin(); it != node.uncovered_trapezoids.rend(); ++it) {
        const GeneralizedTrapezoid& trapezoid = it->trapezoid;
        //std::cout << trapezoid << std::endl;
        node.current_area += trapezoid.area(bb_bin_type.x_min);
        //std::cout << "* " << trapezoid.area() << " " << trapezoid.area(0.0) << std::endl;
        //std::cout << "current_area: " << node.current_area << std::endl;
        if (node.xe_max < trapezoid.x_max())
            node.xe_max = trapezoid.x_max();
        if (trapezoid.x_max() != bb_bin_type.x_min
                && node.ye_max < trapezoid.y_top())
            node.ye_max = trapezoid.y_top();
        if (trapezoid.x_max() > node.xs_max)
            node.guide_area += trapezoid.area(node.xs_max);
    }
    // Add area from extra rectangles.
    for (const UncoveredTrapezoid& extra_trapezoid: node.extra_trapezoids) {
        if (extra_trapezoid.trapezoid.y_top() >= bb_bin_type.y_max + (bb_bin_type.y_max - bb_bin_type.y_min)
                || extra_trapezoid.trapezoid.y_bottom() <= bb_bin_type.y_min - (bb_bin_type.y_max - bb_bin_type.y_min)) {
            continue;
        }
        const GeneralizedTrapezoid& trapezoid = extra_trapezoid.trapezoid;
        node.current_area += trapezoid.area();
        //std::cout << trapezoid << std::endl;
        //std::cout << "current_area " << node.current_area << std::endl;
        if (extra_trapezoid.item_type_id != -1) {
            if (node.xe_max < trapezoid.x_max())
                node.xe_max = trapezoid.x_max();
            if (node.ye_max < trapezoid.y_top())
                node.ye_max = trapezoid.y_top();
        }
        if (trapezoid.x_max() > node.xs_max)
            node.guide_area += trapezoid.area(node.xs_max);
    }

    if (node.number_of_items == instance().number_of_items()) {
        node.current_area = (instance().previous_bin_area(bin_pos) + node.xe_max - bb_bin_type.x_min)
            * (bb_bin_type.y_max - bb_bin_type.y_min);
    }

    node.waste = node.current_area - node.item_area;

    {
        AreaDbl waste = instance().previous_bin_area(bin_pos) + node.xe_max * (bb_bin_type.y_max - bb_bin_type.y_min) - instance().item_area();
        if (node.waste < waste)
            node.waste = waste;
    }

    if (node.waste < -PSTOL) {
        for (Node* current_node = &node;
                current_node->parent != nullptr;
                current_node = current_node->parent.get()) {
            std::cout << current_node->trapezoid_set_id
                << " x " << current_node->x
                << " y " << current_node->y
                << std::endl;
        }
        std::cout << "uncovered_trapezoids" << std::endl;
        for (const UncoveredTrapezoid& uncovered_trapezoid: node.uncovered_trapezoids)
            std::cout << uncovered_trapezoid << std::endl;
        std::cout << "extra_trapezoids" << std::endl;
        for (const UncoveredTrapezoid& extra_trapezoid: node.extra_trapezoids)
            std::cout << extra_trapezoid << std::endl;
        Solution solution = to_solution(std::make_shared<Node>(node));
        solution.write("solution_irregular.json");
        throw std::runtime_error(
                "waste: " + std::to_string(node.waste)
                + "; number_of_items: " + std::to_string(node.number_of_items)
                + "; current_area: " + std::to_string(node.current_area)
                + "; item_area: " + std::to_string(node.item_area));
    }
    if (node.waste < 0.0)
        node.waste = 0.0;

    node.leftover_value = (bb_bin_type.x_max - bb_bin_type.x_min) * (bb_bin_type.y_max - bb_bin_type.y_min)
        - (node.xe_max - bb_bin_type.x_min) * (node.ye_max - bb_bin_type.y_min);

    node.id = node_id_++;
    return node;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& parent) const
{
    //if (parent->number_of_items == 20)
    //    exit(0);
    insertions(parent);
    std::vector<std::shared_ptr<Node>> cs(parent->children_insertions.size());
    for (Counter i = 0; i < (Counter)parent->children_insertions.size(); ++i) {
        cs[i] = std::make_shared<Node>(child_tmp(parent, parent->children_insertions[i]));
        //std::cout << cs[i]->id
        //    << " insertion " << parent->children_insertions[i]
        //    << std::endl;
    }
    return cs;
}

void BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    uncovered_trapezoids_cur_.clear();
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BranchingSchemeBinType& bb_bin_type = (parent->last_bin_direction == Direction::X)?
            bin_types_x_[bin_type_id]:
            bin_types_y_[bin_type_id];
        for (ItemPos uncovered_trapezoid_pos_cur = 0;
                uncovered_trapezoid_pos_cur < (ItemPos)parent->uncovered_trapezoids.size();
                ++uncovered_trapezoid_pos_cur) {
            GeneralizedTrapezoid uncovered_trapezoid = parent->uncovered_trapezoids[uncovered_trapezoid_pos_cur].trapezoid;
            uncovered_trapezoid.extend_left(bb_bin_type.x_min - (bb_bin_type.x_max - bb_bin_type.x_min));
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
    //    << " guide_area " << parent->guide_area
    //    << std::endl;

    // Add all previous insertions which are still valid.
    if (parent->parent != nullptr) {
        for (const Insertion& insertion: parent->parent->children_insertions) {
            if (!striclty_lesser(insertion.ys, parent->ye)
                    || !striclty_greater(insertion.ye, parent->ys)) {
                ItemTypeId item_type_id = trapezoid_sets_x_[insertion.trapezoid_set_id].item_type_id;
                const ItemType& item_type = instance().item_type(item_type_id);
                if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                    continue;
                //std::cout << "copy " << insertion << std::endl;
                parent->children_insertions.push_back(insertion);
                parent->children_insertions.back().new_bin = 0;
            }
        }
    }

    // Check number of items for each rectangle set.
    std::vector<TrapezoidSetId> valid_trapezoid_set_ids;
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_x_.size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set = trapezoid_sets_x_[trapezoid_set_id];

        if (parent->item_number_of_copies[trapezoid_set.item_type_id] + 1
                <= instance().item_type(trapezoid_set.item_type_id).copies) {
            valid_trapezoid_set_ids.push_back(trapezoid_set_id);
        }
    }

    // Insert in the current bin.
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        const std::vector<TrapezoidSet>& trapezoid_sets = (parent->last_bin_direction == Direction::X)?
            trapezoid_sets_x_:
            trapezoid_sets_y_;

        // Loop through rectangle sets.
        for (TrapezoidSetId trapezoid_set_id: valid_trapezoid_set_ids) {
            const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];

            // Loop through rectangles of the rectangle set.
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                    ++item_shape_pos) {
                const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
                for (TrapezoidPos item_shape_trapezoid_pos = 0;
                        item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                        ++item_shape_trapezoid_pos) {

                    for (ItemPos uncovered_trapezoid_pos = 0;
                            uncovered_trapezoid_pos < (ItemPos)parent->uncovered_trapezoids.size();
                            ++uncovered_trapezoid_pos) {
                        insertion_trapezoid_set(
                                parent,
                                trapezoid_set_id,
                                item_shape_pos,
                                item_shape_trapezoid_pos,
                                0,  // new_bin
                                uncovered_trapezoid_pos,
                                -1);  // extra_trapezoid_pos
                    }

                    // Extra rectangles.
                    for (ItemPos extra_trapezoid_pos = 0;
                            extra_trapezoid_pos < (ItemPos)parent->extra_trapezoids.size();
                            ++extra_trapezoid_pos) {
                        insertion_trapezoid_set(
                                parent,
                                trapezoid_set_id,
                                item_shape_pos,
                                item_shape_trapezoid_pos,
                                0,  // new_bin
                                -1,  // uncovered_trapezoid_pos
                                extra_trapezoid_pos);
                    }
                }
            }
        }
    }

    // Insert in a new bin.
    if (parent->children_insertions.empty() && parent->number_of_bins < instance().number_of_bins()) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins);
        const BinType& bin_type = instance().bin_type(bin_type_id);

        int new_bin = 0;
        if (parameters_.direction == Direction::X) {
            new_bin = 1;
        } else if (parameters_.direction == Direction::Y) {
            new_bin = 2;
        } else {
            new_bin = 1;
        }

        const std::vector<TrapezoidSet>& trapezoid_sets = (new_bin == 1)?
            trapezoid_sets_x_:
            trapezoid_sets_y_;
        const BranchingSchemeBinType& bb_bin_type = (new_bin == 1)?
            bin_types_x_[bin_type_id]:
            bin_types_y_[bin_type_id];
        // Loop through rectangle sets.
        for (TrapezoidSetId trapezoid_set_id: valid_trapezoid_set_ids) {
            //std::cout << "trapezoid_set_id " << trapezoid_set_id << std::endl;
            const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];

            // Loop through rectangles of the rectangle set.
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                    ++item_shape_pos) {
                //std::cout << "item_shape_pos " << item_shape_pos << std::endl;
                const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
                for (TrapezoidPos item_shape_trapezoid_pos = 0;
                        item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                        ++item_shape_trapezoid_pos) {
                    //std::cout << "item_shape_trapezoid_pos " << item_shape_trapezoid_pos << std::endl;

                    // Extra rectangles.
                    for (ItemPos extra_trapezoid_pos = 0;
                            extra_trapezoid_pos < (ItemPos)bb_bin_type.defects.size();
                            ++extra_trapezoid_pos) {
                        insertion_trapezoid_set(
                                parent,
                                trapezoid_set_id,
                                item_shape_pos,
                                item_shape_trapezoid_pos,
                                new_bin,
                                -1,  // uncovered_trapezoid_pos
                                extra_trapezoid_pos);
                    }
                }
            }
        }
    }
}

void BranchingScheme::init_position(
        const GeneralizedTrapezoid& item_shape_trapezoid,
        const GeneralizedTrapezoid& supporting_trapezoid,
        State& state,
        LengthDbl& xs,
        LengthDbl& ys) const
{
    if (supporting_trapezoid.left_side_increasing_not_vertical()) {
    //if (supporting_trapezoid.x_top_left() - supporting_trapezoid.x_bottom_left() >= supporting_trapezoid.width_top()) {
        if (item_shape_trapezoid.right_side_increasing_not_vertical()) {
            if (!striclty_greater(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
                state = State::ItemShapeTrapezoidRightSupportingTrapezoidBottomLeft;
                xs = supporting_trapezoid.x_bottom_left() - item_shape_trapezoid.x_top_right();
                ys = supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_top();
            } else {
                state = State::ItemShapeTrapezoidTopRightSupportingTrapezoidLeft;
                xs = supporting_trapezoid.x_bottom_left() - item_shape_trapezoid.x_top_right();
                ys = supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_top();
            }
        } else {
            state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidLeft;
            xs = supporting_trapezoid.x_bottom_left() - item_shape_trapezoid.x_bottom_right();
            ys = supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_bottom();
        }
    } else if (item_shape_trapezoid.right_side_increasing_not_vertical()) {
    //} else if (item_shape_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_right() >= item_shape_trapezoid.width_bottom()) {
        state = State::ItemShapeTrapezoidRightSupportingTrapezoidTopLeft;
        xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_top_right();
        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_top();
    } else if (!supporting_trapezoid.top_covered()) {
        state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidTop;
        xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right();
        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
    } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
    //} else if (supporting_trapezoid.x_bottom_right() - supporting_trapezoid.x_top_right() >= supporting_trapezoid.width_top()) {
        if (item_shape_trapezoid.left_side_decreasing_not_vertical()) {
            if (!striclty_greater(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
                state = State::ItemShapeTrapezoidLeftSupportingTrapezoidTopRight;
                xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
            } else {
                state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
                xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
            }
        } else {
            state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
            xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
            ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
        }
    } else {
        state = State::Infeasible;
    }
}

inline bool BranchingScheme::update_position(
        const GeneralizedTrapezoid& item_shape_trapezoid,
        const GeneralizedTrapezoid& supporting_trapezoid,
        const GeneralizedTrapezoid& trapezoid_to_avoid,
        GeneralizedTrapezoid& current_trapezoid,
        State& state,
        LengthDbl& xs,
        LengthDbl& ys) const
{
    //std::cout << "update_position" << std::endl
    //    << "    item_shape_trapezoid " << item_shape_trapezoid << std::endl
    //    << "    current_trapezoid " << current_trapezoid << std::endl
    //    << "    trapezoid_to_avoid " << trapezoid_to_avoid << std::endl
    //    << "        state " << (int)state << " xs " << xs << " ys " << ys << std::endl;

    bool updated = false;
    for (;;) {
        //std::cout << "update_position" << std::endl
        //    << "    item_shape_trapezoid " << item_shape_trapezoid << std::endl
        //    << "    current_trapezoid " << current_trapezoid << std::endl
        //    << "    trapezoid_to_avoid " << trapezoid_to_avoid << std::endl
        //    << "        state " << (int)state << " xs " << xs << " ys " << ys << std::endl;
        switch (state) {
        case State::ItemShapeTrapezoidRightSupportingTrapezoidBottomLeft: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid))
                return updated;
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / item_shape_trapezoid.a_right());
            if (equal(xs + lx, xs) && equal(ys + lx / item_shape_trapezoid.a_right(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_bottom_right(), supporting_trapezoid.x_bottom_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / item_shape_trapezoid.a_right());
                xs += lx;
                ys += lx / item_shape_trapezoid.a_right();
                return true;
            } else {
                state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidLeft;
                current_trapezoid.shift_right(supporting_trapezoid.x_bottom_left() - item_shape_trapezoid.x_bottom_right() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_bottom() - ys);
                xs = supporting_trapezoid.x_bottom_left() - item_shape_trapezoid.x_bottom_right();
                ys = supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_bottom();
            }
            break;
        } case State::ItemShapeTrapezoidTopRightSupportingTrapezoidLeft: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid))
                return updated;
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / supporting_trapezoid.a_left());
            if (equal(xs + lx, xs) && equal(ys + lx / supporting_trapezoid.a_left(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_top_right(), supporting_trapezoid.x_top_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / supporting_trapezoid.a_left());
                xs += lx;
                ys += lx / supporting_trapezoid.a_left();
                return true;
            } else if (!supporting_trapezoid.top_covered()) {
                state = State::ItemShapeTrapezoidRightSupportingTrapezoidTopLeft;
                current_trapezoid.shift_right(supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_top_right() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_top() - ys);
                xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_top_right();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_top();
            }
            break;
        } case State::ItemShapeTrapezoidBottomRightSupportingTrapezoidLeft: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid))
                return updated;
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / supporting_trapezoid.a_left());
            if (equal(xs + lx, xs) && equal(ys + lx / supporting_trapezoid.a_left(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_bottom_right(), supporting_trapezoid.x_top_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / supporting_trapezoid.a_left());
                xs += lx;
                ys += lx / supporting_trapezoid.a_left();
                return true;
            } else if (!supporting_trapezoid.top_covered()) {
                state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidTop;
                current_trapezoid.shift_right(supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
            } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
                if (item_shape_trapezoid.left_side_decreasing_not_vertical()) {
                    if (!striclty_lesser(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
                        state = State::ItemShapeTrapezoidLeftSupportingTrapezoidTopRight;
                        current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                        current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                        xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                    } else {
                        state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
                        current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                        current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                        xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                    }
                } else {
                    state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
                    current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                    current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                    xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                    ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                }
            } else {
                state = State::Infeasible;
                return false;
            }
            break;
        } case State::ItemShapeTrapezoidRightSupportingTrapezoidTopLeft: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid)) {
                //std::cout << "no intersect" << std::endl;
                return updated;
            }
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / item_shape_trapezoid.a_right());
            //std::cout << "lx " << lx << std::endl;
            if (equal(xs + lx, xs) && equal(ys + lx / item_shape_trapezoid.a_right(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_bottom_right(), supporting_trapezoid.x_top_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / item_shape_trapezoid.a_right());
                xs += lx;
                ys += lx / item_shape_trapezoid.a_right();
                return true;
            } else if (!supporting_trapezoid.top_covered()) {
                state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidTop;
                current_trapezoid.shift_right(supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
            } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
                if (item_shape_trapezoid.left_side_decreasing_not_vertical()) {
                    if (striclty_lesser(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
                        state = State::ItemShapeTrapezoidLeftSupportingTrapezoidTopRight;
                        current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                        current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                        xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                    } else {
                        state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
                        current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                        current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                        xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                    }
                } else {
                    state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
                    current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                    current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                    xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                    ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                }
            } else {
                state = State::Infeasible;
                return false;
            }
            break;
        } case State::ItemShapeTrapezoidBottomRightSupportingTrapezoidTop: {
            LengthDbl lx = current_trapezoid.compute_right_shift_if_intersects(trapezoid_to_avoid);
            if (equal(xs + lx, xs)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_top_right())) {
                current_trapezoid.shift_right(lx);
                xs += lx;
                return true;
            } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
            //} else if (supporting_trapezoid.x_bottom_right() - supporting_trapezoid.x_top_right() >= supporting_trapezoid.width_top()) {
                if (item_shape_trapezoid.left_side_decreasing_not_vertical()) {
                    if (striclty_lesser(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
                        state = State::ItemShapeTrapezoidLeftSupportingTrapezoidTopRight;
                        current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                        current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                        xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                    } else {
                        state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
                        current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                        current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                        xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                        ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                    }
                } else {
                    state = State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight;
                    current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left() - xs);
                    current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                    xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_bottom_left();
                    ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
                }
            } else {
                state = State::Infeasible;
                return false;
            }
            break;
        } case State::ItemShapeTrapezoidLeftSupportingTrapezoidTopRight: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid))
                return updated;
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / item_shape_trapezoid.a_left());
            if (equal(xs + lx, xs) && equal(ys + lx / item_shape_trapezoid.a_left(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_top_left(), supporting_trapezoid.x_top_right())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / item_shape_trapezoid.a_left());
                xs += lx;
                ys += lx / item_shape_trapezoid.a_left();
                return true;
            } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
                state = State::ItemShapeTrapezoidTopLeftSupportingTrapezoidRight;
                current_trapezoid.shift_right(supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_top_left() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_top() - ys);
                xs = supporting_trapezoid.x_top_right() - item_shape_trapezoid.x_top_left();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_top();
            } else {
                state = State::Infeasible;
                return false;
            }
            break;
        } case State::ItemShapeTrapezoidTopLeftSupportingTrapezoidRight: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid))
                return updated;
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / supporting_trapezoid.a_right());
            if (equal(xs + lx, xs) && equal(ys + lx / supporting_trapezoid.a_right(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_top_left(), supporting_trapezoid.x_bottom_right())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / supporting_trapezoid.a_right());
                xs += lx;
                ys += lx / supporting_trapezoid.a_right();
                return true;
            } else {
                state = State::Infeasible;
                return false;
            }
            break;
        } case State::ItemShapeTrapezoidBottomLeftSupportingTrapezoidRight: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid))
                return updated;
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / supporting_trapezoid.a_right());
            if (equal(xs + lx, xs) && equal(ys + lx / supporting_trapezoid.a_right(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_bottom_right())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / supporting_trapezoid.a_right());
                xs += lx;
                ys += lx / supporting_trapezoid.a_right();
                return true;
            } else if (item_shape_trapezoid.left_side_decreasing_not_vertical()
                    && striclty_greater(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
                state = State::ItemShapeTrapezoidLeftSupportingTrapezoidBottomRight;
                current_trapezoid.shift_right(supporting_trapezoid.x_bottom_right() - item_shape_trapezoid.x_bottom_left() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_bottom() - ys);
                xs = supporting_trapezoid.x_bottom_right() - item_shape_trapezoid.x_bottom_left();
                ys = supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_bottom();
            } else {
                state = State::Infeasible;
                return false;
            }
            break;
        } case State::ItemShapeTrapezoidLeftSupportingTrapezoidBottomRight: {
            if (!current_trapezoid.intersect(trapezoid_to_avoid))
                return updated;
            LengthDbl lx = current_trapezoid.compute_top_right_shift(
                    trapezoid_to_avoid,
                    1.0 / item_shape_trapezoid.a_left());
            if (equal(xs + lx, xs) && equal(ys + lx / item_shape_trapezoid.a_left(), ys)) {
                return updated;
            } else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_top_left(), supporting_trapezoid.x_bottom_right())) {
                xs += lx;
                ys += lx / item_shape_trapezoid.a_left();
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(lx / item_shape_trapezoid.a_left());
                return true;
            } else {
                state = State::Infeasible;
                return false;
            }
            break;
        } default: {
            std::cout << int(state) << std::endl;
            throw std::invalid_argument("update_position");
        }
        }
        //std::cout << "     ->"
        //    << " state " << (int)state
        //    << " xs " << xs
        //    << " ys " << ys
        //    << std::endl;
        updated = true;
    }
    //std::cout << "     ->"
    //    << " state " << (int)state
    //    << " xs " << xs
    //    << " ys " << ys
    //    << std::endl;
    return true;
}

void BranchingScheme::insertion_trapezoid_set(
        const std::shared_ptr<Node>& parent,
        TrapezoidSetId trapezoid_set_id,
        ItemShapePos item_shape_pos,
        TrapezoidPos item_shape_trapezoid_pos,
        int8_t new_bin,
        ItemPos uncovered_trapezoid_pos,
        ItemPos extra_trapezoid_pos) const
{
    //std::cout << "insertion_trapezoid_set " << trapezoid_set_id
    //    << " " << item_shape_pos
    //    << " " << item_shape_trapezoid_pos
    //    << " new_bin " << (int)new_bin
    //    << " utp " << uncovered_trapezoid_pos
    //    << " etp " << extra_trapezoid_pos
    //    << std::endl;

    BinTypeId bin_type_id = (new_bin == 0)?
        instance().bin_type_id(parent->number_of_bins - 1):
        instance().bin_type_id(parent->number_of_bins);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = (new_bin == 0)?
        parent->last_bin_direction:
        ((new_bin == 1)? Direction::X: Direction::Y);

    const std::vector<TrapezoidSet>& trapezoid_sets = (o == Direction::X)?
        trapezoid_sets_x_:
        trapezoid_sets_y_;
    const BranchingSchemeBinType& bb_bin_type = (o == Direction::X)?
        bin_types_x_[bin_type_id]:
        bin_types_y_[bin_type_id];
    const std::vector<UncoveredTrapezoid>& extra_trapezoids = (new_bin == 0)?
        parent->extra_trapezoids:
        bb_bin_type.defects;

    const GeneralizedTrapezoid& supporting_trapezoid = (uncovered_trapezoid_pos != -1)?
        parent->uncovered_trapezoids[uncovered_trapezoid_pos].trapezoid:
        extra_trapezoids[extra_trapezoid_pos].trapezoid;
    if (supporting_trapezoid.y_top() == bb_bin_type.y_max + (bb_bin_type.y_max - bb_bin_type.y_min))
        return;
    if (supporting_trapezoid.x_min() == supporting_trapezoid.x_max())
        return;
    //std::cout << "supporting_trapezoid " << supporting_trapezoid << std::endl;

    const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
    const ItemType& item_type = instance().item_type(trapezoid_set.item_type_id);
    const ItemShape& item_shape = item_type.shapes[item_shape_pos];
    const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
    const GeneralizedTrapezoid& item_shape_trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];

    // If inserting above a defect, check if the defect is impacting.
    if (extra_trapezoid_pos != -1) {
        DefectId defect_id = extra_trapezoids[extra_trapezoid_pos].defect_id;
        if (defect_id != -1) {
            if (instance().can_contain(
                        item_shape.quality_rule,
                        bin_type.defects[defect_id].type)) {
                return;
            }
        }
    }

    LengthDbl xs = 0;
    LengthDbl ys = 0;
    State state = State::Infeasible;

    //state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidTop;
    //xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right();
    //ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();

    init_position(
            item_shape_trapezoid,
            supporting_trapezoid,
            state,
            xs,
            ys);
    //std::cout << "intt_position" << std::endl
    //    << "        state " << (int)state << " xs " << xs << " ys " << ys << std::endl;
    if (state == State::Infeasible)
        return;

    if (parent->parent != nullptr) {
        if (striclty_greater(ys + trapezoid_set.y_min, parent->ye)
                || !striclty_greater(ys + trapezoid_set.y_max, parent->ys)) {
            //std::cout << "skip ys " << ys << std::endl;
            return;
        }
    }

    //xs = (std::max)(xs, bb_bin_type.x_min - item_shape_trapezoid.x_max());

    if (striclty_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max))
        return;
    //if (striclty_greater(ys + trapezoid_set.y_max, bb_bin_type.y_max))
    //    return;
    //if (striclty_lesser(ys + trapezoid_set.y_min, bb_bin_type.y_min))
    //    return;


    for (;;) {
        bool stop = true;

        // Handle intersections with skyline.
        if (new_bin == 0) {

            // Loop through trapezoids of the trapezoid set.
            for (ItemShapePos item_shape_cur_pos = 0;
                    item_shape_cur_pos < (ItemShapePos)trapezoid_set.shapes.size();
                    ++item_shape_cur_pos) {
                const auto& item_shape_trapezoids_cur = trapezoid_set.shapes[item_shape_cur_pos];
                for (TrapezoidPos item_shape_trapezoid_cur_pos = 0;
                        item_shape_trapezoid_cur_pos < (TrapezoidPos)item_shape_trapezoids_cur.size();
                        ++item_shape_trapezoid_cur_pos) {
                    GeneralizedTrapezoid item_shape_trapezoid_cur = item_shape_trapezoids_cur[item_shape_trapezoid_cur_pos];
                    item_shape_trapezoid_cur.shift_right(xs);
                    item_shape_trapezoid_cur.shift_top(ys);

                    // Skyline.
                    for (ItemPos uncovered_trapezoid_pos_cur = 0;
                            uncovered_trapezoid_pos_cur < (ItemPos)parent->uncovered_trapezoids.size();
                            ++uncovered_trapezoid_pos_cur) {

                        //LengthDbl lx = item_shape_trapezoid_cur.compute_right_shift_if_intersects(uncovered_trapezoids_cur_[uncovered_trapezoid_pos_cur]);
                        //if (equal(xs + lx, xs)) {
                        //} else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_top_right())) {
                        //    item_shape_trapezoid_cur.shift_right(lx);
                        //    xs += lx;
                        //    if (striclty_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max))
                        //        return;
                        //} else {
                        //    return;
                        //}

                        bool b = update_position(
                                item_shape_trapezoid,
                                supporting_trapezoid,
                                uncovered_trapezoids_cur_[uncovered_trapezoid_pos_cur],
                                item_shape_trapezoid_cur,
                                state,
                                xs,
                                ys);
                        if (state == State::Infeasible)
                            return;
                        if (b) {
                            stop = false;
                            if (striclty_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max))
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
                item_shape_trapezoid_cur.shift_right(xs);
                item_shape_trapezoid_cur.shift_top(ys);

                // Extra trapezoids.
                for (const UncoveredTrapezoid& extra_trapezoid: extra_trapezoids) {

                    //LengthDbl lx = item_shape_trapezoid_cur.compute_right_shift_if_intersects(extra_trapezoid.trapezoid);
                    //if (equal(xs + lx, xs)) {
                    //} else if (striclty_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_top_right())) {
                    //    item_shape_trapezoid_cur.shift_right(lx);
                    //    xs += lx;
                    //    if (striclty_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max))
                    //        return;
                    //    stop = false;
                    //} else {
                    //    return;
                    //}

                    bool b = update_position(
                            item_shape_trapezoid,
                            supporting_trapezoid,
                            extra_trapezoid.trapezoid,
                            item_shape_trapezoid_cur,
                            state,
                            xs,
                            ys);
                    if (state == State::Infeasible)
                        return;
                    if (b) {
                        if (striclty_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max))
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
    if (striclty_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max)) {
        return;
    }

    for (const Insertion& insertion: parent->children_insertions) {
        if (insertion.trapezoid_set_id == trapezoid_set_id
                && equal(insertion.x, xs)
                && equal(insertion.y, ys)) {
            return;
        }
    }

    Insertion insertion;
    insertion.trapezoid_set_id = trapezoid_set_id;
    insertion.item_shape_pos = item_shape_pos;
    insertion.item_shape_trapezoid_pos = item_shape_trapezoid_pos;
    insertion.x = xs;
    insertion.y = ys;
    insertion.ys = supporting_trapezoid.y_bottom() - item_shape_trapezoid.y_top() + trapezoid_set.y_min;
    insertion.ye = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() + trapezoid_set.y_max;
    insertion.new_bin = new_bin;
    parent->children_insertions.push_back(insertion);
    //std::cout << "y " << insertion.y << " x " << insertion.x << " -> ok" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    BranchingScheme::Node node;
    node.item_number_of_copies = std::vector<ItemPos>(instance_.number_of_item_types(), 0);
    node.id = node_id_++;
    return std::make_shared<Node>(node);
}

bool BranchingScheme::better(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        if (node_2->profit > node_1->profit)
            return false;
        if (node_2->profit < node_1->profit)
            return true;
        return node_2->waste > node_1->waste;
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
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
        return node_2->xe_max > node_1->xe_max;
    } case Objective::Knapsack: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'irregular::BranchingScheme'"
            << "does not support objective '" << instance_.objective() << "'.";
        throw std::logic_error(ss.str());
        return false;
    }
    }
}

bool BranchingScheme::bound(
        const std::shared_ptr<Node>& node_1,
        const std::shared_ptr<Node>& node_2) const
{
    switch (instance_.objective()) {
    case Objective::Default: {
        return false;
        //Profit ub = node_1->profit + knapsack_bounds_[instance_.packable_area() - node_1->current_area];
        //if (!leaf(node_2)) {
        //    return (ub <= node_2->profit);
        //} else {
        //    if (ub != node_2->profit)
        //        return (ub <= node_2->profit);
        //    return node_1->waste >= node_2->waste;
        //}
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_2))
            return false;
        BinPos bin_pos = -1;
        AreaDbl a = instance_.item_area() + node_1->waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance_.number_of_bins())
                return true;
            BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
            a -= instance_.bin_type(bin_type_id).area;
        }
        return (bin_pos + 1 >= node_2->number_of_bins);
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
        return std::max(
                node_1->xe_max,
                node_1->waste + instance_.item_area())
            / instance().x_max(instance_.bin_type(0), Direction::X) >= node_2->xe_max;
    } case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return std::max(
                node_1->xe_max,
                node_1->waste + instance_.item_area())
            / instance().y_max(instance_.bin_type(0), Direction::X) >= node_2->xe_max;
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        return false;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'irregular::BranchingScheme'"
            << "does not support objective '" << instance_.objective() << "'.";
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

        const TrapezoidSet& trapezoid_set = (current_node->last_bin_direction == Direction::X)?
            trapezoid_sets_x_[current_node->trapezoid_set_id]:
            trapezoid_sets_y_[current_node->trapezoid_set_id];
        Point bl_corner = (current_node->last_bin_direction == Direction::X)?
            Point{current_node->x, current_node->y}:
            Point{current_node->y, current_node->x};
        //std::cout << "bin_pos " << bin_pos
        //    << " item_type_id " << trapezoid_set_item.item_type_id
        //    << " bl_corner " << bl_corner.to_string()
        //    << " angle " << trapezoid_set_item.angle
        //    << std::endl;
        solution.add_item(
                bin_pos,
                trapezoid_set.item_type_id,
                bl_corner,
                trapezoid_set.angle);
    }

    if (node->last_bin_direction == Direction::X) {
        if (!equal(node->xe_max, solution.x_max())) {
            throw std::runtime_error(
                    "irregular::BranchingScheme::to_solution."
                    " node->xe_max: " + std::to_string(node->xe_max)
                    + "; solution.x_max(): " + std::to_string(solution.x_max())
                    + ".");
        }
        if (!equal(node->ye_max, solution.y_max())) {
            throw std::runtime_error(
                    "irregular::BranchingScheme::to_solution."
                    " node->ye_max: " + std::to_string(node->ye_max)
                    + "; solution.y_max(): " + std::to_string(solution.y_max())
                    + ".");
        }
    }
    if (node->last_bin_direction == Direction::Y) {
        if (!equal(node->ye_max, solution.x_max())) {
            throw std::runtime_error(
                    "irregular::BranchingScheme::to_solution."
                    " node->ye_max: " + std::to_string(node->ye_max)
                    + "; solution.x_max(): " + std::to_string(solution.x_max())
                    + ".");
        }
        if (!equal(node->xe_max, solution.y_max())) {
            throw std::runtime_error(
                    "irregular::BranchingScheme::to_solution."
                    " node->xe_max: " + std::to_string(node->xe_max)
                    + "; solution.y_max(): " + std::to_string(solution.y_max())
                    + ".");
        }
    }
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredTrapezoid& uncovered_trapezoid)
{
    os << "item_type_id " << uncovered_trapezoid.item_type_id
        << " defect_id " << uncovered_trapezoid.defect_id
        << " yb " << uncovered_trapezoid.trapezoid.y_bottom()
        << " yt " << uncovered_trapezoid.trapezoid.y_top()
        << " xbl " << uncovered_trapezoid.trapezoid.x_bottom_left()
        << " xbr " << uncovered_trapezoid.trapezoid.x_bottom_right()
        << " xtl " << uncovered_trapezoid.trapezoid.x_top_left()
        << " xtr " << uncovered_trapezoid.trapezoid.x_top_right()
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
            && (item_shape_pos == insertion.item_shape_pos)
            && (item_shape_trapezoid_pos == insertion.item_shape_trapezoid_pos)
            && (new_bin == insertion.new_bin)
            && (equal(x, insertion.x))
            && (equal(y, insertion.y))
            );
}

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "trapezoid_set_id " << insertion.trapezoid_set_id
        << " item_shape_pos " << insertion.item_shape_pos
        << " item_shape_trapezoid_pos " << insertion.item_shape_trapezoid_pos
        << " new_bin " << (int)insertion.new_bin
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
        << " current_area " << node.current_area
        << std::endl;
    os << "waste " << node.waste
        << " profit " << node.profit
        << std::endl;

    // item_number_of_copies
    os << "item_number_of_copies" << std::flush;
    for (ItemPos j_pos: node.item_number_of_copies)
        os << " " << j_pos;
    os << std::endl;

    return os;
}
