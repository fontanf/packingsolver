#include "box/branching_scheme.hpp"

#include <string>

using namespace packingsolver;
using namespace packingsolver::box;

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BranchingScheme ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool BranchingScheme::dominates(
        ItemTypeId item_type_id_1,
        ItemTypeId item_type_id_2)
{
    const ItemType& item_type_1 = instance().item_type(item_type_id_1);
    const ItemType& item_type_2 = instance().item_type(item_type_id_2);
    return (item_type_1.profit >= item_type_2.profit
            && item_type_1.weight <= item_type_2.weight);
}

BranchingScheme::BranchingScheme(
        const Instance& instance_orig,
        const Parameters& parameters):
    instance_(instance_orig),
    instance_flipper_y_(instance_orig, Direction::Y),
    instance_flipper_z_(instance_orig, Direction::Z),
    parameters_(parameters),
    predecessors_(instance_orig.number_of_item_types())
{
    const Instance& instance = this->instance();

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

            if (!dominates(item_type_id_2, item_type_id))
                continue;
            if (item_type_id < item_type_id_2
                    && dominates(item_type_id, item_type_id_2)) {
                continue;
            }

            if (item_type.box.x == item_type_2.box.x
                        && item_type.box.y == item_type_2.box.y
                        && item_type.box.z == item_type_2.box.z
                        && item_type.rotations == item_type_2.rotations) {
                predecessors_[item_type_id].push_back(item_type_id_2);
                //std::cout << "0 " << item_type_id_2 << " -> " << item_type_id << std::endl;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// children ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void BranchingScheme::update_uncovered_item(
        std::vector<UncoveredItem>& uncovered_items,
        const UncoveredItem& uncovered_item,
        Length ys,
        Length ye,
        Length zs,
        Length ze) const
{
    //std::cout << "update_uncovered_item"
    //    << " " << uncovered_item
    //    << " ys " << ys
    //    << " ye " << ye
    //    << " zs " << zs
    //    << " ze " << ze << std::endl;
    if (ys >= uncovered_item.ye
            || ye <= uncovered_item.ys
            || zs >= uncovered_item.ze
            || ze <= uncovered_item.zs) {
        uncovered_items.push_back(uncovered_item);
        return;
    }
    bool has_y_low = (ys > uncovered_item.ys);
    bool has_y_high = (ye < uncovered_item.ye);
    bool has_z_low = (zs > uncovered_item.zs);
    bool has_z_high = (ze < uncovered_item.ze);
    //std::cout << "has_y_low " << has_y_low
    //    << " has_y_high " << has_y_high
    //    << " has_z_low " << has_z_low
    //    << " has_z_high " << has_z_high
    //    << std::endl;
    if (has_y_low) {
        UncoveredItem uncovered_item_new = uncovered_item;
        uncovered_item_new.ye = ys;
        uncovered_items.push_back(uncovered_item_new);
    }
    if (has_y_low) {
        if (has_y_high) {
            if (has_z_low) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.ys = ys;
                uncovered_item_new.ye = ye;
                uncovered_item_new.ze = zs;
                uncovered_items.push_back(uncovered_item_new);
            }
            if (has_z_high) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.ys = ys;
                uncovered_item_new.ye = ye;
                uncovered_item_new.zs = ze;
                uncovered_items.push_back(uncovered_item_new);
            }
        } else {
            if (has_z_low) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.ys = ys;
                uncovered_item_new.ze = zs;
                uncovered_items.push_back(uncovered_item_new);
            }
            if (has_z_high) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.ys = ys;
                uncovered_item_new.zs = ze;
                uncovered_items.push_back(uncovered_item_new);
            }
        }
    } else {
        if (has_y_high) {
            if (has_z_low) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.ye = ye;
                uncovered_item_new.ze = zs;
                uncovered_items.push_back(uncovered_item_new);
            }
            if (has_z_high) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.ye = ye;
                uncovered_item_new.zs = ze;
                uncovered_items.push_back(uncovered_item_new);
            }
        } else {
            if (has_z_low) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.ze = zs;
                uncovered_items.push_back(uncovered_item_new);
            }
            if (has_z_high) {
                UncoveredItem uncovered_item_new = uncovered_item;
                uncovered_item_new.zs = ze;
                uncovered_items.push_back(uncovered_item_new);
            }
        }
    }
    if (has_y_high) {
        UncoveredItem uncovered_item_new = uncovered_item;
        uncovered_item_new.ys = ye;
        uncovered_items.push_back(uncovered_item_new);
    }
}

