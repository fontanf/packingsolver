#include "packingsolver/rectangle/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include <fstream>

using namespace packingsolver;
using namespace packingsolver::rectangle;

std::ostream& packingsolver::rectangle::operator<<(
        std::ostream& os,
        const SolutionItem& item)
{
    os
        << " bl " << item.bl_corner
        << " item_type_id " << item.item_type_id
        << " rotate " << item.rotate;
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
    bin.weight = std::vector<Weight>(instance().number_of_groups(), 0);
    bin.weight_weighted_sum = std::vector<Weight>(instance().number_of_groups(), 0);
    bins_.push_back(bin);

    bin_copies_[bin_type_id] += copies;
    number_of_bins_ += copies;
    bin_cost_ += copies * bin_type.cost;
    bin_weight_ += copies * bin_type.maximum_weight;
    bin_area_ += copies * bin_type.area();
    x_max_ = 0;
    y_max_ = 0;

    return bins_.size() - 1;
}

void Solution::add_item(
        BinPos bin_pos,
        ItemTypeId item_type_id,
        Point bl_corner,
        bool rotate)
{
    if (bin_pos >= number_of_bins()) {
        throw "";
    }

    if (item_type_id < 0
            || item_type_id >= instance().number_of_item_types()) {
        throw std::invalid_argument(
                "rectangle::Solution::add_item."
                " Item type id " + std::to_string(item_type_id)
                + " invalid.");
    }

    SolutionBin& bin = bins_[bin_pos];

    const BinType& bin_type = instance().bin_type(bin.bin_type_id);
    const ItemType& item_type = instance().item_type(item_type_id);

    if (rotate && item_type.oriented) {
        throw std::invalid_argument(
                "rectangle::Solution::add_item."
                " Item type " + std::to_string(item_type_id)
                + " cannot be rotated.");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.bl_corner = bl_corner;
    item.rotate = rotate;
    bin.items.push_back(item);

    Direction o = Direction::X;
    Length xe = bl_corner.x + instance().x(item_type, rotate, o);
    Length ye = bl_corner.y + instance().y(item_type, rotate, o);

    item_area_ += bin.copies * item_type.area();
    item_weight_ += bin.copies * item_type.weight;
    item_profit_ += bin.copies * item_type.profit;
    number_of_items_ += bin.copies;
    item_copies_[item.item_type_id] += bin.copies;

    middle_axle_overweight_ = 0;
    rear_axle_overweight_ = 0;
    for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
        bin.weight[group_id] += item_type.weight;
        bin.weight_weighted_sum[group_id]
            += ((double)bl_corner.x + (double)(xe - bl_corner.x) / 2) * item_type.weight;
        std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                bin.weight_weighted_sum[group_id], bin.weight[group_id]);
        // Update axle overweight.
        if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL)
            middle_axle_overweight_ += axle_weights.first - bin_type.semi_trailer_truck_data.middle_axle_maximum_weight;
        if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL)
            rear_axle_overweight_ += axle_weights.second - bin_type.semi_trailer_truck_data.rear_axle_maximum_weight;
    }

    if (bin_pos == (BinPos)bins_.size() - 1) {
        if (x_max_ < xe)
            x_max_ = xe;
        if (y_max_ < ye)
            y_max_ = ye;
        area_ = bin_area_ - bin_type.area() + (x_max_ * y_max_);
        leftover_value_ = bin_area_ - area_;
    }
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
    BinPos i_pos = add_bin(bin_type_id, copies);
    for (const SolutionItem& item: solution.bins_[bin_pos].items) {
        ItemTypeId item_type_id = (item_type_ids.empty())?
            item.item_type_id:
            item_type_ids[item.item_type_id];
        add_item(i_pos, item_type_id, item.bl_corner, item.rotate);
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
                "Unable to open file \"" + certificate_path + "\".");
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
        Length lx = -1;
        Length ly = -1;

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
            } else if (labels[i] == "LX") {
                lx = (Length)std::stol(line[i]);
            } else if (labels[i] == "LY") {
                ly = (Length)std::stol(line[i]);
            }
        }

        if (type == "BIN") {
            add_bin(id, copies);
        } else if (type == "ITEM") {
            const ItemType& item_type = instance.item_type(id);
            add_item(
                    bin_pos,
                    id,
                    {x, y},
                    (lx != item_type.rect.x));
        }
    }
}

