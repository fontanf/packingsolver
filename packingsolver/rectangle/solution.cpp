#include "packingsolver/rectangle/solution.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangle;

std::ostream& packingsolver::rectangle::operator<<(
        std::ostream &os, const SolutionItem& item)
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

    bin_copies_[bin_type_id]++;
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
    SolutionBin& bin = bins_[bin_pos];

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.bl_corner = bl_corner;
    item.rotate = rotate;
    bin.items.push_back(item);

    Direction o = Direction::X;
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);
    const ItemType& item_type = instance().item_type(item_type_id);
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
        Length xi = instance().bin_type(bin.bin_type_id).rect.x;
        Length yi = instance().bin_type(bin.bin_type_id).rect.y;
        area_ = bin_area_ - std::max((xi - x_max_) * yi, (yi - y_max_ * xi));
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

inline double Solution::least_load() const
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
        return solution.waste() < waste();
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
        return solution.profit() > profit();
    } case Objective::VariableSizedBinPacking: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return solution.cost() < cost();
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        if (solution.profit() != profit())
            return solution.profit() > profit();
        return solution.middle_axle_overweight() + solution.rear_axle_overweight()
            < middle_axle_overweight() + rear_axle_overweight();
    } default: {
        std::stringstream ss;
        ss << "Solution rectangle::Solution does not support objective \""
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
        info.add_to_json(sol_str, "AdjustedNumberOfBins", adjusted_number_of_bins());
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
        info.add_to_json(sol_str, "X", x_max());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << x_max()
                << std::setw(32) << algorithm.str()
                << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        info.add_to_json(sol_str, "Y", y_max());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(12) << y_max()
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
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        info.add_to_json(sol_str, "Profit", profit());
        info.os()
                << std::setw(12) << std::fixed << std::setprecision(3) << t << std::defaultfloat << std::setprecision(precision)
                << std::setw(14) << profit()
                << std::setw(10) << number_of_items()
                << std::setw(10) << middle_axle_overweight()
                << std::setw(10) << rear_axle_overweight()
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
        write(info.output->certificate_path);
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
            << "rectangle" << std::endl
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
                << std::setw(12) << "-"
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
                << std::setw(12) << "-"
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
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        info.os()
                << std::setw(12) << "Time"
                << std::setw(14) << "Profit"
                << std::setw(10) << "# items"
                << std::setw(10) << "Middle ow."
                << std::setw(10) << "Rear ow."
                << std::setw(32) << "Comment"
                << std::endl
                << std::setw(12) << "----"
                << std::setw(14) << "------"
                << std::setw(10) << "-------"
                << std::setw(10) << "-------"
                << std::setw(10) << "-------"
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
            << "Profit:                   " << profit() << std::endl
            << "Full:                     " << full() << std::endl
            << "Waste:                    " << waste() << std::endl;
        break;
    } case Objective::BinPacking: {
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "AdjustedNumberOfBins", adjusted_number_of_bins());
        info.add_to_json(sol_str, "FullWaste", full_waste());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.os()
            << "Number of bins:           " << number_of_bins() << std::endl
            << "Adjusted Number of bins:  " << adjusted_number_of_bins() << std::endl
            << "Full waste (%):           " << 100 * full_waste_percentage() << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        info.add_to_json(sol_str, "Waste", waste());
        info.add_to_json(sol_str, "WastePercentage", 100 * waste_percentage());
        info.os()
            << "Waste:                    " << waste() << std::endl
            << "Waste (%):                " << 100 * waste_percentage() << std::endl;
        break;
    } case Objective::OpenDimensionX: {
        info.add_to_json(sol_str, "X", x_max());
        info.os()
            << "X:                        " << x_max() << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        info.add_to_json(sol_str, "Y", y_max());
        info.os()
            << "Y:                        " << y_max() << std::endl;
        break;
    } case Objective::Knapsack: {
        info.add_to_json(sol_str, "Profit", profit());
        info.add_to_json(sol_str, "NumberOfItems", number_of_items());
        info.add_to_json(sol_str, "Area", item_area());
        info.add_to_json(sol_str, "Weight", item_weight());
        info.add_to_json(sol_str, "WeightLoad", weight_load());
        info.add_to_json(sol_str, "AreaLoad", area_load());
        info.os()
            << "Profit:                   " << profit() << std::endl
            << "Number of items:          " << number_of_items() << " / " << instance().number_of_items() << " (" << (double)number_of_items() / instance().number_of_items() * 100 << "%)" << std::endl
            << "Item area:                " << item_area() << " / " << bin_area() << " (" << area_load() * 100 << "%)" << std::endl
            << "Item weight:              " << item_weight() << " / " << bin_weight() << " (" << weight_load() * 100 << "%)" << std::endl;
            ;
        break;
    } case Objective::VariableSizedBinPacking: {
        info.add_to_json(sol_str, "Cost", cost());
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.os()
            << "Cost:                     " << cost() << std::endl
            << "Number of bins:           " << number_of_bins() << std::endl
            << "Full waste (%):           " << 100 * full_waste_percentage() << std::endl;
        break;
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        info.add_to_json(sol_str, "Profit", profit());
        info.add_to_json(sol_str, "NumberOfItems", number_of_items());
        info.add_to_json(sol_str, "Area", item_area());
        info.add_to_json(sol_str, "Weight", item_weight());
        info.add_to_json(sol_str, "WeightLoad", weight_load());
        info.add_to_json(sol_str, "AreaLoad", area_load());
        info.add_to_json(sol_str, "MiddleAxleOverweight", middle_axle_overweight());
        info.add_to_json(sol_str, "RearAxleOverweight", rear_axle_overweight());
        info.os()
            << "Profit:                   " << profit() << std::endl
            << "Number of items:          " << number_of_items() << " / " << instance().number_of_items() << " (" << (double)number_of_items() / instance().number_of_items() * 100 << "%)" << std::endl
            << "Middle axle overweight:   " << middle_axle_overweight() << std::endl
            << "Rear axle overweight:     " << rear_axle_overweight() << std::endl
            << "Item area:                " << item_area() << " / " << bin_area() << " (" << area_load() * 100 << "%)" << std::endl
            << "Item weight:              " << item_weight() << " / " << bin_weight() << " (" << weight_load() * 100 << "%)" << std::endl;
            ;
        break;
    } default: {
        std::stringstream ss;
        ss << "Solution rectangleguillotine::Solution does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
    }
    }
    info.add_to_json(sol_str, "Time", t);
    info.os() << "Time:                     " << t << std::endl;

    info.write_json_output();
    write(info.output->certificate_path);
}

