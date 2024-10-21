#include "rectangle/branching_scheme.hpp"

#include <iostream>
#include <string>

using namespace packingsolver;
using namespace packingsolver::rectangle;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BranchingScheme::dominates(
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2)
{
    const ItemType& item_type_1 = instance().item_type(item_type_id_1);
    const ItemType& item_type_2 = instance().item_type(item_type_id_2);
    if (parameters_.predecessor_strategy == 0) {
        return item_type_1.profit >= item_type_2.profit;
    } else if (parameters_.predecessor_strategy == 1) {
        return item_type_1.weight <= item_type_2.weight;
    } else if (parameters_.predecessor_strategy == 2) {
        return item_type_1.weight >= item_type_2.weight;
    } else {
        throw std::invalid_argument("");
        return false;
    }
}

BranchingScheme::BranchingScheme(
        const Instance& instance,
        const Parameters& parameters):
    instance_(instance),
    parameters_(parameters),
    predecessors_(instance.number_of_item_types()),
    predecessors_1_(instance.number_of_item_types()),
    predecessors_2_(instance.number_of_item_types())
{
    // Compute predecessors.
    //instance.format(std::cout, 3);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance.item_type(item_type_id);
        for (ItemTypeId item_type_id_2 = 0;
                item_type_id_2 < instance.number_of_item_types();
                ++item_type_id_2) {
            if (item_type_id_2 == item_type_id)
                continue;
            const ItemType& item_type_2 = instance.item_type(item_type_id_2);

            if (item_type.group_id != item_type_2.group_id)
                continue;

            if (!dominates(item_type_id_2, item_type_id))
                continue;
            if (item_type_id < item_type_id_2
                    && dominates(item_type_id, item_type_id_2)) {
                continue;
            }

            if ((item_type.rect.x == item_type_2.rect.x
                        && item_type.rect.y == item_type_2.rect.y
                        && item_type.oriented == item_type_2.oriented)
                    || (item_type.rect.x == item_type_2.rect.y
                        && item_type.rect.y == item_type_2.rect.x
                        && item_type.oriented == false
                        && item_type_2.oriented == false)) {
                predecessors_[item_type_id].push_back(item_type_id_2);
                //std::cout << "0 " << item_type_id_2 << " -> " << item_type_id << std::endl;
            }

            if (item_type.rect.x == item_type_2.rect.x
                    && item_type.rect.y == item_type_2.rect.y
                    && item_type.oriented == false
                    && item_type_2.oriented == true) {
                predecessors_1_[item_type_id].push_back(item_type_id_2);
                //std::cout << "1 " << item_type_id_2 << " -> " << item_type_id << std::endl;
            }

            if (item_type.rect.x == item_type_2.rect.y
                    && item_type.rect.y == item_type_2.rect.x
                    && item_type.oriented == false
                    && item_type_2.oriented == true) {
                predecessors_2_[item_type_id].push_back(item_type_id_2);
                //std::cout << "2 " << item_type_id_2 << " -> " << item_type_id << std::endl;
            }
        }
    }

    // Build root node if some part of the solution is already fixed.
    if (parameters.fixed_items != nullptr) {
        root_ = root();
        for (BinPos bin_pos = 0; bin_pos < parameters.fixed_items->number_of_different_bins(); ++bin_pos) {
            const SolutionBin& solution_bin = parameters.fixed_items->bin(bin_pos);
            // Get items and sort them by increasing x-coordinate.
            std::vector<SolutionItem> solution_items = solution_bin.items;
            sort(
                    solution_items.begin(),
                    solution_items.end(),
                    [](
                        const SolutionItem& item_1,
                        const SolutionItem& item_2) -> bool
                    {
                        return item_1.bl_corner.x < item_2.bl_corner.x;
                    });
            for (BinPos c = 0; c < solution_bin.copies; ++c) {
                bool new_bin = true;
                for (const auto& solution_item: solution_items) {
                    //std::cout << "j " << solution_item.j
                    //    << " x " << solution_item.bl_corner.x
                    //    << " y " << solution_item.bl_corner.y
                    //    << " lx " << instance.item_type(solution_item.j).rect.x
                    //    << " ly " << instance.item_type(solution_item.j).rect.y
                    //    << std::endl;
                    Insertion insertion;
                    insertion.item_type_id = solution_item.item_type_id;
                    insertion.rotate = solution_item.rotate;
                    insertion.x = solution_item.bl_corner.x;
                    insertion.y = solution_item.bl_corner.y;
                    insertion.new_bin = new_bin;
                    new_bin = false;
                    root_ = child(root_, insertion);
                }
            }
        }
        //std::cout << "number_of_items " << root_->number_of_items
        //    << " item_area " << root_->item_area
        //    << " current_area " << root_->current_area
        //    << std::endl;
    }

    //unbounded_knapsck_ = instance.unbounded_knapsck();
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    const Node& parent = *pparent;
    Node node;

    const ItemType& item_type = instance().item_type(insertion.item_type_id);
    node.parent = pparent;

    node.item_type_id = insertion.item_type_id;
    node.rotate = insertion.rotate;
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

    BinPos bin_pos = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
    const BinType& bin_type = instance().bin_type(bin_type_id);

    Length xj = instance().x(item_type, insertion.rotate, o);
    Length yj = instance().y(item_type, insertion.rotate, o);
    Length xs = insertion.x;
    Length xe = insertion.x + xj;
    Length ys = insertion.y;
    Length ye = insertion.y + yj;
    Length xi = instance().x(bin_type, o);
    Length yi = instance().y(bin_type, o);

    // Update uncovered_items.
    ItemPos new_uncovered_item_pos = -1;
    if (insertion.new_bin > 0) {  // New bin.
        if (ys > 0) {
            UncoveredItem uncovered_item;
            uncovered_item.item_type_id = -1;
            uncovered_item.xs = 0;
            uncovered_item.xe = 0;
            uncovered_item.xe_dominance = 0;
            uncovered_item.ys = 0;
            uncovered_item.ye = ys;
            node.uncovered_items.push_back(uncovered_item);
        }
        {
            new_uncovered_item_pos = node.uncovered_items.size();
            UncoveredItem uncovered_item;
            uncovered_item.item_type_id = insertion.item_type_id;
            uncovered_item.xs = xs;
            uncovered_item.xe = xe;
            uncovered_item.xe_dominance = xe;
            uncovered_item.ys = ys;
            uncovered_item.ye = ye;
            node.uncovered_items.push_back(uncovered_item);
        }
        if (ye < yi) {
            UncoveredItem uncovered_item;
            uncovered_item.item_type_id = -1;
            uncovered_item.xs = 0;
            uncovered_item.xe = 0;
            uncovered_item.xe_dominance = 0;
            uncovered_item.ys = ye;
            uncovered_item.ye = yi;
            node.uncovered_items.push_back(uncovered_item);
        }
    } else {  // Same bin.
        for (const UncoveredItem& uncovered_item: parent.uncovered_items) {
            if (uncovered_item.ye <= ys) {
                UncoveredItem new_uncovered_item = uncovered_item;
                node.uncovered_items.push_back(new_uncovered_item);
            } else if (uncovered_item.ys <= ys) {
                if (uncovered_item.ys < ys) {
                    UncoveredItem new_uncovered_item = uncovered_item;
                    new_uncovered_item.ye = ys;
                    node.uncovered_items.push_back(new_uncovered_item);
                }

                new_uncovered_item_pos = node.uncovered_items.size();
                UncoveredItem new_uncovered_item_2;
                new_uncovered_item_2.item_type_id = insertion.item_type_id;
                new_uncovered_item_2.xs = xs;
                new_uncovered_item_2.xe = xe;
                new_uncovered_item_2.xe_dominance = xe;
                new_uncovered_item_2.ys = ys;
                new_uncovered_item_2.ye = ye;
                node.uncovered_items.push_back(new_uncovered_item_2);

                if (uncovered_item.ye > ye) {
                    UncoveredItem new_uncovered_item = uncovered_item;
                    new_uncovered_item.ys = ye;
                    node.uncovered_items.push_back(new_uncovered_item);
                }
            } else if (uncovered_item.ys >= ye) {
                UncoveredItem new_uncovered_item = uncovered_item;
                node.uncovered_items.push_back(new_uncovered_item);
            } else {
                if (uncovered_item.ye > ye) {
                    UncoveredItem new_uncovered_item = uncovered_item;
                    new_uncovered_item.ys = ye;
                    node.uncovered_items.push_back(new_uncovered_item);
                }
            }
        }
    }

    // Update uncovered_item.xe_dominance.
    Length smallest_item_height = (node.last_bin_direction == Direction::X)?
        instance().smallest_item_height():
        instance().smallest_item_width();
    if (yi - node.uncovered_items[new_uncovered_item_pos].ye
            < smallest_item_height) {
        for (ItemPos uncovered_item_pos = new_uncovered_item_pos + 1;
                uncovered_item_pos < node.uncovered_items.size();
                ++uncovered_item_pos) {
            node.uncovered_items[uncovered_item_pos].xe_dominance
                = (std::max)(
                        node.uncovered_items[uncovered_item_pos].xe_dominance,
                        node.uncovered_items[new_uncovered_item_pos].xe_dominance);
        }
    }
    if (node.uncovered_items[new_uncovered_item_pos].ys
            < smallest_item_height) {
        for (ItemPos uncovered_item_pos = new_uncovered_item_pos - 1;
                uncovered_item_pos >= 0;
                --uncovered_item_pos) {
            node.uncovered_items[uncovered_item_pos].xe_dominance
                = (std::max)(
                        node.uncovered_items[uncovered_item_pos].xe_dominance,
                        node.uncovered_items[new_uncovered_item_pos].xe_dominance);
        }
    }


    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_area and profit.
    node.item_number_of_copies = parent.item_number_of_copies;
    node.item_number_of_copies[insertion.item_type_id]++;
    node.number_of_items = parent.number_of_items + 1;
    node.item_area = parent.item_area + item_type.area();
    node.item_weight = parent.item_weight + item_type.weight;
    node.weight_profit = parent.item_weight
        + (double)1.0 / item_type.weight / insertion.x;
    node.profit = parent.profit + item_type.profit;
    if (item_type.oriented) {
        node.guide_item_pseudo_profit = parent.guide_item_pseudo_profit
            + std::pow(xj, 1.2) * std::pow(yj, 1.1) / item_type.area();
    } else {
        node.guide_item_pseudo_profit = parent.guide_item_pseudo_profit
            + std::pow(xj, 1.1) * std::pow(yj, 1.1) / item_type.area();
    }
    if (parameters_.group_guiding_strategy == 0) {
        node.guide_item_area = parent.guide_item_area
            + item_type.area() * (item_type.group_id + 1);
        node.guide_profit = parent.guide_profit
            + item_type.profit * (item_type.group_id + 1);
    } else if (parameters_.group_guiding_strategy == 1) {
        node.guide_item_area = parent.guide_item_area + item_type.area();
        node.guide_profit = parent.guide_profit + item_type.profit;
    }
    node.group_score = parent.group_score + (item_type.group_id + 1);
    node.groups = parent.groups;

    if (insertion.new_bin != 0) {
        for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
            node.groups[group_id].last_bin_weight = 0;
            node.groups[group_id].last_bin_weight_weighted_sum = 0;
        }
    }
    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        node.groups[group_id].last_bin_weight += item_type.weight;
        node.groups[group_id].last_bin_weight_weighted_sum
            += ((double)xs + (double)(xe - xs) / 2) * item_type.weight;
    }

    for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
        if (!instance().check_weight_constraints(group_id))
            continue;
        if (node.groups[group_id].last_bin_weight == 0)
            continue;
        std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                node.groups[group_id].last_bin_weight_weighted_sum, node.groups[group_id].last_bin_weight);
        node.groups[group_id].last_bin_middle_axle_weight = axle_weights.first;
        node.groups[group_id].last_bin_rear_axle_weight = axle_weights.second;
        if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL)
            node.middle_axle_overweight += axle_weights.first - bin_type.semi_trailer_truck_data.middle_axle_maximum_weight;
        if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL)
            node.rear_axle_overweight += axle_weights.second - bin_type.semi_trailer_truck_data.rear_axle_maximum_weight;
    }

    // Compute current_area, guide_area and width using uncovered_items.
    node.xs_max = (insertion.new_bin == 0)?
        std::max(parent.xs_max, insertion.x):
        insertion.x;
    node.ye_max = (insertion.new_bin == 0)? parent.ye_max: 0;
    node.current_area = instance_.previous_bin_area(bin_pos);
    node.guide_area = instance_.previous_bin_area(bin_pos) + node.xs_max * yi;
    Length x = 0;
    for (auto it = node.uncovered_items.rbegin(); it != node.uncovered_items.rend(); ++it) {
        const auto& uncovered_item = *it;
        x = (parameters_.staircase)? std::max(x, uncovered_item.xe): uncovered_item.xe;
        node.current_area += x * (uncovered_item.ye - uncovered_item.ys);
        if (node.xe_max < x)
            node.xe_max = x;
        if (uncovered_item.xe > 0) {
            if (node.ye_max < uncovered_item.ye)
                node.ye_max = uncovered_item.ye;
        }
        if (x > node.xs_max)
            node.guide_area += (x - node.xs_max) * (uncovered_item.ye - uncovered_item.ys);
    }

    if (node.number_of_items == instance().number_of_items()) {
        node.current_area = instance().previous_bin_area(bin_pos) + node.xe_max * yi;
    }

    node.waste = node.current_area - node.item_area;

    {
        Area waste = instance().previous_bin_area(bin_pos) + node.xe_max * yi - instance().item_area();
        if (node.waste < waste)
            node.waste = waste;
    }

    // If there are few items left, try to detect if an extra bin will be
    // required.
    if (node.number_of_bins < instance().number_of_bins()
            && instance().item_area() - node.item_area < bin_type.area()) {
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance().number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance().item_type(item_type_id);
            if (node.item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            Length xj = instance().x(item_type, false, o);
            Length yj = instance().y(item_type, false, o);
            if (!item_type.oriented) {
                Length yj = instance().y(item_type, false, o);
                if (xj > yj)
                    xj = yj;
            }
            int ok = false;
            for (const UncoveredItem& uncovered_item: node.uncovered_items) {
                if (uncovered_item.ys + yj <= yi
                        && uncovered_item.xe + xj <= xi) {
                    ok = true;
                    break;;
                }
                if (!item_type.oriented
                        && uncovered_item.ys + xj <= yi
                        && uncovered_item.xe + yj <= xi) {
                    ok = true;
                    break;
                }
            }
            if (!ok) {
                if (!item_type.oriented && xj > yj)
                    xj = yj;
                Area waste = instance().previous_bin_area(bin_pos + 1) + xj * yi - instance().item_area();
                if (node.waste < waste)
                    node.waste = waste;
                break;
            }
        }
    }

    if (node.waste < 0) {
        instance().format(std::cout, 3);
        std::cout
            << "current_area " << node.current_area
            << " number_of_items " << node.number_of_items
            << " item_area " << node.item_area
            << " waste " << node.waste
            << std::endl;
        std::cout << "id " << node.id
            << " number_of_items " << node.number_of_items
            << " group_score " << node.group_score
            << " load " << (double)node.item_area / node.guide_area
            << " last_bin_middle_axle_weight " << node.groups.front().last_bin_middle_axle_weight
            << std::endl;
        for (auto it = node.uncovered_items.rbegin(); it != node.uncovered_items.rend(); ++it) {
            std::cout
                << " ys " << it->ys
                << " ye " << it->ye
                << " xe " << it->xe
                << " item_type_id " << it->item_type_id
                << std::endl;
        }
        for (auto node_tmp = &node;
                node_tmp->parent != nullptr;
                node_tmp = node_tmp->parent.get()) {
            std::cout << " item_type_id " << node_tmp->item_type_id
                << " x " << node_tmp->x
                << " y " << node_tmp->y
                << " r " << node_tmp->rotate
                << std::endl;
        }
        throw std::runtime_error("waste");
    }

    node.leftover_value = bin_type.area() - node.xe_max * node.ye_max;

    if (instance().unloading_constraint() == rectangle::UnloadingConstraint::IncreasingX
            || instance().unloading_constraint() == rectangle::UnloadingConstraint::IncreasingY) {
        if (node.groups[item_type.group_id].x_min > insertion.x)
            node.groups[item_type.group_id].x_min = insertion.x;
        if (node.groups[item_type.group_id].x_max < insertion.x)
            node.groups[item_type.group_id].x_max = insertion.x;
    }
    node.groups[item_type.group_id].number_of_items++;

    node.id = node_id_++;
    return node;
}

