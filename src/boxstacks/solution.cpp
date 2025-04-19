#include "packingsolver/boxstacks/solution.hpp"

#include "packingsolver/rectangle/instance.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::boxstacks;

std::ostream& packingsolver::boxstacks::operator<<(
        std::ostream& os,
        const SolutionItem& item)
{
    os
        << " item_type_id " << item.item_type_id
        << " z_start " << item.z_start
        << " rotation " << item.rotation;
    return os;
}

BinPos Solution::add_bin(
        BinTypeId bin_type_id,
        BinPos copies)
{
    const BinType& bin_type = instance().bin_type(bin_type_id);

    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    bin.weight = std::vector<Weight>(instance().number_of_groups(), 0);
    bin.weight_weighted_sum = std::vector<Weight>(instance().number_of_groups(), 0);
    bins_.push_back(bin);

    bin_copies_[bin_type_id] += copies;
    bin_volume_ += copies * bin_type.volume();
    bin_area_ += copies * bin_type.area();
    bin_weight_ += copies * bin_type.maximum_weight;
    bin_cost_ += copies * bin_type.cost;
    number_of_bins_ += copies;
    x_max_ = 0;
    y_max_ = 0;

    return bins_.size() - 1;
}

StackId Solution::add_stack(
        BinPos bin_pos,
        Length x_start,
        Length x_end,
        Length y_start,
        Length y_end)
{
    if (bin_pos >= number_of_bins()) {
        throw std::runtime_error(
                "packingsolver::boxstacks::Solution::add_stack: "
                "bin_pos: " + std::to_string(bin_pos) + "; "
                "number_of_bins(): " + std::to_string(number_of_bins()) + ".");
    }
    SolutionBin& bin = bins_[bin_pos];

    SolutionStack stack;
    stack.x_start = x_start;
    stack.x_end = x_end;
    stack.y_start = y_start;
    stack.y_end = y_end;
    stack.weight = std::vector<Weight>(instance().number_of_groups(), 0.0);
    stack.weight_weighted_sum = std::vector<Weight>(instance().number_of_groups(), 0.0);
    bin.stacks.push_back(stack);
    if (bin_pos == (BinPos)bins_.size() - 1) {
        if (x_max_ < x_end)
            x_max_ = x_end;
        if (y_max_ < y_end)
            y_max_ = y_end;
        Length xi = instance().bin_type(bin.bin_type_id).box.x;
        Length yi = instance().bin_type(bin.bin_type_id).box.y;
        Length zi = instance().bin_type(bin.bin_type_id).box.z;
        volume_ = bin_volume_ - zi * std::max((xi - x_max_) * yi, (yi - y_max_ * xi));
    }

    number_of_stacks_ += bin.copies;
    stack_area_ += bin.copies * (x_end - x_start) * (y_end - y_start);

    return bin.stacks.size() - 1;
}

