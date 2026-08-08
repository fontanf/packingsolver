#include "packingsolver/box/instance.hpp"

#include <fstream>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::box;

std::string box::to_string(Rotation rotation)
{
    switch (rotation) {
    case Rotation::XYZ: return "XYZ";
    case Rotation::YXZ: return "YXZ";
    case Rotation::ZYX: return "ZYX";
    case Rotation::YZX: return "YZX";
    case Rotation::XZY: return "XZY";
    case Rotation::ZXY: return "ZXY";
    default: return "?";
    }
}

std::ostream& box::operator<<(
        std::ostream& os,
        Rotation rotation)
{
    os << to_string(rotation);
    return os;
}

Rotation box::rotation_from_string(const std::string& str)
{
    if (str == "XYZ") return Rotation::XYZ;
    if (str == "YXZ") return Rotation::YXZ;
    if (str == "ZYX") return Rotation::ZYX;
    if (str == "YZX") return Rotation::YZX;
    if (str == "XZY") return Rotation::XZY;
    if (str == "ZXY") return Rotation::ZXY;
    throw std::invalid_argument(
            FUNC_SIGNATURE + ": "
            "unknown rotation string \"" + str + "\".");
}

std::istream& box::operator>>(
        std::istream& in,
        Direction& o)
{
    std::string token;
    in >> token;
    if (token == "x"
            || token == "X") {
        o = Direction::X;
    } else if (token == "y"
            || token == "Y") {
        o = Direction::Y;
    } else if (token == "z"
            || token == "Z") {
        o = Direction::Z;
    } else if (token == "any"
            || token == "Any") {
        o = Direction::Any;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& box::operator<<(
        std::ostream& os,
        Direction o)
{
    switch (o) {
    case Direction::X: {
        os << "X";
        break;
    } case Direction::Y: {
        os << "Y";
        break;
    } case Direction::Z: {
        os << "Z";
        break;
    } case Direction::Any: {
        os << "Any";
        break;
    }
    }
    return os;
}

std::ostream& box::operator<<(
        std::ostream& os,
        Point xyz)
{
    os << "x " << xyz.x << " y " << xyz.y << " z " << xyz.z;
    return os;
}

std::ostream& box::operator<<(
        std::ostream& os,
        Box box)
{
    os << "x " << box.x << " y " << box.y << " z " << box.z;
    return os;
}

std::ostream& packingsolver::box::operator<<(
        std::ostream& os,
        const ItemType& item_type)
{
    os
        << "x " << item_type.box.x
        << " y " << item_type.box.y
        << " z " << item_type.box.z
        << " profit " << item_type.profit
        << " copies " << item_type.copies
        << " rotations";
    for (Rotation r: item_type.rotations)
        os << " " << r;
    return os;
}

std::ostream& packingsolver::box::operator<<(
        std::ostream& os,
        const BinType& bin_type)
{
    os
        << "x " << bin_type.box.x
        << " y " << bin_type.box.y
        << " z " << bin_type.box.z
        << " copies " << bin_type.copies
        ;
    return os;
}

bool Instance::fits_some_bin(
        ItemTypeId item_type_id) const
{
    const ItemType& item_type = this->item_type(item_type_id);
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        for (Rotation rotation: item_type.rotations) {
            Box effective_box = item_type.box.rotate(rotation);
            if (effective_box.x <= bin_type.box.x
                    && effective_box.y <= bin_type.box.y
                    && effective_box.z <= bin_type.box.z) {
                return true;
            }
        }
    }
    return false;
}

bool Instance::resources_matter() const
{
    return number_of_bin_types() == 1
        && bin_type(0).number_of_resources() > 0;
}

std::ostream& Instance::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Objective:             " << this->objective() << std::endl
            << "Number of item types:  " << this->number_of_item_types() << std::endl
            << "Number of items:       " << this->number_of_items() << std::endl
            << "Number of bin types:   " << this->number_of_bin_types() << std::endl
            << "Number of bins:        " << this->number_of_bins() << std::endl
            << "Number of defects:     " << this->number_of_defects() << std::endl
            << "Total item volume:     " << this->item_volume() << std::endl
            << "Total item profit:     " << this->item_profit() << std::endl
            << "Largest item profit:   " << this->largest_item_profit() << std::endl
            << "Total item weight:     " << this->item_weight() << std::endl
            << "Largest item copies:   " << this->largest_item_copies() << std::endl
            << "Smallest item x:       " << this->smallest_item_x() << std::endl
            << "Smallest item y:       " << this->smallest_item_y() << std::endl
            << "Smallest item z:       " << this->smallest_item_z() << std::endl
            << "Total bin volume:      " << this->bin_volume() << std::endl
            << "Total bin weight:      " << this->bin_weight() << std::endl
            << "Largest bin cost:      " << this->largest_bin_cost() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(10) << "X"
            << std::setw(10) << "Y"
            << std::setw(10) << "Z"
            << std::setw(12) << "Cost"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Copies min"
            << std::setw(12) << "Weight"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "----------"
            << std::setw(12) << "------"
            << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < this->number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            os
                << std::setw(12) << bin_type_id
                << std::setw(10) << bin_type.box.x
                << std::setw(10) << bin_type.box.y
                << std::setw(10) << bin_type.box.z
                << std::setw(12) << bin_type.cost
                << std::setw(12) << bin_type.copies
                << std::setw(12) << bin_type.copies_min
                << std::setw(12) << bin_type.maximum_weight
                << std::endl;
        }

        os
            << std::endl
            << std::setw(10) << "Item type"
            << std::setw(10) << "X"
            << std::setw(10) << "Y"
            << std::setw(10) << "Z"
            << std::setw(12) << "Profit"
            << std::setw(10) << "Copies"
            << std::setw(10) << "Rotations"
            << std::setw(10) << "Weight"
            << std::endl
            << std::setw(10) << "---------"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(12) << "------"
            << std::setw(10) << "------"
            << std::setw(10) << "---------"
            << std::setw(10) << "------"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < this->number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            os
                << std::setw(10) << item_type_id
                << std::setw(10) << item_type.box.x
                << std::setw(10) << item_type.box.y
                << std::setw(10) << item_type.box.z
                << std::setw(12) << item_type.profit
                << std::setw(10) << item_type.copies
                << std::setw(10) << item_type.rotations.size()
                << std::setw(10) << item_type.weight
                << std::endl;
        }
    }

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

    this->write_item_types(instance_path + "_items.csv");
    this->write_bin_types(instance_path + "_bins.csv");
    this->write_parameters(instance_path + "_parameters.csv");
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

    j["bin_types"] = nlohmann::json::array();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        nlohmann::json json_bin_type;
        json_bin_type["x"] = bin_type.box.x;
        json_bin_type["y"] = bin_type.box.y;
        json_bin_type["z"] = bin_type.box.z;
        json_bin_type["cost"] = bin_type.cost;
        json_bin_type["copies"] = bin_type.copies;
        json_bin_type["copies_min"] = bin_type.copies_min;
        json_bin_type["maximum_weight"] = bin_type.maximum_weight;

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
        json_item_type["x"] = item_type.box.x;
        json_item_type["y"] = item_type.box.y;
        json_item_type["z"] = item_type.box.z;
        json_item_type["profit"] = item_type.profit;
        json_item_type["copies"] = item_type.copies;
        json_item_type["copies_min"] = item_type.copies_min;
        json_item_type["weight"] = item_type.weight;
        json_item_type["rotations"] = nlohmann::json::array();
        for (Rotation rotation: item_type.rotations)
            json_item_type["rotations"].push_back(to_string(rotation));
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

