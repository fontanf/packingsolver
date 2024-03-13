#include "packingsolver/rectangleguillotine/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <fstream>

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
        std::ostream& os,
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
        << " item_type_id " << node.item_type_id;
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
    solution_bin.bin_type_id = bin_type_id;
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
    BinTypeId bin_type_id = bins_[bin_pos].bin_type_id;
    const BinType& bin_type = instance().bin_type(bin_type_id);

    bins_[bin_pos].nodes.push_back(node);
    if (node.d >= 0)
    if (node.item_type_id >= 0) {
        number_of_items_++;
        item_area_ += instance().item_type(node.item_type_id).area();
        profit_ += instance().item_type(node.item_type_id).profit;
        item_copies_[node.item_type_id]++;
    }
    if (node.item_type_id == -3) // Subtract residual area
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
        solution.bins_[bin_pos].bin_type_id:
        bin_type_ids[solution.bins_[bin_pos].bin_type_id];
    for (BinPos copie = 0; copie < copies; ++copie) {
        BinPos i_pos = add_bin(bin_type_id, {});
        for (SolutionNode node: solution.bin(bin_pos).nodes) {
            if (node.item_type_id >= 0)
                node.item_type_id = (item_type_ids.empty())?
                    node.item_type_id:
                    item_type_ids[node.item_type_id];
            if (node.item_type_id == -3)
                node.item_type_id = -1;
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
            for (const SolutionNode& n: solution_bin.nodes) {
                file
                    << bin_pos << ","
                    << offset + n.id << ","
                    << n.l << ","
                    << n.b << ","
                    << n.r - n.l << ","
                    << n.t - n.b << ","
                    << n.item_type_id << ","
                    << n.d << ",";
                if (n.f != -1)
                    file << n.f;
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
            << "Item area:        " << optimizationtools::Ratio<Profit>(item_area(), instance().item_area()) << std::endl
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
}
