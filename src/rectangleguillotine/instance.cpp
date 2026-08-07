#include "packingsolver/rectangleguillotine/instance.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in,
        CutType& cut_type)
{
    std::string token;
    in >> token;
    if (token == "roadef2018" || token == "Roadef2018") {
        cut_type = CutType::Roadef2018;
    } else if (token == "non-exact" || token == "NonExact") {
        cut_type = CutType::NonExact;
    } else if (token == "exact" || token == "Exact") {
        cut_type = CutType::Exact;
    } else if (token == "homogenous" || token == "Homogenous") {
        cut_type = CutType::Homogenous;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in,
        CutOrientation& o)
{
    std::string token;
    in >> token;
    if (token == "horizontal" || token == "Horizontal") {
        o = CutOrientation::Horizontal;
    } else if (token == "vertical" || token == "Vertical") {
        o = CutOrientation::Vertical;
    } else if (token == "any" || token == "Any") {
        o = CutOrientation::Any;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in,
        TrimType& trim_type)
{
    std::string token;
    in >> token;
    if (token == "h" || token == "H" || token == "hard" || token == "Hard" || token == "0") {
        trim_type = TrimType::Hard;
    } else if (token == "s" || token == "S" || token == "soft" || token == "Soft" || token == "1") {
        trim_type = TrimType::Soft;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        CutType cut_type)
{
    switch (cut_type) {
    case CutType::Roadef2018: {
        os << "Roadef2018";
        break;
    } case CutType::NonExact: {
        os << "NonExact";
        break;
    } case CutType::Exact: {
        os << "Exact";
        break;
    } case CutType::Homogenous: {
        os << "Homogenous";
        break;
    }
    }
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        CutOrientation o)
{
    switch (o) {
    case CutOrientation::Vertical: {
        os << "Vertical";
        break;
    } case CutOrientation::Horizontal: {
        os << "Horizontal";
        break;
    } case CutOrientation::Any: {
        os << "Any";
        break;
    }
    }
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        TrimType trim_type)
{
    switch (trim_type) {
    case TrimType::Hard: {
        os << "H";
        break;
    } case TrimType::Soft: {
        os << "S";
        break;
    }
    }
    return os;
}

bool rectangleguillotine::rect_intersection(
        Coord c1,
        Rectangle r1,
        Coord c2,
        Rectangle r2)
{
    return c1.x + r1.w > c2.x
        && c2.x + r2.w > c1.x
        && c1.y + r1.h > c2.y
        && c2.y + r2.h > c1.y;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        Coord xy)
{
    os << xy.x << " " << xy.y;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        Rectangle r)
{
    os << r.w << " " << r.h;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const ItemType& item_type)
{
    os
        << "w " << item_type.rect.w
        << " h " << item_type.rect.h
        << " profit " << item_type.profit
        << " copies " << item_type.copies
        << " stack_id " << item_type.stack_id
        << " oriented " << item_type.oriented
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const BinType& bin_type)
{
    os
        << "w " << bin_type.rect.w
        << " h " << bin_type.rect.h
        << " copies " << bin_type.copies
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const Defect& defect)
{
    os
        << "defect"
        << " x " << defect.pos.x
        << " y " << defect.pos.y
        << " w " << defect.rect.w
        << " h " << defect.rect.h
        ;
    return os;
}

void Instance::write(
        const std::string& instance_path,
        InstanceFormat format) const
{
    switch (format) {
    case InstanceFormat::Csv:
        write_csv(instance_path);
        break;
    case InstanceFormat::Json:
        write_json(instance_path);
        break;
    }
}

void Instance::write_csv(
        const std::string& instance_path) const
{
    // Check every feature of the instance can actually be represented in
    // the CSV format before writing anything (so a rejected write doesn't
    // leave partial files behind).
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        if (bin_type.number_of_resources() > 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin type " + std::to_string(bin_type_id) + " has "
                    "resources, which the CSV format cannot represent; use "
                    "'InstanceFormat::Json' instead.");
        }
    }

    // Export items.
    std::string items_path = instance_path + "_items.csv";
    std::ofstream f_items(items_path);
    if (!f_items.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + items_path + "\".");
    }
    f_items <<
        "ID,"
        "WIDTH,"
        "HEIGHT,"
        "PROFIT,"
        "COPIES,"
        "COPIES_MIN,"
        "ORIENTED,"
        "STACK_ID" << std::endl;
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        f_items
            << item_type_id << ","
            << item_type.rect.w << ","
            << item_type.rect.h << ","
            << item_type.profit << ","
            << item_type.copies << ","
            << item_type.copies_min << ","
            << item_type.oriented << ","
            << item_type.stack_id << std::endl;
    }

    // Export bins.
    std::string bins_path = instance_path + "_bins.csv";
    std::ofstream f_bins(bins_path);
    if (!f_bins.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + bins_path + "\".");
    }
    f_bins <<
        "ID,"
        "WIDTH,"
        "HEIGHT,"
        "COST,"
        "COPIES,"
        "COPIES_MIN,"
        "BOTTOM_TRIM,"
        "TOP_TRIM,"
        "LEFT_TRIM,"
        "RIGHT_TRIM,"
        "BOTTOM_TRIM_TYPE,"
        "TOP_TRIM_TYPE,"
        "LEFT_TRIM_TYPE,"
        "RIGHT_TRIM_TYPE" << std::endl;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        f_bins
            << bin_type_id << ","
            << bin_type.rect.w << ","
            << bin_type.rect.h << ","
            << bin_type.cost << ","
            << bin_type.copies << ","
            << bin_type.copies_min << ","
            << bin_type.bottom_trim << ","
            << bin_type.top_trim << ","
            << bin_type.left_trim << ","
            << bin_type.right_trim << ","
            << bin_type.bottom_trim_type << ","
            << bin_type.top_trim_type << ","
            << bin_type.left_trim_type << ","
            << bin_type.right_trim_type << std::endl;
    }

    // Export defects.
    if (number_of_defects() > 0) {
        std::string defects_path = instance_path + "_defects.csv";
        std::ofstream f_defects(defects_path);
        if (number_of_defects() > 0 && !f_defects.good()) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "unable to open file \"" + defects_path + "\".");
        }
        f_defects << "ID,BIN,X,Y,WIDTH,HEIGHT" << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            for (DefectId defect_id = 0;
                    defect_id < (DefectId)bin_type.defects.size();
                    ++defect_id) {
                const Defect& defect = bin_type.defects[defect_id];
                f_defects
                    << defect_id << ","
                    << bin_type_id << ","
                    << defect.pos.x << ","
                    << defect.pos.y << ","
                    << defect.rect.w << ","
                    << defect.rect.h << std::endl;
            }
        }
    }

    // Export parameters.
    std::string parameters_path = instance_path + "_parameters.csv";
    std::ofstream f_parameters(parameters_path);
    if (!f_parameters.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + parameters_path + "\".");
    }
    f_parameters << "NAME,VALUE" << std::endl
        << "objective," << objective() << std::endl
        << "number_of_stages," << parameters().number_of_stages << std::endl
        << "cut_type," << parameters().cut_type << std::endl
        << "first_stage_orientation," << parameters().first_stage_orientation << std::endl
        << "minimum_distance_1_cuts," << parameters().minimum_distance_1_cuts << std::endl
        << "maximum_distance_1_cuts," << parameters().maximum_distance_1_cuts << std::endl
        << "minimum_distance_2_cuts," << parameters().minimum_distance_2_cuts << std::endl
        << "maximum_distance_2_cuts," << parameters().maximum_distance_2_cuts << std::endl
        << "minimum_waste_length," << parameters().minimum_waste_length << std::endl
        << "maximum_number_1_cuts," << parameters().maximum_number_1_cuts << std::endl
        << "maximum_number_2_cuts," << parameters().maximum_number_2_cuts << std::endl
        << "cut_thickness," << parameters().cut_thickness << std::endl;
    for (Counter stage_id = 0;
            stage_id < (Counter)parameters().cutting_costs.size();
            ++stage_id) {
        f_parameters
            << "cuts_fixed_cost_" << stage_id << "," << parameters().cutting_costs[stage_id].fixed << std::endl
            << "cuts_variable_cost_" << stage_id << "," << parameters().cutting_costs[stage_id].variable << std::endl;
    }
    if (objective() == Objective::BinPackingCuttingCost) {
        f_parameters
            << "waste_cost," << parameters().waste_cost << std::endl;
    }
}

