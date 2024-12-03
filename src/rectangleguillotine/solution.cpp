#include "packingsolver/rectangleguillotine/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <fstream>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////// Node /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& print(
        std::ostream& os,
        const std::vector<SolutionNode>& res,
        SolutionNodeId id,
        std::string tab)
{
    os << tab << res[id] << std::endl;
    for (SolutionNodeId c: res[id].children)
        print(os, res, c, tab + "  ");
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const SolutionNode& node)
{
    os
        << " f " << node.f
        << " d " << node.d
        << " l " << node.l
        << " r " << node.r
        << " b " << node.b
        << " t " << node.t
        << " item_type_id " << node.item_type_id;
    return os;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Solution ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void Solution::update_indicators(
        BinPos bin_pos)
{
    const SolutionBin& bin = bins_[bin_pos];

    number_of_bins_ += bin.copies;
    bin_copies_[bin.bin_type_id] += bin.copies;
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);
    cost_ += bin.copies * bin_type.cost;
    full_area_ += bin.copies * bin_type.area();
    area_ = full_area_;

    width_ = 0;
    height_ = 0;
    for (const SolutionNode& node: bin.nodes) {
        if (node.item_type_id >= 0) {
            number_of_items_ += bin.copies;
            item_area_ += bin.copies * instance().item_type(node.item_type_id).area();
            profit_ += bin.copies * instance().item_type(node.item_type_id).profit;
            item_copies_[node.item_type_id] += bin.copies;
        }

        // Subtract residual area.
        if (node.item_type_id == -3)
            area_ -= (node.t - node.b) * (node.r - node.l);

        // Update width_ and height_.
        if (node.r < bin_type.rect.w && width_ < node.r)
            width_ = node.r;
        if (node.t < bin_type.rect.h && height_ < node.t)
            height_ = node.t;
    }
}

void Solution::append(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    if (number_of_different_bins() > 0) {
        SolutionNode& node = bins_.back().nodes.back();
        if (node.item_type_id == -3) {
            node.item_type_id = -1;
            area_ -= (node.t - node.b) * (node.r - node.l);
        }
    }
    const SolutionBin& bin_old = solution.bin(bin_pos);
    BinTypeId bin_type_id = (bin_type_ids.empty())?
        bin_old.bin_type_id:
        bin_type_ids[bin_old.bin_type_id];
    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    bin.first_cut_orientation = bin_old.first_cut_orientation;
    for (SolutionNode node: bin_old.nodes) {
        if (node.item_type_id >= 0)
            node.item_type_id = (item_type_ids.empty())?
                node.item_type_id:
                item_type_ids[node.item_type_id];
        bin.nodes.push_back(node);
    }
    bins_.push_back(bin);
    update_indicators(bins_.size() - 1);
}

void Solution::append(
        const Solution& solution,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    for (BinPos bin_pos = 0; bin_pos < solution.number_of_bins(); ++bin_pos)
        append(solution, bin_pos, 1, bin_type_ids, item_type_ids);
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
        return strictly_greater(solution.profit(), profit());
    } case Objective::VariableSizedBinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return strictly_lesser(solution.cost(), cost());
    } default: {
        std::stringstream ss;
        ss << "Solution rectangleguillotine::Solution does not support objective \""
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
    std::ofstream file{certificate_path};
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + certificate_path + "\".");
    }

    file << "PLATE_ID,NODE_ID,X,Y,WIDTH,HEIGHT,TYPE,CUT,PARENT" << std::endl;
    SolutionNodeId offset = 0;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& solution_bin = bins_[bin_pos];
        for (BinPos copie = 0; copie < solution_bin.copies; ++copie) {
            for (SolutionNodeId node_id = 0;
                    node_id < (SolutionNodeId)solution_bin.nodes.size();
                    ++node_id) {
                const SolutionNode& n = solution_bin.nodes[node_id];
                file
                    << bin_pos << ","
                    << offset + node_id << ","
                    << n.l << ","
                    << n.b << ","
                    << n.r - n.l << ","
                    << n.t - n.b << ","
                    << n.item_type_id << ","
                    << n.d << ",";
                if (n.f != -1)
                    file << offset + n.f;
                file << std::endl;
            }
            offset += solution_bin.nodes.size();
        }
    }
}

nlohmann::json Solution::to_json() const
{
    return nlohmann::json {
        {"NumberOfItems", number_of_items()},
        {"ItemArea", item_area()},
        {"ItemProfit", profit()},
        {"NumberOfBins", number_of_bins()},
        //{"BinArea", bin_area()},
        {"BinCost", cost()},
        {"Waste", waste()},
        {"WastePercentage", waste_percentage()},
        {"FullWaste", full_waste()},
        {"FullWastePercentage", full_waste_percentage()},
        //{"AreaLoad", area_load()},
        //{"XMax", x_max()},
        //{"YMax", y_max()},
    };
}

void Solution::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Number of items:  " << optimizationtools::Ratio<ItemPos>(number_of_items(), instance().number_of_items()) << std::endl
            << "Item area:        " << optimizationtools::Ratio<Area>(item_area(), instance().item_area()) << std::endl
            << "Item profit:      " << optimizationtools::Ratio<Profit>(profit(), instance().item_profit()) << std::endl
            << "Number of bins:   " << optimizationtools::Ratio<BinPos>(number_of_bins(), instance().number_of_bins()) << std::endl
            //<< "Bin area:         " << optimizationtools::Ratio<BinPos>(bin_area(), instance().bin_area()) << std::endl
            << "Bin cost:         " << cost() << std::endl
            << "Waste:            " << waste() << std::endl
            << "Waste (%):        " << 100 * waste_percentage() << std::endl
            << "Full waste:       " << full_waste() << std::endl
            << "Full waste (%):   " << 100 * full_waste_percentage() << std::endl
            //<< "Area load:        " << area_load() << std::endl
            //<< "X max:            " << x_max() << std::endl
            //<< "Y max:            " << y_max() << std::endl
            ;
    }
    if (verbosity_level >= 2) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Node"
            << std::setw(12) << "Parent"
            << std::setw(12) << "Depth"
            << std::setw(12) << "Left"
            << std::setw(12) << "Right"
            << std::setw(12) << "Bottom"
            << std::setw(12) << "Top"
            << std::setw(12) << "Item"
            << std::endl
            << std::setw(12) << "---"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "-----"
            << std::setw(12) << "----"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "---"
            << std::setw(12) << "----"
            << std::endl;
        for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
            const SolutionBin& solution_bin = bins_[bin_pos];
            for (SolutionNodeId node_id = 0;
                    node_id < (SolutionNodeId)solution_bin.nodes.size();
                    ++node_id) {
                const SolutionNode& node = solution_bin.nodes[node_id];
                //const BinType& bin_type = instance().bin_type(bin(bin_pos).i);
                os
                    << std::setw(12) << bin_pos
                    << std::setw(12) << node_id
                    << std::setw(12) << node.f
                    << std::setw(12) << node.d
                    << std::setw(12) << node.l
                    << std::setw(12) << node.r
                    << std::setw(12) << node.b
                    << std::setw(12) << node.t
                    << std::setw(12) << node.item_type_id
                    << std::endl;
            }
        }
    }
}