BranchingScheme::Node BranchingScheme::child_tmp(
        const std::shared_ptr<Node>& pparent,
        const Insertion& insertion) const
{
    const Node& parent = *pparent;
    Node node;

    node.parent = pparent;

    node.item_type_id = insertion.item_type_id;
    node.rotation = insertion.rotation;
    node.x = insertion.x;
    node.y = insertion.y;
    node.z = insertion.z;

    // Update number_of_bins and last_bin_direction.
    if (insertion.new_bin > 0) {  // New bin.
        node.number_of_bins = parent.number_of_bins + 1;
        node.last_bin_direction = this->direction(insertion.new_bin);
    } else {  // Same bin.
        node.number_of_bins = parent.number_of_bins;
        node.last_bin_direction = parent.last_bin_direction;
    }
    const Instance& instance = this->instance(node.last_bin_direction);

    BinPos i = node.number_of_bins - 1;
    Direction o = node.last_bin_direction;
    BinTypeId bin_type_id = instance.bin_type_id(i);
    const BinType& bin_type = instance.bin_type(bin_type_id);
    const ItemType& item_type = instance.item_type(insertion.item_type_id);

    Box box = item_type.box.rotate(insertion.rotation);
    Length xs = insertion.x;
    Length ys = insertion.y;
    Length zs = insertion.z;
    Length xe = xs + box.x;
    Length ye = ys + box.y;
    Length ze = zs + box.z;
    Length xi = bin_type.box.x;
    Length yi = bin_type.box.y;
    Length zi = bin_type.box.z;
    Volume item_volume = box.volume();
    if (insertion.z + box.z > zi) {
        throw std::runtime_error(
                FUNC_SIGNATURE + "; "
                "insertion.z: " + std::to_string(insertion.z)
                + "; zj: " + std::to_string(box.z)
                + "; zi: " + std::to_string(zi)
                + ".");
    }

    UncoveredItem new_uncovered_item;
    new_uncovered_item.x = xe;
    new_uncovered_item.x_dominance = xe;
    new_uncovered_item.ys = ys;
    new_uncovered_item.ye = ye;
    new_uncovered_item.zs = zs;
    new_uncovered_item.ze = ze;

    // Update uncovered_items.
    if (insertion.new_bin > 0) {  // New bin.
        {
            UncoveredItem uncovered_item;
            uncovered_item.x = 0;
            uncovered_item.x_dominance = 0;
            uncovered_item.ys = 0;
            uncovered_item.ye = bin_type.box.y;
            uncovered_item.zs = 0;
            uncovered_item.ze = bin_type.box.z;
            update_uncovered_item(node.uncovered_items, uncovered_item, ys, ye, zs, ze);
        }
        node.uncovered_items.push_back(new_uncovered_item);
    } else {
        // Compute node.uncovered_item.
        for (const UncoveredItem& uncovered_item: parent.uncovered_items)
            update_uncovered_item(node.uncovered_items, uncovered_item, ys, ye, zs, ze);
        node.uncovered_items.push_back(new_uncovered_item);
    }

    // Update x_dominance for regions adjacent to thin gaps.
    // If a gap between the new item's edge and the bin boundary is smaller than
    // the smallest item dimension in that direction, no item can ever fill it,
    // so inflate x_dominance of uncovered items in that strip.
    {
        const UncoveredItem& new_ui = node.uncovered_items.back();
        // Gap above new item in y.
        if (yi - new_ui.ye < instance.smallest_item_y()) {
            for (UncoveredItem& ui: node.uncovered_items)
                if (ui.ys >= new_ui.ye)
                    ui.x_dominance = std::max(ui.x_dominance, new_ui.x_dominance);
        }
        // Gap below new item in y.
        if (new_ui.ys < instance.smallest_item_y()) {
            for (UncoveredItem& ui: node.uncovered_items)
                if (ui.ye <= new_ui.ys)
                    ui.x_dominance = std::max(ui.x_dominance, new_ui.x_dominance);
        }
        // Gap above new item in z.
        if (zi - new_ui.ze < instance.smallest_item_z()) {
            for (UncoveredItem& ui: node.uncovered_items)
                if (ui.zs >= new_ui.ze)
                    ui.x_dominance = std::max(ui.x_dominance, new_ui.x_dominance);
        }
        // Gap below new item in z.
        if (new_ui.zs < instance.smallest_item_z()) {
            for (UncoveredItem& ui: node.uncovered_items)
                if (ui.ze <= new_ui.zs)
                    ui.x_dominance = std::max(ui.x_dominance, new_ui.x_dominance);
        }
    }

    // Compute item_number_of_copies, number_of_items, items_area,
    // squared_item_volume and profit.
    node.last_bin_weight = (insertion.new_bin == 0)?
        parent.last_bin_weight + item_type.weight:
        item_type.weight;
    node.item_number_of_copies = parent.item_number_of_copies;
    node.item_number_of_copies[insertion.item_type_id]++;
    node.number_of_items = parent.number_of_items + 1;
    node.item_volume = parent.item_volume + item_volume;
    node.item_weight = parent.item_weight + item_type.weight;
    node.profit = parent.profit + item_type.profit;

    // Compute current_volume, guide_volume and width using uncovered_items.
    node.xs_max = (insertion.new_bin == 0)?
        std::max(parent.xs_max, insertion.x):
        insertion.x;
    node.current_volume = instance.previous_bin_volume(i);
    node.guide_volume = instance.previous_bin_volume(i) + node.xs_max * yi * zi;
    for (const UncoveredItem& uncovered_item: node.uncovered_items) {
        node.current_volume += uncovered_item.x_dominance
            * (uncovered_item.ye - uncovered_item.ys)
            * (uncovered_item.ze - uncovered_item.zs);
        if (node.xe_max < uncovered_item.x)
            node.xe_max = uncovered_item.x;
        if (uncovered_item.x_dominance > node.xs_max) {
            node.guide_volume += (uncovered_item.x_dominance - node.xs_max)
                * (uncovered_item.ye - uncovered_item.ys)
                * (uncovered_item.ze - uncovered_item.zs);
        }
    }
    node.waste = node.current_volume - node.item_volume;

    node.id = node_id_++;

    //std::cout << "node.number_of_items " << node.number_of_items << std::endl;
    //std::cout << "node.xs_max " << node.xs_max
    //    << " expected_length " << expected_lengths_[node.number_of_items] << std::endl;
    //std::cout << "node.last_bin_middle_axle_weight " << node.last_bin_middle_axle_weight
    //    << " expected_axle_weight " << expected_axle_weights_[node.number_of_items].first << std::endl;

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
    //std::cout << "insertions " << *parent;

    insertions_.clear();

    std::vector<bool> ok(instance().number_of_item_types(), true);
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance().number_of_item_types();
            ++item_type_id) {
        for (ItemTypeId item_type_id_pred: predecessors_[item_type_id])
            if (parent->item_number_of_copies[item_type_id_pred]
                    != instance().item_type(item_type_id_pred).copies)
                ok[item_type_id] = false;
    }

    // Insert in the current bin.
    if (parent->number_of_bins > 0) {
        const Instance& instance = this->instance(parent->last_bin_direction);
        BinTypeId bin_type_id = instance.bin_type_id(parent->number_of_bins - 1);
        const BinType& bin_type = instance.bin_type(bin_type_id);

        // For all y uncovered item.
        for (ItemPos y_uncovered_item_pos = -1;
                y_uncovered_item_pos < (ItemPos)parent->uncovered_items.size();
                ++y_uncovered_item_pos) {
            // For all z uncovered item.
            for (ItemPos z_uncovered_item_pos = -1;
                    z_uncovered_item_pos < (ItemPos)parent->uncovered_items.size();
                    ++z_uncovered_item_pos) {
                // For all remaining items.
                for (ItemTypeId item_type_id = 0;
                        item_type_id < instance.number_of_item_types();
                        ++item_type_id) {
                    if (!ok[item_type_id])
                        continue;
                    const ItemType& item_type = instance.item_type(item_type_id);
                    if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                        continue;
                    // For all valid rotations.
                    for (Rotation rotation: item_type.rotations) {
                        insertion_item(
                                parent,
                                item_type_id,
                                rotation,
                                0,  // new_bin
                                y_uncovered_item_pos,
                                z_uncovered_item_pos);
                    }
                }
            }
        }
    }

    // Insert in a new bin.
    if (insertions_.empty() && parent->number_of_bins < instance().number_of_bins()) {
        int new_bin = 0;
        if (parameters_.direction == Direction::X) {
            new_bin = 1;
        } else if (parameters_.direction == Direction::Y) {
            new_bin = 2;
        } else if (parameters_.direction == Direction::Z) {
            new_bin = 3;
        } else {
            BinTypeId bin_type_id = instance().bin_type_id(parent->number_of_bins);
            const BinType& bin_type = instance().bin_type(bin_type_id);
            if (bin_type.box.x >= std::max(bin_type.box.y, bin_type.box.z)) {
                new_bin = 1;
            } else if (bin_type.box.y >= bin_type.box.z) {
                new_bin = 2;
            } else {
                new_bin = 3;
            }
        }
        //std::cout << "new bin " << (int)new_bin << std::endl;

        const Instance& instance = this->instance(new_bin);
        BinTypeId bin_type_id = instance.bin_type_id(parent->number_of_bins);
        const BinType& bin_type = instance.bin_type(bin_type_id);

        // Items.
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance.number_of_item_types();
                ++item_type_id) {
            //std::cout << "item_type_id " << item_type_id << std::endl;

            if (!ok[item_type_id])
                continue;

            const ItemType& item_type = instance.item_type(item_type_id);
            if (parent->item_number_of_copies[item_type_id] == item_type.copies)
                continue;
            for (Rotation rotation: item_type.rotations) {
                insertion_item(
                        parent,
                        item_type_id,
                        rotation,
                        new_bin,
                        -1,  // y_uncovered_item_pos
                        -1);  // z_uncovered_item_pos
            }
        }
    }

    //std::cout << "insertion:" << std::endl;
    //for (const Insertion& insertion: insertions_)
    //    std::cout << "- " << insertion << std::endl;
    return insertions_;
}