void Instance::write_json(
        const std::string& instance_path) const
{
    nlohmann::json j;

    {
        std::stringstream ss;
        ss << objective();
        j["objective"] = ss.str();
    }
    j["number_of_stages"] = parameters().number_of_stages;
    {
        std::stringstream ss;
        ss << parameters().cut_type;
        j["cut_type"] = ss.str();
    }
    {
        std::stringstream ss;
        ss << parameters().first_stage_orientation;
        j["first_stage_orientation"] = ss.str();
    }
    j["minimum_distance_1_cuts"] = parameters().minimum_distance_1_cuts;
    j["maximum_distance_1_cuts"] = parameters().maximum_distance_1_cuts;
    j["minimum_distance_2_cuts"] = parameters().minimum_distance_2_cuts;
    j["maximum_distance_2_cuts"] = parameters().maximum_distance_2_cuts;
    j["minimum_waste_length"] = parameters().minimum_waste_length;
    j["maximum_number_1_cuts"] = parameters().maximum_number_1_cuts;
    j["maximum_number_2_cuts"] = parameters().maximum_number_2_cuts;
    j["cut_through_defects"] = parameters().cut_through_defects;
    j["cut_thickness"] = parameters().cut_thickness;
    if (!parameters().cutting_costs.empty()) {
        j["cutting_costs"] = nlohmann::json::array();
        for (const CutCost& cutting_cost: parameters().cutting_costs) {
            nlohmann::json json_cutting_cost;
            json_cutting_cost["fixed"] = cutting_cost.fixed;
            json_cutting_cost["variable"] = cutting_cost.variable;
            j["cutting_costs"].push_back(json_cutting_cost);
        }
    }
    if (objective() == Objective::BinPackingCuttingCost)
        j["waste_cost"] = parameters().waste_cost;

    j["bin_types"] = nlohmann::json::array();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        nlohmann::json json_bin_type;
        json_bin_type["width"] = bin_type.rect.w;
        json_bin_type["height"] = bin_type.rect.h;
        json_bin_type["cost"] = bin_type.cost;
        json_bin_type["copies"] = bin_type.copies;
        json_bin_type["copies_min"] = bin_type.copies_min;
        json_bin_type["bottom_trim"] = bin_type.bottom_trim;
        json_bin_type["top_trim"] = bin_type.top_trim;
        json_bin_type["left_trim"] = bin_type.left_trim;
        json_bin_type["right_trim"] = bin_type.right_trim;
        {
            std::stringstream ss;
            ss << bin_type.bottom_trim_type;
            json_bin_type["bottom_trim_type"] = ss.str();
        }
        {
            std::stringstream ss;
            ss << bin_type.top_trim_type;
            json_bin_type["top_trim_type"] = ss.str();
        }
        {
            std::stringstream ss;
            ss << bin_type.left_trim_type;
            json_bin_type["left_trim_type"] = ss.str();
        }
        {
            std::stringstream ss;
            ss << bin_type.right_trim_type;
            json_bin_type["right_trim_type"] = ss.str();
        }

        if (!bin_type.defects.empty()) {
            json_bin_type["defects"] = nlohmann::json::array();
            for (const Defect& defect: bin_type.defects) {
                nlohmann::json json_defect;
                json_defect["x"] = defect.pos.x;
                json_defect["y"] = defect.pos.y;
                json_defect["width"] = defect.rect.w;
                json_defect["height"] = defect.rect.h;
                json_bin_type["defects"].push_back(json_defect);
            }
        }

        if (bin_type.number_of_resources() > 0) {
            json_bin_type["resources"] = nlohmann::json::array();
            for (ResourceId resource_id = 0;
                    resource_id < bin_type.number_of_resources();
                    ++resource_id) {
                const Resource& resource = bin_type.resource(resource_id);
                nlohmann::json json_resource;
                json_resource["capacity"] = resource.capacity;
                json_resource["penalize"] = resource.penalize;
                json_resource["penalty"] = resource.penalty;
                json_resource["consumptions"] = nlohmann::json::array();
                for (ItemTypeId item_type_id = 0;
                        item_type_id < (ItemTypeId)resource.item_consumptions.size();
                        ++item_type_id) {
                    const std::vector<double>& schedule = resource.item_consumptions[item_type_id];
                    if (schedule.empty())
                        continue;
                    nlohmann::json json_consumption;
                    json_consumption["item_type_id"] = item_type_id;
                    if (schedule.size() == 1) {
                        json_consumption["consumption"] = schedule[0];
                    } else {
                        json_consumption["consumption_schedule"] = schedule;
                    }
                    json_resource["consumptions"].push_back(json_consumption);
                }
                json_bin_type["resources"].push_back(json_resource);
            }
        }

        j["bin_types"].push_back(json_bin_type);
    }

    j["item_types"] = nlohmann::json::array();
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        nlohmann::json json_item_type;
        json_item_type["width"] = item_type.rect.w;
        json_item_type["height"] = item_type.rect.h;
        json_item_type["profit"] = item_type.profit;
        json_item_type["copies"] = item_type.copies;
        json_item_type["copies_min"] = item_type.copies_min;
        json_item_type["oriented"] = item_type.oriented;
        json_item_type["stack_id"] = item_type.stack_id;
        j["item_types"].push_back(json_item_type);
    }

    std::ofstream file(instance_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + instance_path + "\".");
    }
    file << j.dump(4) << std::endl;
}

