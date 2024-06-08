#include "irregular/branching_scheme.hpp"

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
    item_types_rectangles_x_ = std::vector<ItemTypeRectangles>(instance.number_of_item_types());
    item_types_rectangles_y_ = std::vector<ItemTypeRectangles>(instance.number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        //std::cout << "item_type_id " << item_type_id << std::endl;
        const ItemType& item_type = instance.item_type(item_type_id);
        for (const ItemShape& item_shape: item_type.shapes) {
            item_types_rectangles_x_[item_type_id].shapes.push_back({
                    compute_covering_with_rectangle(
                            item_shape.shape,
                            item_shape.holes)});

            item_types_rectangles_y_[item_type_id].shapes.push_back({});
            for (const auto& item_shape_rectangle: item_types_rectangles_x_[item_type_id].shapes.back().rectangles) {
                ShapeRectangle shape_rectangle;
                shape_rectangle.bottom_left.x = item_shape_rectangle.bottom_left.y;
                shape_rectangle.bottom_left.y = item_shape_rectangle.bottom_left.x;
                shape_rectangle.top_right.x = item_shape_rectangle.top_right.y;
                shape_rectangle.top_right.y = item_shape_rectangle.top_right.x;
                item_types_rectangles_y_[item_type_id].shapes.back().rectangles.push_back(shape_rectangle);
            }
            //for (const ShapeRectangle& shape_rectangle: item_types_rectangles_[item_type_id].shapes.back().rectangles) {
            //    std::cout << shape_rectangle.bottom_left.to_string() << " " << shape_rectangle.top_right.to_string() << std::endl;
            //}
        }
    }

    // Compute rectangle_sets_.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemTypeRectangles& item_type_rectangles = item_types_rectangles_x_[item_type_id];
        RectangleSet rectangle_set;
        RectangleSetItem rectangle_set_item;
        rectangle_set_item.bottom_left = {0, 0};
        for (const ItemShapeRectangles& item_shape_rectangles: item_type_rectangles.shapes) {
            for (const ShapeRectangle& shape_rectangle: item_shape_rectangles.rectangles) {
                if (rectangle_set_item.bottom_left.x < -shape_rectangle.bottom_left.x)
                    rectangle_set_item.bottom_left.x = -shape_rectangle.bottom_left.x;
                if (rectangle_set_item.bottom_left.y < -shape_rectangle.bottom_left.y)
                    rectangle_set_item.bottom_left.y = -shape_rectangle.bottom_left.y;
            }
        }
        rectangle_set_item.item_type_id = item_type_id;
        rectangle_set_item.angle = 0.0;
        rectangle_set.items = {rectangle_set_item};
        rectangle_set.item_types = {{item_type_id, 1}};
        rectangle_set.x_min = std::numeric_limits<LengthDbl>::infinity();
        rectangle_set.x_max = -std::numeric_limits<LengthDbl>::infinity();
        rectangle_set.y_min = std::numeric_limits<LengthDbl>::infinity();
        rectangle_set.y_max = -std::numeric_limits<LengthDbl>::infinity();
        for (const ItemShapeRectangles& item_shape_rectangles: item_type_rectangles.shapes) {
            for (const ShapeRectangle& shape_rectangle: item_shape_rectangles.rectangles) {
                LengthDbl xs = rectangle_set_item.bottom_left.x + shape_rectangle.bottom_left.x;
                LengthDbl xe = rectangle_set_item.bottom_left.x + shape_rectangle.top_right.x;
                LengthDbl ys = rectangle_set_item.bottom_left.y + shape_rectangle.bottom_left.y;
                LengthDbl ye = rectangle_set_item.bottom_left.y + shape_rectangle.top_right.y;
                if (rectangle_set.x_min > xs)
                    rectangle_set.x_min = xs;
                if (rectangle_set.x_max < xe)
                    rectangle_set.x_max = xe;
                if (rectangle_set.y_min > ys)
                    rectangle_set.y_min = ys;
                if (rectangle_set.y_max < ye)
                    rectangle_set.y_max = ye;
            }
        }
        rectangle_sets_x_.push_back(rectangle_set);

        RectangleSet rectangle_set_y = rectangle_set;
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)rectangle_set.items.size();
                ++item_pos) {
            rectangle_set_y.items[item_pos].bottom_left.x = rectangle_set.items[item_pos].bottom_left.y;
            rectangle_set_y.items[item_pos].bottom_left.y = rectangle_set.items[item_pos].bottom_left.x;
        }
        rectangle_set_y.x_min = rectangle_set.y_min;
        rectangle_set_y.x_max = rectangle_set.y_max;
        rectangle_set_y.y_min = rectangle_set.x_min;
        rectangle_set_y.y_max = rectangle_set.x_max;
        rectangle_sets_y_.push_back(rectangle_set_y);
    }

    //for (RectangleSetId rectangle_set_id = 0;
    //        rectangle_set_id < (RectangleSetId)rectangle_sets_.size();
    //        ++rectangle_set_id) {
    //    const RectangleSet& rectangle_set = rectangle_sets_[rectangle_set_id];
    //    std::cout << "rectangle_set_id " << rectangle_set_id << std::endl;
    //    std::cout << "x_max " << rectangle_set.x_max << std::endl;
    //    std::cout << "y_max " << rectangle_set.y_max << std::endl;

    //    // Loop through rectangles of the rectangle set.
    //    for (ItemPos item_pos = 0;
    //            item_pos < (ItemPos)rectangle_set.items.size();
    //            ++item_pos) {
    //        const RectangleSetItem& rectangle_set_item = rectangle_set.items[item_pos];
    //        const ItemTypeRectangles item_type_rectangles = item_types_rectangles_[rectangle_set_item.item_type_id];
    //        std::cout << "item_type_id " << rectangle_set_item.item_type_id << std::endl;
    //        for (ItemShapePos item_shape_pos = 0;
    //                item_shape_pos < (ItemShapePos)item_type_rectangles.shapes.size();
    //                ++item_shape_pos) {
    //            const ItemShapeRectangles& item_shape_rectangles = item_type_rectangles.shapes[item_shape_pos];
    //            for (RectanglePos item_shape_rectangle_pos = 0;
    //                    item_shape_rectangle_pos < (RectanglePos)item_shape_rectangles.rectangles.size();
    //                    ++item_shape_rectangle_pos) {
    //            }
    //        }
    //    }
    //}


}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pfather,
        const Insertion& insertion) const
{
    const Node& father = *pfather;
    Node node;

    node.father = pfather;

    node.rectangle_set_id = insertion.rectangle_set_id;
    node.x = insertion.x;
    node.y = insertion.y;

    // Update number_of_bins and last_bin_direction.
    if (insertion.new_bin > 0) {  // New bin.
        node.number_of_bins = father.number_of_bins + 1;
        node.last_bin_direction = (insertion.new_bin == 1)?
            Direction::X:
            Direction::Y;
    } else {  // Same bin.
        node.number_of_bins = father.number_of_bins;
        node.last_bin_direction = father.last_bin_direction;
    }

    const RectangleSet& rectangle_set = (node.last_bin_direction == Direction::X)?
        rectangle_sets_x_[insertion.rectangle_set_id]:
        rectangle_sets_y_[insertion.rectangle_set_id];
    const RectangleSetItem& rectangle_set_item = rectangle_set.items[insertion.item_pos];
    const ItemTypeRectangles& item_type_rectangles = (node.last_bin_direction == Direction::X)?
        item_types_rectangles_x_[rectangle_set_item.item_type_id]:
        item_types_rectangles_y_[rectangle_set_item.item_type_id];
    const ItemShapeRectangles& item_shape_rectangles = item_type_rectangles.shapes[insertion.item_shape_pos];
    const ShapeRectangle& shape_rectangle = item_shape_rectangles.rectangles[insertion.item_shape_rectangle_pos];

    BinPos bin_pos = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);

    LengthDbl xs = insertion.x + rectangle_set_item.bottom_left.x + shape_rectangle.bottom_left.x;
    LengthDbl ys = insertion.y + rectangle_set_item.bottom_left.y + shape_rectangle.bottom_left.y;
    LengthDbl xe = insertion.x + rectangle_set_item.bottom_left.x + shape_rectangle.top_right.x;
    LengthDbl ye = insertion.y + rectangle_set_item.bottom_left.y + shape_rectangle.top_right.y;
    LengthDbl xi = instance().x_max(bin_type, o);
    LengthDbl yi = instance().y_max(bin_type, o);

    // Update uncovered_rectangles.
    ItemPos new_uncovered_rectangle_pos = -1;
    if (insertion.new_bin > 0) {  // New bin.
        if (ys > 0) {
            UncoveredRectangle uncovered_rectangle;
            uncovered_rectangle.item_type_id = -1;
            uncovered_rectangle.xs = 0;
            uncovered_rectangle.xe = 0;
            uncovered_rectangle.ys = 0;
            uncovered_rectangle.ye = ys;
            node.uncovered_rectangles.push_back(uncovered_rectangle);
        }
        {
            new_uncovered_rectangle_pos = node.uncovered_rectangles.size();
            UncoveredRectangle uncovered_rectangle;
            uncovered_rectangle.item_type_id = rectangle_set_item.item_type_id;
            uncovered_rectangle.item_shape_pos = insertion.item_shape_pos;
            uncovered_rectangle.item_shape_rectangle_pos = insertion.item_shape_rectangle_pos;
            uncovered_rectangle.xs = xs;
            uncovered_rectangle.xe = xe;
            uncovered_rectangle.ys = ys;
            uncovered_rectangle.ye = ye;
            node.uncovered_rectangles.push_back(uncovered_rectangle);
        }
        if (ye < yi) {
            UncoveredRectangle uncovered_rectangle;
            uncovered_rectangle.item_type_id = -1;
            uncovered_rectangle.xs = 0;
            uncovered_rectangle.xe = 0;
            uncovered_rectangle.ys = ye;
            uncovered_rectangle.ye = yi;
            node.uncovered_rectangles.push_back(uncovered_rectangle);
        }
    } else {  // Same bin.
        for (const UncoveredRectangle& uncovered_rectangle: father.uncovered_rectangles) {
            if (uncovered_rectangle.ye <= ys) {
                UncoveredRectangle new_uncovered_rectangle = uncovered_rectangle;
                node.uncovered_rectangles.push_back(new_uncovered_rectangle);
            } else if (uncovered_rectangle.ys <= ys) {
                if (uncovered_rectangle.ys < ys) {
                    UncoveredRectangle new_uncovered_rectangle = uncovered_rectangle;
                    new_uncovered_rectangle.ye = ys;
                    node.uncovered_rectangles.push_back(new_uncovered_rectangle);
                }

                new_uncovered_rectangle_pos = node.uncovered_rectangles.size();
                UncoveredRectangle new_uncovered_rectangle_2;
                new_uncovered_rectangle_2.item_type_id = rectangle_set_item.item_type_id;
                new_uncovered_rectangle_2.item_shape_pos = insertion.item_shape_pos;
                new_uncovered_rectangle_2.item_shape_rectangle_pos = insertion.item_shape_rectangle_pos;
                new_uncovered_rectangle_2.xs = xs;
                new_uncovered_rectangle_2.xe = xe;
                new_uncovered_rectangle_2.ys = ys;
                new_uncovered_rectangle_2.ye = ye;
                node.uncovered_rectangles.push_back(new_uncovered_rectangle_2);

                if (uncovered_rectangle.ye > ye) {
                    UncoveredRectangle new_uncovered_rectangle = uncovered_rectangle;
                    new_uncovered_rectangle.ys = ye;
                    node.uncovered_rectangles.push_back(new_uncovered_rectangle);
                }
            } else if (uncovered_rectangle.ys >= ye) {
                UncoveredRectangle new_uncovered_rectangle = uncovered_rectangle;
                node.uncovered_rectangles.push_back(new_uncovered_rectangle);
            } else {
                if (uncovered_rectangle.ye > ye) {
                    UncoveredRectangle new_uncovered_rectangle = uncovered_rectangle;
                    new_uncovered_rectangle.ys = ye;
                    node.uncovered_rectangles.push_back(new_uncovered_rectangle);
                }
            }
        }

        // Update extra rectangles.
        node.extra_rectangles = father.extra_rectangles;
    }

    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_area and profit.
    node.item_number_of_copies = father.item_number_of_copies;
    for (const RectangleSetItem& rectangle_set_item: rectangle_set.items) {
        const ItemType& item_type = instance().item_type(rectangle_set_item.item_type_id);
        node.item_number_of_copies[rectangle_set_item.item_type_id]++;
        node.number_of_items = father.number_of_items + 1;
        node.item_area = father.item_area + item_type.area;
        node.profit = father.profit + item_type.profit;
    }

    // Compute current_area, guide_area and width using uncovered_rectangles.
    node.xs_max = (insertion.new_bin == 0)?
        std::max(father.xs_max, insertion.x):
        insertion.x;
    node.current_area = instance_.previous_bin_area(bin_pos);
    node.guide_area = instance_.previous_bin_area(bin_pos) + node.xs_max * yi;
    LengthDbl x = 0;
    for (auto it = node.uncovered_rectangles.rbegin(); it != node.uncovered_rectangles.rend(); ++it) {
        const auto& uncovered_rectangle = *it;
        x = (parameters_.staircase)? std::max(x, uncovered_rectangle.xe): uncovered_rectangle.xe;
        node.current_area += x * (uncovered_rectangle.ye - uncovered_rectangle.ys);
        if (node.xe_max < x)
            node.xe_max = x;
        if (x > node.xs_max)
            node.guide_area += (x - node.xs_max) * (uncovered_rectangle.ye - uncovered_rectangle.ys);
    }

    for (const UncoveredRectangle& extra_rectangle: node.extra_rectangles) {
        if (node.xe_max < extra_rectangle.xe)
            node.xe_max = extra_rectangle.xe;
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
        throw std::runtime_error("waste");
    }
    if (node.waste < 0.0)
        node.waste = 0.0;

    node.id = node_id_++;
    return node;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& father) const
{
    insertions(father);
    std::vector<std::shared_ptr<Node>> cs(insertions_.size());
    for (Counter i = 0; i < (Counter)insertions_.size(); ++i) {
        cs[i] = std::make_shared<Node>(child_tmp(father, insertions_[i]));
        //std::cout << cs[i]->id
        //    << " rid " << cs[i]->rectangle_set_id
        //    << " x " << cs[i]->x
        //    << " y " << cs[i]->y
        //    << std::endl;
    }
    return cs;
}

