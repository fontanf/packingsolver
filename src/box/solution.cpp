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
        << " x " << item.x
        << " y " << item.y
        << " z " << item.z
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

void Solution::add_item(
        BinPos bin_pos,
        ItemTypeId item_type_id,
        Length x,
        Length y,
        Length z,
        int rotation)
{
    if (bin_pos >= number_of_bins()) {
        throw std::runtime_error(
                "packingsolver::box::Solution::add_item: "
                "bin_pos: " + std::to_string(bin_pos) + "; "
                "number_of_bins(): " + std::to_string(number_of_bins()) + ".");
    }
    if (item_type_id < 0 || item_type_id >= instance().number_of_item_types()) {
        throw std::runtime_error(
                "packingsolver::box::Solution::add_item: "
                "item_type_id: " + std::to_string(item_type_id)
                + " / " + std::to_string(instance().number_of_item_types()) + ".");
    }
    SolutionBin& bin = bins_[bin_pos];
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);

    const ItemType& item_type = instance().item_type(item_type_id);
    Length xj = item_type.x(rotation);
    Length yj = item_type.y(rotation);
    Length zj = item_type.z(rotation);
    Length xe = x + xj;
    Length ye = y + yj;
    Length ze = z + zj;
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
                "packingsolver::box::Solution::add_item: "
                "forbidden rotation; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "item_type.rotations: " + std::to_string(item_type.rotations) + "; "
                "rot: " + std::to_string(rotation) + "; "
                "xj: " + std::to_string(xj) + "; "
                "yj: " + std::to_string(yj) + ".");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.x = x;
    item.y = y;
    item.z = z;
    item.rotation = rotation;
    bin.items.push_back(item);

    bin.weight += item_type.weight;
    bin.profit += item_type.profit;

    number_of_items_ += bin.copies;
    item_copies_[item.item_type_id] += bin.copies;
    item_volume_ += bin.copies * item_type.box.volume();
    item_weight_ += bin.copies * item_type.weight;
    item_profit_ += bin.copies * item_type.profit;

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

bool Solution::feasible() const
{
    return true;
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
    for (const SolutionItem& item: bin.items) {
        ItemTypeId item_type_id = (item_type_ids.empty())?
            item.item_type_id:
            item_type_ids[item.item_type_id];
        add_item(i, item_type_id, item.x, item.y, item.z, item.rotation);
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

Solution::Solution(
        const Instance& instance,
        const std::string& certificate_path):
    Solution(instance)
{
    std::ifstream file(certificate_path);
    if (!file.good()) {
        throw std::runtime_error(
                "packingsolver::rectangle::Solution::Solution: "
                "unable to open file \"" + certificate_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(file, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(file, tmp)) {
        line = optimizationtools::split(tmp, ',');

        std::string type = "";
        ItemTypeId id = -1;
        BinPos copies = -1;
        BinPos bin_pos = -1;
        Length x = -1;
        Length y = -1;
        Length z = -1;
        Length lx = -1;
        Length ly = -1;
        Length lz = -1;
        int rotation = -1;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "TYPE") {
                type = line[i];
            } else if (labels[i] == "ID") {
                id = (ItemTypeId)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (Length)std::stol(line[i]);
            } else if (labels[i] == "BIN") {
                bin_pos = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "Y") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "Z") {
                z = (Length)std::stol(line[i]);
            } else if (labels[i] == "LX") {
                lx = (Length)std::stol(line[i]);
            } else if (labels[i] == "LY") {
                ly = (Length)std::stol(line[i]);
            } else if (labels[i] == "LZ") {
                lz = (Length)std::stol(line[i]);
            } else if (labels[i] == "ROTATION") {
                rotation = (int)std::stol(line[i]);
            }
        }

        if (type == "BIN") {
            add_bin(id, copies);
        } else if (type == "ITEM") {
            const ItemType& item_type = instance.item_type(id);
            add_item(
                    bin_pos,
                    id,
                    x, y, z,
                    rotation);
        }
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
        if (solution.number_of_bins() != number_of_bins())
            return solution.number_of_bins() < number_of_bins();
        return strictly_greater(solution.leftover_value(), leftover_value());
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
    } case Objective::OpenDimensionZ: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.z_max() < z_max();
    } case Objective::Knapsack: {
        return solution.profit() > profit();
    } default: {
        std::stringstream ss;
        ss << "Solution \"box::Solution\" does not support objective \""
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
                "Unable to open file \"" + certificate_path + "\".");
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
            file
                << "ITEM,"
                << item.item_type_id << ","
                << bin.copies << ","
                << bin_pos << ","
                << item.x << ","
                << item.y << ","
                << item.z << ","
                << item_type.x(item.rotation) << ","
                << item_type.y(item.rotation) << ","
                << item_type.z(item.rotation) << ","
                << item.rotation << std::endl;
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