void Solution::write(std::string certificate_path) const
{
    if (certificate_path.empty())
        return;
    std::ofstream f(certificate_path);
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + certificate_path + "\".");
    }

    f << "TYPE,ID,COPIES,BIN,X,Y,LX,LY" << std::endl;
    for (BinPos i = 0; i < (BinPos)bins_.size(); ++i) {
        const SolutionBin& bin = bins_[i];
        BinTypeId bin_type_id = bin.bin_type_id;
        f
            << "BIN,"
            << bin_type_id << ","
            << bin.copies << ","
            << i << ","
            << "0,"
            << "0,"
            << instance().bin_type(bin_type_id).rect.x << ","
            << instance().bin_type(bin_type_id).rect.y << std::endl;

        for (const Defect& defect: instance().bin_type(bin_type_id).defects) {
            f
                << "DEFECT,"
                << defect.id << ","
                << bin.copies << ","
                << i << ","
                << defect.pos.x << ","
                << defect.pos.y << ","
                << defect.rect.x << ","
                << defect.rect.y << std::endl;
        }

        for (const SolutionItem& item: bin.items) {
            const ItemType& item_type = instance().item_type(item.item_type_id);
            f
                << "ITEM,"
                << item.item_type_id << ","
                << bin.copies << ","
                << i << ","
                << item.bl_corner.x << ","
                << item.bl_corner.y << ","
                << ((!item.rotate)? item_type.rect.x: item_type.rect.y) << ","
                << ((!item.rotate)? item_type.rect.y: item_type.rect.x)
                << std::endl;
        }
    }
    f.close();
}