std::vector<std::shared_ptr<BranchingScheme::Node>> BranchingScheme::children(
        const std::shared_ptr<Node>& parent) const
{
    insertions(parent);
    std::vector<std::shared_ptr<Node>> cs(insertions_.size());
    for (Counter i = 0; i < (Counter)insertions_.size(); ++i)
        cs[i] = std::make_shared<Node>(child_tmp(parent, insertions_[i]));
    return cs;
}

const std::vector<BranchingScheme::Insertion>& BranchingScheme::insertions(
        const std::shared_ptr<Node>& parent) const
{
    //std::cout << "id " << parent->id
    //    << " number_of_items " << parent->number_of_items
    //    << " number_of_bins " << parent->number_of_bins
    //    << " waste " << parent->waste
    //    << " group_score " << parent->group_score
    //    << " load " << (double)parent->item_area / parent->guide_area
    //    << " last_bin_middle_axle_weight " << parent->groups.front().last_bin_middle_axle_weight
    //    << std::endl;
    //for (auto it = parent->uncovered_items.rbegin(); it != parent->uncovered_items.rend(); ++it) {
    //    std::cout << " ys " << it->ys << " ye " << it->ye << " xe " << it->xe << " item_type_id " << it->item_type_id << std::endl;
    //}
    //for (auto node = parent; node->parent != nullptr; node = node->parent) {
    //    std::cout << " item_type_id " << node->item_type_id << " x " << node->x << " y " << node->y << " r " << node->rotate << std::endl;
    //}

    insertions_.clear();

    std::vector<bool> ok(instance_.number_of_item_types(), true);
    std::vector<bool> ok_1(instance_.number_of_item_types(), true);
    std::vector<bool> ok_2(instance_.number_of_item_types(), true);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        for (ItemTypeId item_type_id_pred: predecessors_[item_type_id])
            if (parent->item_number_of_copies[item_type_id_pred]
                    != instance_.item_type(item_type_id_pred).copies)
                ok[item_type_id] = false;
        for (ItemTypeId item_type_id_pred: predecessors_1_[item_type_id])
            if (parent->item_number_of_copies[item_type_id_pred]
                    != instance_.item_type(item_type_id_pred).copies)
                ok_1[item_type_id] = false;
        for (ItemTypeId item_type_id_pred: predecessors_2_[item_type_id])
            if (parent->item_number_of_copies[item_type_id_pred]
                    != instance_.item_type(item_type_id_pred).copies)
                ok_2[item_type_id] = false;
    }

    // Insert in the current bin.
    if (parent->number_of_bins > 0) {
        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
        const BinType& bin_type = instance().bin_type(bin_type_id);

        // Items.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {

            if (!ok[item_type_id])
                continue;

            const ItemType& item_type = instance_.item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;

            for (ItemPos uncovered_item_pos = 0;
                    uncovered_item_pos < (ItemPos)parent->uncovered_items.size();
                    ++uncovered_item_pos) {

                if (ok_1[item_type_id]) {
                    insertion_item(
                            parent,
                            item_type_id,
                            false,  // rotate
                            0,  // new_bin
                            uncovered_item_pos,
                            -1);  // defect_id

                    if (parameters().fixed_items != nullptr) {
                        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
                        const BinType& bin_type = instance().bin_type(bin_type_id);
                        Direction o = parent->last_bin_direction;
                        bool rotate = false;
                        Length yj = instance().y(item_type, rotate, o);
                        //if (uncovered_item_pos > 0)
                        //    std::cout << "xs " << parent->uncovered_items[uncovered_item_pos - 1].xs
                        //        << " xe " << parent->uncovered_items[uncovered_item_pos - 1].xe
                        //        << " ye " << parent->uncovered_items[uncovered_item_pos - 1].ys
                        //        << " ye " << parent->uncovered_items[uncovered_item_pos - 1].ye
                        //        << std::endl;
                        //std::cout << "xs " << parent->uncovered_items[uncovered_item_pos].xs
                        //    << " xe " << parent->uncovered_items[uncovered_item_pos].xe
                        //    << " ye " << parent->uncovered_items[uncovered_item_pos].ys
                        //    << " ye " << parent->uncovered_items[uncovered_item_pos].ye
                        //    << std::endl;
                        //if (uncovered_item_pos + 1 < parent->uncovered_items.size())
                        //    std::cout << "xs " << parent->uncovered_items[uncovered_item_pos + 1].xs
                        //        << " xe " << parent->uncovered_items[uncovered_item_pos + 1].xe
                        //        << " ye " << parent->uncovered_items[uncovered_item_pos + 1].ys
                        //        << " ye " << parent->uncovered_items[uncovered_item_pos + 1].ye
                        //        << std::endl;
                        if (uncovered_item_pos + 1 < parent->uncovered_items.size()
                                && parent->uncovered_items[uncovered_item_pos + 1].xe
                                < parameters().fixed_items->x_max()) {
                            insertion_item_fixed(
                                    parent,
                                    item_type_id,
                                    rotate,
                                    parent->uncovered_items[uncovered_item_pos].xe,
                                    parent->uncovered_items[uncovered_item_pos].ye);
                        }
                        if (uncovered_item_pos > 0
                                && parent->uncovered_items[uncovered_item_pos - 1].xe
                                < parameters().fixed_items->x_max()) {
                            insertion_item_fixed(
                                    parent,
                                    item_type_id,
                                    rotate,
                                    parent->uncovered_items[uncovered_item_pos].xe,
                                    parent->uncovered_items[uncovered_item_pos].ys - yj);
                        }
                    }

                }

                if (!item_type.oriented && ok_2[item_type_id]) {
                    insertion_item(
                            parent,
                            item_type_id,
                            true,  // rotate
                            0,  // new_bin
                            uncovered_item_pos,
                            -1);  // defect_id

                    if (parameters().fixed_items != nullptr) {
                        BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
                        const BinType& bin_type = instance().bin_type(bin_type_id);
                        Direction o = parent->last_bin_direction;
                        bool rotate = true;
                        Length yj = instance().y(item_type, rotate, o);
                        if (uncovered_item_pos + 1 < parent->uncovered_items.size()
                                && parent->uncovered_items[uncovered_item_pos + 1].xe
                                < parameters().fixed_items->x_max()) {
                            insertion_item_fixed(
                                    parent,
                                    item_type_id,
                                    rotate,
                                    parent->uncovered_items[uncovered_item_pos].xe,
                                    parent->uncovered_items[uncovered_item_pos].ye);
                        }
                        if (uncovered_item_pos > 0
                                && parent->uncovered_items[uncovered_item_pos - 1].xe
                                < parameters().fixed_items->x_max()) {
                            insertion_item_fixed(
                                    parent,
                                    item_type_id,
                                    rotate,
                                    parent->uncovered_items[uncovered_item_pos].xe,
                                    parent->uncovered_items[uncovered_item_pos].ys - yj);
                        }
                    }

                }
            }

            // Defects.
            for (const Defect& defect: bin_type.defects) {

                if (ok_1[item_type_id])
                    insertion_item(
                            parent,
                            item_type_id,
                            false,  // rotate
                            0,  // new_bin
                            -1,  // uncovered_item_pos
                            defect.id);

                if (!item_type.oriented && ok_2[item_type_id])
                    insertion_item(
                            parent,
                            item_type_id,
                            true,  // rotate
                            0,  // new_bin
                            -1,  // uncovered_item_pos
                            defect.id);
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
            if ((double)instance().total_item_height() / bin_type.rect.y
                    >= (double)instance().total_item_width() / bin_type.rect.x) {
                new_bin = 1;
            } else {
                new_bin = 2;
            }
        }

        // Items.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {

            if (!ok[item_type_id])
                continue;

            const ItemType& item_type = instance_.item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;

            if (ok_1[item_type_id])
                insertion_item(
                        parent,
                        item_type_id,
                        false,
                        new_bin,
                        0,  // uncovered_item_pos
                        -1);  // defect_id

            if (!item_type.oriented && ok_2[item_type_id])
                insertion_item(
                        parent,
                        item_type_id,
                        true,  // rotate
                        new_bin,
                        0,  // uncovered_item_pos
                        -1);  // defect_id

            // Defects.
            for (const Defect& defect: bin_type.defects) {

                if (ok_1[item_type_id])
                    insertion_item(
                            parent,
                            item_type_id,
                            false,  // rotate
                            new_bin,
                            -1,  // uncovered_item_pos
                            defect.id);

                if (!item_type.oriented && ok_2[item_type_id])
                    insertion_item(
                            parent,
                            item_type_id,
                            true,  // rotate
                            new_bin,
                            -1,  // uncovered_item_pos
                            defect.id);
            }
        }
    }

    return insertions_;
}

