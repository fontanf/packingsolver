#include "packingsolver/boxstacks/instance.hpp"

#include "packingsolver/boxstacks/solution.hpp"

#include <iostream>
#include <fstream>
#include <climits>

using namespace packingsolver;
using namespace packingsolver::boxstacks;

std::ostream& boxstacks::operator<<(
        std::ostream& os,
        Point xyz)
{
    os << "x " << xyz.x << " y " << xyz.y << " z " << xyz.z;
    return os;
}

std::ostream& packingsolver::boxstacks::operator<<(
        std::ostream& os,
        const ItemType& item_type)
{
    os
        << "item type id " << item_type.id
        << " x " << item_type.box.x
        << " y " << item_type.box.y
        << " z " << item_type.box.z
        << " profit " << item_type.profit
        << " copies " << item_type.copies
        << " group_id " << item_type.group_id
        << " rotations " << item_type.rotations
        ;
    return os;
}

std::ostream& packingsolver::boxstacks::operator<<(
        std::ostream& os,
        const BinType& bin_type)
{
    os
        << "bin type id " << bin_type.id
        << " x " << bin_type.box.x
        << " y " << bin_type.box.y
        << " z " << bin_type.box.z
        << " copies " << bin_type.copies
        ;
    return os;
}

std::ostream& Instance::print(
        std::ostream& os,
        int verbose) const
{
    if (verbose >= 1) {
        os
            << "Objective:                " << objective() << std::endl
            << "Number of item types:     " << number_of_item_types() << std::endl
            << "Number of items:          " << number_of_items() << std::endl
            << "Number of bin types:      " << number_of_bin_types() << std::endl
            << "Number of bins:           " << number_of_bins() << std::endl
            << "Number of groups:         " << number_of_groups() << std::endl
            << "Number of defects:        " << number_of_defects() << std::endl
            << "Unloading constraint:     " << unloading_constraint() << std::endl
            << "Item volume:              " << item_volume() << std::endl
            << "Bin volume:               " << bin_volume() << std::endl
            << "Item weight:              " << item_weight() << std::endl
            << "Bin weight:               " << bin_weight() << std::endl
            ;
    }

    if (verbose >= 2) {
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
            << std::setw(12) << "MaxStckDen"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "----------"
            << std::setw(12) << "------"
            << std::setw(12) << "----------"
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
                << std::setw(12) << bin_type.maximum_stack_density
                << std::endl;
        }

        os
            << std::endl
            << std::setw(12) << "Bin type"
            ;
        SemiTrailerTruckData::print_header_1(os);
        os
            << std::endl
            << std::setw(12) << "--------"
            ;
        SemiTrailerTruckData::print_header_2(os);
        os << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            os
                << std::setw(12) << bin_type_id;
            bin_type.semi_trailer_truck_data.print(os);
        }

        if (number_of_defects() > 0) {
            os
                << std::endl
                << std::setw(12) << "Defect"
                << std::setw(12) << "Bin type"
                << std::setw(12) << "X"
                << std::setw(12) << "Y"
                << std::setw(12) << "LX"
                << std::setw(12) << "LY"
                << std::endl
                << std::setw(12) << "------"
                << std::setw(12) << "--------"
                << std::setw(12) << "-"
                << std::setw(12) << "-"
                << std::setw(12) << "--"
                << std::setw(12) << "--"
                << std::endl;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < number_of_bin_types();
                    ++bin_type_id) {
                const BinType& bin_type = this->bin_type(bin_type_id);
                for (DefectId defect_id = 0;
                        defect_id < (DefectId)bin_type.defects.size();
                        ++defect_id) {
                    const auto& defect = bin_type.defects[defect_id];
                    os
                        << std::setw(12) << defect_id
                        << std::setw(12) << defect.bin_type_id
                        << std::setw(12) << defect.pos.x
                        << std::setw(12) << defect.pos.y
                        << std::setw(12) << defect.rect.x
                        << std::setw(12) << defect.rect.y
                        << std::endl;
                }
            }
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
            << std::setw(10) << "Group id"
            << std::setw(10) << "Weight"
            << std::setw(10) << "Stack id"
            << std::endl
            << std::setw(10) << "---------"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(10) << "-"
            << std::setw(12) << "------"
            << std::setw(10) << "------"
            << std::setw(10) << "---------"
            << std::setw(10) << "--------"
            << std::setw(10) << "------"
            << std::setw(10) << "--------"
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
                << std::setw(10) << item_type.group_id
                << std::setw(10) << item_type.weight
                << std::setw(10) << item_type.stackability_id
                << std::endl;
        }
    }

    return os;
}

void Instance::write(std::string instance_path) const
{
    write_item_types(instance_path + "_items.csv");
    write_bin_types(instance_path + "_bins.csv");
    write_parameters(instance_path + "_parameters.csv");
}

void Instance::write_item_types(std::string items_path) const
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
        "GROUP_ID,"
        "ROTATIONS,"
        "WEIGHT,"
        "STACKABILITY_ID,"
        "NESTING_HEIGHT,"
        "MAXIMUM_STACKABILITY,"
        "MAXIMUM_WEIGHT_ABOVE" << std::endl;
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
            << item_type.group_id << ","
            << item_type.rotations << ","
            << item_type.weight << ","
            << item_type.stackability_id << ","
            << item_type.nesting_height << ","
            << item_type.maximum_stackability << ","
            << item_type.maximum_weight_above << std::endl;
    }
}

void Instance::write_bin_types(std::string bins_path) const
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
        "MAXIMUM_WEIGHT,"
        "MAXIMUM_STACK_DENSITY,";
    SemiTrailerTruckData::write_header(file);
    file << std::endl;
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
            << bin_type.maximum_weight << ","
            << bin_type.maximum_stack_density << ",";
        bin_type.semi_trailer_truck_data.write(file);
        file << std::endl;
    }
}

void Instance::write_parameters(std::string parameters_path) const
{
    std::ofstream file(parameters_path);
    if (!file.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + parameters_path + "\".");
    }
    file
        << "NAME,VALUE" << std::endl
        << "unloading-constraint," << parameters_.unloading_constraint << std::endl
        ;
}
