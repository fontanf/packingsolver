#include "packingsolver/rectangleguillotine/solution.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Node /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& print(std::ostream& os,
        const std::vector<SolutionNode>& res,
        SolutionNodeId id, std::string tab)
{
    os << tab << res[id] << std::endl;
    for (SolutionNodeId c: res[id].children)
        print(os, res, c, tab + "  ");
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        const SolutionNode& node)
{
    os
        << "id " << node.id
        << " f " << node.f
        << " d " << node.d
        << " l " << node.l
        << " r " << node.r
        << " b " << node.b
        << " t " << node.t
        << " j " << node.j;
    return os;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Solution ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinPos Solution::add_bin(
        BinTypeId bin_type_id,
        const std::vector<SolutionNode>& nodes)
{
    BinPos bin_pos = bins_.size();
    SolutionBin solution_bin;
    solution_bin.i = bin_type_id;
    solution_bin.copies = 1;
    bins_.push_back(solution_bin);

    number_of_bins_++;
    bin_copies_[bin_type_id]++;
    const BinType& bin_type = instance().bin_type(bin_type_id);
    cost_ += bin_type.cost;
    area_ += bin_type.area();
    full_area_ += bin_type.area();

    for (const SolutionNode& node: nodes)
        add_node(bin_pos, node);

    return bin_pos;
}

void Solution::add_node(
        BinPos bin_pos,
        const SolutionNode& node)
{
    BinTypeId bin_type_id = bins_[bin_pos].i;
    const BinType& bin_type = instance().bin_type(bin_type_id);

    bins_[bin_pos].nodes.push_back(node);
    if (node.d >= 0)
    if (node.j >= 0) {
        number_of_items_++;
        item_area_ += instance().item_type(node.j).area();
        profit_ += instance().item_type(node.j).profit;
        item_copies_[node.j]++;
    }
    if (node.j == -3) // Subtract residual area
        area_ -= (node.t - node.b) * (node.r - node.l);
    // Update width_ and height_
    if (node.r < bin_type.rect.w && width_ < node.r)
        width_ = node.r;
    if (node.t < bin_type.rect.h && height_ < node.t)
        height_ = node.t;
}

void Solution::append(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    BinTypeId bin_type_id = (bin_type_ids.empty())?
        solution.bins_[bin_pos].i:
        bin_type_ids[solution.bins_[bin_pos].i];
    for (BinPos copie = 0; copie < copies; ++copie) {
        BinPos i_pos = add_bin(bin_type_id, {});
        for (SolutionNode node: solution.bin(bin_pos).nodes) {
            if (node.j >= 0)
                node.j = (item_type_ids.empty())?
                    node.j:
                    item_type_ids[node.j];
            if (node.j == -3)
                node.j = -1;
            add_node(i_pos, node);
        }
    }
}

void Solution::append(
        const Solution& solution,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    for (BinPos i = 0; i < solution.number_of_bins(); ++i)
        append(solution, i, 1, bin_type_ids, item_type_ids);
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
        return solution.width() < width();
    } case Objective::OpenDimensionY: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.height() < height();
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
        ss << "Solution rectangleguillotine::Solution does not support objective \""
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

    std::streamsize precision = info.os().precision();
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
    } case Objective::OpenDimensionX: {
        info.add_to_json(sol_str, "X", width());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << width()
                << std::setw(32) << algorithm.str()
                << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        info.add_to_json(sol_str, "Y", height());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << height()
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
        ss << "Solution rectangleguillotine::Solution does not support objective \""
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
            << "rectangleguillotine" << std::endl
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
    } case Objective::OpenDimensionX: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(12) << "X"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "-----"
                << std::setw(32) << "-------"
                << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(12) << "Y"
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(12) << "-----"
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
        ss << "Solution rectangleguillotine::Solution does not support objective \""
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
            << "Profit:            " << profit() << std::endl
            << "Full:              " << full() << std::endl
            << "Waste:             " << waste() << std::endl;
        break;
    } case Objective::BinPacking: {
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWaste", full_waste());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.os()
            << "Number of bins:    " << number_of_bins() << std::endl
            << "Full waste (%):    " << 100 * full_waste_percentage() << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        info.add_to_json(sol_str, "Waste", waste());
        info.add_to_json(sol_str, "WastePercentage", 100 * waste_percentage());
        info.os()
            << "Waste:             " << waste() << std::endl
            << "Waste (%):         " << 100 * waste_percentage() << std::endl;
        break;
    } case Objective::OpenDimensionX: {
        info.add_to_json(sol_str, "X", width());
        info.os()
            << "X:                 " << width() << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        info.add_to_json(sol_str, "Y", height());
        info.os()
            << "Y:                 " << height() << std::endl;
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
        ss << "Solution rectangleguillotine::Solution does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
    info.add_to_json(sol_str, "Time", t);
    info.os() << "Time:              " << t << std::endl;

    info.write_json_output();
    write(info);
}

void Solution::write(Info& info) const
{
    if (info.output->certificate_path.empty())
        return;
    std::ofstream f{info.output->certificate_path};
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + info.output->certificate_path + "\".");
    }

    f << "PLATE_ID,NODE_ID,X,Y,WIDTH,HEIGHT,TYPE,CUT,PARENT" << std::endl;
    SolutionNodeId offset = 0;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = bins_[bin_pos];
        for (BinPos copie = 0; copie < solution_bin.copies; ++copie) {
            for (const SolutionNode& n: solution_bin.nodes) {
                f
                    << bin_pos << ","
                    << offset + n.id << ","
                    << n.l << ","
                    << n.b << ","
                    << n.r - n.l << ","
                    << n.t - n.b << ","
                    << n.j << ","
                    << n.d << ",";
                if (n.f != -1)
                    f << n.f;
                f << std::endl;
            }
            offset += solution_bin.nodes.size();
        }
    }
    f.close();
}
