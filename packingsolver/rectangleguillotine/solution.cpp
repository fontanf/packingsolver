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

Solution::Solution(const Instance& instance, const std::vector<Solution::Node>& nodes):
    instance_(instance), nodes_(nodes)
{
    for (const Solution::Node& node: nodes_) {
        if (bin_number_ < node.i + 1)
            bin_number_ = node.i + 1;
        if (node.d == 0) {
            assert(node.b == 0);
            assert(node.l == 0);
            area_      += node.r * node.t;
            full_area_ += node.r * node.t;
        }
        if (node.j >= 0) {
            item_number_++;
            item_area_ += instance.item(node.j).rect.area();
            profit_    += instance.item(node.j).profit;
        }
        if (node.j == -3) // Subtract residual area
            area_ -= (node.t - node.b) * (node.r - node.l);
        // Update width_ and height_
        if (node.r != instance.bin(node.i).rect.w && width_ < node.r)
            width_ = node.r;
        if (node.t != instance.bin(node.i).rect.h && height_ < node.t)
            height_ = node.t;
    }
}

Solution::Solution(const Solution& solution):
    instance_(solution.instance_),
    nodes_(solution.nodes_),
    item_number_(solution.item_number_),
    bin_number_(solution.bin_number_),
    area_(solution.area_),
    full_area_(solution.full_area_),
    item_area_(solution.item_area_),
    profit_(solution.profit_),
    width_(solution.width_),
    height_(solution.height_)
{
}

Solution& Solution::operator=(const Solution& solution)
{
    if (this != &solution) {
        assert(&instance_ == &solution.instance_);
        nodes_       = solution.nodes_;
        item_number_ = solution.item_number_;
        bin_number_  = solution.bin_number_;
        area_        = solution.area_;
        full_area_   = solution.full_area_;
        item_area_   = solution.item_area_;
        profit_      = solution.profit_;
        width_       = solution.width_;
        height_      = solution.height_;
    }
    return *this;
}

void Solution::update(const Solution& solution, const std::stringstream& algorithm, Info& info)
{
    info.output->mutex_sol.lock();

    if (operator<(solution)) {
        info.output->sol_number++;
        *this = solution;
        double t = info.elapsed_time();

        std::string sol_str = "Solution" + std::to_string(info.output->sol_number);
        VER(info, std::left << std::setw(6) << info.output->sol_number);
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
            PUT(info, sol_str, "BinNumber", bin_number());
            PUT(info, sol_str, "FullWastePercentage", 100 * full_waste_percentage());
            VER(info, std::left << std::setw(8) << bin_number());
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
            VER(info, std::left << std::setw(12) << profit());
            break;
        } default: {
            assert(false);
            std::cerr << "\033[31m" << "ERROR, branching scheme rectangle::BranchingScheme does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
        }
        }
        PUT(info, sol_str, "Time", t);
        VER(info, t << std::endl);

        if (!info.output->onlywriteattheend) {
            info.write_ini();
            write(info);
        }
    }

    info.output->mutex_sol.unlock();
}

void Solution::algorithm_start(Info& info)
{
    VER(info, std::left << std::setw(6) << "Sol");
    VER(info, std::left << std::setw(32) << "Branching scheme");
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
        VER(info, std::left << std::setw(12) << "Profit");
        break;
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, branching scheme rectangle::BranchingScheme does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
    }
    }
    VER(info, "Time" << std::endl);
}

void Solution::algorithm_end(Info& info)
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
        PUT(info, sol_str, "BinNumber", bin_number());
        PUT(info, sol_str, "FullWaste", full_waste());
        PUT(info, sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        VER(info, "Bin number: " << bin_number() << std::endl);
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
    } default: {
        assert(false);
        std::cerr << "\033[31m" << "ERROR, branching scheme rectangle::BranchingScheme does not implement objective \"" << instance().objective() << "\"" << "\033[0m" << std::endl;
    }
    }
    PUT(info, sol_str, "Time", t);
    VER(info, "Time: " << t << std::endl);

    info.write_ini();
    write(info);
}

void Solution::write(Info& info) const
{
    if (info.output->certfile.empty())
        return;
    std::ofstream f{info.output->certfile};
    if (!f.good()) {
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << info.output->certfile << "\"" << "\033[0m" << std::endl;
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

