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
                Shape cleaned_shape = clean_shape(item_shape.shape);
                Shape rotated_shape = cleaned_shape.rotate(angle_range.first);
                Shape cleaned_y_shape = clean_shape_y(rotated_shape);
                auto trapezoids = polygon_trapezoidation(cleaned_y_shape);
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
                Shape cleaned_shape = clean_shape(item_shape.shape);
                Shape sym_shape = cleaned_shape.identity_line_axial_symmetry();
                Shape rotated_shape = sym_shape.rotate(angle_range.first);
                Shape cleaned_y_shape = clean_shape_y(rotated_shape);
                auto trapezoids = polygon_trapezoidation(cleaned_y_shape);
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
    //    std::cout << "x_max " << trapezoid_set.x_max << std::endl;
    //    std::cout << "y_max " << trapezoid_set.y_max << std::endl;
    //    std::cout << "item_type_id " << trapezoid_set.item_type_id << std::endl;

    //    // Loop through rectangles of the rectangle set.
    //    for (ItemShapePos item_shape_pos = 0;
    //            item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
    //            ++item_shape_pos) {
    //        const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
    //        for (TrapezoidPos item_shape_trapezoid_pos = 0;
    //                item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
    //                ++item_shape_trapezoid_pos) {
    //        }
    //    }
    //}

}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    //std::cout << std::endl;
    //std::cout << "child_tmp " << insertion << std::endl;
    const Node& parent = *pparent;
    Node node;

    node.parent = pparent;

    node.trapezoid_set_id = insertion.trapezoid_set_id;
    node.x = insertion.x;
    node.y = insertion.y;

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

    LengthDbl ys = insertion.y + shape_trapezoid.y_bottom();
    LengthDbl ye = insertion.y + shape_trapezoid.y_top();
    LengthDbl xi = instance().x_max(bin_type, o);
    LengthDbl yi = instance().y_max(bin_type, o);
    //std::cout << "ys " << ys << " ye " << ye << std::endl;

    // Update uncovered_trapezoids.
    ItemPos new_uncovered_trapezoid_pos = -1;
    if (insertion.new_bin > 0) {  // New bin.
        if (ys > 0) {
            UncoveredTrapezoid uncovered_trapezoid(
                    GeneralizedTrapezoid(0, ys, 0, 0, 0, 0));
            node.uncovered_trapezoids.push_back(uncovered_trapezoid);
        }
        {
            new_uncovered_trapezoid_pos = node.uncovered_trapezoids.size();
            GeneralizedTrapezoid trapezoid = shape_trapezoid;
            trapezoid.shift_top(insertion.y);
            trapezoid.shift_right(insertion.x);
            UncoveredTrapezoid uncovered_trapezoid(
                    trapezoid_set.item_type_id,
                    insertion.item_shape_pos,
                    insertion.item_shape_trapezoid_pos,
                    trapezoid);
            node.uncovered_trapezoids.push_back(uncovered_trapezoid);
        }
        if (ye < yi) {
            UncoveredTrapezoid uncovered_trapezoid(
                    GeneralizedTrapezoid(ye, yi, 0, 0, 0, 0));
            node.uncovered_trapezoids.push_back(uncovered_trapezoid);
        }
    } else {  // Same bin.
        for (const UncoveredTrapezoid& uncovered_trapezoid: parent.uncovered_trapezoids) {
            if (uncovered_trapezoid.trapezoid.y_top() <= ys) {
                UncoveredTrapezoid new_uncovered_trapezoid = uncovered_trapezoid;
                node.uncovered_trapezoids.push_back(new_uncovered_trapezoid);
            } else if (uncovered_trapezoid.trapezoid.y_bottom() <= ys) {
                if (uncovered_trapezoid.trapezoid.y_bottom() < ys) {
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
                    node.uncovered_trapezoids.push_back(new_uncovered_trapezoid);
                }

                new_uncovered_trapezoid_pos = node.uncovered_trapezoids.size();
                GeneralizedTrapezoid trapezoid = shape_trapezoid;
                trapezoid.shift_top(insertion.y);
                trapezoid.shift_right(insertion.x);
                UncoveredTrapezoid new_uncovered_trapezoid(
                        trapezoid_set.item_type_id,
                        insertion.item_shape_pos,
                        insertion.item_shape_trapezoid_pos,
                        trapezoid);
                node.uncovered_trapezoids.push_back(new_uncovered_trapezoid);

                if (uncovered_trapezoid.trapezoid.y_top() > ye) {
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
                    node.uncovered_trapezoids.push_back(new_uncovered_trapezoid);
                }
            } else if (uncovered_trapezoid.trapezoid.y_bottom() >= ye) {
                UncoveredTrapezoid new_uncovered_trapezoid = uncovered_trapezoid;
                node.uncovered_trapezoids.push_back(new_uncovered_trapezoid);
            } else {
                if (uncovered_trapezoid.trapezoid.y_top() > ye) {
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
                    node.uncovered_trapezoids.push_back(new_uncovered_trapezoid);
                }
            }
        }
        //std::cout << "uncovered_trapezoids:" << std::endl;
        //for (const UncoveredTrapezoid& uncovered_trapezoid: parent.uncovered_trapezoids)
        //    std::cout << "* " << uncovered_trapezoid << std::endl;

        // Update extra rectangles.
        // Don't add extra rectangles which are behind the skyline.
        //std::cout << "check previous extra trapezoids:" << std::endl;
        for (const UncoveredTrapezoid& extra_trapezoid: parent.extra_trapezoids) {
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
                LengthDbl x_extra = extra_trapezoid.trapezoid.x_right(yb);
                LengthDbl x_uncov = uncovered_trapezoid.trapezoid.x_right(yb);
                //std::cout << "yb " << yb << " yt " << yt << std::endl;
                //std::cout << "x_extra " << x_extra << " x_uncov " << x_uncov << std::endl;
                if (!striclty_greater(x_extra, x_uncov)) {
                    //std::cout << "covers" << std::endl;
                    covered_length += (yt - yb);
                }
            }
            //std::cout << "extra_trapezoid " << extra_trapezoid
            //    << " length " << extra_trapezoid.trapezoid.height()
            //    << " covered " << covered_length
            //    << std::endl;
            if (striclty_lesser(covered_length, extra_trapezoid.trapezoid.height()))
                node.extra_trapezoids.push_back(extra_trapezoid);
        }
    }

    // Add extra rectangles.
    for (ItemShapePos item_shape_pos = 0;
            item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
            ++item_shape_pos) {
        const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
        for (TrapezoidPos item_shape_trapezoid_pos = 0;
                item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                ++item_shape_trapezoid_pos) {
            // Skip the rectangle added to the uncovered items.
            if (item_shape_pos == insertion.item_shape_pos
                    && item_shape_trapezoid_pos == insertion.item_shape_trapezoid_pos) {
                continue;
            }
            GeneralizedTrapezoid trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];
            trapezoid.shift_right(insertion.x);
            trapezoid.shift_top(insertion.y);
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
    node.guide_area = instance_.previous_bin_area(bin_pos) + node.xs_max * yi;
    for (auto it = node.uncovered_trapezoids.rbegin(); it != node.uncovered_trapezoids.rend(); ++it) {
        const GeneralizedTrapezoid& trapezoid = it->trapezoid;
        node.current_area += trapezoid.area(0.0);
        //std::cout << trapezoid << std::endl;
        //std::cout << "* " << trapezoid.area() << " " << trapezoid.area(0.0) << std::endl;
        //std::cout << "current_area: " << node.current_area << std::endl;
        if (node.xe_max < trapezoid.x_max())
            node.xe_max = trapezoid.x_max();
        if (trapezoid.x_max() > node.xs_max)
            node.guide_area += trapezoid.area(node.xs_max);
    }
    // Add area from extra rectangles.
    for (const UncoveredTrapezoid& extra_trapezoid: node.extra_trapezoids) {
        const GeneralizedTrapezoid& trapezoid = extra_trapezoid.trapezoid;
        node.current_area += trapezoid.area();
        //std::cout << trapezoid << std::endl;
        //std::cout << "current_area " << node.current_area << std::endl;
        if (node.xe_max < trapezoid.x_max())
            node.xe_max = trapezoid.x_max();
        if (trapezoid.x_max() > node.xs_max)
            node.guide_area += trapezoid.area(node.xs_max);
    }

    if (node.number_of_items == instance().number_of_items()) {
        node.current_area = instance().previous_bin_area(bin_pos) + node.xe_max * yi;
    }

    node.waste = node.current_area - node.item_area;

    {
        AreaDbl waste = instance().previous_bin_area(bin_pos) + node.xe_max * yi - instance().item_area();
        if (node.waste < waste)
            node.waste = waste;
    }

    if (node.waste < -PSTOL) {
        Solution solution = to_solution(std::make_shared<Node>(node));
        solution.write("solution_irregular.json");
        throw std::runtime_error(
                "waste: " + std::to_string(node.waste)
                + "; current_area: " + std::to_string(node.current_area)
                + "; item_area: " + std::to_string(node.item_area));
    }
    if (node.waste < 0.0)
        node.waste = 0.0;

    node.id = node_id_++;
    return node;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& parent) const
{
    insertions(parent);
    std::vector<std::shared_ptr<Node>> cs(insertions_.size());
    for (Counter i = 0; i < (Counter)insertions_.size(); ++i) {
        cs[i] = std::make_shared<Node>(child_tmp(parent, insertions_[i]));
        //std::cout << cs[i]->id
        //    << " rid " << cs[i]->trapezoid_set_id
        //    << " x " << cs[i]->x
        //    << " y " << cs[i]->y
        //    << std::endl;
    }
    return cs;
}

