#include "packingsolver/onedimensional/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <fstream>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const SolutionItem& item)
{
    os
        << " item_type_id " << item.item_type_id
        << " start " << item.start;
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
    bin_cost_ += copies * bin_type.cost;
    bin_length_ += copies * bin_type.length;
    number_of_bins_ += copies;
    return bins_.size() - 1;
}

void Solution::add_item(
        BinPos bin_pos,
        ItemTypeId item_type_id)
{
    if (bin_pos >= number_of_bins()) {
        throw "";
    }
    SolutionBin& bin = bins_[bin_pos];

    const ItemType& item_type = instance().item_type(item_type_id);
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);

    SolutionItem item;
    item.item_type_id = item_type_id;

    item.start = bin.end;
    if (!bin.items.empty())
        item.start -= item_type.nesting_length;

    bin.end = item.start + item_type.length;
    if (bin.end > bin_type.length) {
        feasible_ = false;
    }

    // Update bin.weight.
    bin.weight += item_type.weight;
    if (bin.weight > bin_type.maximum_weight) {
        feasible_ = false;
    }

    bin.items.push_back(item);

    // Update bin.maximum_number_of_items and bin.maximum_number_of_items.
    if (bin.items.size() == 1) {
        bin.maximum_number_of_items = item_type.maximum_stackability;
        bin.remaiing_weight = item_type.maximum_weight_after;
    } else {
        bin.maximum_number_of_items = std::min(
                bin.maximum_number_of_items,
                item_type.maximum_stackability);
        bin.remaiing_weight = std::min(
                bin.remaiing_weight - item_type.weight,
                item_type.maximum_weight_after);
    }
    if (bin.items.size() > bin.maximum_number_of_items) {
        feasible_ = false;
    }
    if (bin.remaiing_weight < 0) {
        feasible_ = false;
    }

    number_of_items_ += bin.copies;
    item_copies_[item.item_type_id] += bin.copies;
    if (item_copies_[item.item_type_id] > item_type.copies) {
        throw std::runtime_error(
                "onedimensional::Solution::add_item"
                "; item_copies_[item.item_type_id]: " + std::to_string(item_copies_[item.item_type_id])
                + "; item_type.copies: " + std::to_string(item_type.copies));
    }
    item_length_ += bin.copies * item_type.length;
    item_profit_ += bin.copies * item_type.profit;

    // Update length_.
    if (bin_pos == (BinPos)bins_.size() - 1)
        length_ = bin_length_ - bin_type.length + bin.end;
}