const std::vector<BranchingScheme::Insertion>& BranchingScheme::insertions(
        const std::shared_ptr<Node>& father) const
{
    insertions_.clear();
    //std::cout << "insertions"
    //    << " id " << father->id
    //    << " rs " << father->rectangle_set_id
    //    << " x " << father->x
    //    << " y " << father->y
    //    << " n " << father->number_of_items
    //    << " item_area " << father->item_area
    //    << " guide_area " << father->guide_area
    //    << std::endl;

    // Check number of items for each rectangle set.
    const std::vector<RectangleSet>& rectangle_sets = (father->last_bin_direction == Direction::X)?
        rectangle_sets_x_:
        rectangle_sets_y_;
    const std::vector<ItemTypeRectangles>& item_types_rectangles = (father->last_bin_direction == Direction::X)?
        item_types_rectangles_x_:
        item_types_rectangles_y_;
    std::vector<RectangleSetId> valid_rectangle_set_ids;
    for (RectangleSetId rectangle_set_id = 0;
            rectangle_set_id < (RectangleSetId)rectangle_sets.size();
            ++rectangle_set_id) {
        const RectangleSet& rectangle_set = rectangle_sets[rectangle_set_id];

        bool ok = true;
        for (const auto& p: rectangle_set.item_types) {
            ItemTypeId item_type_id = p.first;
            ItemPos copies = p.second;
            if (father->item_number_of_copies[item_type_id] + copies
                    > instance().item_type(item_type_id).copies) {
                ok = false;
                break;
            }
        }
        if (ok)
            valid_rectangle_set_ids.push_back(rectangle_set_id);
    }

    // Insert in the current bin.
    if (father->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(father->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);

        // Loop through rectangle sets.
        for (RectangleSetId rectangle_set_id: valid_rectangle_set_ids) {
            const RectangleSet& rectangle_set = rectangle_sets[rectangle_set_id];

            // Loop through rectangles of the rectangle set.
            for (ItemPos item_pos = 0;
                    item_pos < (ItemPos)rectangle_set.items.size();
                    ++item_pos) {
                const RectangleSetItem& rectangle_set_item = rectangle_set.items[item_pos];
                const ItemTypeRectangles item_type_rectangles = item_types_rectangles[rectangle_set_item.item_type_id];
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type_rectangles.shapes.size();
                        ++item_shape_pos) {
                    const ItemShapeRectangles& item_shape_rectangles = item_type_rectangles.shapes[item_shape_pos];
                    for (RectanglePos item_shape_rectangle_pos = 0;
                            item_shape_rectangle_pos < (RectanglePos)item_shape_rectangles.rectangles.size();
                            ++item_shape_rectangle_pos) {

                        for (ItemPos uncovered_rectangle_pos = 0;
                                uncovered_rectangle_pos < (ItemPos)father->uncovered_rectangles.size();
                                ++uncovered_rectangle_pos) {
                            insertion_rectangle_set(
                                    father,
                                    rectangle_set_id,
                                    item_pos,
                                    item_shape_pos,
                                    item_shape_rectangle_pos,
                                    0,  // new_bin
                                    uncovered_rectangle_pos,
                                    -1,  // extra_rectangle_pos
                                    -1);  // defect_id
                        }

                        // Extra rectangles.
                        for (ItemPos extra_rectangle_pos = 0;
                                extra_rectangle_pos < (ItemPos)father->extra_rectangles.size();
                                ++extra_rectangle_pos) {
                            insertion_rectangle_set(
                                    father,
                                    rectangle_set_id,
                                    item_pos,
                                    item_shape_pos,
                                    item_shape_rectangle_pos,
                                    0,  // new_bin
                                    -1,  // uncovered_rectangle_pos
                                    extra_rectangle_pos,  // extra_rectangle_pos
                                    -1);
                        }

                        // Defects.
                        for (const Defect& defect: bin_type.defects) {
                            insertion_rectangle_set(
                                    father,
                                    rectangle_set_id,
                                    item_pos,
                                    item_shape_pos,
                                    item_shape_rectangle_pos,
                                    0,  // new_bin
                                    -1,  // uncovered_rectangle_pos
                                    -1,  // extra_rectangle_pos
                                    defect.id);
                        }

                    }
                }
            }
        }
    }

    // Insert in a new bin.
    if (insertions_.empty() && father->number_of_bins < instance().number_of_bins()) {
        BinTypeId bin_type_id = instance().bin_type_id(father->number_of_bins);
        const BinType& bin_type = instance().bin_type(bin_type_id);

        int new_bin = 0;
        if (parameters_.direction == Direction::X) {
            new_bin = 1;
        } else if (parameters_.direction == Direction::Y) {
            new_bin = 2;
        } else {
            new_bin = 1;
        }

        const std::vector<RectangleSet>& rectangle_sets = (new_bin == 1)?
            rectangle_sets_x_:
            rectangle_sets_y_;
        const std::vector<ItemTypeRectangles>& item_types_rectangles = (new_bin == 1)?
            item_types_rectangles_x_:
            item_types_rectangles_y_;

        // Loop through rectangle sets.
        for (RectangleSetId rectangle_set_id: valid_rectangle_set_ids) {
            const RectangleSet& rectangle_set = rectangle_sets[rectangle_set_id];

            // Loop through rectangles of the rectangle set.
            for (ItemPos item_pos = 0;
                    item_pos < (ItemPos)rectangle_set.items.size();
                    ++item_pos) {
                const RectangleSetItem& rectangle_set_item = rectangle_set.items[item_pos];
                const ItemTypeRectangles item_type_rectangles = item_types_rectangles[rectangle_set_item.item_type_id];
                for (ItemShapePos item_shape_pos = 0;
                        item_shape_pos < (ItemShapePos)item_type_rectangles.shapes.size();
                        ++item_shape_pos) {
                    const ItemShapeRectangles& item_shape_rectangles = item_type_rectangles.shapes[item_shape_pos];
                    for (RectanglePos item_shape_rectangle_pos = 0;
                            item_shape_rectangle_pos < (RectanglePos)item_shape_rectangles.rectangles.size();
                            ++item_shape_rectangle_pos) {

                        insertion_rectangle_set(
                                father,
                                rectangle_set_id,
                                item_pos,
                                item_shape_pos,
                                item_shape_rectangle_pos,
                                new_bin,
                                0,  // uncovered_rectangle_pos
                                -1,  // extra_rectangle_pos
                                -1);  // defect_id


                        // Defects.
                        for (const Defect& defect: bin_type.defects) {

                            insertion_rectangle_set(
                                    father,
                                    rectangle_set_id,
                                    item_pos,
                                    item_shape_pos,
                                    item_shape_rectangle_pos,
                                    new_bin,
                                    -1,  // uncovered_rectangle_pos
                                    -1,  // extra_rectangle_pos
                                    defect.id);
                        }
                    }
                }
            }
        }
    }

    return insertions_;
}