const std::vector<BranchingScheme::Insertion>& BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    insertions_.clear();
    //std::cout << std::endl;
    //std::cout << "insertions"
    //    << " id " << parent->id
    //    << " ts " << parent->trapezoid_set_id
    //    << " x " << parent->x
    //    << " y " << parent->y
    //    << " n " << parent->number_of_items
    //    << " item_area " << parent->item_area
    //    << " guide_area " << parent->guide_area
    //    << std::endl;

    // Check number of items for each rectangle set.
    const std::vector<TrapezoidSet>& trapezoid_sets = (parent->last_bin_direction == Direction::X)?
        trapezoid_sets_x_:
        trapezoid_sets_y_;
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

    // Insert in the current bin.
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);

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
                                -1,  // extra_trapezoid_pos
                                -1);  // defect_id
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
                                extra_trapezoid_pos,  // extra_trapezoid_pos
                                -1);
                    }

                    // Defects.
                    for (const Defect& defect: bin_type.defects) {
                        insertion_trapezoid_set(
                                parent,
                                trapezoid_set_id,
                                item_shape_pos,
                                item_shape_trapezoid_pos,
                                0,  // new_bin
                                -1,  // uncovered_trapezoid_pos
                                -1,  // extra_trapezoid_pos
                                defect.id);
                    }

                }
            }
        }
    }

    // Insert in a new bin.
    if (insertions_.empty() && parent->number_of_bins < instance().number_of_bins()) {
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

                    insertion_trapezoid_set(
                            parent,
                            trapezoid_set_id,
                            item_shape_pos,
                            item_shape_trapezoid_pos,
                            new_bin,
                            0,  // uncovered_trapezoid_pos
                            -1,  // extra_trapezoid_pos
                            -1);  // defect_id


                    // Defects.
                    for (const Defect& defect: bin_type.defects) {

                        insertion_trapezoid_set(
                                parent,
                                trapezoid_set_id,
                                item_shape_pos,
                                item_shape_trapezoid_pos,
                                new_bin,
                                -1,  // uncovered_trapezoid_pos
                                -1,  // extra_trapezoid_pos
                                defect.id);
                    }
                }
            }
        }
    }

    //std::cout << "insertions:" << std::endl;
    //for (const Insertion& insertion: insertions_)
    //    std::cout << insertion << std::endl;

    return insertions_;
}

