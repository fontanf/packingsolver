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
    bin_area_ += copies * bin_type.area_orig;
    auto mm = bin_type.shape_orig.compute_min_max();
    x_min_ = mm.second.x;
    y_min_ = mm.second.y;
    x_max_ = mm.first.x;
    y_max_ = mm.first.y;

    return bins_.size() - 1;
}

void Solution::add_item(
        BinPos bin_pos,
        ItemTypeId item_type_id,
        Point bl_corner,
        Angle angle,
        bool mirror)
{
    if (item_type_id < 0
            || item_type_id >= instance().number_of_item_types()) {
        throw std::invalid_argument(
                "packingsolver::irregular::Solution::add_item: "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + ".");
    }

    SolutionBin& bin = bins_[bin_pos];

    const BinType& bin_type = instance().bin_type(bin.bin_type_id);
    const ItemType& item_type = instance().item_type(item_type_id);

    // Check angle.
    bool angle_ok = false;
    for (auto angles: item_type.allowed_rotations)
        if (angles.first <= angle && angles.second <= angle)
            angle_ok = true;
    if (!angle_ok) {
        throw std::invalid_argument(
                "packingsolver::irregular::Solution::add_item: "
                "invalid 'angle'; "
                "angle: " + std::to_string(angle) + "; "
                "item_type_id: " + std::to_string(item_type_id) + ".");
    }

    if (mirror && !item_type.allow_mirroring) {
        throw std::invalid_argument(
                "packingsolver::irregular::Solution::add_item: "
                " mirroring is not allowed for this item type; "
                "item_type_id: " + std::to_string(item_type_id) + ".");
    }

    SolutionItem item;
    item.item_type_id = item_type_id;
    item.bl_corner = bl_corner;
    item.angle = angle;
    item.mirror = mirror;
    bin.items.push_back(item);

    item_area_ += bin.copies * item_type.area_orig;
    item_profit_ += bin.copies * item_type.profit;
    number_of_items_ += bin.copies;
    item_copies_[item_type_id] += bin.copies;

    if (bin_pos == (BinPos)bins_.size() - 1) {
        auto mm = item_type.compute_min_max(angle, mirror);
        x_min_ = std::min(x_min_, bl_corner.x + mm.first.x);
        y_min_ = std::min(y_min_, bl_corner.y + mm.first.y);
        x_max_ = std::max(x_max_, bl_corner.x + mm.second.x);
        y_max_ = std::max(y_max_, bl_corner.y + mm.second.y);
        leftover_value_ = (bin_type.x_max - bin_type.x_min) * (bin_type.y_max - bin_type.y_min)
            - (x_max_ - bin_type.x_min) * (y_max_ - bin_type.y_min);
    }

    //if (strictly_lesser(leftover_value_, 0.0)) {
    //    write("solution_irregular.json");
    //    throw std::invalid_argument(
    //            "irregular::Solution::add_item."
    //            " Negative leftover value: "
    //            + std::to_string(leftover_value_) + ".");
    //}
}

void Solution::append(
        const Solution& solution,
        BinPos bin_pos,
        BinPos copies,
        const std::vector<BinTypeId>& bin_type_ids,
        const std::vector<ItemTypeId>& item_type_ids)
{
    // Check bin_pos.
    if (bin_pos >= solution.number_of_different_bins()) {
        throw std::invalid_argument(
                "packingsolver::irregular::Solution::append: "
                "invalid 'bin_pos'; "
                "bin_pos: " + std::to_string(bin_pos) + "; "
                "solution.number_of_different_bins(): " + std::to_string(solution.number_of_different_bins()) + ".");
    }

    BinTypeId bin_type_id = (bin_type_ids.empty())?
        solution.bins_[bin_pos].bin_type_id:
        bin_type_ids[solution.bins_[bin_pos].bin_type_id];
    BinPos i_pos = add_bin(bin_type_id, copies);
    for (const SolutionItem& item: solution.bins_[bin_pos].items) {
        ItemTypeId item_type_id = (item_type_ids.empty())?
            item.item_type_id:
            item_type_ids[item.item_type_id];
        add_item(i_pos, item_type_id, item.bl_corner, item.angle, item.mirror);
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
            bool mirror = false;
            if (json_item.contains("mirror"))
                mirror = json_item["mirror"];
            add_item(
                    bin_pos,
                    json_item["id"],
                    {json_item["x"], json_item["y"]},
                    json_item["angle"],
                    mirror);
        }
    }
}