void BranchingScheme::insertion_rectangle_set(
        const std::shared_ptr<Node>& father,
        RectangleSetId rectangle_set_id,
        ItemPos item_pos,
        ItemShapePos item_shape_pos,
        RectanglePos item_shape_rectangle_pos,
        int8_t new_bin,
        ItemPos uncovered_rectangle_pos,
        ItemPos extra_rectangle_pos,
        DefectId defect_id) const
{
    //std::cout << "insertion_rectangle_set " << rectangle_set_id
    //    << " " << item_pos
    //    << " " << item_shape_pos
    //    << " " << item_shape_rectangle_pos
    //    << " new_bin " << new_bin
    //    << " urp " << uncovered_rectangle_pos
    //    << " erp " << extra_rectangle_pos
    //    << " d " << defect_id
    //    << std::endl;
    BinTypeId bin_type_id = (new_bin == 0)?
        instance().bin_type_id(father->number_of_bins - 1):
        instance().bin_type_id(father->number_of_bins);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = (new_bin == 0)?
        father->last_bin_direction:
        ((new_bin == 1)? Direction::X: Direction::Y);

    const std::vector<RectangleSet>& rectangle_sets = (o == Direction::X)?
        rectangle_sets_x_:
        rectangle_sets_y_;
    const std::vector<ItemTypeRectangles>& item_types_rectangles = (o == Direction::X)?
        item_types_rectangles_x_:
        item_types_rectangles_y_;
    const RectangleSet& rectangle_set = rectangle_sets[rectangle_set_id];
    const RectangleSetItem& rectangle_set_item = rectangle_set.items[item_pos];
    const ItemTypeRectangles& item_type_rectangles = item_types_rectangles[rectangle_set_item.item_type_id];
    const ItemShapeRectangles& item_shape_rectangles = item_type_rectangles.shapes[item_shape_pos];
    const ShapeRectangle& item_shape_rectangle = item_shape_rectangles.rectangles[item_shape_rectangle_pos];

    LengthDbl xi = instance().x_max(bin_type, o);
    LengthDbl yi = instance().y_max(bin_type, o);
    LengthDbl ys;
    if (uncovered_rectangle_pos > 0) {
        ys = father->uncovered_rectangles[uncovered_rectangle_pos].ys;
    //} else if (defect_id != -1) {
    //    ys = instance().y_end(bin_type.defects[defect_id], o);
    } else if (extra_rectangle_pos != -1) {  // new bin.
        ys = father->extra_rectangles[extra_rectangle_pos].ys;
    } else {  // new bin.
        ys = 0;
    }
    ys -= rectangle_set_item.bottom_left.y;
    ys -= item_shape_rectangle.bottom_left.y;
    LengthDbl ye = ys + rectangle_set.y_max;
    // Check bin top.
    if (ye > yi) {
        //std::cout << "too high " << ye << " / " << yi << std::endl;
        return;
    }
    // Check bin bottom.
    if (ys < -rectangle_set.y_min) {
        //std::cout << "too low"
        //    << " urp " << uncovered_rectangle_pos
        //    << " rsiy " << rectangle_set_item.bottom_left.y
        //    << " isry " << item_shape_rectangle.bottom_left.y
        //    << " ys " << ys << " / " << 0.0 << std::endl;
        return;
    }

    // Compute xs.
    LengthDbl xs = -rectangle_set.x_min;
    if (new_bin == 0) {

        // Loop through rectangles of the rectangle set.
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)rectangle_set.items.size();
                ++item_pos) {
            const RectangleSetItem& rectangle_set_item = rectangle_set.items[item_pos];
            const ItemTypeRectangles item_type_rectangles = item_types_rectangles[rectangle_set_item.item_type_id];
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type_rectangles.shapes.size();
                    ++item_shape_pos) {
                const ItemShapeRectangles& item_shape_rectangles = item_type_rectangles.shapes[item_shape_pos];
                for (RectanglePos item_shape_rectangle_pos = 0;
                        item_shape_rectangle_pos < (RectanglePos)item_shape_rectangles.rectangles.size();
                        ++item_shape_rectangle_pos) {
                    const ShapeRectangle& item_shape_rectangle = item_shape_rectangles.rectangles[item_shape_rectangle_pos];

                    LengthDbl ys_cur = ys + rectangle_set_item.bottom_left.y + item_shape_rectangle.bottom_left.y;
                    LengthDbl ye_cur = ys + rectangle_set_item.bottom_left.y + item_shape_rectangle.top_right.y;

                    for (const UncoveredRectangle& uncovered_rectangle: father->uncovered_rectangles) {
                        if (uncovered_rectangle.ye <= ys_cur || uncovered_rectangle.ys >= ye_cur)
                            continue;
                        LengthDbl xs_cur = xs + rectangle_set_item.bottom_left.x + item_shape_rectangle.bottom_left.x;
                        if (xs_cur < uncovered_rectangle.xe) {
                            //std::cout << "xs_cur " << xs_cur << " xs " << xs;
                            xs += (uncovered_rectangle.xe - xs_cur);
                            //std::cout << " -> " << xs << std::endl;
                        }
                    }

                }
            }
        }
    }

    // Extra rectangles.

    for (;;) {
        bool stop = true;

        // Loop through rectangles of the rectangle set.
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)rectangle_set.items.size();
                ++item_pos) {
            const RectangleSetItem& rectangle_set_item = rectangle_set.items[item_pos];
            const ItemTypeRectangles item_type_rectangles = item_types_rectangles[rectangle_set_item.item_type_id];
            for (ItemShapePos item_shape_pos = 0;
                    item_shape_pos < (ItemShapePos)item_type_rectangles.shapes.size();
                    ++item_shape_pos) {
                const ItemShapeRectangles& item_shape_rectangles = item_type_rectangles.shapes[item_shape_pos];
                for (RectanglePos item_shape_rectangle_pos = 0;
                        item_shape_rectangle_pos < (RectanglePos)item_shape_rectangles.rectangles.size();
                        ++item_shape_rectangle_pos) {
                    const ShapeRectangle& item_shape_rectangle = item_shape_rectangles.rectangles[item_shape_rectangle_pos];

                    LengthDbl ys_cur = ys + rectangle_set_item.bottom_left.y + item_shape_rectangle.bottom_left.y;
                    LengthDbl ye_cur = ys + rectangle_set_item.bottom_left.y + item_shape_rectangle.top_right.y;

                    // Extra rectangles.
                    for (const UncoveredRectangle& extra_rectangle: father->extra_rectangles) {
                        LengthDbl xs_cur = xs + rectangle_set_item.bottom_left.x + item_shape_rectangle.bottom_left.x;
                        LengthDbl xe_cur = xs + rectangle_set_item.bottom_left.x + item_shape_rectangle.top_right.x;
                        if (ys_cur >= extra_rectangle.ye)
                            continue;
                        if (extra_rectangle.xs >= ye_cur)
                            continue;
                        if (extra_rectangle.xs >= xe_cur)
                            continue;
                        if (xs_cur >= extra_rectangle.xe)
                            continue;
                        xs = (extra_rectangle.xe - rectangle_set_item.bottom_left.x - item_shape_rectangle.top_right.x);
                        stop = false;
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
        }

        if (stop)
            break;
    }

    LengthDbl xe = xs + rectangle_set.x_max;
    // Check bin width.
    if (xe > xi) {
        //std::cout << "too wide " << xe << " / " << xi
        //    << " xs " << xs
        //    << " x_max " << rectangle_set.x_max
        //    << std::endl;
        return;
    }

    if (parameters_.staircase && new_bin == 0 && uncovered_rectangle_pos != -1)
        for (const UncoveredRectangle& uncovered_rectangle: father->uncovered_rectangles)
            if (uncovered_rectangle.ys > ye && uncovered_rectangle.xe > xs)
                return;

    if (uncovered_rectangle_pos > 0) {
        if (xe <= father->uncovered_rectangles[uncovered_rectangle_pos - 1].xs) {
            //std::cout << "urp " << xe << " / " << father->uncovered_rectangles[uncovered_rectangle_pos - 1].xs << std::endl;
            return;
        }
    } else if (extra_rectangle_pos != -1) {
        if (xe <= father->extra_rectangles[extra_rectangle_pos].xs)
            return;
        if (xs >= father->extra_rectangles[extra_rectangle_pos].xe)
            return;
    //} else if (defect_id != -1) {
    //    if (xe <= instance().x_start(bin_type.defects[defect_id], o))
    //        return;
    //    if (xs >= instance().x_end(bin_type.defects[defect_id], o))
    //        return;
    }

    Insertion insertion;
    insertion.rectangle_set_id = rectangle_set_id;
    insertion.item_pos = item_pos;
    insertion.item_shape_pos = item_shape_pos;
    insertion.item_shape_rectangle_pos = item_shape_rectangle_pos;
    insertion.x = xs;
    insertion.y = ys;
    insertion.new_bin = new_bin;
    insertions_.push_back(insertion);
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
    } case Objective::OpenDimensionX: case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return (std::max(node_1->xe_max, node_1->waste + instance_.item_area() - 1)
                / (instance().x_max(instance_.bin_type(0), Direction::X) + 1)) >= node_2->xe_max;
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
            current_node->father != nullptr;
            current_node = current_node->father) {
        descendents.push_back(current_node);
    }
    std::reverse(descendents.begin(), descendents.end());

    Solution solution(instance());
    BinPos bin_pos = -1;
    for (auto current_node: descendents) {
        if (current_node->number_of_bins > solution.number_of_bins())
            bin_pos = solution.add_bin(instance().bin_type_id(current_node->number_of_bins - 1), 1);

        const RectangleSet& rectangle_set = (current_node->last_bin_direction == Direction::X)?
            rectangle_sets_x_[current_node->rectangle_set_id]:
            rectangle_sets_y_[current_node->rectangle_set_id];
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)rectangle_set.items.size();
                ++item_pos) {
            const RectangleSetItem& rectangle_set_item = rectangle_set.items[item_pos];
            LengthDbl x = current_node->x + rectangle_set_item.bottom_left.x;
            LengthDbl y = current_node->y + rectangle_set_item.bottom_left.y;
            Point bl_corner = (current_node->last_bin_direction == Direction::X)?
                Point{x, y}:
                Point{y, x};
            //std::cout << "bin_pos " << bin_pos
            //    << " item_type_id " << rectangle_set_item.item_type_id
            //    << " bl_corner " << bl_corner.to_string()
            //    << " angle " << rectangle_set_item.angle
            //    << std::endl;
            solution.add_item(
                    bin_pos,
                    rectangle_set_item.item_type_id,
                    bl_corner,
                    rectangle_set_item.angle);
        }
    }
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredRectangle& uncovered_rectangle)
{
    os << "item_type_id " << uncovered_rectangle.item_type_id
        << " xs " << uncovered_rectangle.xs
        << " xe " << uncovered_rectangle.xe
        << " ys " << uncovered_rectangle.ys
        << " ye " << uncovered_rectangle.ye
        ;
    return os;
}

bool BranchingScheme::UncoveredRectangle::operator==(
        const UncoveredRectangle& uncovered_rectangle) const
{
    return ((item_type_id == uncovered_rectangle.item_type_id)
            && (xs == uncovered_rectangle.xs)
            && (xe == uncovered_rectangle.xe)
            && (ys == uncovered_rectangle.ys)
            && (ye == uncovered_rectangle.ye)
            );
}

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return ((rectangle_set_id == insertion.rectangle_set_id)
            && (item_pos == insertion.item_pos)
            && (item_shape_pos == insertion.item_shape_pos)
            && (item_shape_rectangle_pos == insertion.item_shape_rectangle_pos)
            && (new_bin == insertion.new_bin)
            && (x == insertion.x)
            && (y == insertion.y)
            );
}

std::ostream& packingsolver::irregular::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "rectangle_set_id " << insertion.rectangle_set_id
        << " item_pos " << insertion.item_pos
        << " item_shape_pos " << insertion.item_shape_pos
        << " item_shape_rectangle_pos " << insertion.item_shape_rectangle_pos
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