void BranchingScheme::insertion_item(
        const std::shared_ptr<Node>& parent,
        ItemTypeId item_type_id,
        bool rotate,
        int8_t new_bin,
        ItemPos uncovered_item_pos,
        DefectId defect_id) const
{
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = (new_bin == 0)?
        instance().bin_type_id(parent->number_of_bins - 1):
        instance().bin_type_id(parent->number_of_bins);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = (new_bin == 0)?
        parent->last_bin_direction:
        ((new_bin == 1)? Direction::X: Direction::Y);
    Length xj = instance().x(item_type, rotate, o);
    Length yj = instance().y(item_type, rotate, o);
    Length xi = instance().x(bin_type, o);
    Length yi = instance().y(bin_type, o);
    Length ys;
    if (uncovered_item_pos > 0) {
        ys = parent->uncovered_items[uncovered_item_pos].ys;
    } else if (defect_id != -1) {
        ys = instance().y_end(bin_type.defects[defect_id], o);
    } else {  // new bin.
        ys = 0;
    }
    Length ye = ys + yj;
    // Check bin y.
    if (ye > yi)
        return;
    // Check maximum weight.
    double last_bin_weight = (new_bin == 0)?
        parent->groups.front().last_bin_weight + item_type.weight:
        item_type.weight;
    if (last_bin_weight > bin_type.maximum_weight * PSTOL)
        return;

    // Compute xs.
    Length xs = 0;
    if (new_bin == 0) {
        for (const UncoveredItem& uncovered_item: parent->uncovered_items) {
            if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
                continue;
            if (xs < uncovered_item.xe)
                xs = uncovered_item.xe;
        }
    }

    // Check unloading constraints.
    switch (instance().unloading_constraint()) {
    case UnloadingConstraint::None: {
        break;
    } case UnloadingConstraint::OnlyXMovements: case::UnloadingConstraint::OnlyYMovements: {
        // Check if an item from the uncovered item with higher group is not
        // blocked by the new item.
        for (const UncoveredItem& uncovered_item: parent->uncovered_items) {
            if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
                continue;
            if (uncovered_item.item_type_id == -1)
                continue;
            const ItemType& item_type_pred = instance().item_type(uncovered_item.item_type_id);
            if (item_type.group_id > item_type_pred.group_id)
                return;
        }
        break;
    } case UnloadingConstraint::IncreasingX: case UnloadingConstraint::IncreasingY: {
        for (GroupId group = item_type.group_id + 1; group < instance().number_of_groups(); ++group)
            if (xs < parent->groups[group].x_max)
                return;
        for (GroupId group = 0; group < item_type.group_id; ++group)
            if (xs > parent->groups[group].x_min)
                return;
        break;
    }
    }

    // Defects
    // While the item intersects a defect, move it to the right.
    for (;;) {
        bool stop = true;
        for (const Defect& defect: bin_type.defects) {
            if (instance().x_start(defect, o) >= xs + xj)
                continue;
            if (xs >= instance().x_end(defect, o))
                continue;
            if (instance().y_start(defect, o) >= ye)
                continue;
            if (ys >= instance().y_end(defect, o))
                continue;
            xs = instance().x_end(defect, o);
            stop = false;
        }
        if (stop)
            break;
    }

    Length xe = xs + xj;
    // Check bin width.
    if (xe > xi)
        return;

    if (parameters_.staircase && new_bin == 0 && uncovered_item_pos != -1)
        for (const UncoveredItem& uncovered_item: parent->uncovered_items)
            if (uncovered_item.ys > ye && uncovered_item.xe > xs)
                return;

    if (uncovered_item_pos > 0) {
        if (xe <= parent->uncovered_items[uncovered_item_pos - 1].xs)
            return;
    } else if (defect_id != -1) {
        if (xe <= instance().x_start(bin_type.defects[defect_id], o))
            return;
        if (xs >= instance().x_end(bin_type.defects[defect_id], o))
            return;
    }

    // If there are fixed items, we forbid adding an item before the end of the
    // fixed items.
    if (parameters().fixed_items != nullptr
            && xs < parameters().fixed_items->x_max())
        return;

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.rotate = rotate;
    insertion.x = xs;
    insertion.y = ys;
    insertion.new_bin = new_bin;
    insertions_.push_back(insertion);
}