void BranchingScheme::insertion_item(
        const std::shared_ptr<Node>& parent,
        ItemTypeId item_type_id,
        Rotation rotation,
        int8_t new_bin,
        ItemPos y_uncovered_item_pos,
        ItemPos z_uncovered_item_pos) const
{
    //std::cout << "insertion_item"
    //    << " item_type_id " << item_type_id
    //    << " rotation " << rotation
    //    << " new_bin " << (int)new_bin
    //    << " y_uncovered_item_pos " << y_uncovered_item_pos
    //    << " z_uncovered_item_pos " << z_uncovered_item_pos
    //    << std::endl;

    const Instance& instance = (new_bin == 0)?
        this->instance(parent->last_bin_direction):
        this->instance(new_bin);
    const ItemType& item_type = instance.item_type(item_type_id);
    BinTypeId bin_type_id = (new_bin == 0)?
        instance.bin_type_id(parent->number_of_bins - 1):
        instance.bin_type_id(parent->number_of_bins);
    const BinType& bin_type = instance.bin_type(bin_type_id);
    Box box = item_type.box.rotate(rotation);
    Length xi = bin_type.box.x;
    Length yi = bin_type.box.y;
    Length zi = bin_type.box.z;
    Length ys = (y_uncovered_item_pos >= 0)? parent->uncovered_items[y_uncovered_item_pos].ye: 0;
    Length zs = (z_uncovered_item_pos >= 0)? parent->uncovered_items[z_uncovered_item_pos].ze: 0;
    Length ye = ys + box.y;
    Length ze = zs + box.z;
    // Check bin y.
    if (ye > yi) {
        //std::cout << "ye " << ye << " > yi " << yi << std::endl;
        return;
    }
    // Check bin z.
    if (ze > zi) {
        //std::cout << "ze " << ze << " > zi " << zi << std::endl;
        return;
    }
    // Check maximum weight.
    double last_bin_weight = (new_bin == 0)?  // Same bin
        parent->last_bin_weight + item_type.weight:
        item_type.weight;
    if (last_bin_weight > bin_type.maximum_weight * PSTOL) {
        //std::cout << "maximum_weight" << std::endl;
        return;
    }

    // Check contact with y and z uncovered items.
    if (z_uncovered_item_pos >= 0) {
        if (ye <= parent->uncovered_items[z_uncovered_item_pos].ys) {
            //std::cout << "ye " << ye << " <= zuys " << parent->uncovered_items[z_uncovered_item_pos].ys << std::endl;
            return;
        }
        if (ys >= parent->uncovered_items[z_uncovered_item_pos].ye) {
            //std::cout << "ys " << ys << " <= zuye " << parent->uncovered_items[z_uncovered_item_pos].ye << std::endl;
            return;
        }
    }
    if (y_uncovered_item_pos >= 0) {
        if (ze <= parent->uncovered_items[y_uncovered_item_pos].zs) {
            //std::cout << "ze " << ze << " <= yuzs " << parent->y_uncovered_items[y_uncovered_item_pos].zs << std::endl;
            return;
        }
        if (zs >= parent->uncovered_items[y_uncovered_item_pos].ze) {
            //std::cout << "zs " << zs << " <= yuze " << parent->y_uncovered_items[y_uncovered_item_pos].ze << std::endl;
            return;
        }
    }

    // Compute xs.
    Length xs = 0;
    if (new_bin == 0) {
        for (const UncoveredItem& uncovered_item: parent->uncovered_items) {
            if (uncovered_item.ye <= ys || uncovered_item.ys >= ye)
                continue;
            if (uncovered_item.ze <= zs || uncovered_item.zs >= ze)
                continue;
            if (xs < uncovered_item.x)
                xs = uncovered_item.x;
        }
    }
    Length xe = xs + box.x;

    // Check bin x.
    if (xe > xi) {
        //std::cout << "xe " << xe << " > xi " << xi << std::endl;
        return;
    }

    // Check contact with y and z uncovered items.
    if (y_uncovered_item_pos >= 0) {
        if (xs >= parent->uncovered_items[y_uncovered_item_pos].x) {
            //std::cout << "xs " << xs << " <= yuxe " << parent->y_uncovered_items[y_uncovered_item_pos].xe << std::endl;
            return;
        }
    }
    if (z_uncovered_item_pos >= 0) {
        if (xs >= parent->uncovered_items[z_uncovered_item_pos].x) {
            //std::cout << "xs " << xs << " <= zuxe " << parent->uncovered_items[z_uncovered_item_pos].xe << std::endl;
            return;
        }
    }

    Insertion insertion;
    insertion.item_type_id = item_type_id;
    insertion.rotation = rotation;
    insertion.x = xs;
    insertion.y = ys;
    insertion.z = zs;
    insertion.new_bin = new_bin;
    insertions_.push_back(insertion);
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
    } case Objective::Feasibility: {
        return node_2->profit < node_1->profit;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "Branching scheme 'box::BranchingScheme' "
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
        if (!leaf(node_2)) {
            return (ubkp(*node_1) <= node_2->profit);
        } else {
            if (ubkp(*node_1) != node_2->profit)
                return (ubkp(*node_1) <= node_2->profit);
            return node_1->waste >= node_2->waste;
        }
    } case Objective::BinPacking: case Objective::VariableSizedBinPacking: {
        if (!leaf(node_2))
            return false;
        BinPos bin_pos = -1;
        Area a = instance().item_volume() + node_1->waste;
        while (a > 0) {
            bin_pos++;
            if (bin_pos >= instance().number_of_bins())
                return true;
            BinTypeId bin_type_id = instance().bin_type_id(bin_pos);
            const BinType& bin_type = instance().bin_type(bin_type_id);
            a -= bin_type.volume();
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
    } case Objective::Feasibility: {
        if (leaf(node_2))
            return true;
        return false;
    } case Objective::OpenDimensionX: case Objective::OpenDimensionY: case packingsolver::Objective::OpenDimensionZ: {
        if (!leaf(node_2))
            return false;
        Length b = std::max(
                node_1->xe_max,
                (node_1->waste + instance().item_volume() - 1)
                / (instance().bin_type(0).box.y * instance().bin_type(0).box.z + 1));
        //std::cout << "b " << b << " xe_max " << node_2->xe_max << std::endl;
        return b >= node_2->xe_max;
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "branching scheme 'box::BranchingScheme' "
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
        const Instance& instance = this->instance(current_node->last_bin_direction);
        if (current_node->number_of_bins > solution.number_of_bins())
            bin_pos = solution.add_bin(
                    instance.bin_type_id(current_node->number_of_bins - 1),
                    1);
        const ItemType& item_type = instance.item_type(current_node->item_type_id);
        Point bl_corner = convert_point_back(
                {current_node->x, current_node->y, current_node->z},
                current_node->last_bin_direction);
        Rotation original_rotation = get_flipped_rotation(
                current_node->rotation,
                current_node->last_bin_direction);
        //std::cout
        //    << " item_type_id " << current_node->item_type_id
        //    << " x " << current_node->x
        //    << " y " << current_node->y
        //    << " z " << current_node->z
        //    << " xj " << xj
        //    << " yj " << yj
        //    << " rot " << current_node->rotation
        //    << " o " << current_node->last_bin_direction
        //    << std::endl;
        solution.add_item(
                bin_pos,
                current_node->item_type_id,
                bl_corner,
                original_rotation);
    }
    return solution;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

nlohmann::json BranchingScheme::json_export_init() const
{
    nlohmann::json json_init;
    Counter i = 0;

    // Bins.
    json_bins_init_ids_ = std::vector<Counter>(instance().number_of_bin_types(), -1);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance().number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance().bin_type(bin_type_id);

        json_bins_init_ids_[bin_type_id] = i;
        json_init[i][0] = {
            {"Type", "Box"},
            {"X", bin_type.box.x},
            {"Y", bin_type.box.y},
            {"Z", bin_type.box.z},
            {"FillColor", ""},
        };
        i++;
    }

    // Items.
    json_items_init_ids_ = std::vector<std::vector<Counter>>(instance().number_of_item_types());
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance().number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance().item_type(item_type_id);

        json_items_init_ids_[item_type_id] = std::vector<Counter>(NUMBER_OF_ROTATIONS, -1);
        for (Rotation rotation: item_type.rotations) {
            json_items_init_ids_[item_type_id][(int)rotation] = i;
            Box box = item_type.box.rotate(rotation);
            json_init[i] = {
                {"Type", "Box"},
                {"X", box.x},
                {"Y", box.y},
                {"Z", box.z},
                {"FillColor", "blue"},
            };
            i++;
        }
    }

    return json_init;
}

