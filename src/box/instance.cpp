#include "packingsolver/box/instance.hpp"

#include <iostream>
#include <fstream>

using namespace packingsolver;
using namespace packingsolver::box;

std::ostream& box::operator<<(
        std::ostream& os,
        Point xyz)
{
    os << "x " << xyz.x << " y " << xyz.y << " z " << xyz.z;
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
        << " rotations " << item_type.rotations
        ;
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

std::ostream& Instance::format(
        std::ostream& os,
        int verbosity_level) const
{
    if (verbosity_level >= 1) {
        os
            << "Objective:             " << objective() << std::endl
            << "Number of item types:  " << number_of_item_types() << std::endl
            << "Number of items:       " << number_of_items() << std::endl
            << "Number of bin types:   " << number_of_bin_types() << std::endl
            << "Number of bins:        " << number_of_bins() << std::endl
            << "Number of defects:     " << number_of_defects() << std::endl
            << "Total item volume:     " << item_volume() << std::endl
            << "Total item profit:     " << item_profit() << std::endl
            << "Largest item profit:   " << largest_item_profit() << std::endl
            << "Total item weight:     " << item_weight() << std::endl
            << "Largest item copies:   " << largest_item_copies() << std::endl
            << "Total bin volume:      " << bin_volume() << std::endl
            << "Total bin weight:      " << bin_weight() << std::endl
            << "Largest bin cost:      " << largest_bin_cost() << std::endl
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
                bin_type_id < number_of_bin_types();
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
            << std::setw(10) << "Rot."
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
                item_type_id < number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            os
                << std::setw(10) << item_type_id
                << std::setw(10) << item_type.box.x
                << std::setw(10) << item_type.box.y
                << std::setw(10) << item_type.box.z
                << std::setw(12) << item_type.profit
                << std::setw(10) << item_type.copies
                << std::setw(10) << item_type.rotations
                << std::setw(10) << item_type.weight
                << std::endl;
        }
    }

    return os;
}

void Instance::write(
        const std::string& instance_path) const
{
    write_item_types(instance_path + "_items.csv");
    write_bin_types(instance_path + "_bins.csv");
    write_parameters(instance_path + "_parameters.csv");
}

void Instance::write_item_types(
        const std::string& items_path) const
{
    std::ofstream file(items_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + items_path + "\".");
    }
    file << "ID,"
        "X,"
        "Y,"
        "Z,"
        "COPIES,"
        "PROFIT,"
        "ROTATIONS,"
        "WEIGHT" << std::endl;
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        file
            << item_type_id << ","
            << item_type.box.x << ","
            << item_type.box.y << ","
            << item_type.box.z << ","
            << item_type.copies << ","
            << item_type.profit << ","
            << item_type.rotations << ","
            << item_type.weight << std::endl;
    }
}

void Instance::write_bin_types(
        const std::string& bins_path) const
{
    std::ofstream file(bins_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + bins_path + "\".");
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
            bin_type_id < number_of_bin_types();
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
                "Unable to open file \"" + parameters_path + "\".");
    }
    file
        << "NAME,VALUE" << std::endl
        ;
}