void Instance::write_item_types(
        const std::string& items_path) const
{
    std::ofstream file(items_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + items_path + "\".");
    }
    file << "ID,"
        "X,"
        "Y,"
        "Z,"
        "COPIES,"
        "COPIES_MIN,"
        "PROFIT,"
        "ROTATION_XYZ,"
        "ROTATION_YXZ,"
        "ROTATION_ZYX,"
        "ROTATION_YZX,"
        "ROTATION_XZY,"
        "ROTATION_ZXY,"
        "WEIGHT" << std::endl;
    for (ItemTypeId item_type_id = 0;
            item_type_id < this->number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        file
            << item_type_id << ","
            << item_type.box.x << ","
            << item_type.box.y << ","
            << item_type.box.z << ","
            << item_type.copies << ","
            << item_type.copies_min << ","
            << item_type.profit << ","
            << (item_type.can_rotate(Rotation::XYZ)? 1: 0) << ","
            << (item_type.can_rotate(Rotation::YXZ)? 1: 0) << ","
            << (item_type.can_rotate(Rotation::ZYX)? 1: 0) << ","
            << (item_type.can_rotate(Rotation::YZX)? 1: 0) << ","
            << (item_type.can_rotate(Rotation::XZY)? 1: 0) << ","
            << (item_type.can_rotate(Rotation::ZXY)? 1: 0) << ","
            << item_type.weight << std::endl;
    }
}

void Instance::write_bin_types(
        const std::string& bins_path) const
{
    std::ofstream file(bins_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + bins_path + "\".");
    }
    file << "ID,"
        "X,"
        "Y,"
        "Z,"
        "COST,"
        "COPIES,"
        "COPIES_MIN,"
        "MAXIMUM_WEIGHT" << std::endl;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < this->number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        file
            << bin_type_id << ","
            << bin_type.box.x << ","
            << bin_type.box.y << ","
            << bin_type.box.z << ","
            << bin_type.cost << ","
            << bin_type.copies << ","
            << bin_type.copies_min << ","
            << bin_type.maximum_weight << std::endl;
    }
}

void Instance::write_parameters(
        const std::string& parameters_path) const
{
    std::ofstream file(parameters_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + parameters_path + "\".");
    }
    file
        << "NAME,VALUE" << std::endl
        ;
}
