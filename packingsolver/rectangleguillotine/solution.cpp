#include "packingsolver/rectangleguillotine/solution.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

/************************************ Node ************************************/

std::ostream& print(std::ostream& os,
        const std::vector<Solution::Node>& res,
        SolutionNodeId id, std::string tab)
{
    os << tab << res[id] << std::endl;
    for (SolutionNodeId c: res[id].children)
        print(os, res, c, tab + "  ");
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(std::ostream &os, const Solution::Node& node)
{
    os
        << "id " << node.id
        << " f " << node.f
        << " d " << node.d
        << " i " << node.i
        << " l " << node.l
        << " r " << node.r
        << " b " << node.b
        << " t " << node.t
        << " j " << node.j
        << " rotate " << node.rotate;
    return os;
}

/********************************** Solution **********************************/

Solution::Solution(const Solution& solution):
    instance_(solution.instance_),
    nodes_(solution.nodes_),
    number_of_items_(solution.number_of_items_),
    number_of_bins_(solution.number_of_bins_),
    area_(solution.area_),
    full_area_(solution.full_area_),
    item_area_(solution.item_area_),
    profit_(solution.profit_),
    cost_(solution.cost_),
    width_(solution.width_),
    height_(solution.height_),
    bin_copies_(solution.bin_copies_),
    item_copies_(solution.item_copies_)
{
}

Solution& Solution::operator=(const Solution& solution)
{
    if (this != &solution) {
        assert(&instance_ == &solution.instance_);
        nodes_       = solution.nodes_;
        number_of_items_ = solution.number_of_items_;
        number_of_bins_  = solution.number_of_bins_;
        area_        = solution.area_;
        full_area_   = solution.full_area_;
        item_area_   = solution.item_area_;
        profit_      = solution.profit_;
        cost_        = solution.cost_;
        width_       = solution.width_;
        height_      = solution.height_;
        bin_copies_  = solution.bin_copies_;
        item_copies_ = solution.item_copies_;
        assert(number_of_items_ >= 0);
    }
    assert(&instance_ == &solution.instance_);
    return *this;
}

void Solution::add_node(const Node& node)
{
    nodes_.push_back(node);
    if (number_of_bins_ < node.i + 1) {
        number_of_bins_ = node.i + 1;
        bin_copies_[node.bin_type_id]++;
        cost_ += instance_.bin_type(node.bin_type_id).cost;
    }
    if (node.d == 0) {
        assert(node.b == 0);
        assert(node.l == 0);
        area_      += node.r * node.t;
        full_area_ += node.r * node.t;
    }
    if (node.j >= 0) {
        number_of_items_++;
        item_area_ += instance().item_type(node.j).rect.area();
        profit_    += instance().item_type(node.j).profit;
        item_copies_[node.j]++;
    }
    if (node.j == -3) // Subtract residual area
        area_ -= (node.t - node.b) * (node.r - node.l);
    // Update width_ and height_
    if (node.r != instance().bin(node.i).rect.w && width_ < node.r)
        width_ = node.r;
    if (node.t != instance().bin(node.i).rect.h && height_ < node.t)
        height_ = node.t;
}

Solution::Solution(const Instance& instance, const std::vector<Solution::Node>& nodes):
    instance_(instance),
    bin_copies_(instance.number_of_bin_types(), 0),
    item_copies_(instance.number_of_item_types(), 0)
{
    for (const Solution::Node& node: nodes)
        add_node(node);
}

void Solution::append(
        const Solution& solution,
        BinTypeId bin_type_id,
        const std::vector<ItemTypeId>& item_type_ids,
        BinPos copies)
{
    SolutionNodeId offset_node = nodes_.size();
    BinPos offset_bin = number_of_bins_;
    for (BinPos i = 0; i < copies; ++i) {
        for (Solution::Node node: solution.nodes()) {
            node.bin_type_id = bin_type_id;
            node.id += offset_node;
            if (node.f != -1)
                node.f += offset_node;
            if (node.j >= 0)
                node.j = item_type_ids[node.j];
            if (node.j == -3)
                node.j = -1;
            for (SolutionNodeId& child: node.children)
                child += offset_node;
            node.i += i + offset_bin;
            add_node(node);
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
    } case Objective::StripPackingWidth: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.width() < width();
    } case Objective::StripPackingHeight: {
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
        assert(false);
        std::cerr << "\033[31m" << "ERROR, rectangleguillotine::Solution does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
        return false;
    }
    }
}