double Solution::density_x() const
{
    AreaDbl area = bin_area();
    if (number_of_different_bins() > 0) {
        BinTypeId bin_type_id = bins_.back().bin_type_id;
        const BinType& bin_type = instance().bin_type(bin_type_id);
        area -= bin_type.area_orig;
        area += x_max_ * (bin_type.y_max - bin_type.y_min);
    }
    return item_area() / area;
}

double Solution::density_y() const
{
    AreaDbl area = bin_area();
    if (number_of_different_bins() > 0) {
        BinTypeId bin_type_id = bins_.back().bin_type_id;
        const BinType& bin_type = instance().bin_type(bin_type_id);
        area -= bin_type.area_orig;
        area += y_max_ * (bin_type.x_max - bin_type.x_min);
    }
    return item_area() / area;
}

bool Solution::operator<(const Solution& solution) const
{
    switch (instance().objective()) {
    case Objective::BinPacking: {
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
        return strictly_lesser(solution.x_max(), x_max());
    } case Objective::OpenDimensionY: {
        if (!solution.full())
            return false;
        if (!full())
            return true;
        return strictly_lesser(solution.y_max(), y_max());
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
                element_pos < (Counter)bin_type.shape_orig.elements.size();
                ++element_pos) {
            const ShapeElement& element = bin_type.shape_orig.elements[element_pos];
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
                    element_pos < (Counter)defect.shape_orig.elements.size();
                    ++element_pos) {
                const ShapeElement& element = defect.shape_orig.elements[element_pos];
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
                        hole_pos < (Counter)defect.holes_orig.size();
                        ++hole_pos) {
                    const Shape& hole = defect.holes_orig[hole_pos];
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
            json["bins"][bin_pos]["items"][item_pos]["mirror"] = item.mirror;
            for (Counter item_shape_pos = 0;
                    item_shape_pos < (Counter)item_type.shapes.size();
                    ++item_shape_pos) {
                const ItemShape& item_shape = item_type.shapes[item_shape_pos];
                Shape shape = item_shape.shape_orig;
                if (item.mirror)
                    shape = shape.axial_symmetry_y_axis();
                shape = shape.rotate(item.angle);
                for (Counter element_pos = 0;
                        element_pos < (Counter)shape.elements.size();
                        ++element_pos) {
                    ShapeElement element = shape.elements[element_pos];
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
                        hole_pos < (Counter)item_shape.holes_orig.size();
                        ++hole_pos) {
                    Shape hole = item_shape.holes_orig[hole_pos];
                    if (item.mirror)
                        hole = hole.axial_symmetry_y_axis();
                    hole = hole.rotate(item.angle);
                    for (Counter element_pos = 0;
                            element_pos < (Counter)hole.elements.size();
                            ++element_pos) {
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

    file << std::setw(4) << json << std::endl;
}

void Solution::write_svg(
        const std::string& file_path,
        BinPos bin_pos) const
{
    if (file_path.empty())
        return;
    std::ofstream file{file_path};
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + file_path + "\".");
    }

    const SolutionBin& bin = this->bin(bin_pos);
    const BinType& bin_type = instance().bin_type(bin.bin_type_id);
    LengthDbl width = (bin_type.x_max - bin_type.x_min);
    LengthDbl height = (bin_type.y_max - bin_type.y_min);

    double factor = compute_svg_factor(width);

    std::string s = "<svg viewBox=\""
        + std::to_string(bin_type.x_min * factor)
        + " " + std::to_string(-bin_type.y_min * factor - height * factor)
        + " " + std::to_string(width * factor)
        + " " + std::to_string(height * factor)
        + "\" version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n";
    file << s;

    // Write bin.
    file << "<g>" << std::endl;
    file << to_svg(bin_type.shape_scaled, {}, factor, "white");
    for (const Defect& defect: bin_type.defects)
        file << to_svg(defect.shape_scaled, defect.holes_scaled, factor, "red");
    file << "</g>" << std::endl;

    // Write items.
    for (const SolutionItem& item: bin.items) {
        const ItemType& item_type = instance().item_type(item.item_type_id);

        LengthDbl x_min = std::numeric_limits<LengthDbl>::infinity();
        LengthDbl x_max = -std::numeric_limits<LengthDbl>::infinity();
        LengthDbl y_min = std::numeric_limits<LengthDbl>::infinity();
        LengthDbl y_max = -std::numeric_limits<LengthDbl>::infinity();

        file << "<g>" << std::endl;
        for (const ItemShape& item_shape: item_type.shapes) {
            Shape shape = item_shape.shape_scaled;
            std::vector<Shape> holes = item_shape.holes_scaled;

            // Apply mirroring.
            if (item.mirror)
                shape = shape.axial_symmetry_y_axis();
            // Apply angles.
            shape = shape.rotate(item.angle);
            // Apply shift.
            shape.shift(item.bl_corner.x, item.bl_corner.y);

            for (Counter hole_pos = 0;
                    hole_pos < (Counter)item_shape.holes_scaled.size();
                    ++hole_pos) {
                // Apply mirroring.
                if (item.mirror)
                    holes[hole_pos] = holes[hole_pos].axial_symmetry_y_axis();
                // Apply angles.
                holes[hole_pos] = holes[hole_pos].rotate(item.angle);
                // Apply shift.
                holes[hole_pos].shift(item.bl_corner.x, item.bl_corner.y);
            }

            file << to_svg(shape, holes, factor, "blue");

            auto mm = shape.compute_min_max(0.0);
            x_min = (std::min)(x_min, mm.first.x);
            x_max = (std::max)(x_max, mm.second.x);
            y_min = (std::min)(y_min, mm.first.y);
            y_max = (std::max)(y_max, mm.second.y);
        }

        // Write item type id.
        LengthDbl x = (x_min + x_max) / 2;
        LengthDbl y = (y_min + y_max) / 2;
        file << "<text x=\"" << std::to_string(x * factor)
            << "\" y=\"" << std::to_string(-y * factor)
            << "\" dominant-baseline=\"middle\" text-anchor=\"middle\">"
            << std::to_string(item.item_type_id)
            << "</text>"
            << std::endl;

        file << "</g>" << std::endl;
    }

    file << "</svg>" << std::endl;
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
        {"FullWaste", full_waste()},
        {"FullWastePercentage", full_waste_percentage()},
        {"XMin", x_min()},
        {"YMin", y_min()},
        {"XMax", x_max()},
        {"YMax", y_max()},
        {"DensityX", density_x()},
        {"DensityY", density_y()},
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
            << "Item area:        " << optimizationtools::Ratio<Profit>(item_area(), instance().item_area()) << std::endl
            << "Item profit:      " << optimizationtools::Ratio<Profit>(profit(), instance().item_profit()) << std::endl
            << "Number of bins:   " << optimizationtools::Ratio<BinPos>(number_of_bins(), instance().number_of_bins()) << std::endl
            << "Bin area:         " << optimizationtools::Ratio<BinPos>(bin_area(), instance().bin_area()) << std::endl
            << "Bin cost:         " << cost() << std::endl
            << "Full waste:       " << full_waste() << std::endl
            << "Full waste (%):   " << 100 * full_waste_percentage() << std::endl
            << "X min:            " << x_min() << std::endl
            << "Y min:            " << y_min() << std::endl
            << "X max:            " << x_max() << std::endl
            << "Y max:            " << y_max() << std::endl
            << "Density X:        " << density_x() << std::endl
            << "Density Y:        " << density_y() << std::endl
            << "Leftover value:   " << leftover_value() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::right << std::endl
            << std::setw(12) << "Bin"
            << std::setw(12) << "Type"
            << std::setw(12) << "Copies"
            << std::setw(12) << "# items"
            << std::endl
            << std::setw(12) << "---"
            << std::setw(12) << "----"
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
                << std::setw(12) << bin(bin_pos).items.size()
                << std::endl;
        }
    }

    if (verbosity_level >= 3) {
        os
            << std::right << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "Copies"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "------"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance().number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance().item_type(item_type_id);
            os
                << std::setw(12) << item_type_id
                << std::setw(12) << item_copies(item_type_id)
                << std::endl;
        }
    }
}
