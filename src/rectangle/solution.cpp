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

void Solution::update_indicators(
        BinPos bin_pos)
{
    SolutionBin& bin = bins_[bin_pos];
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);

    bin.weight = std::vector<Weight>(this->instance().number_of_groups(), 0);
    bin.weight_weighted_sum = std::vector<Weight>(this->instance().number_of_groups(), 0);
    this->bin_copies_[bin.bin_type_id] += bin.copies;
    this->number_of_bins_ += bin.copies;
    this->bin_cost_ += bin.copies * bin_type.cost;
    this->bin_weight_ += bin.copies * bin_type.maximum_weight;
    this->bin_area_ += bin.copies * bin_type.area();
    this->x_max_ = 0;
    this->y_max_ = 0;

    for (const SolutionItem& solution_item: bin.items) {
        const ItemType& item_type = instance().item_type(solution_item.item_type_id);

        Length xe = solution_item.bl_corner.x + item_type.x(solution_item.rotate);
        Length ye = solution_item.bl_corner.y + item_type.y(solution_item.rotate);

        this->item_area_ += bin.copies * item_type.area();
        this->item_weight_ += bin.copies * item_type.weight;
        this->item_profit_ += bin.copies * item_type.profit;
        this->number_of_items_ += bin.copies;
        this->item_copies_[solution_item.item_type_id] += bin.copies;

        this->middle_axle_overweight_ = 0;
        this->rear_axle_overweight_ = 0;
        for (GroupId group_id = 0; group_id <= item_type.group_id; ++group_id) {
            bin.weight[group_id] += item_type.weight;
            bin.weight_weighted_sum[group_id]
                += ((double)solution_item.bl_corner.x + (double)(xe - solution_item.bl_corner.x) / 2) * item_type.weight;
            std::pair<double, double> axle_weights = bin_type.semi_trailer_truck_data.compute_axle_weights(
                    bin.weight_weighted_sum[group_id], bin.weight[group_id]);
            // Update axle overweight.
            if (axle_weights.first > bin_type.semi_trailer_truck_data.middle_axle_maximum_weight * PSTOL)
                this->middle_axle_overweight_ += axle_weights.first - bin_type.semi_trailer_truck_data.middle_axle_maximum_weight;
            if (axle_weights.second > bin_type.semi_trailer_truck_data.rear_axle_maximum_weight * PSTOL)
                this->rear_axle_overweight_ += axle_weights.second - bin_type.semi_trailer_truck_data.rear_axle_maximum_weight;
        }

        if (bin_pos == (BinPos)this->bins_.size() - 1) {
            if (this->x_max_ < xe)
                this->x_max_ = xe;
            if (this->y_max_ < ye)
                this->y_max_ = ye;
            this->area_ = this->bin_area_ - bin_type.area() + (this->x_max_ * this->y_max_);
            this->leftover_value_ = this->bin_area_ - this->area_;
        }
    }
}

void Solution::append(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    const SolutionBin& bin_old = solution.bin(bin_pos);
    BinTypeId bin_type_id = (bin_type_ids.empty())?
        bin_old.bin_type_id:
        bin_type_ids[bin_old.bin_type_id];
    SolutionBin bin;
    bin.bin_type_id = bin_type_id;
    bin.copies = copies;
    for (SolutionItem solution_item: bin_old.items) {
        solution_item.item_type_id = (item_type_ids.empty())?
            solution_item.item_type_id:
            item_type_ids[solution_item.item_type_id];
        bin.items.push_back(solution_item);
    }
    bins_.push_back(bin);
    update_indicators(bins_.size() - 1);
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
        ss << FUNC_SIGNATURE << ": "
            << "does not support objective \"" << instance().objective() << "\".";
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
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + certificate_path + "\".");
    }

    file << "TYPE,ID,COPIES,BIN,X,Y,LX,LY" << std::endl;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
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
            << instance().bin_type(bin_type_id).rect.x << ","
            << instance().bin_type(bin_type_id).rect.y << std::endl;


        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const Defect& defect = bin_type.defects[defect_id];
            file
                << "DEFECT,"
                << defect_id << ","
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
            << std::setw(12) << "Type"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Weight"
            << std::setw(12) << "# items"
            << std::endl
            << std::setw(12) << "---"
            << std::setw(12) << "----"
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
                << std::setw(12) << bin(bin_pos).weight[0]
                << std::setw(12) << bin(bin_pos).items.size()
                << std::endl;
        }
    }

    if (verbosity_level >= 3) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Item"
            << std::setw(12) << "Rotate"
            << std::setw(12) << "X"
            << std::setw(12) << "Y"
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

    if (verbosity_level >= 3) {
        os
            << std::right << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "X"
            << std::setw(12) << "Y"
            << std::setw(12) << "Weight"
            << std::setw(12) << "Copies"
            << std::endl
            << std::setw(12) << "---------"
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
                << std::setw(12) << item_type.rect.x
                << std::setw(12) << item_type.rect.y
                << std::setw(12) << item_type.weight
                << std::setw(12) << item_copies(item_type_id)
                << std::endl;
        }
    }
}
