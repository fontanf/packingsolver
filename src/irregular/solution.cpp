#include "packingsolver/irregular/solution.hpp"

#include "optimizationtools/utils/utils.hpp"

#include "nlohmann/json.hpp"

#include <fstream>
#include <iomanip>

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

    bin_copies_[bin_type_id] += copies;
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
    if (item_type_id < 0
            || item_type_id >= instance().number_of_item_types()) {
        throw std::invalid_argument(
                "irregular::Solution::add_item."
                " Item type id " + std::to_string(item_type_id)
                + " invalid.");
    }

    SolutionBin& bin = bins_[bin_pos];

    const ItemType& item_type = instance().item_type(item_type_id);

    // Check angle.
    bool angle_ok = false;
    for (auto angles: item_type.allowed_rotations)
        if (angles.first <= angle && angles.second <= angle)
            angle_ok = true;
    if (!angle_ok) {
        throw std::invalid_argument(
                "irregular::Solution::add_item."
                " Angle " + std::to_string(angle)
                + " is not allowed for item type "
                + std::to_string(item_type_id) + ".");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.bl_corner = bl_corner;
    item.angle = angle;
    bin.items.push_back(item);

    item_copies_[item_type_id]++;
    item_area_ += item_type.area;
    item_profit_ += item_type.profit;
    auto points = item_type.compute_min_max(angle);
    x_max_ = std::max(x_max_, bl_corner.x + points.second.x);
    y_max_ = std::max(y_max_, bl_corner.y + points.second.y);
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

    nlohmann ::json j;
    file >> j;

    for (const auto& json_bin: j["bins"]) {
        BinPos bin_pos = add_bin(json_bin["id"], json_bin["copies"]);
        for (const auto& json_item: json_bin["items"]) {
            add_item(
                    bin_pos,
                    json_item["id"],
                    {json_item["x"], json_item["y"]},
                    json_item["angle"]);
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

    nlohmann::json json;
    for (BinPos bin_pos = 0; bin_pos < number_of_different_bins(); ++bin_pos) {
        const SolutionBin& bin = bins_[bin_pos];
        const BinType& bin_type = instance().bin_type(bin.bin_type_id);
        json["bins"][bin_pos]["copies"] = bin.copies;
        json["bins"][bin_pos]["id"] = bin.bin_type_id;
        // Bin shape.
        for (Counter element_pos = 0;
                element_pos < (Counter)bin_type.shape.elements.size();
                ++element_pos) {
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
        for (DefectId defect_id = 0;
                defect_id < (DefectId)bin_type.defects.size();
                ++defect_id) {
            const Defect& defect = bin_type.defects[defect_id];
            for (Counter element_pos = 0;
                    element_pos < (Counter)defect.shape.elements.size();
                    ++element_pos) {
                const ShapeElement& element = defect.shape.elements[element_pos];
                json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["type"] = element2str(element.type);
                json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["xs"] = element.start.x;
                json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["ys"] = element.start.y;
                json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["xe"] = element.end.x;
                json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["ye"] = element.end.y;
                if (element.type == ShapeElementType::CircularArc) {
                    json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["xc"] = element.center.x;
                    json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["yc"] = element.center.y;
                    json["bins"][bin_pos]["defects"][defect_id]["shape"][element_pos]["anticlockwise"] = element.anticlockwise;
                }
                for (Counter hole_pos = 0;
                        hole_pos < (Counter)defect.holes.size();
                        ++hole_pos) {
                    const Shape& hole = defect.holes[hole_pos];
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
                        const ShapeElement& element = hole.elements[element_pos];
                        json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["type"] = element2str(element.type);
                        json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["xs"] = element.start.x;
                        json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["ys"] = element.start.y;
                        json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["xe"] = element.end.x;
                        json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["ye"] = element.end.y;
                        if (element.type == ShapeElementType::CircularArc) {
                            json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["xc"] = element.center.x;
                            json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["yc"] = element.center.y;
                            json["bins"][bin_pos]["defects"][defect_id]["holes"][hole_pos][element_pos]["anticlockwise"] = element.anticlockwise;
                        }
                    }
                }
            }
        }
        // Items.
        for (ItemPos item_pos = 0;
                item_pos < (ItemPos)bin.items.size();
                ++item_pos) {
            const SolutionItem& item = bin.items[item_pos];
            const ItemType& item_type = instance().item_type(item.item_type_id);
            json["bins"][bin_pos]["items"][item_pos]["id"] = item.item_type_id;
            json["bins"][bin_pos]["items"][item_pos]["x"] = item.bl_corner.x;
            json["bins"][bin_pos]["items"][item_pos]["y"] = item.bl_corner.y;
            json["bins"][bin_pos]["items"][item_pos]["angle"] = item.angle;
            for (Counter item_shape_pos = 0;
                    item_shape_pos < (Counter)item_type.shapes.size();
                    ++item_shape_pos) {
                const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                for (Counter element_pos = 0;
                        element_pos < (Counter)item_shape.shape.elements.size();
                        ++element_pos) {
                    const ShapeElement& element_orig = item_shape.shape.elements[element_pos];
                    ShapeElement element = element_orig.rotate(item.angle);
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
                for (Counter hole_pos = 0;
                        hole_pos < (Counter)item_shape.holes.size();
                        ++hole_pos) {
                    const Shape& hole = item_shape.holes[hole_pos];
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
                        const ShapeElement& element_orig = hole.elements[element_pos];
                        ShapeElement element = element_orig.rotate(item.angle);
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

    file << std::setw(4) << json << std::endl;
}

nlohmann::json Solution::to_json() const
{
    return nlohmann::json {
        {"NumberOfItems", number_of_items()},
        {"ItemArea", item_area()},
        {"ItemProfit", profit()},
        {"NumberOfBins", number_of_bins()},
        {"BinArea", bin_area()},
        {"BinCost", cost()},
        {"Waste", waste()},
        {"WastePercentage", waste_percentage()},
        {"FullWaste", full_waste()},
        {"FullWastePercentage", full_waste_percentage()},
        //{"AreaLoad", area_load()},
        {"XMax", x_max()},
        {"YMax", y_max()},
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
            << "Bin area:         " << optimizationtools::Ratio<BinPos>(bin_area(), instance().bin_area()) << std::endl
            << "Bin cost:         " << cost() << std::endl
            << "Waste:            " << waste() << std::endl
            << "Waste (%):        " << 100 * waste_percentage() << std::endl
            << "Full waste:       " << full_waste() << std::endl
            << "Full waste (%):   " << 100 * full_waste_percentage() << std::endl
            //<< "Area load:        " << area_load() << std::endl
            << "X max:            " << x_max() << std::endl
            << "Y max:            " << y_max() << std::endl
            ;
    }
}