double Solution::least_load() const
{
    if (number_of_bins() == 0)
        return 0;
    double least_load = std::numeric_limits<double>::infinity();
    for (BinPos bin_pos = 0; bin_pos < (BinPos)bins_.size(); ++bin_pos) {
        double load = 0;
        for (const SolutionItem& item: bins_[bin_pos].items)
            load += instance().item_type(item.item_type_id).area();
        load /= instance().bin_type(bins_[bin_pos].bin_type_id).area();
        if (least_load > load)
            least_load = load;
    }
    return least_load;
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
    } case Objective::Knapsack: {
        return strictly_greater(solution.profit(), profit());
    } case Objective::VariableSizedBinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return strictly_lesser(solution.cost(), cost());
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        if (!equal(solution.profit(), profit()))
            return strictly_greater(solution.profit(), profit());
        if (solution.middle_axle_overweight() + solution.rear_axle_overweight()
                != middle_axle_overweight() + rear_axle_overweight())
            return solution.middle_axle_overweight() + solution.rear_axle_overweight()
                < middle_axle_overweight() + rear_axle_overweight();
        return solution.x_max() < x_max();
    } default: {
        std::stringstream ss;
        ss << "Solution rectangle::Solution does not support objective \""
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

    file << "TYPE,ID,COPIES,BIN,X,Y,LX,LY" << std::endl;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        BinTypeId bin_type_id = bin.bin_type_id;
        file
            << "BIN,"
            << bin_type_id << ","
            << bin.copies << ","
            << bin_pos << ","
            << "0,"
            << "0,"
            << instance().bin_type(bin_type_id).rect.x << ","
            << instance().bin_type(bin_type_id).rect.y << std::endl;

        for (const Defect& defect: instance().bin_type(bin_type_id).defects) {
            file
                << "DEFECT,"
                << defect.id << ","
                << bin.copies << ","
                << bin_pos << ","
                << defect.pos.x << ","
                << defect.pos.y << ","
                << defect.rect.x << ","
                << defect.rect.y << std::endl;
        }

        for (const SolutionItem& item: bin.items) {
            const ItemType& item_type = instance().item_type(item.item_type_id);
            file
                << "ITEM,"
                << item.item_type_id << ","
                << bin.copies << ","
                << bin_pos << ","
                << item.bl_corner.x << ","
                << item.bl_corner.y << ","
                << ((!item.rotate)? item_type.rect.x: item_type.rect.y) << ","
                << ((!item.rotate)? item_type.rect.y: item_type.rect.x)
                << std::endl;
        }
    }
}

nlohmann::json Solution::to_json() const
{
    return nlohmann::json {
        {"NumberOfItems", number_of_items()},
        {"ItemArea", item_area()},
        {"ItemWeight", item_weight()},
        {"ItemProfit", profit()},
        {"NumberOfBins", number_of_bins()},
        {"BinArea", bin_area()},
        {"BinWeight", bin_weight()},
        {"BinCost", cost()},
        {"Waste", waste()},
        {"WastePercentage", waste_percentage()},
        {"FullWaste", full_waste()},
        {"FullWastePercentage", full_waste_percentage()},
        {"AreaLoad", area_load()},
        {"WeightLoad", weight_load()},
        {"XMax", x_max()},
        {"YMax", y_max()},
        {"LeftoverValue", leftover_value()},
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
            << "Item weight:      " << optimizationtools::Ratio<Weight>(item_weight(), instance().item_weight()) << std::endl
            << "Item profit:      " << optimizationtools::Ratio<Profit>(profit(), instance().item_profit()) << std::endl
            << "Number of bins:   " << optimizationtools::Ratio<BinPos>(number_of_bins(), instance().number_of_bins()) << std::endl
            << "Bin area:         " << optimizationtools::Ratio<Area>(bin_area(), instance().bin_area()) << std::endl
            << "Bin weight:       " << optimizationtools::Ratio<Weight>(bin_weight(), instance().bin_weight()) << std::endl
            << "Bin cost:         " << cost() << std::endl
            << "Waste:            " << waste() << std::endl
            << "Waste (%):        " << 100 * waste_percentage() << std::endl
            << "Full waste:       " << full_waste() << std::endl
            << "Full waste (%):   " << 100 * full_waste_percentage() << std::endl
            << "Area load:        " << area_load() << std::endl
            << "Weight load:      " << weight_load() << std::endl
            << "X max:            " << x_max() << std::endl
            << "Y max:            " << y_max() << std::endl
            << "Leftover value:   " << leftover_value() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Item"
            << std::setw(12) << "Rotate"
            << std::setw(12) << "X"
            << std::setw(12) << "y"
            << std::setw(12) << "Width"
            << std::setw(12) << "Height"
            << std::setw(12) << "Weight"
            << std::endl
            << std::setw(12) << "---"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "-"
            << std::setw(12) << "-"
            << std::setw(12) << "-----"
            << std::setw(12) << "-------"
            << std::setw(12) << "-------"
            << std::endl;
        for (BinPos bin_pos = 0;
                bin_pos < number_of_different_bins();
                ++bin_pos) {
            const SolutionBin& solution_bin = bin(bin_pos);
            for (const SolutionItem& solution_item: solution_bin.items) {
                const ItemType& item_type = instance().item_type(solution_item.item_type_id);
                os
                    << std::setw(12) << bin_pos
                    << std::setw(12) << solution_item.item_type_id
                    << std::setw(12) << solution_item.rotate
                    << std::setw(12) << solution_item.bl_corner.x
                    << std::setw(12) << solution_item.bl_corner.y
                    << std::setw(12) << item_type.rect.x
                    << std::setw(12) << item_type.rect.y
                    << std::setw(12) << item_type.weight
                    << std::endl;
            }
        }
    }
}