void Solution::add_item(
        BinPos bin_pos,
        StackId stack_id,
        ItemTypeId item_type_id,
        int rotation)
{
    if (bin_pos >= number_of_bins()) {
        throw std::runtime_error(
                "packingsolver::boxstacks::Solution::add_item: "
                "bin_pos: " + std::to_string(bin_pos) + "; "
                "number_of_bins(): " + std::to_string(number_of_bins()) + ".");
    }
    if (item_type_id < 0 || item_type_id >= instance().number_of_item_types()) {
        throw std::runtime_error(
                "packingsolver::boxstacks::Solution::add_item: "
                "item_type_id: " + std::to_string(item_type_id)
                + " / " + std::to_string(instance().number_of_item_types()) + ".");
    }
    SolutionBin& bin = bins_[bin_pos];

    SolutionStack& stack = bin.stacks[stack_id];
    const ItemType& item_type = instance().item_type(item_type_id);
    Length xj = item_type.x(rotation);
    Length yj = item_type.y(rotation);
    Length zj = item_type.z(rotation);
    //std::cout
    //    << "j " << j
    //    << " x " << stack.x_start
    //    << " y " << stack.y_start
    //    << " z " << stack.z_end
    //    << " xj " << xj
    //    << " yj " << yj
    //    << " zj " << zj
    //    << std::endl;

    if (!item_type.can_rotate(rotation)) {
        throw std::runtime_error(
                "packingsolver::boxstacks::Solution::add_item: "
                "forbidden rotation; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "item_type.rotations: " + std::to_string(item_type.rotations) + "; "
                "rot: " + std::to_string(rotation) + "; "
                "xj: " + std::to_string(xj) + "; "
                "yj: " + std::to_string(yj) + "; "
                "stack.x_start: " + std::to_string(stack.x_start) + "; "
                "stack.x_end: " + std::to_string(stack.x_end) + "; "
                "stack.y_start: " + std::to_string(stack.y_start) + "; "
                "stack.y_end: " + std::to_string(stack.y_end) + ".");
    }
    if (xj != stack.x_end - stack.x_start) {
        throw std::runtime_error(
                "packingsolver::boxstacks::Solution::add_item; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "z: " + std::to_string(stack.z_end) + "; "
                "rot: " + std::to_string(rotation) + "; "
                "xj: " + std::to_string(xj) + "; "
                "yj: " + std::to_string(yj) + "; "
                "zj: " + std::to_string(zj) + "; "
                "stack.x_start: " + std::to_string(stack.x_start) + "; "
                "stack.x_end: " + std::to_string(stack.x_end) + "; "
                "stack.y_start: " + std::to_string(stack.y_start) + "; "
                "stack.y_end: " + std::to_string(stack.y_end) + ".");
    }
    if (yj != stack.y_end - stack.y_start) {
        throw std::runtime_error(
                "packingsolver::boxstacks::Solution::add_item: "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "rot: " + std::to_string(rotation) + "; "
                "xj: " + std::to_string(xj) + "; "
                "yj: " + std::to_string(yj) + "; "
                "zj: " + std::to_string(zj) + "; "
                "stack.x_start: " + std::to_string(stack.x_start) + "; "
                "stack.x_end: " + std::to_string(stack.x_end) + "; "
                "stack.y_start: " + std::to_string(stack.y_start) + "; "
                "stack.y_end: " + std::to_string(stack.y_end) + ".");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.z_start = stack.z_end;
    if (!stack.items.empty())
        item.z_start -= item_type.nesting_height;
    item.rotation = rotation;

    stack.z_end = item.z_start + zj;
    stack.items.push_back(item);
    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        stack.weight[group_id] += item_type.weight;
        stack.weight_weighted_sum[group_id]
            += ((double)stack.x_start + (double)(stack.x_end - stack.x_start) / 2) * item_type.weight;
    }
    stack.profit += item_type.profit;

    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        bin.weight[group_id] += item_type.weight;
        bin.weight_weighted_sum[group_id]
            += ((double)stack.x_start + (double)(stack.x_end - stack.x_start) / 2) * item_type.weight;
    }
    bin.profit += item_type.profit;

    number_of_items_ += bin.copies;
    item_copies_[item.item_type_id] += bin.copies;
    item_volume_ += bin.copies * (stack.z_end - item.z_start)
        * (stack.y_end - stack.y_start)
        * (stack.x_end - stack.x_start);
    item_weight_ += bin.copies * item_type.weight;
    item_profit_ += bin.copies * item_type.profit;
}

bool Solution::feasible_total_weight() const
{
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        const BinType& bin_type = instance().bin_type(bin.bin_type_id);
        Weight w = (bin.weight.size() == 0)? 0: bin.weight.front();
        if (w > bin_type.maximum_weight * PSTOL)
            return false;
    }
    return true;
}

bool Solution::feasible_axle_weights() const
{
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        const BinType& bin_type = instance().bin_type(bin.bin_type_id);
        for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
            if (!instance().check_weight_constraints(group_id))
                continue;
            std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                    bin.weight_weighted_sum[group_id], bin.weight[group_id]);
            if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL)
                return false;
            if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL)
                return false;
        }
    }
    return true;
}

bool Solution::feasible() const
{
    return feasible_total_weight() && feasible_axle_weights();
}

bool Solution::check_stack(
        BinTypeId bin_type_id,
        const std::vector<std::pair<ItemTypeId, int>>& item_type_ids) const
{
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Length z_end = 0;
    Weight weight = 0.0;
    ItemPos maximum_number_of_items = std::numeric_limits<ItemPos>::max();
    Weight remaining_weight = std::numeric_limits<Weight>::infinity();
    for (ItemPos item_pos = 0; item_pos < (ItemPos)item_type_ids.size(); ++item_pos) {
        const ItemType& item_type = instance().item_type(item_type_ids[item_pos].first);
        int rotation = item_type_ids[item_pos].second;

        Length xj = item_type.x(rotation);
        Length yj = item_type.y(rotation);
        Length zj = item_type.z(rotation);
        if (item_pos > 0)
            zj -=  - item_type.nesting_height;
        Length zi = bin_type.box.z;

        // Check bin z.
        z_end += zj;
        if (z_end > zi)
            return false;

        // Check maximum stack density.
        weight += item_type.weight;
        double stack_density = (double)(weight) / xj / yj;
        if (strictly_greater(stack_density, bin_type.maximum_stack_density))
            return false;

        // Check maximum stackability.
        maximum_number_of_items = std::min(
                maximum_number_of_items,
                item_type.maximum_stackability);
        if ((ItemPos)item_type_ids.size() > maximum_number_of_items)
            return false;

        // Check maximum weight above.
        if (strictly_greater(item_type.weight, remaining_weight))
            return false;
        remaining_weight = std::min(
                remaining_weight - item_type.weight,
                item_type.maximum_weight_above);
    }
    return true;
}