void BranchingScheme::insertion_trapezoid_set(
        const std::shared_ptr<Node>& parent,
        TrapezoidSetId trapezoid_set_id,
        ItemShapePos item_shape_pos,
        TrapezoidPos item_shape_trapezoid_pos,
        int8_t new_bin,
        ItemPos uncovered_trapezoid_pos,
        ItemPos extra_trapezoid_pos,
        DefectId defect_id) const
{
    //std::cout << "insertion_trapezoid_set " << trapezoid_set_id
    //    << " " << item_shape_pos
    //    << " " << item_shape_trapezoid_pos
    //    << " new_bin " << new_bin
    //    << " utp " << uncovered_trapezoid_pos
    //    << " etp " << extra_trapezoid_pos
    //    << " d " << defect_id
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
    const TrapezoidSet& trapezoid_set = trapezoid_sets[trapezoid_set_id];
    const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
    const GeneralizedTrapezoid& item_shape_trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];

    LengthDbl xi = instance().x_max(bin_type, o);
    LengthDbl yi = instance().y_max(bin_type, o);
    LengthDbl ys;
    if (uncovered_trapezoid_pos > 0) {
        ys = parent->uncovered_trapezoids[uncovered_trapezoid_pos].trapezoid.y_bottom();
    //} else if (defect_id != -1) {
    //    ys = instance().y_end(bin_type.defects[defect_id], o);
    } else if (extra_trapezoid_pos != -1) {
        ys = parent->extra_trapezoids[extra_trapezoid_pos].trapezoid.y_top();
    } else {  // new bin.
        ys = 0;
    }
    ys -= item_shape_trapezoid.y_bottom();
    LengthDbl ye = ys + trapezoid_set.y_max;
    // Check bin top.
    if (ye > yi) {
        //std::cout << "too high " << ye << " / " << yi << std::endl;
        return;
    }
    // Check bin bottom.
    if (ys < -trapezoid_set.y_min) {
        //std::cout << "too low"
        //    << " utp " << uncovered_trapezoid_pos
        //    << " rsiy " << trapezoid_set_item.bottom_left.y
        //    << " isry " << item_shape_trapezoid.bottom_left.y
        //    << " ys " << ys << " / " << 0.0 << std::endl;
        return;
    }

    // Compute xs.
    LengthDbl xs = -trapezoid_set.x_min;
    if (new_bin == 0) {

        // Loop through rectangles of the rectangle set.
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_pos) {
            const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
            for (TrapezoidPos item_shape_trapezoid_pos = 0;
                    item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                    ++item_shape_trapezoid_pos) {
                GeneralizedTrapezoid item_shape_trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];
                item_shape_trapezoid.shift_right(xs);
                item_shape_trapezoid.shift_top(ys);

                for (const UncoveredTrapezoid& uncovered_trapezoid: parent->uncovered_trapezoids) {
                    LengthDbl l = item_shape_trapezoid.compute_right_shift(uncovered_trapezoid.trapezoid);
                    if (l > 0.0) {
                        //std::cout << "uncovered_trapezoid " << uncovered_trapezoid << std::endl;
                        //std::cout << "xs " << xs << " -> " << xs + l << std::endl;
                        xs += l;
                        item_shape_trapezoid.shift_right(l);
                    }
                }

            }
        }
    }

    // Extra rectangles.

    for (;;) {
        bool stop = true;

        // Loop through rectangles of the rectangle set.
        for (ItemShapePos item_shape_pos = 0;
                item_shape_pos < (ItemShapePos)trapezoid_set.shapes.size();
                ++item_shape_pos) {
            const auto& item_shape_trapezoids = trapezoid_set.shapes[item_shape_pos];
            for (TrapezoidPos item_shape_trapezoid_pos = 0;
                    item_shape_trapezoid_pos < (TrapezoidPos)item_shape_trapezoids.size();
                    ++item_shape_trapezoid_pos) {
                GeneralizedTrapezoid item_shape_trapezoid = item_shape_trapezoids[item_shape_trapezoid_pos];
                item_shape_trapezoid.shift_right(xs);
                item_shape_trapezoid.shift_top(ys);

                // Extra rectangles.
                for (const UncoveredTrapezoid& extra_trapezoid: parent->extra_trapezoids) {
                    LengthDbl l = item_shape_trapezoid.compute_right_shift_if_intersects(extra_trapezoid.trapezoid);
                    if (l > 0.0) {
                        //std::cout << "extra_trapezoid " << extra_trapezoid << std::endl;
                        //std::cout << "xs " << xs << " -> " << xs + l << std::endl;
                        xs += l;
                        item_shape_trapezoid.shift_right(l);
                        stop = false;
                    }
                }

                // Defects
                // While the item intersects a defect, move it to the right.
                //for (const Defect& defect: bin_type.defects) {
                //    if (instance().x_start(defect, o) >= xs + xj)
                //        continue;
                //    if (xs >= instance().x_end(defect, o))
                //        continue;
                //    if (instance().y_start(defect, o) >= ye)
                //        continue;
                //    if (ys >= instance().y_end(defect, o))
                //        continue;
                //    xs = instance().x_end(defect, o);
                //    stop = false;
                //}

            }
        }

        if (stop)
            break;
    }

    LengthDbl xe = xs + trapezoid_set.x_max;
    // Check bin width.
    if (xe > xi) {
        //std::cout << "too wide " << xe << " / " << xi
        //    << " xs " << xs
        //    << " x_max " << trapezoid_set.x_max
        //    << std::endl;
        return;
    }

    if (uncovered_trapezoid_pos > 0) {
        if (!striclty_greater(
                    item_shape_trapezoid.x_max() + xs,
                    parent->uncovered_trapezoids[uncovered_trapezoid_pos - 1].trapezoid.x_min())) {
            return;
        }
        if (!striclty_lesser(
                    item_shape_trapezoid.x_min() + xs,
                    parent->uncovered_trapezoids[uncovered_trapezoid_pos - 1].trapezoid.x_max())) {
            return;
        }
    } else if (extra_trapezoid_pos != -1) {
        if (!striclty_greater(
                    item_shape_trapezoid.x_max() + xs,
                    parent->extra_trapezoids[extra_trapezoid_pos].trapezoid.x_min())) {
            return;
        }
        if (!striclty_lesser(
                    item_shape_trapezoid.x_min() + xs,
                    parent->extra_trapezoids[extra_trapezoid_pos].trapezoid.x_max())) {
            return;
        }
    //} else if (defect_id != -1) {
    //    if (xe <= instance().x_start(bin_type.defects[defect_id], o))
    //        return;
    //    if (xs >= instance().x_end(bin_type.defects[defect_id], o))
    //        return;
    }

    Insertion insertion;
    insertion.trapezoid_set_id = trapezoid_set_id;
    insertion.item_shape_pos = item_shape_pos;
    insertion.item_shape_trapezoid_pos = item_shape_trapezoid_pos;
    insertion.x = xs;
    insertion.y = ys;
    insertion.new_bin = new_bin;
    insertions_.push_back(insertion);
    //std::cout << "xs " << xs << std::endl;
    //std::cout << "ok" << std::endl;
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
        return node_2->waste > node_1->waste;
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
        return node_1->waste >= node_2->waste;
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
        << " new_bin " << insertion.new_bin
        << " x " << insertion.x
        << " y " << insertion.y
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
