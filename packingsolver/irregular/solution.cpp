#include "packingsolver/irregular/solution.hpp"

using namespace packingsolver;
using namespace packingsolver::irregular;

std::ostream& packingsolver::irregular::operator<<(
        std::ostream &os, const SolutionItem& item)
{
    os
        << " item_type_id " << item.item_type_id
        << " l " << item.bl_corner.x
        << " b " << item.bl_corner.y
        << " angle " << item.angle
        ;
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
    bins_.push_back(bin);

    bin_copies_[bin_type_id]++;
    number_of_bins_ += copies;
    bin_cost_ += copies * bin_type.cost;
    bin_area_ += copies * bin_type.area;
    return bins_.size() - 1;
}

void Solution::add_item(
        BinPos bin_pos,
        ItemTypeId item_type_id,
        Point bl_corner,
        Angle angle)
{
    SolutionBin& bin = bins_[bin_pos];

    const ItemType& item_type = instance().item_type(item_type_id);
    SolutionItem item;
    item.item_type_id = item_type_id;
    item.bl_corner = bl_corner;
    item.angle = angle;
    bin.items.push_back(item);

    item_copies_[item_type_id]++;
    item_area_ += item_type.area;
    item_profit_ += item_type.profit;
    x_max_ = std::max(x_max_, bl_corner.x + item_type.x_max);
    y_max_ = std::max(y_max_, bl_corner.x + item_type.y_max);
    number_of_items_ += bin.copies;
    item_copies_[item.item_type_id] += bin.copies;
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
        add_item(i_pos, item_type_id, item.bl_corner, item.angle);
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
    } default: {
        std::stringstream ss;
        ss << "Solution irregular::Solution does not support objective \""
            << instance().objective() << "\"";
        throw std::logic_error(ss.str());
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
            << "irregular" << std::endl
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
                << "Profit:           " << profit() << std::endl
                << "Full:             " << full() << std::endl
                << "Waste:            " << waste() << std::endl;
        break;
    } case Objective::BinPacking: {
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWaste", full_waste());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.add_to_json(sol_str, "Value", number_of_bins());
        info.os() << "Number of bins:   " << number_of_bins() << std::endl;
        info.os() << "Full waste (%):   " << 100 * full_waste_percentage() << std::endl;
        break;
    } case Objective::BinPackingWithLeftovers: {
        info.add_to_json(sol_str, "Waste", waste());
        info.add_to_json(sol_str, "WastePercentage", 100 * waste_percentage());
        info.add_to_json(sol_str, "Value", waste());
        info.os()
                << "Waste:             " << waste() << std::endl
                << "Waste (%):         " << 100 * waste_percentage() << std::endl;
        break;
    } case Objective::OpenDimensionX: {
        info.add_to_json(sol_str, "NumberOfItems", number_of_items());
        info.add_to_json(sol_str, "X", x_max());
        info.add_to_json(sol_str, "Feasible", full());
        info.add_to_json(sol_str, "Value", x_max());
        info.os() << "Number of items:   " << number_of_items() << std::endl;
        info.os() << "X:                 " << x_max() << std::endl;
        break;
    } case Objective::OpenDimensionY: {
        info.add_to_json(sol_str, "NumberOfItems", number_of_items());
        info.add_to_json(sol_str, "Y", y_max());
        info.add_to_json(sol_str, "Value", y_max());
        info.add_to_json(sol_str, "Feasible", full());
        info.os() << "Number of items:   " << number_of_items() << std::endl;
        info.os() << "Y:                 " << y_max() << std::endl;
        break;
    } case Objective::Knapsack: {
        info.add_to_json(sol_str, "Profit", profit());
        info.add_to_json(sol_str, "Value", profit());
        info.os()
                << "Profit:            " << profit() << std::endl
                << "Number of items:   " << number_of_items() << std::endl;
        break;
    } case Objective::VariableSizedBinPacking: {
        info.add_to_json(sol_str, "Cost", cost());
        info.add_to_json(sol_str, "NumberOfBins", number_of_bins());
        info.add_to_json(sol_str, "FullWastePercentage", 100 * full_waste_percentage());
        info.add_to_json(sol_str, "Value", cost());
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

    nlohmann::json json;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        const BinType& bin_type = instance().bin_type(bin.bin_type_id);
        json["bins"][bin_pos]["copies"] = bin.copies;
        json["bins"][bin_pos]["id"] = bin.bin_type_id;
        // Bin shape.
        for (Counter element_pos = 0; element_pos < (Counter)bin_type.shape.elements.size(); ++element_pos) {
            const ShapeElement& element = bin_type.shape.elements[element_pos];
            json["bins"][bin_pos]["shape"][element_pos]["type"] = element2str(element.type);
            json["bins"][bin_pos]["shape"][element_pos]["xs"] = element.start.x;
            json["bins"][bin_pos]["shape"][element_pos]["ys"] = element.start.y;
            json["bins"][bin_pos]["shape"][element_pos]["xe"] = element.end.x;
            json["bins"][bin_pos]["shape"][element_pos]["ye"] = element.end.y;
            if (element.type == ShapeElementType::CircularArc) {
                json["bins"][bin_pos]["shape"][element_pos]["xc"] = element.center.x;
                json["bins"][bin_pos]["shape"][element_pos]["yc"] = element.center.y;
                json["bins"][bin_pos]["shape"][element_pos]["anticlockwise"] = element.anticlockwise;
            }
        }
        // Bin defects.
        for (DefectId k = 0; k < (DefectId)bin_type.defects.size(); ++k) {
            const Defect& defect = bin_type.defects[k];
            for (Counter element_pos = 0; element_pos < (Counter)defect.shape.elements.size(); ++element_pos) {
                const ShapeElement& element = defect.shape.elements[element_pos];
                json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["type"] = element2str(element.type);
                json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["xs"] = element.start.x;
                json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["ys"] = element.start.y;
                json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["xe"] = element.end.x;
                json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["ye"] = element.end.y;
                if (element.type == ShapeElementType::CircularArc) {
                    json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["xc"] = element.center.x;
                    json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["yc"] = element.center.y;
                    json["bins"][bin_pos]["defects"][k]["shape"][element_pos]["anticlockwise"] = element.anticlockwise;
                }
                for (Counter hole_pos = 0; hole_pos < (Counter)defect.holes.size(); ++hole_pos) {
                    const Shape& hole = defect.holes[hole_pos];
                    for (Counter element_pos = 0; element_pos < (Counter)hole.elements.size(); ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["type"] = element2str(element.type);
                        json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["xs"] = element.start.x;
                        json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["ys"] = element.start.y;
                        json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["xe"] = element.end.x;
                        json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["ye"] = element.end.y;
                        if (element.type == ShapeElementType::CircularArc) {
                            json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["xc"] = element.center.x;
                            json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["yc"] = element.center.y;
                            json["bins"][bin_pos]["defects"][k]["holes"][hole_pos][element_pos]["anticlockwise"] = element.anticlockwise;
                        }
                    }
                }
            }
        }
        // Items.
        for (ItemPos item_pos = 0; item_pos < (ItemPos)bin.items.size(); ++item_pos) {
            const SolutionItem& item = bin.items[item_pos];
            const ItemType& item_type = instance().item_type(item.item_type_id);
            json["bins"][bin_pos]["items"][item_pos]["id"] = item.item_type_id;
            for (Counter item_shape_pos = 0; item_shape_pos < (Counter)item_type.shapes.size(); ++item_shape_pos) {
                const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                for (Counter element_pos = 0; element_pos < (Counter)item_shape.shape.elements.size(); ++element_pos) {
                    const ShapeElement& element = item_shape.shape.elements[element_pos];
                    json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["type"] = element2str(element.type);
                    json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["xs"] = element.start.x + item.bl_corner.x;
                    json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["ys"] = element.start.y + item.bl_corner.y;
                    json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["xe"] = element.end.x + item.bl_corner.x;
                    json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["ye"] = element.end.y + item.bl_corner.y;
                    if (element.type == ShapeElementType::CircularArc) {
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["xc"] = element.center.x + item.bl_corner.x;
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["yc"] = element.center.y + item.bl_corner.y;
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["shape"][element_pos]["anticlockwise"] = element.anticlockwise;
                    }
                }
                for (Counter hole_pos = 0; hole_pos < (Counter)item_shape.holes.size(); ++hole_pos) {
                    const Shape& hole = item_shape.holes[hole_pos];
                    for (Counter element_pos = 0; element_pos < (Counter)hole.elements.size(); ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["type"] = element2str(element.type);
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["xs"] = element.start.x + item.bl_corner.x;
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["ys"] = element.start.y + item.bl_corner.y;
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["xe"] = element.end.x + item.bl_corner.x;
                        json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["ye"] = element.end.y + item.bl_corner.y;
                        if (element.type == ShapeElementType::CircularArc) {
                            json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["xc"] = element.center.x + item.bl_corner.x;
                            json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["yc"] = element.center.y + item.bl_corner.y;
                            json["bins"][bin_pos]["items"][item_pos]["item_shapes"][item_shape_pos]["holes"][hole_pos][element_pos]["anticlockwise"] = element.anticlockwise;
                        }
                    }
                }
            }
        }
    }

    f << std::setw(4) << json << std::endl;
}