nlohmann::json BranchingScheme::json_export(
        const std::shared_ptr<Node>& node) const
{
    if (node->number_of_items == 0) {
        nlohmann::json json = {
            {"Id", node->id},
            {"ParentId", (node->parent == nullptr)? -1: node->parent->id},
        };
        return json;
    }

    Point bl_orig = convert_point_back(
            {node->x, node->y, node->z},
            node->last_bin_direction);
    nlohmann::json json = {
        {"Id", node->id},
        {"ParentId", (node->parent == nullptr)? -1: node->parent->id},
        {"ItemTypeId", node->item_type_id},
        {"Rotation", (int)node->rotation},
        {"X", node->x},
        {"Y", node->y},
        {"Z", node->z},
        {"XOrig", bl_orig.x},
        {"YOrig", bl_orig.y},
        {"ZOrig", bl_orig.z},
        {"NumberOfItems", node->number_of_items},
        {"NumberOfBins", node->number_of_bins},
        {"Profit", node->profit},
        {"ItemVolume", node->item_volume},
        {"GuideVolume", node->guide_volume},
    };

    nlohmann::json plot;
    Counter i = 0;
    // Plot bin.
    BinTypeId bin_type_id = instance().bin_type_id(node->number_of_bins - 1);
    plot[i] = {
        {"Id",json_bins_init_ids_[bin_type_id]},
        {"X", 0},
        {"Y", 0},
        {"Z", 0},
    };
    i++;
    // Plot items.
    for (std::shared_ptr<const Node> node_tmp = node;
            node_tmp->number_of_bins == node->number_of_bins;
            node_tmp = node_tmp->parent) {
        Point bl_corner = convert_point_back(
                {node_tmp->x, node_tmp->y, node_tmp->z},
                node_tmp->last_bin_direction);
        plot[i] = {
            {"Id", json_items_init_ids_[node->item_type_id][(int)node->rotation]},
            {"X", bl_corner.x},
            {"Y", bl_corner.y},
            {"Z", bl_corner.z},
        };
        if (node_tmp == node)
            plot[i]["FillColor"] = "green";
        i++;
    }
    json["Plot"] = plot;
    return json;
}