void Solution::display(
        const std::stringstream& algorithm,
        Info& info) const
{
    info.output->number_of_solutions++;
    double t = info.elapsed_time();

    std::string sol_str = "Solution" + std::to_string(info.output->number_of_solutions);
    VER(info, std::left << std::setw(6) << info.output->number_of_solutions);
    PUT(info, sol_str, "Algorithm", algorithm.str());
    VER(info, std::left << std::setw(32) << algorithm.str());
    switch (instance().objective()) {
    case Objective::Default: {
        PUT(info, sol_str, "Profit", profit());
        PUT(info, sol_str, "Full", (full())? 1: 0);
        PUT(info, sol_str, "Waste", waste());
        VER(info, std::left << std::setw(12) << profit());
        VER(info, std::left << std::setw(6) << full());
        VER(info, std::left << std::setw(12) << waste());
        break;
    } case Objective::BinPacking: {
        PUT(info, sol_str, "NumberOfBins", number_of_bins());
        PUT(info, sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        VER(info, std::left << std::setw(8) << number_of_bins());
        VER(info, std::left << std::setw(16) << 100 * full_waste_percentage());
        break;
    } case Objective::BinPackingWithLeftovers: {
        PUT(info, sol_str, "Waste", waste());
        PUT(info, sol_str, "WastePercentage", 100 * waste_percentage());
        VER(info, std::left << std::setw(12) << waste());
        VER(info, std::left << std::setw(12) << 100 * waste_percentage());
        break;
    } case Objective::StripPackingWidth: {
        PUT(info, sol_str, "Width", width());
        VER(info, std::left << std::setw(12) << width());
        break;
    } case Objective::StripPackingHeight: {
        PUT(info, sol_str, "Height", height());
        VER(info, std::left << std::setw(12) << height());
        break;
    } case Objective::Knapsack: {
        PUT(info, sol_str, "Profit", profit());
        VER(info, std::left << std::setw(14) << profit());
        break;
    } case Objective::VariableSizedBinPacking: {
        PUT(info, sol_str, "Cost", cost());
        PUT(info, sol_str, "NumberOfBins", number_of_bins());
        PUT(info, sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        VER(info, std::left << std::setw(14) << cost());
        VER(info, std::left << std::setw(8) << number_of_bins());
        VER(info, std::left << std::setw(16) << 100 * full_waste_percentage());
        break;
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, rectangleguillotine::Solution does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
    }
    }
    PUT(info, sol_str, "Time", t);
    VER(info, t << std::endl);

    if (!info.output->only_write_at_the_end) {
        info.write_json_output();
        write(info);
    }
}

void Solution::algorithm_start(Info& info) const
{
    VER(info, std::left << std::setw(6) << "Sol");
    VER(info, std::left << std::setw(32) << "Comment");
    switch (instance().objective()) {
    case Objective::Default: {
        VER(info, std::left << std::setw(12) << "Profit");
        VER(info, std::left << std::setw(6) << "Full");
        VER(info, std::left << std::setw(12) << "Waste");
        break;
    } case Objective::BinPacking: {
        VER(info, std::left << std::setw(8) << "Bins");
        VER(info, std::left << std::setw(16) << "Full waste (%)");
        break;
    } case Objective::BinPackingWithLeftovers: {
        VER(info, std::left << std::setw(12) << "Waste");
        VER(info, std::left << std::setw(12) << "Waste (%)");
        break;
    } case Objective::StripPackingWidth: {
        VER(info, std::left << std::setw(12) << "Width");
        break;
    } case Objective::StripPackingHeight: {
        VER(info, std::left << std::setw(12) << "Height");
        break;
    } case Objective::Knapsack: {
        VER(info, std::left << std::setw(14) << "Profit");
        break;
    } case Objective::VariableSizedBinPacking: {
        VER(info, std::left << std::setw(14) << "Cost");
        VER(info, std::left << std::setw(8) << "Bins");
        VER(info, std::left << std::setw(16) << "Full waste (%)");
        break;
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, rectangleguillotine::Solution does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
    }
    }
    VER(info, "Time" << std::endl);
}

void Solution::algorithm_end(Info& info) const
{
    double t = info.elapsed_time();
    VER(info, "---" << std::endl);

    std::string sol_str = "Solution";
    switch (instance().objective()) {
    case Objective::Default: {
        PUT(info, sol_str, "Profit", profit());
        PUT(info, sol_str, "Full", (full())? 1: 0);
        PUT(info, sol_str, "Waste", waste());
        VER(info, "Profit: " << profit() << std::endl);
        VER(info, "Full: " << full() << std::endl);
        VER(info, "Waste: " << waste() << std::endl);
        break;
    } case Objective::BinPacking: {
        PUT(info, sol_str, "NumberOfBins", number_of_bins());
        PUT(info, sol_str, "FullWaste", full_waste());
        PUT(info, sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        VER(info, "Number of bins: " << number_of_bins() << std::endl);
        VER(info, "Full waste (%): " << 100 * full_waste_percentage() << std::endl);
        break;
    } case Objective::BinPackingWithLeftovers: {
        PUT(info, sol_str, "Waste", waste());
        PUT(info, sol_str, "WastePercentage", 100 * waste_percentage());
        VER(info, "Waste: " << waste() << std::endl);
        VER(info, "Waste (%): " << 100 * waste_percentage() << std::endl);
        break;
    } case Objective::StripPackingWidth: {
        PUT(info, sol_str, "Width", width());
        VER(info, "Width: " << width() << std::endl);
        break;
    } case Objective::StripPackingHeight: {
        PUT(info, sol_str, "Height", height());
        VER(info, "Height: " << height() << std::endl);
        break;
    } case Objective::Knapsack: {
        PUT(info, sol_str, "Profit", profit());
        VER(info, "Profit: " << profit() << std::endl);
        break;
    } case Objective::VariableSizedBinPacking: {
        PUT(info, sol_str, "Cost", cost());
        PUT(info, sol_str, "NumberOfBins", number_of_bins());
        PUT(info, sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        VER(info, "Cost: " << cost() << std::endl);
        VER(info, "Number of bins: " << number_of_bins() << std::endl);
        VER(info, "Full waste (%): " << 100 * full_waste_percentage() << std::endl);
        break;
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, rectangleguillotine::Solution does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
    }
    }
    PUT(info, sol_str, "Time", t);
    VER(info, "Time: " << t << std::endl);

    info.write_json_output();
    write(info);
}

void Solution::write(Info& info) const
{
    if (info.output->certificate_path.empty())
        return;
    std::ofstream f{info.output->certificate_path};
    if (!f.good()) {
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << info.output->certificate_path << "\"" << "\033[0m" << std::endl;
        return;
    }

    f << "PLATE_ID,NODE_ID,X,Y,WIDTH,HEIGHT,TYPE,CUT,PARENT" << std::endl;
    for (const Solution::Node& n: nodes_) {
        f
            << n.i << ","
            << n.id << ","
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
    f.close();
}