void Solution::append(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    const SolutionBin& bin = solution.bins_[bin_pos];

    if (!bin_type_ids.empty()) {
        if (bin.bin_type_id >= (BinPos)bin_type_ids.size()) {
            throw std::runtime_error(
                    "onedimensional::Solution::append"
                    "; bin.bin_type_id: " + std::to_string(bin.bin_type_id)
                    + "; bin_type_ids.size(): " + std::to_string(bin_type_ids.size()));
        }
    }

    BinTypeId bin_type_id = (bin_type_ids.empty())?
        solution.bins_[bin_pos].bin_type_id:
        bin_type_ids[bin.bin_type_id];
    BinPos i = add_bin(bin_type_id, copies);
    for (const SolutionItem& item: bin.items) {
        ItemTypeId item_type_id = (item_type_ids.empty())?
            item.item_type_id:
            item_type_ids[item.item_type_id];
        add_item(i, item_type_id);
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
    std::ifstream f(certificate_path);
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + certificate_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    // read bin file
    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        std::string type;
        BinTypeId type_id = -1;
        BinPos copies = -1;
        BinPos bin_pos = -1;
        Length x = -1;
        Length lx = -1;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "TYPE") {
                type = line[i];
            } else if (labels[i] == "ID") {
                type_id = (BinTypeId)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "BIN") {
                bin_pos = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "X") {
                x = (Length)std::stod(line[i]);
            } else if (labels[i] == "LX") {
                lx = (Length)std::stod(line[i]);
            }
        }
        if (type == "") {
            throw std::runtime_error(
                    "Missing \"TYPE\" value in \"" + certificate_path + "\".");
        }
        if (type_id == -1) {
            throw std::runtime_error(
                    "Missing \"ID\" value in \"" + certificate_path + "\".");
        }
        if (copies == -1) {
            throw std::runtime_error(
                    "Missing \"COPIES\" value in \"" + certificate_path + "\".");
        }
        if (bin_pos == -1) {
            throw std::runtime_error(
                    "Missing \"BIN\" value in \"" + certificate_path + "\".");
        }
        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"X\" value in \"" + certificate_path + "\".");
        }
        if (lx == -1) {
            throw std::runtime_error(
                    "Missing \"LX\" value in \"" + certificate_path + "\".");
        }

        if (type == "BIN") {
            add_bin(
                    type_id,
                    copies);
        } else if (type == "ITEM") {
            add_item(
                    bin_pos,
                    type_id);
        } else {
            throw std::runtime_error(
                    "Wrong \"TYPE\" value in \"" + certificate_path + "\".");
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
        return solution.waste() < waste();
    } case Objective::Knapsack: {
        return strictly_greater(solution.profit(), profit());
    } case Objective::VariableSizedBinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return strictly_lesser(solution.cost(), cost());
    } default: {
        std::stringstream ss;
        ss << "Solution onedimensional::Solution does not support objective \""
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

    file << "TYPE,ID,COPIES,BIN,X,LX" << std::endl;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        BinTypeId bin_type_id = bin.bin_type_id;
        file
            << "BIN,"
            << bin_type_id << ","
            << bin.copies << ","
            << bin_pos << ","
            << "0,"
            << instance().bin_type(bin_type_id).length << std::endl;

        for (const SolutionItem& item: bin.items) {
            const ItemType& item_type = instance().item_type(item.item_type_id);
            file
                << "ITEM,"
                << item.item_type_id << ","
                << bin.copies << ","
                << bin_pos << ","
                << item.start << ","
                << item_type.length << std::endl;
        }

    }
}

nlohmann::json Solution::to_json() const
{
    return nlohmann::json {
        {"NumberOfItems", number_of_items()},
        {"ItemLength", item_length()},
        {"ItemProfit", profit()},
        {"NumberOfBins", number_of_bins()},
        {"BinLength", bin_length()},
        {"BinCost", cost()},
        {"Waste", waste()},
        {"WastePercentage", waste_percentage()},
        {"FullWaste", full_waste()},
        {"FullWastePercentage", full_waste_percentage()},
    };
}

void Solution::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Number of items:  " << optimizationtools::Ratio<ItemPos>(number_of_items(), instance().number_of_items()) << std::endl
            << "Item length:      " << optimizationtools::Ratio<Profit>(item_length(), instance().item_length()) << std::endl
            << "Item profit:      " << optimizationtools::Ratio<Profit>(profit(), instance().item_profit()) << std::endl
            << "Number of bins:   " << optimizationtools::Ratio<BinPos>(number_of_bins(), instance().number_of_bins()) << std::endl
            << "Bin length:       " << optimizationtools::Ratio<BinPos>(bin_length(), instance().bin_length()) << std::endl
            << "Bin cost:         " << cost() << std::endl
            << "Waste:            " << waste() << std::endl
            << "Waste (%):        " << 100 * waste_percentage() << std::endl
            << "Full waste:       " << full_waste() << std::endl
            << "Full waste (%):   " << 100 * full_waste_percentage() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Type"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Length"
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
        for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
            //const BinType& bin_type = instance().bin_type(bin(bin_pos).i);
            os
                << std::setw(12) << bin_pos
                << std::setw(12) << bin(bin_pos).bin_type_id
                << std::setw(12) << bin(bin_pos).copies
                << std::setw(12) << bin(bin_pos).end
                << std::setw(12) << bin(bin_pos).weight
                << std::setw(12) << bin(bin_pos).items.size()
                << std::endl;
        }
    }
}