void BranchingScheme::insertion_item_fixed(
        const std::shared_ptr<Node>& parent,
        ItemTypeId item_type_id,
        bool rotate,
        Length xs,
        Length ys) const
{
    const ItemType& item_type = instance_.item_type(item_type_id);
    BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins - 1);
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Direction o = parent->last_bin_direction;
    Length xj = instance().x(item_type, rotate, o);
    Length yj = instance().y(item_type, rotate, o);
    Length xi = instance().x(bin_type, o);
    Length yi = instance().y(bin_type, o);
    Length xe = xs + xj;
    Length ye = ys + yj;

    //std::cout << "insertion_item_fixed " << item_type_id
    //    << " xs " << xs
    //    << " ys " << ys
    //    << std::endl;

    // Check bin y.
    if (ys < 0)
        return;
    if (ye > yi)
        return;
    // Check bin x.
    if (xe > xi)
        return;
    // Check maximum weight.
    double last_bin_weight = parent->groups.front().last_bin_weight + item_type.weight;
    if (last_bin_weight > bin_type.maximum_weight * PSTOL)
        return;

    // Check intersections with other packed items.
    for (const UncoveredItem& uncovered_item: parent->uncovered_items) {
        if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
            continue;
        if (xs < uncovered_item.xe) {
            //std::cout << "xe " << xe << std::endl;
            return;
        }
    }

    // Check unloading constraints.
    switch (instance().unloading_constraint()) {
    case UnloadingConstraint::None: {
        break;
    } case UnloadingConstraint::OnlyXMovements: case::UnloadingConstraint::OnlyYMovements: {
        // Check if an item from the uncovered item with higher group is not
        // blocked by the new item.
        for (const UncoveredItem& uncovered_item: parent->uncovered_items) {
            if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
                continue;
            if (uncovered_item.item_type_id == -1)
                continue;
            const ItemType& item_type_pred = instance().item_type(uncovered_item.item_type_id);
            if (item_type.group_id > item_type_pred.group_id)
                return;
        }
        break;
    } case UnloadingConstraint::IncreasingX: case UnloadingConstraint::IncreasingY: {
        for (GroupId group = item_type.group_id + 1; group < instance().number_of_groups(); ++group)
            if (xs < parent->groups[group].x_max)
                return;
        for (GroupId group = 0; group < item_type.group_id; ++group)
            if (xs > parent->groups[group].x_min)
                return;
        break;
    }
    }

    // Check intersection with defects.
    for (const Defect& defect: bin_type.defects) {
        if (instance().x_start(defect, o) >= xs + xj)
            continue;
        if (xs >= instance().x_end(defect, o))
            continue;
        if (instance().y_start(defect, o) >= ye)
            continue;
        if (ys >= instance().y_end(defect, o))
            continue;
        return;
    }

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.rotate = rotate;
    insertion.x = xs;
    insertion.y = ys;
    insertion.new_bin = 0;
    insertions_.push_back(insertion);
    //std::cout << "ok" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const std::shared_ptr<BranchingScheme::Node> BranchingScheme::root() const
{
    if (root_ != nullptr)
        return std::make_shared<Node>(*root_);

    BranchingScheme::Node node;
    node.item_number_of_copies = std::vector<ItemPos>(instance_.number_of_item_types(), 0);
    node.groups = std::vector<NodeGroup>(instance().number_of_groups());
    for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
        node.groups[group_id].x_min = std::numeric_limits<Length>::max();
    }
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
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        if (node_1->profit != node_2->profit)
            return node_1->profit > node_2->profit;
        return node_1->middle_axle_overweight + node_1->rear_axle_overweight
            < node_2->middle_axle_overweight + node_2->rear_axle_overweight;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'rectangle::BranchingScheme'"
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
        Area a = instance_.item_area() + node_1->waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance_.number_of_bins())
                return true;
            BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
            a -= instance_.bin_type(bin_type_id).area();
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
    } case Objective::OpenDimensionX: case Objective::OpenDimensionY: {
        if (!leaf(node_2))
            return false;
        return (std::max(node_1->xe_max, node_1->waste + instance_.item_area() - 1)
                / (instance().x(instance_.bin_type(0), Direction::X) + 1)) >= node_2->xe_max;
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        return false;
    } default: {
        std::stringstream ss;
        ss << "Branching scheme 'rectangle::BranchingScheme'"
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
        Point bl_corner = (current_node->last_bin_direction == Direction::X)?
            Point{current_node->x, current_node->y}:
            Point{current_node->y, current_node->x};
        solution.add_item(
                bin_pos,
                current_node->item_type_id,
                bl_corner,
                current_node->rotate);
        //std::cout << "i " << i
        //    << " item_type_id " << current_node->item_type_id
        //    << " bl_corner " << bl_corner
        //    << " rotate " << current_node->rotate
        //    << " waste " << current_node->waste
        //    << std::endl;
    }
    if (node->number_of_bins == 1
            && node->last_bin_direction == Direction::Y
            && node->xe_max != solution.y_max()) {
        throw std::runtime_error(
                "node->xe_max: " + std::to_string(node->xe_max)
                + "; solution.y_max(): " + std::to_string(solution.y_max())
                + ".");
    }
    //if (node->waste != solution.waste()) {
    //    std::cout << node->xe_max << std::endl;
    //    std::cout << node->current_area << std::endl;
    //    std::cout << node->item_area << std::endl;
    //    std::cout << solution.x_max() << std::endl;
    //    std::cout << node->parent->waste << std::endl;
    //    throw std::runtime_error(
    //            "node->waste: " + std::to_string(node->waste)
    //            + "; solution.waste(): " + std::to_string(solution.waste())
    //            + ".\n");
    //}
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::rectangle::operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredItem& uncovered_item)
{
    os << "item_type_id " << uncovered_item.item_type_id
        << " xs " << uncovered_item.xs
        << " xe " << uncovered_item.xe << " " << uncovered_item.xe_dominance
        << " ys " << uncovered_item.ys
        << " ye " << uncovered_item.ye
        ;
    return os;
}

bool BranchingScheme::UncoveredItem::operator==(
        const UncoveredItem& uncovered_item) const
{
    return ((item_type_id == uncovered_item.item_type_id)
            && (xs == uncovered_item.xs)
            && (xe == uncovered_item.xe)
            && (ys == uncovered_item.ys)
            && (ye == uncovered_item.ye)
            );
}

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return ((item_type_id == insertion.item_type_id)
            && (rotate == insertion.rotate)
            && (new_bin == insertion.new_bin)
            && (x == insertion.x)
            && (y == insertion.y)
            );
}

std::ostream& packingsolver::rectangle::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "item_type_id " << insertion.item_type_id
        << " rotate " << insertion.rotate
        << " new_bin " << (int)insertion.new_bin
        << " x " << insertion.x
        << " y " << insertion.y
        ;
    return os;
}

std::ostream& packingsolver::rectangle::operator<<(
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
