#include "packingsolver/box/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

std::ostream& packingsolver::box::operator<<(
        std::ostream& os,
        const SolutionItem& item)
{
    os
        << " item_type_id " << item.item_type_id
        << " x " << item.bl_corner.x
        << " y " << item.bl_corner.y
        << " z " << item.bl_corner.z
        << " rotation " << to_string(item.rotation);
    return os;
}

void Solution::update_indicators(
        BinPos bin_pos)
{
    SolutionBin& bin = bins_[bin_pos];
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);

    bin_copies_[bin.bin_type_id] += bin.copies;
    bin_volume_ += bin.copies * bin_type.volume();
    bin_area_ += bin.copies * bin_type.area();
    bin_weight_ += bin.copies * bin_type.maximum_weight;
    bin_cost_ += bin.copies * bin_type.cost;
    number_of_bins_ += bin.copies;
    bin.resource_consumption.assign(bin_type.number_of_resources(), 0.0);
    x_max_ = 0;
    y_max_ = 0;

    // Number of copies of each item type already placed in this bin, so
    // far, at the point each item below is processed - needed to look up
    // the correct entry of a per-copy resource consumption schedule.
    std::vector<ItemPos> item_type_copies_in_bin(instance().number_of_item_types(), 0);
    for (const SolutionItem& item: bin.items) {
        const ItemType& item_type = instance().item_type(item.item_type_id);
        Box box = item_type.box.rotate(item.rotation);
        Length xe = item.bl_corner.x + box.x;
        Length ye = item.bl_corner.y + box.y;
        Length ze = item.bl_corner.z + box.z;

        bin.weight += item_type.weight;
        bin.profit += item_type.profit;

        number_of_items_ += bin.copies;
        item_copies_[item.item_type_id] += bin.copies;
        item_volume_ += bin.copies * item_type.box.volume();
        item_weight_ += bin.copies * item_type.weight;
        item_profit_ += bin.copies * item_type.profit;

        // Update bin.resource_consumption.
        for (ResourceId resource_id: item_type.resource_ids[bin.bin_type_id]) {
            const Resource& resource = bin_type.resource(resource_id);
            double previous_consumption = bin.resource_consumption[resource_id];
            bin.resource_consumption[resource_id]
                += resource.item_consumption(
                        item.item_type_id,
                        item_type_copies_in_bin[item.item_type_id]);
            if (bin.resource_consumption[resource_id] > resource.capacity) {
                if (resource.penalize) {
                    // Charge the penalty only once per bin, the first time
                    // consumption crosses the capacity (it never decreases
                    // afterwards, so this check would otherwise keep firing
                    // for every subsequent item added to the same bin).
                    if (previous_consumption <= resource.capacity)
                        this->item_profit_ -= resource.penalty;
                } else {
                    this->resource_feasible_ = false;
                }
            }
        }
        ++item_type_copies_in_bin[item.item_type_id];

        if (bin_pos == (BinPos)bins_.size() - 1) {
            if (x_max_ < xe)
                x_max_ = xe;
            if (y_max_ < ye)
                y_max_ = ye;
            if (z_max_ < ze)
                z_max_ = ze;
            volume_ = bin_volume_ - bin_type.volume() + (x_max_ * y_max_ * z_max_);
            leftover_value_ = bin_volume_ - volume_;
        }
    }

    // Update number_of_infeasible_item_copies_min_ and item_copies_feasible_.
    this->number_of_infeasible_item_copies_min_ = 0;
    this->item_copies_feasible_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance().number_of_item_types();
            ++item_type_id) {
        if (item_copies_[item_type_id] < instance().item_type(item_type_id).copies_min)
            this->number_of_infeasible_item_copies_min_++;
        if (item_copies_[item_type_id] > instance().item_type(item_type_id).copies)
            this->item_copies_feasible_ = false;
    }

    // Feasibility callback.
    callback_feasible_ = instance().feasibility_callback()(*this);
    feasible_ = item_copies_feasible_
        && callback_feasible_
        && resource_feasible_
        && (number_of_infeasible_item_copies_min_ == 0);
}

void Solution::append_bin(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    const SolutionBin& bin_old = solution.bin(bin_pos);
    BinTypeId bin_type_id = (bin_type_ids.empty())?
        bin_old.bin_type_id:
        bin_type_ids[bin_old.bin_type_id];
    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    for (SolutionItem item: bin_old.items) {
        item.item_type_id = (item_type_ids.empty())?
            item.item_type_id:
            item_type_ids[item.item_type_id];
        bin.items.push_back(item);
    }
    bins_.push_back(bin);
    update_indicators(bins_.size() - 1);
}

