#include "packingsolver/onedimensional/solution.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream &os, const SolutionItem& item)
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

void Solution::display(
        const std::stringstream& algorithm,
        Info& info) const
{
    info.output->number_of_solutions++;
    double t = info.elapsed_time();

    std::streamsize precision = std::cout.precision();
    std::string sol_str = "Solution" + std::to_string(info.output->number_of_solutions);
    switch (instance().objective()) {
    case Objective::Default: {
        info.add_to_json(sol_str, "Profit", profit());
        info.add_to_json(sol_str, "Full", (full())? 1: 0);
        info.add_to_json(sol_str, "Waste", waste());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << profit()
                << std::setw(6) << full()
                << std::setw(12) << waste()
                << std::setw(32) << algorithm.str()
                << std::endl;
        break;
    } case Objective::BinPacking: {
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(8) << number_of_bins()
                << std::setw(16) << std::fixed << std::setprecision(2) << 100 * full_waste_percentage() << std::defaultfloat << std::setprecision(precision)
                << std::setw(32) << algorithm.str()
                << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        info.add_to_json(sol_str, "Waste", waste());
        info.add_to_json(sol_str, "WastePercentage", 100 * waste_percentage());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << waste()
                << std::setw(12) << std::fixed << std::setprecision(2) << 100 * waste_percentage() << std::defaultfloat << std::setprecision(precision)
                << std::setw(32) << algorithm.str()
                << std::endl;
        break;
    } case Objective::Knapsack: {
        info.add_to_json(sol_str, "Profit", profit());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(14) << profit()
                << std::setw(10) << number_of_items()
                << std::setw(32) << algorithm.str()
                << std::endl;
        break;
    } case Objective::VariableSizedBinPacking: {
        info.add_to_json(sol_str, "Cost", cost());
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(14) << cost()
                << std::setw(8) << number_of_bins()
                << std::setw(16) << std::fixed << std::setprecision(2) << 100 * full_waste_percentage() << std::defaultfloat << std::setprecision(precision)
                << std::setw(32) << algorithm.str()
                << std::endl;
        break;
    } default: {
        std::stringstream ss;
        ss << "Solution boxguillotine::Solution does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
    info.add_to_json(sol_str, "Algorithm", algorithm.str());
    info.add_to_json(sol_str, "Time", t);

    if (!info.output->only_write_at_the_end) {
        info.write_json_output();
        write(info);
    }
}

void Solution::algorithm_start(
        Info& info,
        Algorithm algorithm) const
{
    info.os()
            << "===================================" << std::endl
            << "           PackingSolver           " << std::endl
            << "===================================" << std::endl
            << std::endl
            << "Problem type" << std::endl
            << "------------" << std::endl
            << "onedimensional" << std::endl
            << std::endl
            << "Instance" << std::endl
            << "--------" << std::endl;
    instance().print(info.os(), info.verbosity_level());
    info.os()
            << std::endl
            << "Algorithm" << std::endl
            << "---------" << std::endl
            << algorithm << std::endl;
    info.os() << std::endl;

    switch (instance().objective()) {
    case Objective::Default: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(12) << "Profit"
                << std::setw(6) << "Full"
                << std::setw(12) << "Waste"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "------"
                << std::setw(6) << "----"
                << std::setw(12) << "-----"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::BinPacking: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(8) << "Bins"
                << std::setw(16) << "Full waste (%)"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(8) << "----"
                << std::setw(16) << "--------------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(12) << "Waste"
                << std::setw(12) << "Waste (%)"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "-----"
                << std::setw(12) << "---------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::Knapsack: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(14) << "Profit"
                << std::setw(10) << "# items"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(14) << "------"
                << std::setw(10) << "-------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::VariableSizedBinPacking: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(14) << "Cost"
                << std::setw(8) << "# bins"
                << std::setw(16) << "Full waste (%)"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(14) << "----"
                << std::setw(8) << "------"
                << std::setw(16) << "--------------"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } default: {
        std::stringstream ss;
        ss << "Solution boxguillotine::Solution does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
}

void Solution::algorithm_end(Info& info) const
{
    double t = info.elapsed_time();

    std::string sol_str = "Solution";
    info.os()
            << std::endl
            << "Final statistics" << std::endl
            << "----------------" << std::endl;
    switch (instance().objective()) {
    case Objective::Default: {
        info.add_to_json(sol_str, "Profit", profit());
        info.add_to_json(sol_str, "Full", (full())? 1: 0);
        info.add_to_json(sol_str, "Waste", waste());
        info.os()
                << "Profit:           " << profit() << std::endl
                << "Full:             " << full() << std::endl
                << "Waste:            " << waste() << std::endl;
        break;
    } case Objective::BinPacking: {
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWaste", full_waste());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.os() << "Number of bins:   " << number_of_bins() << std::endl;
        info.os() << "Full waste (%):   " << 100 * full_waste_percentage() << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        info.add_to_json(sol_str, "Waste", waste());
        info.add_to_json(sol_str, "WastePercentage", 100 * waste_percentage());
        info.os()
                << "Waste:             " << waste() << std::endl
                << "Waste (%):         " << 100 * waste_percentage() << std::endl;
        break;
    } case Objective::Knapsack: {
        info.add_to_json(sol_str, "Profit", profit());
        info.os()
                << "Profit:            " << profit() << std::endl
                << "Number of items:   " << number_of_items() << std::endl;
        break;
    } case Objective::VariableSizedBinPacking: {
        info.add_to_json(sol_str, "Cost", cost());
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.os()
                << "Cost:              " << cost() << std::endl
                << "Number of bins:    " << number_of_bins() << std::endl
                << "Full waste (%):    " << 100 * full_waste_percentage() << std::endl;
        break;
    } default: {
        std::stringstream ss;
        ss << "Solution boxguillotine::Solution does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
    info.add_to_json(sol_str, "Time", t);
    info.os() << "Time:              " << t << std::endl;

    if (info.verbosity_level() >= 2) {
        info.os()
            << std::endl
            << std::setw(12) << "BIN"
            << std::setw(12) << "TYPE"
            << std::setw(12) << "COPIES"
            << std::setw(12) << "LENGTH"
            << std::setw(12) << "WEIGHT"
            << std::setw(12) << "# ITEMS"
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
            const SolutionBin& solution_bin = bin(bin_pos);
            info.os()
                << std::setw(12) << bin_pos
                << std::setw(12) << solution_bin.bin_type_id
                << std::setw(12) << solution_bin.copies
                << std::setw(12) << solution_bin.end
                << std::setw(12) << solution_bin.weight
                << std::setw(12) << solution_bin.items.size()
                << std::endl;
        }
    }

    info.write_json_output();
    write(info);
}

void Solution::write(Info& info) const
{
    if (info.output->certificate_path.empty())
        return;
    std::ofstream f(info.output->certificate_path);
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + info.output->certificate_path + "\".");
    }

    f << "TYPE,ID,COPIES,BIN,X,LX" << std::endl;
    for (BinPos bin_pos = 0; bin_pos < (BinPos)bins_.size(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        BinTypeId bin_type_id = bin.bin_type_id;
        f
            << "BIN,"
            << bin_type_id << ","
            << bin.copies << ","
            << bin_pos << ","
            << "0,"
            << instance().bin_type(bin_type_id).length << std::endl;

        for (const SolutionItem& item: bin.items) {
            const ItemType& item_type = instance().item_type(item.item_type_id);
            f
                << "ITEM,"
                << item.item_type_id << ","
                << bin.copies << ","
                << bin_pos << ","
                << item.start << ","
                << item_type.length << std::endl;
        }

    }
    f.close();
}
