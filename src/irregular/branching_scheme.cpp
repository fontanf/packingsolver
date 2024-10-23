#include "irregular/branching_scheme.hpp"

#include "irregular/polygon_convex_hull.hpp"
#include "irregular/polygon_trapezoidation.hpp"
#include "irregular/polygon_simplification.hpp"
#include "irregular/shape.hpp"

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
                "Unable to open file \"" + file_path + "\".");
    }

    LengthDbl width = (x_max - x_min);
    LengthDbl height = (y_max - y_min);

    double factor = compute_svg_factor(width);

    std::string s = "<svg viewBox=\""
        + std::to_string(x_min * factor)
        + " " + std::to_string(-y_min * factor - height * factor)
        + " " + std::to_string(width * factor)
        + " " + std::to_string(height * factor)
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
            file << trapezoid.to_svg("blue", factor);
            LengthDbl x = (trapezoid.x_max() + trapezoid.x_min()) / 2;
            LengthDbl y = (trapezoid.y_top() + trapezoid.y_bottom()) / 2;
            file << "<text x=\"" << std::to_string(x * factor)
                << "\" y=\"" << std::to_string(-y * factor)
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
    parameters_(parameters)
{
    bool write_shapes = false;

    // Compute branching scheme bin types.
    bin_types_ = std::vector<std::vector<BranchingSchemeBinType>>(
            8,
            std::vector<BranchingSchemeBinType>(instance.number_of_bin_types()));
    trapezoid_sets_ = std::vector<std::vector<TrapezoidSet>>(8);

    for (Direction direction: {
            Direction::LeftToRightThenBottomToTop,
            Direction::LeftToRightThenTopToBottom,
            Direction::RightToLeftThenBottomToTop,
            Direction::RightToLeftThenTopToBottom,
            Direction::BottomToTopThenLeftToRight,
            Direction::BottomToTopThenRightToLeft,
            Direction::TopToBottomThenLeftToRight,
            Direction::TopToBottomThenRightToLeft}) {
        for (BinTypeId bin_type_id = 0;
                bin_type_id < instance.number_of_bin_types();
                ++bin_type_id) {
            //std::cout << "bin_type_id " << bin_type_id << std::endl;
            const BinType& bin_type = instance.bin_type(bin_type_id);
            BranchingSchemeBinType& bb_bin_type = bin_types_[(int)direction][bin_type_id];
            Shape bin_shape = bin_type.shape;
            if (direction == Direction::LeftToRightThenTopToBottom) {
                bin_shape = bin_shape.axial_symmetry_x_axis();
            } else if (direction == Direction::RightToLeftThenBottomToTop) {
                bin_shape = bin_shape.axial_symmetry_y_axis();
            } else if (direction == Direction::RightToLeftThenTopToBottom) {
                bin_shape = bin_shape.axial_symmetry_x_axis();
                bin_shape = bin_shape.axial_symmetry_y_axis();
            } else if (direction == Direction::BottomToTopThenLeftToRight) {
                bin_shape = bin_shape.axial_symmetry_identity_line();
            } else if (direction == Direction::BottomToTopThenRightToLeft) {
                bin_shape = bin_shape.axial_symmetry_identity_line();
                bin_shape = bin_shape.axial_symmetry_x_axis();
            } else if (direction == Direction::TopToBottomThenLeftToRight) {
                bin_shape = bin_shape.axial_symmetry_identity_line();
                bin_shape = bin_shape.axial_symmetry_y_axis();
            } else if (direction == Direction::TopToBottomThenRightToLeft) {
                bin_shape = bin_shape.axial_symmetry_identity_line();
                bin_shape = bin_shape.axial_symmetry_y_axis();
                bin_shape = bin_shape.axial_symmetry_x_axis();
            }

            auto mm = bin_shape.compute_min_max();
            bb_bin_type.x_min = mm.first.x;
            bb_bin_type.x_max = mm.second.x;
            bb_bin_type.y_min = mm.first.y;
            bb_bin_type.y_max = mm.second.y;

            LengthDbl y_min = bb_bin_type.y_min - (bb_bin_type.y_max - bb_bin_type.y_min);
            LengthDbl y_max = bb_bin_type.y_max + (bb_bin_type.y_max - bb_bin_type.y_min);
            LengthDbl x_min = bb_bin_type.x_min - (bb_bin_type.x_max - bb_bin_type.x_min);
            LengthDbl x_max = bb_bin_type.x_max + (bb_bin_type.x_max - bb_bin_type.x_min);

            GeneralizedTrapezoid trapezoid_bottom(
                    y_min,
                    bb_bin_type.y_min + instance.parameters().item_bin_minimum_spacing,
                    bb_bin_type.x_min,
                    bb_bin_type.x_max,
                    bb_bin_type.x_min,
                    bb_bin_type.x_max);
            UncoveredTrapezoid defect_bottom(-1, trapezoid_bottom);
            bb_bin_type.defects.push_back(defect_bottom);

            GeneralizedTrapezoid trapezoid_top(
                    bb_bin_type.y_max - instance.parameters().item_bin_minimum_spacing,
                    y_max,
                    x_min,
                    x_max,
                    x_min,
                    x_max);
            UncoveredTrapezoid defect_top(-1, trapezoid_top);
            bb_bin_type.defects.push_back(defect_top);
        }
    }

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        //std::cout << "bin_type_id " << bin_type_id << std::endl;
        const BinType& bin_type = instance.bin_type(bin_type_id);
        BranchingSchemeBinType& bb_bin_type_x = bin_types_[(int)Direction::LeftToRightThenBottomToTop][bin_type_id];
        BranchingSchemeBinType& bb_bin_type_y = bin_types_[(int)Direction::BottomToTopThenLeftToRight][bin_type_id];

        for (const Shape& shape_border: borders(bin_type.shape)) {
            {
                Shape cleaned_shape = clean_shape(shape_border);
                //std::cout << cleaned_shape.to_string(0) << std::endl;
                if (cleaned_shape.elements.size() > 2) {
                    auto trapezoids = polygon_trapezoidation(cleaned_shape);
                    for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                        UncoveredTrapezoid defect(
                                -1,
                                trapezoid.clean().inflate(instance.parameters().item_bin_minimum_spacing));
                        bb_bin_type_x.defects.push_back(defect);
                    }
                }
            }
            {
                Shape sym_shape = shape_border.axial_symmetry_identity_line();
                Shape cleaned_shape = clean_shape(sym_shape);
                if (cleaned_shape.elements.size() > 2) {
                    auto trapezoids = polygon_trapezoidation(cleaned_shape);
                    for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                        UncoveredTrapezoid defect(
                                -1,
                                trapezoid.clean().inflate(instance.parameters().item_bin_minimum_spacing));
                        bb_bin_type_y.defects.push_back(defect);
                    }
                }
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
                    if (!strictly_lesser(hole.compute_area(), instance.smallest_item_area()))
                        cleaned_holes.push_back(clean_shape(hole));
                {
                    auto trapezoids = polygon_trapezoidation(
                            cleaned_shape,
                            cleaned_holes);
                    for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                        UncoveredTrapezoid defect(
                                defect_id,
                                trapezoid.clean().inflate(instance.parameters().item_bin_minimum_spacing));
                        bb_bin_type_x.defects.push_back(defect);
                    }
                }
            }
            {
                Shape sym_shape = defect.shape.axial_symmetry_identity_line();
                std::vector<Shape> sym_holes;
                for (const Shape& hole: defect.holes)
                    if (!strictly_lesser(hole.compute_area(), instance.smallest_item_area()))
                        sym_holes.push_back(hole.axial_symmetry_identity_line());

                Shape cleaned_shape = clean_shape(sym_shape);
                std::vector<Shape> cleaned_holes;
                for (const Shape& hole: sym_holes)
                    cleaned_holes.push_back(clean_shape(hole));

                auto trapezoids = polygon_trapezoidation(
                        cleaned_shape,
                        cleaned_holes);
                for (const GeneralizedTrapezoid& trapezoid: trapezoids) {
                    UncoveredTrapezoid defect(
                            defect_id,
                            trapezoid.clean().inflate(instance.parameters().item_bin_minimum_spacing));
                    bb_bin_type_y.defects.push_back(defect);
                }
            }
        }
    }

    // Compute item_types_trapezoidss_.
    std::vector<TrapezoidSet>& trapezoid_sets_x = trapezoid_sets_[(int)Direction::LeftToRightThenBottomToTop];
    std::vector<TrapezoidSet>& trapezoid_sets_y = trapezoid_sets_[(int)Direction::BottomToTopThenLeftToRight];
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        //std::cout << "item_type_id " << item_type_id << std::endl;
        const ItemType& item_type = instance.item_type(item_type_id);

        // Write item type.
        if (write_shapes) {
            item_type.write_svg(
                    "item_type_" + std::to_string(item_type_id)
                    + ".svg");
        }

        for (bool mirror: {false, true}) {
            if (mirror && !item_type.allow_mirroring)
                continue;

            for (const auto& angle_range: item_type.allowed_rotations) {
                //std::cout << "angle " << angle_range.first;
                TrapezoidSet trapezoid_set_x;
                trapezoid_set_x.item_type_id = item_type_id;
                trapezoid_set_x.angle = angle_range.first;
                trapezoid_set_x.mirror = mirror;

                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                    //std::cout << "item_shape " << item_shape.to_string(0) << std::endl;

                    Shape shape = (!mirror)?
                        item_shape.shape:
                        item_shape.shape.axial_symmetry_y_axis();
                    std::vector<Shape> holes;
                    for (const Shape& hole: item_shape.holes) {
                        if (strictly_lesser(hole.compute_area(), instance.smallest_item_area()))
                            continue;
                        holes.push_back((!(mirror)?
                                hole:
                                hole.axial_symmetry_y_axis()));
                    }
                    // Write item shape.
                    if (write_shapes) {
                        std::string name = "item_type_" + std::to_string(item_type_id)
                                + "_" + std::to_string(item_shape_pos)
                                + "_mirror_" + std::to_string(mirror)
                                + ".svg";
                        irregular::write_svg(
                                item_shape.shape,
                                item_shape.holes,
                                name);
                    }

                    Shape rotated_shape = shape.rotate(angle_range.first);
                    std::vector<Shape> rotated_holes;
                    for (const Shape& hole: holes)
                        rotated_holes.push_back(hole.rotate(angle_range.first));
                    // Write rotated item shape.
                    if (write_shapes) {
                        std::string name = "item_type_" + std::to_string(item_type_id)
                                + "_x"
                                + "_" + std::to_string(item_shape_pos)
                                + "_mirror_" + std::to_string(mirror)
                                + "_rotated_" + std::to_string(angle_range.first)
                                + ".svg";
                        irregular::write_svg(
                                rotated_shape,
                                rotated_holes,
                                name);
                    }

                    Shape cleaned_shape = clean_shape(rotated_shape);
                    std::vector<Shape> cleaned_holes;
                    for (const Shape& hole: rotated_holes)
                        cleaned_holes.push_back(clean_shape(hole));
                    // Write cleaned item shape.
                    if (write_shapes) {
                        std::string name = "item_type_" + std::to_string(item_type_id)
                                + "_x"
                                + "_" + std::to_string(item_shape_pos)
                                + "_mirror_" + std::to_string(mirror)
                                + "_rotated_" + std::to_string(angle_range.first)
                                + "_cleaned.svg";
                        irregular::write_svg(
                                cleaned_shape,
                                cleaned_holes,
                                name);
                    }

                    auto trapezoids = polygon_trapezoidation(
                            cleaned_shape,
                            cleaned_holes);
                    trapezoid_set_x.shapes.push_back({});
                    for (const GeneralizedTrapezoid& trapezoid: trapezoids)
                        trapezoid_set_x.shapes.back().push_back(trapezoid.clean());
                }
                trapezoid_sets_x.push_back(trapezoid_set_x);

                TrapezoidSet trapezoid_set_y;
                trapezoid_set_y.item_type_id = item_type_id;
                trapezoid_set_y.angle = angle_range.first;
                trapezoid_set_y.mirror = mirror;
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type.shapes.size();
                        ++item_shape_pos) {
                    const ItemShape& item_shape = item_type.shapes[item_shape_pos];

                    Shape shape = (!mirror)?
                        item_shape.shape:
                        item_shape.shape.axial_symmetry_y_axis();
                    std::vector<Shape> holes;
                    for (const Shape& hole: item_shape.holes) {
                        if (strictly_lesser(hole.compute_area(), instance.smallest_item_area()))
                            continue;
                        holes.push_back((!(mirror)?
                                hole:
                                hole.axial_symmetry_y_axis()));
                    }

                    Shape rotated_shape = shape.rotate(angle_range.first);
                    std::vector<Shape> rotated_holes;
                    for (const Shape& hole: holes)
                        rotated_holes.push_back(hole.rotate(angle_range.first));

                    Shape sym_shape = rotated_shape.axial_symmetry_identity_line();
                    std::vector<Shape> sym_holes;
                    for (const Shape& hole: rotated_holes)
                        sym_holes.push_back(hole.axial_symmetry_identity_line());
                    // Write rotated item shape.
                    if (write_shapes) {
                        std::string name = "item_type_" + std::to_string(item_type_id)
                                + "_y"
                                + "_" + std::to_string(item_shape_pos)
                                + "_mirror_" + std::to_string(mirror)
                                + "_rotated_" + std::to_string(angle_range.first)
                                + ".svg";
                        irregular::write_svg(
                                rotated_shape,
                                rotated_holes,
                                name);
                    }

                    Shape cleaned_shape = clean_shape(sym_shape);
                    std::vector<Shape> cleaned_holes;
                    for (const Shape& hole: sym_holes)
                        cleaned_holes.push_back(clean_shape(hole));
                    // Write cleaned item shape.
                    if (write_shapes) {
                        std::string name = "item_type_" + std::to_string(item_type_id)
                                + "_y"
                                + "_" + std::to_string(item_shape_pos)
                                + "_mirror_" + std::to_string(mirror)
                                + "_rotated_" + std::to_string(angle_range.first)
                                + "_cleaned.svg";
                        irregular::write_svg(
                                cleaned_shape,
                                cleaned_holes,
                                name);
                    }

                    auto trapezoids = polygon_trapezoidation(
                            cleaned_shape,
                            cleaned_holes);
                    trapezoid_set_y.shapes.push_back({});
                    for (const GeneralizedTrapezoid& trapezoid: trapezoids)
                        trapezoid_set_y.shapes.back().push_back(trapezoid.clean());
                }
                trapezoid_sets_y.push_back(trapezoid_set_y);
            }
        }
    }

    trapezoid_sets_x = polygon_simplification(
            instance,
            trapezoid_sets_x);
    trapezoid_sets_y = polygon_simplification(
            instance,
            trapezoid_sets_y);

    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_x.size();
            ++trapezoid_set_id) {

        TrapezoidSet& trapezoid_set_x = trapezoid_sets_x[trapezoid_set_id];
        TrapezoidSet& trapezoid_set_y = trapezoid_sets_y[trapezoid_set_id];

        for (std::vector<GeneralizedTrapezoid>& item_shape_trapezoids: trapezoid_set_x.shapes) {
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

        for (std::vector<GeneralizedTrapezoid>& item_shape_trapezoids: trapezoid_set_y.shapes) {
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

        // Write trapezoidation.
        if (write_shapes) {
            std::string name = "item_type_" + std::to_string(trapezoid_set_x.item_type_id)
                + "_x"
                + "_mirror_" + std::to_string(trapezoid_set_x.mirror)
                + "_rotated_" + std::to_string(trapezoid_set_x.angle)
                + "_trapezoidation.svg";
            trapezoid_set_x.write_svg(
                    name);
        }
        if (write_shapes) {
            std::string name = "item_type_" + std::to_string(trapezoid_set_y.item_type_id)
                + "_y"
                + "_mirror_" + std::to_string(trapezoid_set_y.mirror)
                + "_rotated_" + std::to_string(trapezoid_set_y.angle)
                + "_trapezoidation.svg";
            trapezoid_set_y.write_svg(
                    name);
        }

    }

    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BranchingSchemeBinType& bb_bin_type_ref = bin_types_[(int)Direction::LeftToRightThenBottomToTop][bin_type_id];
        {
            BranchingSchemeBinType& bb_bin_type = bin_types_[(int)Direction::LeftToRightThenTopToBottom][bin_type_id];
            for (auto it_ref = bb_bin_type_ref.defects.begin() + bb_bin_type.defects.size();
                    it_ref != bb_bin_type_ref.defects.end();
                    ++it_ref) {
                const auto& defect_ref = *it_ref;
                UncoveredTrapezoid defect(
                        defect_ref.defect_id,
                        defect_ref.trapezoid.axial_symmetry_x_axis());
                bb_bin_type.defects.push_back(defect);
            }
        }
        {
            BranchingSchemeBinType& bb_bin_type = bin_types_[(int)Direction::RightToLeftThenBottomToTop][bin_type_id];
            for (auto it_ref = bb_bin_type_ref.defects.begin() + bb_bin_type.defects.size();
                    it_ref != bb_bin_type_ref.defects.end();
                    ++it_ref) {
                const auto& defect_ref = *it_ref;
                UncoveredTrapezoid defect(
                        defect_ref.defect_id,
                        defect_ref.trapezoid.axial_symmetry_y_axis());
                bb_bin_type.defects.push_back(defect);
            }
        }
        {
            BranchingSchemeBinType& bb_bin_type = bin_types_[(int)Direction::RightToLeftThenTopToBottom][bin_type_id];
            for (auto it_ref = bb_bin_type_ref.defects.begin() + bb_bin_type.defects.size();
                    it_ref != bb_bin_type_ref.defects.end();
                    ++it_ref) {
                const auto& defect_ref = *it_ref;
                UncoveredTrapezoid defect(
                        defect_ref.defect_id,
                        defect_ref.trapezoid.axial_symmetry_x_axis().axial_symmetry_y_axis());
                bb_bin_type.defects.push_back(defect);
            }
        }
    }
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_x.size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set_ref = trapezoid_sets_[(int)Direction::LeftToRightThenBottomToTop][trapezoid_set_id];
        {
            TrapezoidSet trapezoid_set;
            trapezoid_set.item_type_id = trapezoid_set_ref.item_type_id;
            trapezoid_set.angle = trapezoid_set_ref.angle;
            trapezoid_set.mirror = trapezoid_set_ref.mirror;
            trapezoid_set.x_min = trapezoid_set_ref.x_min;
            trapezoid_set.x_max = trapezoid_set_ref.x_max;
            trapezoid_set.y_min = -trapezoid_set_ref.y_max;
            trapezoid_set.y_max = -trapezoid_set_ref.y_min;
            for (const std::vector<GeneralizedTrapezoid>& shapes_ref: trapezoid_set_ref.shapes) {
                trapezoid_set.shapes.push_back({});
                for (const GeneralizedTrapezoid& shape_ref: shapes_ref)
                    trapezoid_set.shapes.back().push_back(shape_ref.axial_symmetry_x_axis());
            }
            trapezoid_sets_[(int)Direction::LeftToRightThenTopToBottom].push_back(trapezoid_set);
        }
        {
            TrapezoidSet trapezoid_set;
            trapezoid_set.item_type_id = trapezoid_set_ref.item_type_id;
            trapezoid_set.angle = trapezoid_set_ref.angle;
            trapezoid_set.mirror = trapezoid_set_ref.mirror;
            trapezoid_set.x_min = -trapezoid_set_ref.x_max;
            trapezoid_set.x_max = -trapezoid_set_ref.x_min;
            trapezoid_set.y_min = trapezoid_set_ref.y_min;
            trapezoid_set.y_max = trapezoid_set_ref.y_max;
            for (const std::vector<GeneralizedTrapezoid>& shapes_ref: trapezoid_set_ref.shapes) {
                trapezoid_set.shapes.push_back({});
                for (const GeneralizedTrapezoid& shape_ref: shapes_ref)
                    trapezoid_set.shapes.back().push_back(shape_ref.axial_symmetry_y_axis());
            }
            trapezoid_sets_[(int)Direction::RightToLeftThenBottomToTop].push_back(trapezoid_set);
        }
        {
            TrapezoidSet trapezoid_set;
            trapezoid_set.item_type_id = trapezoid_set_ref.item_type_id;
            trapezoid_set.angle = trapezoid_set_ref.angle;
            trapezoid_set.mirror = trapezoid_set_ref.mirror;
            trapezoid_set.x_min = -trapezoid_set_ref.x_max;
            trapezoid_set.x_max = -trapezoid_set_ref.x_min;
            trapezoid_set.y_min = -trapezoid_set_ref.y_max;
            trapezoid_set.y_max = -trapezoid_set_ref.y_min;
            for (const std::vector<GeneralizedTrapezoid>& shapes_ref: trapezoid_set_ref.shapes) {
                trapezoid_set.shapes.push_back({});
                for (const GeneralizedTrapezoid& shape_ref: shapes_ref)
                    trapezoid_set.shapes.back().push_back(shape_ref.axial_symmetry_x_axis().axial_symmetry_y_axis());
            }
            trapezoid_sets_[(int)Direction::RightToLeftThenTopToBottom].push_back(trapezoid_set);
        }
    }
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance.number_of_bin_types();
            ++bin_type_id) {
        const BranchingSchemeBinType& bb_bin_type_ref = bin_types_[(int)Direction::BottomToTopThenLeftToRight][bin_type_id];
        {
            BranchingSchemeBinType& bb_bin_type = bin_types_[(int)Direction::BottomToTopThenRightToLeft][bin_type_id];
            for (auto it_ref = bb_bin_type_ref.defects.begin() + bb_bin_type.defects.size();
                    it_ref != bb_bin_type_ref.defects.end();
                    ++it_ref) {
                const auto& defect_ref = *it_ref;
                UncoveredTrapezoid defect(
                        defect_ref.defect_id,
                        defect_ref.trapezoid.axial_symmetry_x_axis());
                bb_bin_type.defects.push_back(defect);
            }
        }
        {
            BranchingSchemeBinType& bb_bin_type = bin_types_[(int)Direction::TopToBottomThenLeftToRight][bin_type_id];
            for (auto it_ref = bb_bin_type_ref.defects.begin() + bb_bin_type.defects.size();
                    it_ref != bb_bin_type_ref.defects.end();
                    ++it_ref) {
                const auto& defect_ref = *it_ref;
                UncoveredTrapezoid defect(
                        defect_ref.defect_id,
                        defect_ref.trapezoid.axial_symmetry_y_axis());
                bb_bin_type.defects.push_back(defect);
            }
        }
        {
            BranchingSchemeBinType& bb_bin_type = bin_types_[(int)Direction::TopToBottomThenRightToLeft][bin_type_id];
            for (auto it_ref = bb_bin_type_ref.defects.begin() + bb_bin_type.defects.size();
                    it_ref != bb_bin_type_ref.defects.end();
                    ++it_ref) {
                const auto& defect_ref = *it_ref;
                UncoveredTrapezoid defect(
                        defect_ref.defect_id,
                        defect_ref.trapezoid.axial_symmetry_x_axis().axial_symmetry_y_axis());
                bb_bin_type.defects.push_back(defect);
            }
        }
    }
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_x.size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set_ref = trapezoid_sets_[(int)Direction::BottomToTopThenLeftToRight][trapezoid_set_id];
        {
            TrapezoidSet trapezoid_set;
            trapezoid_set.item_type_id = trapezoid_set_ref.item_type_id;
            trapezoid_set.angle = trapezoid_set_ref.angle;
            trapezoid_set.mirror = trapezoid_set_ref.mirror;
            trapezoid_set.x_min = trapezoid_set_ref.x_min;
            trapezoid_set.x_max = trapezoid_set_ref.x_max;
            trapezoid_set.y_min = -trapezoid_set_ref.y_max;
            trapezoid_set.y_max = -trapezoid_set_ref.y_min;
            for (const std::vector<GeneralizedTrapezoid>& shapes_ref: trapezoid_set_ref.shapes) {
                trapezoid_set.shapes.push_back({});
                for (const GeneralizedTrapezoid& shape_ref: shapes_ref)
                    trapezoid_set.shapes.back().push_back(shape_ref.axial_symmetry_x_axis());
            }
            trapezoid_sets_[(int)Direction::BottomToTopThenRightToLeft].push_back(trapezoid_set);
        }
        {
            TrapezoidSet trapezoid_set;
            trapezoid_set.item_type_id = trapezoid_set_ref.item_type_id;
            trapezoid_set.angle = trapezoid_set_ref.angle;
            trapezoid_set.mirror = trapezoid_set_ref.mirror;
            trapezoid_set.x_min = -trapezoid_set_ref.x_max;
            trapezoid_set.x_max = -trapezoid_set_ref.x_min;
            trapezoid_set.y_min = trapezoid_set_ref.y_min;
            trapezoid_set.y_max = trapezoid_set_ref.y_max;
            for (const std::vector<GeneralizedTrapezoid>& shapes_ref: trapezoid_set_ref.shapes) {
                trapezoid_set.shapes.push_back({});
                for (const GeneralizedTrapezoid& shape_ref: shapes_ref)
                    trapezoid_set.shapes.back().push_back(shape_ref.axial_symmetry_y_axis());
            }
            trapezoid_sets_[(int)Direction::TopToBottomThenLeftToRight].push_back(trapezoid_set);
        }
        {
            TrapezoidSet trapezoid_set;
            trapezoid_set.item_type_id = trapezoid_set_ref.item_type_id;
            trapezoid_set.angle = trapezoid_set_ref.angle;
            trapezoid_set.mirror = trapezoid_set_ref.mirror;
            trapezoid_set.x_min = -trapezoid_set_ref.x_max;
            trapezoid_set.x_max = -trapezoid_set_ref.x_min;
            trapezoid_set.y_min = -trapezoid_set_ref.y_max;
            trapezoid_set.y_max = -trapezoid_set_ref.y_min;
            for (const std::vector<GeneralizedTrapezoid>& shapes_ref: trapezoid_set_ref.shapes) {
                trapezoid_set.shapes.push_back({});
                for (const GeneralizedTrapezoid& shape_ref: shapes_ref)
                    trapezoid_set.shapes.back().push_back(shape_ref.axial_symmetry_x_axis().axial_symmetry_y_axis());
            }
            trapezoid_sets_[(int)Direction::TopToBottomThenRightToLeft].push_back(trapezoid_set);
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

    compute_inflated_trapezoid_sets();

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
            Shape convex_hull = polygon_convex_hull(item_shape.shape);
            AreaDbl convex_hull_area = convex_hull.compute_area();
            item_types_convex_hull_area_[item_type_id] += convex_hull_area;
        }
        //std::cout << "item_type_id " << item_type_id
        //    << " area " << item_type.area
        //    << " convex_hull_area " << item_types_convex_hull_area_[item_type_id]
        //    << std::endl;
    }
}

