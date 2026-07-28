#include "packingsolver/onedimensional/instance.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace packingsolver;
using namespace packingsolver::onedimensional;

std::ostream& packingsolver::onedimensional::operator<<(
        std::ostream& os,
        const ItemType& item_type)
{
    os
        << "length " << item_type.length
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
        << "length " << bin_type.length
        << " weight " << bin_type.maximum_weight
        << " copies " << bin_type.copies
        ;
    return os;
}

bool Instance::fits_some_bin(
        ItemTypeId item_type_id) const
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        if (item_type_fits_bin_type(item_type_id, bin_type_id))
            return true;
    }
    return false;
}

bool Instance::item_type_fits_bin_type(
        ItemTypeId item_type_id,
        BinTypeId bin_type_id) const
{
    const ItemType& item_type = this->item_type(item_type_id);
    const BinType& bin_type = this->bin_type(bin_type_id);
    if (item_type.length > bin_type.length)
        return false;
    if (item_type.eligibility_id != -1
            && std::find(
                bin_type.eligibility_ids.begin(),
                bin_type.eligibility_ids.end(),
                item_type.eligibility_id)
            == bin_type.eligibility_ids.end()) {
        return false;
    }
    return true;
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
            << "Total item length:     " << item_length() << std::endl
            << "Total item profit:     " << item_profit() << std::endl
            << "Largest item profit:   " << largest_item_profit() << std::endl
            << "Largest item copies:   " << largest_item_copies() << std::endl
            << "Largest bin cost:      " << largest_bin_cost() << std::endl
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
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        if (this->item_type(item_type_id).eligibility_id != -1) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "item type " + std::to_string(item_type_id) + " has an "
                    "eligibility id, which the CSV format cannot represent; "
                    "use 'InstanceFormat::Json' instead.");
        }
    }
    if (!precedences().empty()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "the instance has item type precedences, which the CSV "
                "format cannot represent; use 'InstanceFormat::Json' "
                "instead.");
    }
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
        if (!bin_type.eligibility_ids.empty()) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin type " + std::to_string(bin_type_id) + " has "
                    "eligibility ids, which the CSV format cannot "
                    "represent; use 'InstanceFormat::Json' instead.");
        }
        if (!bin_type.fixed_items.empty()) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin type " + std::to_string(bin_type_id) + " has fixed "
                    "items, which the CSV format cannot represent; use "
                    "'InstanceFormat::Json' instead.");
        }
    }

    std::string items_path = instance_path + "_items.csv";
    std::string bins_path = instance_path + "_bins.csv";
    std::string parameters_path = instance_path + "_parameters.csv";
    std::ofstream f_items(items_path);
    std::ofstream f_bins(bins_path);
    std::ofstream f_parameters(parameters_path);
    if (!f_items.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + items_path + "\".");
    }
    if (!f_bins.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + bins_path + "\".");
    }
    if (!f_parameters.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + parameters_path + "\".");
    }

    // Export items.
    f_items <<
        "ID,"
        "X,"
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
        "X,"
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

void Instance::write_json(
        const std::string& instance_path) const
{
    std::ofstream file(instance_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + instance_path + "\".");
    }

    nlohmann::json json;

    std::stringstream objective_ss;
    objective_ss << objective();
    json["objective"] = objective_ss.str();

    json["bin_types"] = nlohmann::json::array();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        nlohmann::json json_bin_type;
        json_bin_type["length"] = bin_type.length;
        json_bin_type["cost"] = bin_type.cost;
        json_bin_type["copies"] = bin_type.copies;
        json_bin_type["copies_min"] = bin_type.copies_min;
        if (bin_type.maximum_weight != std::numeric_limits<Weight>::infinity())
            json_bin_type["maximum_weight"] = bin_type.maximum_weight;
        if (!bin_type.eligibility_ids.empty())
            json_bin_type["eligibility_ids"] = bin_type.eligibility_ids;
        if (bin_type.number_of_resources() > 0) {
            nlohmann::json json_resources = nlohmann::json::array();
            for (ResourceId resource_id = 0;
                    resource_id < bin_type.number_of_resources();
                    ++resource_id) {
                nlohmann::json json_resource;
                json_resource["capacity"] = bin_type.resource_capacities[resource_id];
                nlohmann::json json_consumptions = nlohmann::json::array();
                const std::vector<std::vector<double>>& consumptions
                    = bin_type.item_resource_consumptions[resource_id];
                for (ItemTypeId item_type_id = 0;
                        item_type_id < (ItemTypeId)consumptions.size();
                        ++item_type_id) {
                    const std::vector<double>& schedule = consumptions[item_type_id];
                    if (schedule.empty())
                        continue;
                    nlohmann::json json_consumption;
                    json_consumption["item_type_id"] = item_type_id;
                    if (schedule.size() == 1) {
                        json_consumption["consumption"] = schedule[0];
                    } else {
                        json_consumption["consumption_schedule"] = schedule;
                    }
                    json_consumptions.push_back(json_consumption);
                }
                json_resource["consumptions"] = json_consumptions;
                json_resources.push_back(json_resource);
            }
            json_bin_type["resources"] = json_resources;
        }
        json["bin_types"].push_back(json_bin_type);
    }

    json["item_types"] = nlohmann::json::array();
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        nlohmann::json json_item_type;
        json_item_type["length"] = item_type.length;
        json_item_type["profit"] = item_type.profit;
        json_item_type["weight"] = item_type.weight;
        json_item_type["copies"] = item_type.copies;
        json_item_type["nesting_length"] = item_type.nesting_length;
        if (item_type.maximum_stackability != std::numeric_limits<ItemPos>::max())
            json_item_type["maximum_stackability"] = item_type.maximum_stackability;
        if (item_type.maximum_weight_after != std::numeric_limits<Weight>::infinity())
            json_item_type["maximum_weight_after"] = item_type.maximum_weight_after;
        if (item_type.eligibility_id != -1)
            json_item_type["eligibility_id"] = item_type.eligibility_id;
        json["item_types"].push_back(json_item_type);
    }

    if (!precedences().empty()) {
        json["precedences"] = nlohmann::json::array();
        for (const Precedence& precedence: precedences()) {
            nlohmann::json json_precedence;
            json_precedence["dominated_item_type_id"] = precedence.dominated_item_type_id;
            json_precedence["dominating_item_type_id"] = precedence.dominating_item_type_id;
            json["precedences"].push_back(json_precedence);
        }
    }

    file << std::setw(4) << json << std::endl;
}
