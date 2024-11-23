#include "packingsolver/onedimensional/instance.hpp"

#include <iostream>
#include <iomanip>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const ItemType& item_type)
{
    os
        << "item type id " << item_type.id
        << " length " << item_type.length
        << " weight " << item_type.weight
        << " profit " << item_type.profit
        << " copies " << item_type.copies
        ;
    return os;
}

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const BinType& bin_type)
{
    os
        << "bin type id " << bin_type.id
        << " length " << bin_type.length
        << " weight " << bin_type.maximum_weight
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
            << "Maximum item copies:   " << maximum_item_copies() << std::endl
            << "Maximum bin cost:      " << maximum_bin_cost() << std::endl
            ;
    }

    if (verbosity_level >= 2) {
        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Length"
            << std::setw(12) << "Max wght"
            << std::setw(12) << "Cost"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Copies min"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "------"
            << std::setw(12) << "--------"
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
                << std::setw(12) << bin_type.length
                << std::setw(12) << bin_type.maximum_weight
                << std::setw(12) << bin_type.cost
                << std::setw(12) << bin_type.copies
                << std::setw(12) << bin_type.copies_min
                << std::endl;
        }

        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Eligibility"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "-----------"
            << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            for (EligibilityId eligibility_id: bin_type.eligibility_ids) {
                os
                    << std::setw(12) << bin_type_id
                    << std::setw(12) << eligibility_id
                    << std::endl;
            }
        }

        os
            << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "Length"
            << std::setw(12) << "Weight"
            << std::setw(12) << "MaxWgtAft"
            << std::setw(12) << "MaxStck"
            << std::setw(12) << "Profit"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Eligibility"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "---------"
            << std::setw(12) << "-------"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "-----------"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            os
                << std::setw(12) << item_type_id
                << std::setw(12) << item_type.length
                << std::setw(12) << item_type.weight
                << std::setw(12) << item_type.maximum_weight_after
                << std::setw(12) << item_type.maximum_stackability
                << std::setw(12) << item_type.profit
                << std::setw(12) << item_type.copies
                << std::setw(12) << item_type.eligibility_id
                << std::endl;
        }
    }

    return os;
}

void Instance::write(
        const std::string& instance_path) const
{
    std::string items_path = instance_path + "_items.csv";
    std::string bins_path = instance_path + "_bins.csv";
    std::string parameters_path = instance_path + "_parameters.csv";
    std::ofstream f_items(items_path);
    std::ofstream f_bins(bins_path);
    std::ofstream f_parameters(parameters_path);
    if (!f_items.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + items_path + "\".");
    }
    if (!f_bins.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + bins_path + "\".");
    }
    if (!f_parameters.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + parameters_path + "\".");
    }

    // Export items.
    f_items <<
        "ID,"
        "LENGTH,"
        "PROFIT,"
        "COPIES,"
        "WEIGHT,"
        "NESTING_LENGTH,"
        "MAXIMUM_STACKABILITY,"
        "MAXIMUM_WEIGHT_AFTER" << std::endl;
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        f_items
            << item_type_id << ","
            << item_type.length << ","
            << item_type.profit << ","
            << item_type.copies << ","
            << item_type.weight << ","
            << item_type.nesting_length << ","
            << item_type.maximum_stackability << ","
            << item_type.maximum_weight_after << std::endl;
    }

    // Export bins.
    f_bins <<
        "ID,"
        "LENGTH,"
        "COST,"
        "COPIES,"
        "COPIES_MIN,"
        "MAXIMUM_WEIGHT" << std::endl;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        f_bins
            << bin_type_id << ","
            << bin_type.length << ","
            << bin_type.cost << ","
            << bin_type.copies << ","
            << bin_type.copies_min << ","
            << bin_type.maximum_weight << std::endl;
    }

    // Export parameters.
    f_parameters << "NAME,VALUE" << std::endl
        << "objective," << objective() << std::endl;
}