void Solution::append_bins(
        const Solution& solution,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    for (BinPos i_pos = 0; i_pos < (BinPos)solution.bins_.size(); ++i_pos) {
        const SolutionBin& bin = solution.bins_[i_pos];
        append_bin(solution, i_pos, bin.copies, bin_type_ids, item_type_ids);
    }
}

void Solution::append_empty_bin(
        BinTypeId bin_type_id,
        BinPos copies)
{
    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    bins_.push_back(bin);
    update_indicators(bins_.size() - 1);
}

bool Solution::operator<(const Solution& solution) const
{
    // Check feasibility.
    if (!solution.feasible_)
        return false;
    if (!feasible_)
        return true;

    switch (instance().objective()) {
    case Objective::Default: {
        if (strictly_lesser_profit(solution.profit(), profit()))
            return false;
        if (strictly_greater_profit(solution.profit(), profit()))
            return true;
        return solution.waste() < waste();
    } case Objective::BinPacking: {
        return solution.number_of_bins() < number_of_bins();
    } case Objective::BinPackingWithLeftovers: {
        if (solution.number_of_bins() != number_of_bins())
            return solution.number_of_bins() < number_of_bins();
        return strictly_greater(solution.leftover_value(), leftover_value());
    } case Objective::OpenDimensionX: {
        return solution.x_max() < x_max();
    } case Objective::OpenDimensionY: {
        return solution.y_max() < y_max();
    } case Objective::OpenDimensionZ: {
        return solution.z_max() < z_max();
    } case Objective::Knapsack: {
        return strictly_greater_profit(solution.profit(), profit());
    } case Objective::Feasibility: {
        return strictly_greater_profit(solution.profit(), profit());
    } case Objective::VariableSizedBinPacking: {
        return strictly_lesser_cost(solution.cost(), cost());
    } default: {
        std::stringstream ss;
        ss << FUNC_SIGNATURE << ": "
            << "solution \"box::Solution\" does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
}

void Solution::write(
        const std::string& certificate_path) const
{
    if (certificate_path.empty())
        return;
    std::ofstream file(certificate_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + certificate_path + "\".");
    }

    file << "TYPE,ID,COPIES,BIN,X,Y,Z,LX,LY,LZ,ROTATION" << std::endl;
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
            << "0,"
            << "0,"
            << "0,"
            << instance().bin_type(bin_type_id).box.x << ","
            << instance().bin_type(bin_type_id).box.y << ","
            << instance().bin_type(bin_type_id).box.z << ","
            << std::endl;

        for (const SolutionItem& item: bin.items) {
            const ItemType& item_type = instance().item_type(item.item_type_id);
            Box box = item_type.box.rotate(item.rotation);
            file
                << "ITEM,"
                << item.item_type_id << ","
                << bin.copies << ","
                << bin_pos << ","
                << item.bl_corner.x << ","
                << item.bl_corner.y << ","
                << item.bl_corner.z << ","
                << box.x << ","
                << box.y << ","
                << box.z << ","
                << to_string(item.rotation) << std::endl;
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
        {"ZMax", z_max()},
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
            << "Z max:             " << z_max() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Type"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Profit"
            << std::setw(12) << "Weight"
            << std::setw(12) << "# items"
            << std::endl
            << std::setw(12) << "---"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "-------"
            << std::endl;
        for (BinPos bin_pos = 0;
                bin_pos < number_of_different_bins();
                ++bin_pos) {
            os
                << std::setw(12) << bin_pos
                << std::setw(12) << bin(bin_pos).bin_type_id
                << std::setw(12) << bin(bin_pos).copies
                << std::setw(12) << bin(bin_pos).profit
                << std::setw(12) << bin(bin_pos).weight
                << std::setw(12) << bin(bin_pos).items.size()
                << std::endl;
        }
    }

    if (verbosity_level >= 3) {
        os
            << std::right << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "X"
            << std::setw(12) << "Y"
            << std::setw(12) << "Z"
            << std::setw(12) << "Weight"
            << std::setw(12) << "Copies"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "-"
            << std::setw(12) << "-"
            << std::setw(12) << "-"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance().number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance().item_type(item_type_id);
            os
                << std::setw(12) << item_type_id
                << std::setw(12) << item_type.box.x
                << std::setw(12) << item_type.box.y
                << std::setw(12) << item_type.box.z
                << std::setw(12) << item_type.weight
                << std::setw(12) << item_copies(item_type_id)
                << std::endl;
        }
    }
}

bool packingsolver::box::operator==(
        const SolutionBin& solution_bin_1,
        const SolutionBin& solution_bin_2)
{
    return solution_bin_1.bin_type_id == solution_bin_2.bin_type_id
        && solution_bin_1.items == solution_bin_2.items;
}

bool packingsolver::box::operator!=(
        const SolutionBin& solution_bin_1,
        const SolutionBin& solution_bin_2)
{
    return !(solution_bin_1 == solution_bin_2);
}