Weight Solution::compute_weight_constraints_violation() const
{
    Weight violation = 0.0;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        violation += compute_middle_axle_weight_constraints_violation(
                bin.bin_type_id, bin.weight, bin.weight_weighted_sum);
        violation += compute_rear_axle_weight_constraints_violation(
                bin.bin_type_id, bin.weight, bin.weight_weighted_sum);
    }
    return violation;
}

Weight Solution::compute_middle_axle_weight_constraints_violation() const
{
    Weight violation = 0.0;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        violation += compute_middle_axle_weight_constraints_violation(
                bin.bin_type_id, bin.weight, bin.weight_weighted_sum);
    }
    return violation;
}

Weight Solution::compute_rear_axle_weight_constraints_violation() const
{
    Weight violation = 0.0;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        violation += compute_rear_axle_weight_constraints_violation(
                bin.bin_type_id, bin.weight, bin.weight_weighted_sum);
    }
    return violation;
}

void Solution::append(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    BinTypeId bin_type_id = (bin_type_ids.empty())?
        solution.bins_[bin_pos].bin_type_id:
        bin_type_ids[solution.bins_[bin_pos].bin_type_id];
    const SolutionBin& bin = solution.bins_[bin_pos];
    BinPos i = add_bin(bin_type_id, copies);
    for (const SolutionStack& stack: bin.stacks) {
        StackId stack_id = add_stack(
                i, stack.x_start, stack.x_end, stack.y_start, stack.y_end);
        for (const SolutionItem& item: stack.items) {
            ItemTypeId item_type_id = (item_type_ids.empty())?
                item.item_type_id:
                item_type_ids[item.item_type_id];
            add_item(i, stack_id, item_type_id, item.rotation);
        }
    }
}

void Solution::append(
        const Solution& solution,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    for (BinPos i_pos = 0; i_pos < (BinPos)solution.bins_.size(); ++i_pos) {
        const SolutionBin& bin = solution.bins_[i_pos];
        append(solution, i_pos, bin.copies, bin_type_ids, item_type_ids);
    }
}

bool Solution::operator<(const Solution& solution) const
{
    switch (instance().objective()) {
    case Objective::Default: {
        if (solution.profit() < profit())
            return false;
        if (solution.profit() > profit())
            return true;
        return solution.waste() < waste();
    } case Objective::BinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.number_of_bins() < number_of_bins();
    } case Objective::BinPackingWithLeftovers: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.waste() < waste();
    } case Objective::OpenDimensionX: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.x_max() < x_max();
    } case Objective::OpenDimensionY: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.y_max() < y_max();
    } case Objective::Knapsack: {
        return solution.profit() > profit();
    } default: {
        std::stringstream ss;
        ss << "Solution \"boxstacks::Solution\" does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
}

Weight Solution::compute_middle_axle_weight_constraints_violation(
        BinTypeId bin_type_id,
        const std::vector<Weight>& bin_weight,
        const std::vector<Weight>& bin_weight_weighted_sum) const
{
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Weight violation = 0.0;

    // Axle weight constraints.
    for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
        if (!instance().check_weight_constraints(group_id))
            continue;
        std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                bin_weight_weighted_sum[group_id], bin_weight[group_id]);
        if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL)
            violation += (group_id + 1) * (axle_weights.first - bin_type.semi_trailer_truck_data.middle_axle_maximum_weight);
    }

    return violation;
}

Weight Solution::compute_rear_axle_weight_constraints_violation(
        BinTypeId bin_type_id,
        const std::vector<Weight>& bin_weight,
        const std::vector<Weight>& bin_weight_weighted_sum) const
{
    const BinType& bin_type = instance().bin_type(bin_type_id);
    Weight violation = 0.0;

    // Axle weight constraints.
    for (GroupId group_id = 0; group_id < instance().number_of_groups(); ++group_id) {
        if (!instance().check_weight_constraints(group_id))
            continue;
        std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                bin_weight_weighted_sum[group_id], bin_weight[group_id]);
        if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL)
            violation += (group_id + 1) * (axle_weights.second - bin_type.semi_trailer_truck_data.rear_axle_maximum_weight);
    }

    return violation;
}