std::ostream& Instance::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Objective:                             " << objective() << std::endl
            << "Number of item types:                  " << number_of_item_types() << std::endl
            << "Number of items:                       " << number_of_items() << std::endl
            << "Number of bin types:                   " << number_of_bin_types() << std::endl
            << "Number of bins:                        " << number_of_bins() << std::endl
            << "Number of stacks:                      " << number_of_stacks() << std::endl
            << "Number of defects:                     " << number_of_defects() << std::endl
            << "Number of stages:                      " << parameters().number_of_stages << std::endl
            << "Cut type:                              " << parameters().cut_type << std::endl
            << "First stage orientation:               " << parameters().first_stage_orientation << std::endl
            << "Minimum distance between 1-cuts:       " << parameters().minimum_distance_1_cuts << std::endl
            << "Maximum distance between 1-cuts:       " << parameters().maximum_distance_1_cuts << std::endl
            << "Minimum distance between 2-cuts:       " << parameters().minimum_distance_2_cuts << std::endl
            << "Maximum distance between 2-cuts:       " << parameters().maximum_distance_2_cuts << std::endl
            << "Minimum waste length:                  " << parameters().minimum_waste_length << std::endl
            << "Maximum number of consecutive 1-cuts:  " << parameters().maximum_number_1_cuts << std::endl
            << "Maximum number of consecutive 2-cuts:  " << parameters().maximum_number_2_cuts << std::endl
            << "Cut through defects:                   " << parameters().cut_through_defects << std::endl
            << "Cut thickness:                         " << parameters().cut_thickness << std::endl
            << "Total item area:                       " << item_area() << std::endl
            << "Total item profit:                     " << item_profit() << std::endl
            << "Largest item profit:                   " << largest_item_profit() << std::endl
            << "Largest item copies:                   " << largest_item_copies() << std::endl
            << "Total bin area:                        " << bin_area() << std::endl
            << "Largest bin cost:                      " << largest_bin_cost() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Width"
            << std::setw(12) << "Height"
            << std::setw(12) << "Cost"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Copies min"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "----------"
            << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            os
                << std::setw(12) << bin_type_id
                << std::setw(12) << bin_type.rect.w
                << std::setw(12) << bin_type.rect.h
                << std::setw(12) << bin_type.cost
                << std::setw(12) << bin_type.copies
                << std::setw(12) << bin_type.copies_min
                << std::endl;
        }

        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Left trim"
            << std::setw(12) << "Right trim"
            << std::setw(12) << "Bottom trim"
            << std::setw(12) << "Top trim"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "---------"
            << std::setw(12) << "----------"
            << std::setw(12) << "-----------"
            << std::setw(12) << "--------"
            << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            os
                << std::setw(12) << bin_type_id
                << std::setw(8) << bin_type.left_trim
                << std::setw(4) << bin_type.left_trim_type
                << std::setw(8) << bin_type.right_trim
                << std::setw(4) << bin_type.right_trim_type
                << std::setw(8) << bin_type.bottom_trim
                << std::setw(4) << bin_type.bottom_trim_type
                << std::setw(8) << bin_type.top_trim
                << std::setw(4) << bin_type.top_trim_type
                << std::endl;
        }

        if (number_of_defects() > 0) {
            os
                << std::endl
                << std::setw(12) << "Defect"
                << std::setw(12) << "Bin type"
                << std::setw(12) << "X"
                << std::setw(12) << "Y"
                << std::setw(12) << "width"
                << std::setw(12) << "Height"
                << std::endl
                << std::setw(12) << "------"
                << std::setw(12) << "--------"
                << std::setw(12) << "-"
                << std::setw(12) << "-"
                << std::setw(12) << "-----"
                << std::setw(12) << "------"
                << std::endl;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < number_of_bin_types();
                    ++bin_type_id) {
                const BinType& bin_type = this->bin_type(bin_type_id);
                for (DefectId defect_id = 0;
                        defect_id < (DefectId)bin_type.defects.size();
                        ++defect_id) {
                    const Defect& defect = bin_type.defects[defect_id];
                    os
                        << std::setw(12) << defect_id
                        << std::setw(12) << bin_type_id
                        << std::setw(12) << defect.pos.x
                        << std::setw(12) << defect.pos.y
                        << std::setw(12) << defect.rect.w
                        << std::setw(12) << defect.rect.h
                        << std::endl;
                }
            }
        }

        os
            << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "Width"
            << std::setw(12) << "Height"
            << std::setw(12) << "Profit"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Oriented"
            << std::setw(12) << "Stack id"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "--------"
            << std::setw(12) << "-----"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            os
                << std::setw(12) << item_type_id
                << std::setw(12) << item_type.rect.w
                << std::setw(12) << item_type.rect.h
                << std::setw(12) << item_type.profit
                << std::setw(12) << item_type.copies
                << std::setw(12) << item_type.oriented
                << std::setw(12) << item_type.stack_id
                << std::endl;
        }

        if (objective() == Objective::BinPackingCuttingCost) {
            os
                << std::endl
                << std::setw(12) << "Stage"
                << std::setw(12) << "Fixed cost"
                << std::setw(16) << "Variable cost"
                << std::endl
                << std::setw(12) << "-----"
                << std::setw(12) << "----------"
                << std::setw(16) << "-------------"
                << std::endl;
            for (Counter stage_id = 0;
                    stage_id < (Counter)parameters().cutting_costs.size();
                    ++stage_id) {
                const CutCost& cutting_cost = parameters().cutting_costs[stage_id];
                os
                    << std::setw(12) << stage_id
                    << std::setw(12) << cutting_cost.fixed
                    << std::setw(16) << cutting_cost.variable
                    << std::endl;
            }
            os
                << std::endl
                << "Waste cost:            " << parameters().waste_cost << std::endl
                ;
        }
    }

    return os;
}