void BranchingScheme::compute_inflated_trapezoid_sets()
{
    for (Direction direction: {
            Direction::LeftToRightThenBottomToTop,
            Direction::LeftToRightThenTopToBottom,
            Direction::RightToLeftThenBottomToTop,
            Direction::RightToLeftThenTopToBottom,
            Direction::BottomToTopThenLeftToRight,
            Direction::BottomToTopThenRightToLeft,
            Direction::TopToBottomThenLeftToRight,
            Direction::TopToBottomThenRightToLeft}) {
        trapezoid_sets_inflated_.push_back({});

        for (TrapezoidSetId trapezoid_set_id = 0;
                trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_[(int)direction].size();
                ++trapezoid_set_id) {
            const TrapezoidSet& trapezoid_set = trapezoid_sets_[(int)direction][trapezoid_set_id];
            trapezoid_sets_inflated_.back().push_back({});

            // Loop through rectangles of the rectangle set.
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                    ++item_shape_pos) {
                const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
                trapezoid_sets_inflated_.back().back().push_back({});

                for (TrapezoidPos item_shape_trapezoid_pos = 0;
                        item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                        ++item_shape_trapezoid_pos) {
                    const auto& item_shape_trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];
                    trapezoid_sets_inflated_.back().back().back().push_back(
                            item_shape_trapezoid.inflate(instance().parameters().item_item_minimum_spacing));
                }
            }
        }
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
                    "check_skyline."
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

    const TrapezoidSet& trapezoid_set = trapezoid_sets_[(int)node.last_bin_direction][insertion.trapezoid_set_id];
    const auto& item_shape_trapezoids = trapezoid_set.shapes[insertion.item_shape_pos];
    const GeneralizedTrapezoid& shape_trapezoid = item_shape_trapezoids[insertion.item_shape_trapezoid_pos];
    //std::cout << "shape_trapezoid " << shape_trapezoid << std::endl;

    BinPos bin_pos = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const BranchingSchemeBinType& bb_bin_type = bin_types_[(int)o][bin_type_id];

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
    if (insertion.new_bin_direction != Direction::Any) {  // New bin.
        UncoveredTrapezoid uncovered_trapezoid(GeneralizedTrapezoid(
                    bb_bin_type.y_min,
                    bb_bin_type.y_max,
                    bb_bin_type.x_min,
                    bb_bin_type.x_min + instance().parameters().item_bin_minimum_spacing,
                    bb_bin_type.x_min,
                    bb_bin_type.x_min + instance().parameters().item_bin_minimum_spacing));
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
            item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
            ++item_shape_pos) {
        const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
        for (TrapezoidPos item_shape_trapezoid_pos = 0;
                item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                ++item_shape_trapezoid_pos) {
            //std::cout << "insertion " << insertion << std::endl;
            //std::cout << "add extra_trapezoid " << item_shape_trapezoids[item_shape_trapezoid_pos] << std::endl;
            GeneralizedTrapezoid trapezoid
                = trapezoid_sets_inflated_[(int)node.last_bin_direction][node.trapezoid_set_id][item_shape_pos][item_shape_trapezoid_pos];
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

            // Add the item to the skyline.
            if (item_shape_pos == insertion.item_shape_pos
                    && item_shape_trapezoid_pos == insertion.item_shape_trapezoid_pos
                    && !trapezoid.left_side_increasing_not_vertical()
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
                if (!strictly_greater(xb_t, xb_ut)
                        && !strictly_greater(xt_t, xt_ut)) {
                    touching_height += (yt - yb);
                }
            }
            //std::cout << "touching_height " << touching_height << " " << trapezoid.height() << std::endl;
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
            node.extra_trapezoids.push_back(extra_trapezoid);
        }
    }

    // Update extra rectangles.
    if (insertion.new_bin_direction == Direction::Any) {  // Same bin

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
    node.item_convex_hull_area = parent.item_convex_hull_area
        + item_types_convex_hull_area_[trapezoid_set.item_type_id];

    // Compute, guide_area and width using uncovered_trapezoids.
    node.xs_max = (insertion.new_bin_direction == Direction::Any)?  // Same bin
        std::max(parent.xs_max, insertion.x + trapezoid_set.x_min):
        insertion.x + trapezoid_set.x_min;
    node.guide_area = instance_.previous_bin_area(bin_pos)
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
    LengthDbl x = 0.0;
    LengthDbl y = 0.0;
    if (node.last_bin_direction == Direction::LeftToRightThenBottomToTop) {
        x = node.x;
        y = node.y;
    } else if (node.last_bin_direction == Direction::LeftToRightThenTopToBottom) {
        x = node.x;
        y = -node.y;
    } else if (node.last_bin_direction == Direction::RightToLeftThenBottomToTop) {
        x = -node.x;
        y = node.y;
    } else if (node.last_bin_direction == Direction::RightToLeftThenTopToBottom) {
        x = -node.x;
        y = -node.y;
    } else if (node.last_bin_direction == Direction::BottomToTopThenLeftToRight) {
        x = node.y;
        y = node.x;
    } else if (node.last_bin_direction == Direction::TopToBottomThenLeftToRight) {
        x = node.y;
        y = -node.x;
    } else if (node.last_bin_direction == Direction::BottomToTopThenRightToLeft) {
        x = -node.y;
        y = node.x;
    } else if (node.last_bin_direction == Direction::TopToBottomThenRightToLeft) {
        x = -node.y;
        y = -node.x;
    }
    auto mm = item_type.compute_min_max(
            trapezoid_set.angle,
            trapezoid_set.mirror);
    if (insertion.new_bin_direction == Direction::Any) {  // Same bin
        node.xe_max = std::max(parent.xe_max, x + mm.second.x);
        node.ye_max = std::max(parent.ye_max, y + mm.second.y);
    } else {
        node.xe_max = x + mm.second.x;
        node.ye_max = y + mm.second.y;
    }
    if (strictly_greater(node.ye_max, bin_type.y_max)) {
        throw std::runtime_error(
                "irregular::BranchingScheme::child_tmp."
                " node.ye_max: " + std::to_string(node.ye_max)
                + "; bin_type.y_max: " + std::to_string(bin_type.y_max)
                + "; insertions.trapezoid_set_id: " + std::to_string(insertion.trapezoid_set_id)
                + "; insertions.x: " + std::to_string(insertion.x)
                + "; insertions.y: " + std::to_string(insertion.y)
                + "; trapezoid_set.x_min: " + std::to_string(trapezoid_set.x_min)
                + "; trapezoid_set.x_max: " + std::to_string(trapezoid_set.x_max)
                + "; trapezoid_set.y_min: " + std::to_string(trapezoid_set.y_min)
                + "; trapezoid_set.y_max: " + std::to_string(trapezoid_set.y_max)
                + "; bb_bin_type.x_max: " + std::to_string(bb_bin_type.x_max)
                + "; bb_bin_type.y_max: " + std::to_string(bb_bin_type.y_max)
                + ".");
    }

    node.leftover_value = (bin_type.x_max - bin_type.x_min) * (bin_type.y_max - bin_type.y_min)
        - (node.xe_max - bin_type.x_min) * (node.ye_max - bin_type.y_min);

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
        const BranchingSchemeBinType& bb_bin_type = bin_types_[(int)cs[i]->last_bin_direction][cs[i]->number_of_bins - 1];
        //std::cout << cs[i]->id
        //    << " insertion " << parent->children_insertions[i]
        //    << " xs_max " << cs[i]->xs_max
        //    << " xi_min " << bb_bin_type.x_min
        //    << " yi_min " << bb_bin_type.y_min
        //    << " yi_max " << bb_bin_type.y_max
        //    << " guide_area " << cs[i]->guide_area
        //    << std::endl;
        //write_svg(cs[i], "node_" + std::to_string(cs[i]->id) + ".svg");
    }
    return cs;
}

void BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    uncovered_trapezoids_cur_.clear();
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BranchingSchemeBinType& bb_bin_type = bin_types_[(int)parent->last_bin_direction][bin_type_id];
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
    //for (const UncoveredTrapezoid& uncovered_trapezoid: parent->uncovered_trapezoids)
    //    std::cout << "* " << uncovered_trapezoid << std::endl;
    //write_svg(parent, "node_" + std::to_string(parent->id) + ".svg");

    // Add all previous insertions which are still valid.
    if (parent->parent != nullptr) {
        for (const Insertion& insertion: parent->parent->children_insertions) {
            if (!strictly_lesser(insertion.ys, parent->ye)
                    || !strictly_greater(insertion.ye, parent->ys)) {
                ItemTypeId item_type_id = trapezoid_sets_[(int)Direction::LeftToRightThenBottomToTop][insertion.trapezoid_set_id].item_type_id;
                const ItemType& item_type = instance().item_type(item_type_id);
                if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                    continue;
                //std::cout << "copy " << insertion << std::endl;
                parent->children_insertions.push_back(insertion);
                parent->children_insertions.back().new_bin_direction = Direction::Any;
            }
        }
    }

    // Check number of items for each rectangle set.
    std::vector<TrapezoidSetId> valid_trapezoid_set_ids;
    for (TrapezoidSetId trapezoid_set_id = 0;
            trapezoid_set_id < (TrapezoidSetId)trapezoid_sets_[(int)Direction::LeftToRightThenBottomToTop].size();
            ++trapezoid_set_id) {
        const TrapezoidSet& trapezoid_set = trapezoid_sets_[(int)Direction::LeftToRightThenBottomToTop][trapezoid_set_id];

        if (parent->item_number_of_copies[trapezoid_set.item_type_id] + 1
                <= instance().item_type(trapezoid_set.item_type_id).copies) {
            valid_trapezoid_set_ids.push_back(trapezoid_set_id);
        }
    }

    // Insert in the current bin.
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);
        const std::vector<TrapezoidSet>& trapezoid_sets = trapezoid_sets_[(int)parent->last_bin_direction];

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

                    for (ItemPos uncovered_trapezoid_pos = 0;
                            uncovered_trapezoid_pos < (ItemPos)parent->uncovered_trapezoids.size();
                            ++uncovered_trapezoid_pos) {
                        insertion_trapezoid_set(
                                parent,
                                trapezoid_set_id,
                                item_shape_pos,
                                item_shape_trapezoid_pos,
                                Direction::Any,
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
                                Direction::Any,
                                -1,  // uncovered_trapezoid_pos
                                extra_trapezoid_pos);
                    }
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

        const std::vector<TrapezoidSet>& trapezoid_sets = trapezoid_sets_[(int)new_bin_direction];
        const BranchingSchemeBinType& bb_bin_type = bin_types_[(int)new_bin_direction][bin_type_id];
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
                                new_bin_direction,
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
            if (!strictly_greater(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
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
            if (!strictly_greater(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
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
            LengthDbl ly = lx / item_shape_trapezoid.a_right();
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_bottom_right(), supporting_trapezoid.x_bottom_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
                xs += lx;
                ys += ly;
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
            LengthDbl ly = lx / supporting_trapezoid.a_left();
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_top_right(), supporting_trapezoid.x_top_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
                xs += lx;
                ys += ly;
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
            LengthDbl ly = lx / supporting_trapezoid.a_left();
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_bottom_right(), supporting_trapezoid.x_top_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
                xs += lx;
                ys += ly;
                return true;
            } else if (!supporting_trapezoid.top_covered()) {
                state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidTop;
                current_trapezoid.shift_right(supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
            } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
                if (item_shape_trapezoid.left_side_decreasing_not_vertical()) {
                    if (!strictly_lesser(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
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
            LengthDbl ly = lx / item_shape_trapezoid.a_right();
            //std::cout << "lx " << lx << std::endl;
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_bottom_right(), supporting_trapezoid.x_top_left())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
                xs += lx;
                ys += ly;
                return true;
            } else if (!supporting_trapezoid.top_covered()) {
                state = State::ItemShapeTrapezoidBottomRightSupportingTrapezoidTop;
                current_trapezoid.shift_right(supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right() - xs);
                current_trapezoid.shift_top(supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom() - ys);
                xs = supporting_trapezoid.x_top_left() - item_shape_trapezoid.x_bottom_right();
                ys = supporting_trapezoid.y_top() - item_shape_trapezoid.y_bottom();
            } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
                if (item_shape_trapezoid.left_side_decreasing_not_vertical()) {
                    if (strictly_lesser(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
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
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_top_right())) {
                current_trapezoid.shift_right(lx);
                xs += lx;
                return true;
            } else if (supporting_trapezoid.right_side_decreasing_not_vertical()) {
            //} else if (supporting_trapezoid.x_bottom_right() - supporting_trapezoid.x_top_right() >= supporting_trapezoid.width_top()) {
                if (item_shape_trapezoid.left_side_decreasing_not_vertical()) {
                    if (strictly_lesser(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
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
                //std::cout << "infeasible" << std::endl;
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
            LengthDbl ly = lx / item_shape_trapezoid.a_left();
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_top_left(), supporting_trapezoid.x_top_right())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
                xs += lx;
                ys += ly;
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
            LengthDbl ly = lx / supporting_trapezoid.a_right();
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_top_left(), supporting_trapezoid.x_bottom_right())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
                xs += lx;
                ys += ly;
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
            LengthDbl ly = lx / supporting_trapezoid.a_right();
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_bottom_right())) {
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
                xs += lx;
                ys += ly;
                return true;
            } else if (item_shape_trapezoid.left_side_decreasing_not_vertical()
                    && strictly_greater(item_shape_trapezoid.a_left(), supporting_trapezoid.a_right())) {
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
            LengthDbl ly = lx / item_shape_trapezoid.a_left();
            if (equal(xs + lx, xs) && equal(ys + ly, ys)) {
                return updated;
            } else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_top_left(), supporting_trapezoid.x_bottom_right())) {
                xs += lx;
                ys += ly;
                current_trapezoid.shift_right(lx);
                current_trapezoid.shift_top(ly);
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
        Direction new_bin_direction,
        ItemPos uncovered_trapezoid_pos,
        ItemPos extra_trapezoid_pos) const
{
    //std::cout << "insertion_trapezoid_set " << trapezoid_set_id
    //    << " " << item_shape_pos
    //    << " " << item_shape_trapezoid_pos
    //    << " new_bin_direction " << (int)new_bin_direction
    //    << " utp " << uncovered_trapezoid_pos
    //    << " etp " << extra_trapezoid_pos
    //    << std::endl;

    BinTypeId bin_type_id = (new_bin_direction == Direction::Any)?
        instance().bin_type_id(parent->number_of_bins - 1):
        instance().bin_type_id(parent->number_of_bins);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = (new_bin_direction == Direction::Any)?
        parent->last_bin_direction:
        new_bin_direction;

    const std::vector<TrapezoidSet>& trapezoid_sets = trapezoid_sets_[(int)o];
    const BranchingSchemeBinType& bb_bin_type = bin_types_[(int)o][bin_type_id];
    const std::vector<UncoveredTrapezoid>& extra_trapezoids = (new_bin_direction == Direction::Any)?
        parent->extra_trapezoids:
        bb_bin_type.defects;

    const GeneralizedTrapezoid& supporting_trapezoid = (uncovered_trapezoid_pos != -1)?
        uncovered_trapezoids_cur_[uncovered_trapezoid_pos]:
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
    //std::cout << "init_position" << std::endl
    //    << "        state " << (int)state << " xs " << xs << " ys " << ys << std::endl;
    if (state == State::Infeasible)
        return;

    if (parent->parent != nullptr) {
        LengthDbl ys_tmp = supporting_trapezoid.y_bottom()
            - item_shape_trapezoid.y_top()
            + trapezoid_set.y_min
            - instance().parameters().item_item_minimum_spacing;
        LengthDbl ye_tmp = supporting_trapezoid.y_top()
            - item_shape_trapezoid.y_bottom()
            + trapezoid_set.y_max
            + instance().parameters().item_item_minimum_spacing;
        if (strictly_greater(ys_tmp, parent->ye)
                || strictly_lesser(ye_tmp, parent->ys)) {
            return;
        }
    }

    //xs = (std::max)(xs, bb_bin_type.x_min - item_shape_trapezoid.x_max());

    if (strictly_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max - instance().parameters().item_bin_minimum_spacing))
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
                item_shape_trapezoid_cur.shift_right(xs);
                item_shape_trapezoid_cur.shift_top(ys);

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
                        bb_bin_type.x_min + instance().parameters().item_bin_minimum_spacing,
                        std::min(
                            bb_bin_type.x_min - (bb_bin_type.x_max - bb_bin_type.x_min),
                            item_shape_trapezoid_cur.x_min()),
                        bb_bin_type.x_min + instance().parameters().item_bin_minimum_spacing);

                bool b = update_position(
                        item_shape_trapezoid,
                        supporting_trapezoid,
                        trapezoid_to_avoid,
                        item_shape_trapezoid_cur,
                        state,
                        xs,
                        ys);
                if (state == State::Infeasible)
                    return;
                if (b) {
                    stop = false;
                    if (strictly_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max - instance().parameters().item_bin_minimum_spacing))
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
                    item_shape_trapezoid_cur.shift_right(xs);
                    item_shape_trapezoid_cur.shift_top(ys);

                    // Skyline.
                    for (ItemPos uncovered_trapezoid_pos_cur = 0;
                            uncovered_trapezoid_pos_cur < (ItemPos)parent->uncovered_trapezoids.size();
                            ++uncovered_trapezoid_pos_cur) {

                        //LengthDbl lx = item_shape_trapezoid_cur.compute_right_shift_if_intersects(uncovered_trapezoids_cur_[uncovered_trapezoid_pos_cur]);
                        //if (equal(xs + lx, xs)) {
                        //} else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_top_right())) {
                        //    item_shape_trapezoid_cur.shift_right(lx);
                        //    xs += lx;
                        //    if (strictly_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max))
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
                            if (strictly_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max - instance().parameters().item_bin_minimum_spacing))
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
                    //} else if (strictly_lesser(xs + lx + item_shape_trapezoid.x_bottom_left(), supporting_trapezoid.x_top_right())) {
                    //    item_shape_trapezoid_cur.shift_right(lx);
                    //    xs += lx;
                    //    if (strictly_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max))
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
                        if (strictly_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max - instance().parameters().item_bin_minimum_spacing))
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
    if (strictly_greater(xs + trapezoid_set.x_max, bb_bin_type.x_max - instance().parameters().item_bin_minimum_spacing)) {
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
    insertion.ys = supporting_trapezoid.y_bottom()
        - item_shape_trapezoid.y_top()
        + trapezoid_set.y_min
        - instance().parameters().item_item_minimum_spacing;
    insertion.ye = supporting_trapezoid.y_top()
        - item_shape_trapezoid.y_bottom()
        + trapezoid_set.y_max
        + instance().parameters().item_item_minimum_spacing;
    insertion.new_bin_direction = new_bin_direction;
    parent->children_insertions.push_back(insertion);
    //std::cout << "- trapezoid_set_id " << trapezoid_set_id
    //    << " " << item_shape_pos
    //    << " " << item_shape_trapezoid_pos
    //    << " new_bin_direction " << (int)new_bin_direction
    //    << " utp " << uncovered_trapezoid_pos
    //    << " etp " << extra_trapezoid_pos
    //    << " y " << insertion.y
    //    << " x " << insertion.x
    //    << " nbd " << (int)insertion.new_bin_direction
    //    << " -> ok" << std::endl;
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

        const TrapezoidSet& trapezoid_set = trapezoid_sets_[(int)current_node->last_bin_direction][current_node->trapezoid_set_id];
        Point bl_corner = {0, 0};
        if (current_node->last_bin_direction == Direction::LeftToRightThenBottomToTop) {
            bl_corner = Point{current_node->x, current_node->y};
        } else if (current_node->last_bin_direction == Direction::LeftToRightThenTopToBottom) {
            bl_corner = Point{current_node->x, -current_node->y};
        } else if (current_node->last_bin_direction == Direction::RightToLeftThenBottomToTop) {
            bl_corner = Point{-current_node->x, current_node->y};
        } else if (current_node->last_bin_direction == Direction::RightToLeftThenTopToBottom) {
            bl_corner = Point{-current_node->x, -current_node->y};
        } else if (current_node->last_bin_direction == Direction::BottomToTopThenLeftToRight) {
            bl_corner = Point{current_node->y, current_node->x};
        } else if (current_node->last_bin_direction == Direction::BottomToTopThenRightToLeft) {
            bl_corner = Point{-current_node->y, current_node->x};
        } else if (current_node->last_bin_direction == Direction::TopToBottomThenLeftToRight) {
            bl_corner = Point{current_node->y, -current_node->x};
        } else if (current_node->last_bin_direction == Direction::TopToBottomThenRightToLeft) {
            bl_corner = Point{-current_node->y, -current_node->x};
        }
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

    if (node->last_bin_direction == Direction::LeftToRightThenBottomToTop
            || node->last_bin_direction == Direction::BottomToTopThenLeftToRight) {
        if (!equal(node->xe_max, solution.x_max())) {
            solution.write("solution_irregular.json");
            throw std::runtime_error(
                    "irregular::BranchingScheme::to_solution."
                    " node->xe_max: " + std::to_string(node->xe_max)
                    + "; solution.x_max(): " + std::to_string(solution.x_max())
                    + "; d: " + std::to_string((int)node->last_bin_direction)
                    + ".");
        }
        if (!equal(node->ye_max, solution.y_max())) {
            throw std::runtime_error(
                    "irregular::BranchingScheme::to_solution."
                    " node->ye_max: " + std::to_string(node->ye_max)
                    + "; solution.y_max(): " + std::to_string(solution.y_max())
                    + "; d: " + std::to_string((int)node->last_bin_direction)
                    + ".");
        }
    }
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void BranchingScheme::write_svg(
        const std::shared_ptr<Node>& node,
        const std::string& file_path) const
{
    if (file_path.empty())
        return;
    std::ofstream file{file_path};
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + file_path + "\".");
    }

    BinPos bin_pos = node->number_of_bins - 1;
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    const BranchingSchemeBinType& bb_bin_type = bin_types_[(int)node->last_bin_direction][bin_type_id];
    LengthDbl width = (bb_bin_type.x_max - bb_bin_type.x_min);
    LengthDbl height = (bb_bin_type.y_max - bb_bin_type.y_min);

    double factor = compute_svg_factor(width);

    std::string s = "<svg viewBox=\""
        + std::to_string(bb_bin_type.x_min * factor)
        + " " + std::to_string(-bb_bin_type.y_min * factor - height * factor)
        + " " + std::to_string(width * factor)
        + " " + std::to_string(height * factor)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    for (ItemPos extra_trapezoid_pos = 0;
            extra_trapezoid_pos < (ItemPos)node->extra_trapezoids.size();
            ++extra_trapezoid_pos) {
        const GeneralizedTrapezoid& trapezoid = node->extra_trapezoids[extra_trapezoid_pos].trapezoid;

        file << "<g>" << std::endl;
        file << trapezoid.to_svg("red", factor);
        LengthDbl x = (trapezoid.x_max() + trapezoid.x_min()) / 2;
        LengthDbl y = (trapezoid.y_top() + trapezoid.y_bottom()) / 2;
        file << "<text x=\"" << std::to_string(x * factor) << "\" y=\"" << std::to_string(-y * factor) << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">" << std::to_string(extra_trapezoid_pos) << "</text>" << std::endl;
        file << "</g>" << std::endl;
    }
    for (TrapezoidPos uncovered_trapezoid_pos = 0;
            uncovered_trapezoid_pos < (ItemPos)node->uncovered_trapezoids.size();
            ++uncovered_trapezoid_pos) {
        GeneralizedTrapezoid trapezoid = node->uncovered_trapezoids[uncovered_trapezoid_pos].trapezoid;
        trapezoid.extend_left(bb_bin_type.x_min);

        file << "<g>" << std::endl;
        file << trapezoid.to_svg("blue", factor);
        LengthDbl x = (trapezoid.x_max() + trapezoid.x_min()) / 2;
        LengthDbl y = (trapezoid.y_top() + trapezoid.y_bottom()) / 2;
        file << "<text x=\"" << std::to_string(x * factor) << "\" y=\"" << std::to_string(-y * factor) << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">" << std::to_string(uncovered_trapezoid_pos) << "</text>" << std::endl;
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
            && (item_shape_pos == insertion.item_shape_pos)
            && (item_shape_trapezoid_pos == insertion.item_shape_trapezoid_pos)
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
        << " item_shape_pos " << insertion.item_shape_pos
        << " item_shape_trapezoid_pos " << insertion.item_shape_trapezoid_pos
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