std::ostream& packingsolver::box::operator<<(
        std::ostream& os,
        const BranchingScheme::UncoveredItem& uncovered_item)
{
    os << "x " << uncovered_item.x
        << " ys " << uncovered_item.ys
        << " ye " << uncovered_item.ye
        << " zs " << uncovered_item.zs
        << " ze " << uncovered_item.ze
        ;
    return os;
}

bool BranchingScheme::UncoveredItem::operator==(
        const UncoveredItem& uncovered_item) const
{
    return ((x == uncovered_item.x)
            && (ys == uncovered_item.ys)
            && (ye == uncovered_item.ye)
            && (zs == uncovered_item.zs)
            && (ze == uncovered_item.ze)
            );
}

bool BranchingScheme::Insertion::operator==(
        const Insertion& insertion) const
{
    return ((item_type_id == insertion.item_type_id)
            && (rotation == insertion.rotation)
            && (new_bin == insertion.new_bin)
            && (x == insertion.x)
            && (y == insertion.y)
            && (z == insertion.z)
            );
}

std::ostream& packingsolver::box::operator<<(
        std::ostream& os,
        const BranchingScheme::Insertion& insertion)
{
    os << "item_type_id " << insertion.item_type_id
        << " rotation " << insertion.rotation
        << " new_bin " << (int)insertion.new_bin
        << " x " << insertion.x
        << " y " << insertion.y
        << " z " << insertion.z
        ;
    return os;
}

std::ostream& packingsolver::box::operator<<(
        std::ostream& os,
        const BranchingScheme::Node& node)
{
    os << "node id " << node.id << std::endl
        << "- number_of_items " << node.number_of_items << " number_of_bins " << node.number_of_bins << std::endl
        << "- item_volume " << node.item_volume << " current_volume " << node.current_volume << std::endl
        << "- waste " << node.waste << " profit " << node.profit << std::endl
        << "- item_type_id " << node.item_type_id << " x " << node.x << " y " << node.y << " z " << node.z << std::endl;

    // item_number_of_copies
    os << "- item_number_of_copies" << std::flush;
    for (ItemPos j_pos: node.item_number_of_copies)
        os << " " << j_pos;
    os << std::endl;

    os << "- uncovered_items" << std::endl;
    for (const BranchingScheme::UncoveredItem& uncovered_item: node.uncovered_items)
        os << "  - " << uncovered_item << std::endl;

    return os;
}