void Solution::write(
        const std::string& certificate_path) const
{
    if (certificate_path.empty())
        return;
    std::ofstream file(certificate_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + certificate_path + "\".");
    }

    file << "TYPE,ID,COPIES,BIN,STACK,X,Y,Z,LX,LY,LZ" << std::endl;
    for (BinPos bin_pos = 0;
            bin_pos < number_of_different_bins();
            ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        BinTypeId bin_type_id = bin.bin_type_id;
        const BinType& bin_type = instance().bin_type(bin_type_id);
        file
            << "BIN,"
            << bin_type_id << ","
            << bin.copies << ","
            << bin_pos << ","
            << "-1,"
            << "0,"
            << "0,"
            << "0,"
            << instance().bin_type(bin_type_id).box.x << ","
            << instance().bin_type(bin_type_id).box.y << ","
            << instance().bin_type(bin_type_id).box.z << std::endl;

        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const rectangle::Defect& defect = bin_type.defects[defect_id];
            file
                << "DEFECT,"
                << defect_id << ","
                << bin.copies << ","
                << bin_pos << ","
                << "-1,"
                << defect.pos.x << ","
                << defect.pos.y << ","
                << "0,"
                << defect.rect.x << ","
                << defect.rect.y << ","
                << "0,"
                << defect.rect.x << std::endl;
        }

        for (StackId stack_id = 0; stack_id < (StackId)bin.stacks.size(); ++stack_id) {
            const SolutionStack& stack = bin.stacks[stack_id];
            file
                << "STACK,"
                << stack_id << ","
                << bin.copies << ","
                << bin_pos << ","
                << stack_id << ","
                << stack.x_start << ","
                << stack.y_start << ","
                << 0 << ","
                << stack.x_end - stack.x_start << ","
                << stack.y_end - stack.y_start << ","
                << stack.z_end << std::endl;

            for (const SolutionItem& item: stack.items) {
                const ItemType& item_type = instance().item_type(item.item_type_id);
                file
                    << "ITEM,"
                    << item.item_type_id << ","
                    << bin.copies << ","
                    << bin_pos << ","
                    << stack_id << ","
                    << stack.x_start << ","
                    << stack.y_start << ","
                    << item.z_start << ","
                    << item_type.x(item.rotation) << ","
                    << item_type.y(item.rotation) << ","
                    << item_type.z(item.rotation) << std::endl;
            }

        }
    }
}

nlohmann::json Solution::to_json() const
{
    return nlohmann::json {
        {"NumberOfItems", number_of_items()},
        {"NumberOfUnpackedItems", instance().number_of_items() - number_of_items()},
        {"ItemVolume", item_volume()},
        {"ItemWeight", item_weight()},
        {"ItemProfit", profit()},
        {"NumberOfStacks", number_of_stacks()},
        {"StackArea", stack_area()},
        {"NumberOfBins", number_of_bins()},
        {"BinVolume", bin_volume()},
        {"BinArea", bin_area()},
        {"BinWeight", bin_weight()},
        {"BinCost", cost()},
        {"Waste", waste()},
        {"WastePercentage", waste_percentage()},
        {"FullWaste", full_waste()},
        {"FullWastePercentage", full_waste_percentage()},
        {"VolumeLoad", volume_load()},
        {"AreaLoad", area_load()},
        {"WeightLoad", weight_load()},
        {"XMax", x_max()},
        {"YMax", y_max()},
    };
}

void Solution::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Number of items:   " << optimizationtools::Ratio<ItemPos>(number_of_items(), instance().number_of_items()) << std::endl
            << "Item volume:       " << optimizationtools::Ratio<Profit>(item_volume(), instance().item_volume()) << std::endl
            << "Item weight:       " << optimizationtools::Ratio<Profit>(item_weight(), instance().item_weight()) << std::endl
            << "Item profit:       " << optimizationtools::Ratio<Profit>(profit(), instance().item_profit()) << std::endl
            << "Number of stacks:  " << number_of_stacks() << std::endl
            << "Stack area:        " << stack_area() << std::endl
            << "Number of bins:    " << optimizationtools::Ratio<BinPos>(number_of_bins(), instance().number_of_bins()) << std::endl
            << "Bin volume:        " << optimizationtools::Ratio<Volume>(bin_volume(), instance().bin_volume()) << std::endl
            << "Bin area:          " << optimizationtools::Ratio<Area>(bin_area(), instance().bin_area()) << std::endl
            << "Bin weight:        " << optimizationtools::Ratio<Weight>(bin_weight(), instance().bin_weight()) << std::endl
            << "Bin cost:          " << cost() << std::endl
            << "Waste:             " << waste() << std::endl
            << "Waste (%):         " << 100 * waste_percentage() << std::endl
            << "Full waste:        " << full_waste() << std::endl
            << "Full waste (%):    " << 100 * full_waste_percentage() << std::endl
            << "Volume load:       " << volume_load() << std::endl
            << "Area load:         " << area_load() << std::endl
            << "Weight load:       " << weight_load() << std::endl
            << "X max:             " << x_max() << std::endl
            << "Y max:             " << y_max() << std::endl
            ;
    }
}
