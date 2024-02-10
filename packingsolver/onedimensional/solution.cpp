#include "packingsolver/onedimensional/solution.hpp"

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
    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    bins_.push_back(bin);

    bin_copies_[bin_type_id]++;
    cost_ += copies * instance().bin_type(bin_type_id).cost;
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

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.start = bin.end;
    if (!bin.items.empty())
        item.start -= item_type.nesting_length;

    bin.end = item.start + item_type.length;
    bin.weight += item_type.weight;
    bin.items.push_back(item);

    number_of_items_ += bin.copies;
    item_copies_[item.item_type_id] += bin.copies;
    item_length_ += bin.copies * item_type.length;
    profit_ += bin.copies * item_type.profit;
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
        return solution.profit() > profit();
    } case Objective::VariableSizedBinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.cost() < cost();
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
            << std::endl
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
