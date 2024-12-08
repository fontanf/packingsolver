#include "packingsolver/onedimensional/instance_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::onedimensional;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Parameters //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Bin types ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinTypeId InstanceBuilder::add_bin_type(
        Length length,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (length <= 0) {
        throw std::runtime_error(
                "'onedimensional::InstanceBuilder::add_bin_type'"
                " requires 'length > 0'.");
    }
    if (cost < 0 && cost != -1) {
        throw std::runtime_error(
                "'onedimensional::InstanceBuilder::add_bin_type'"
                " requires 'cost >= 0' or 'cost == -1'.");
    }
    if (copies_min < 0) {
        throw std::runtime_error(
                "'onedimensional::InstanceBuilder::add_bin_type'"
                " requires 'copies_min >= 0'.");
    }
    if (copies != -1) {
        if (copies <= 0) {
            throw std::runtime_error(
                    "'onedimensional::InstanceBuilder::add_bin_type'"
                    " requires 'copies > 0' or 'copies == -1'.");
        }
        if (copies_min > copies) {
            throw std::runtime_error(
                    "'onedimensional::InstanceBuilder::add_bin_type'"
                    " requires 'copies_min <= copies' or 'copies == -1'.");
        }
    }

    BinType bin_type;
    bin_type.id = instance_.bin_types_.size();
    bin_type.length = length;
    bin_type.cost = (cost == -1)? length: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return bin_type.id;
}

void InstanceBuilder::set_bin_type_maximum_weight(
        BinTypeId bin_type_id,
        Weight maximum_weight)
{
    instance_.bin_types_[bin_type_id].maximum_weight = maximum_weight;
}

void InstanceBuilder::add_bin_type_eligibility(
        BinTypeId bin_type_id,
        EligibilityId eligibility_id)
{
    instance_.bin_types_[bin_type_id].eligibility_ids.push_back(eligibility_id);
}

void InstanceBuilder::add_bin_type(
        const BinType& bin_type,
        BinPos copies)
{
    BinTypeId bin_type_id = add_bin_type(
            bin_type.length,
            bin_type.cost,
            copies);
    set_bin_type_maximum_weight(
            bin_type_id,
            bin_type.maximum_weight);
    for (EligibilityId eligibility_id: bin_type.eligibility_ids) {
        add_bin_type_eligibility(
                bin_type_id,
                eligibility_id);
    }
}

void InstanceBuilder::set_bin_types_infinite_copies()
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].copies = -1;
    }
}

void InstanceBuilder::set_bin_types_unweighted()
{
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].cost = instance_.bin_types_[bin_type_id].length;
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Item types //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ItemTypeId InstanceBuilder::add_item_type(
        Length length,
        Profit profit,
        ItemPos copies)
{
    if (copies <= 0) {
        throw std::runtime_error(
                "'onedimensional::InstanceBuilder::add_item_type'"
                " requires 'copies > 0'.");
    }

    ItemType item_type;
    item_type.id = instance_.item_types_.size();
    item_type.length = length;
    item_type.profit = (profit == -1)? length: profit;
    item_type.copies = copies;
    instance_.item_types_.push_back(item_type);
    return item_type.id;
}

void InstanceBuilder::set_item_type_weight(
        ItemTypeId item_type_id,
        Weight weight)
{
    instance_.item_types_[item_type_id].weight = weight;
}

void InstanceBuilder::set_item_type_nesting_length(
        ItemTypeId item_type_id,
        Length nesting_length)
{
    instance_.item_types_[item_type_id].nesting_length = nesting_length;
}

void InstanceBuilder::set_item_type_maximum_stackability(
        ItemTypeId item_type_id,
        ItemPos maximum_stackability)
{
    instance_.item_types_[item_type_id].maximum_stackability = maximum_stackability;
}

void InstanceBuilder::set_item_type_maximum_weight_after(
        ItemTypeId item_type_id,
        Weight maximum_weight_after)
{
    instance_.item_types_[item_type_id].maximum_weight_after = maximum_weight_after;
}

void InstanceBuilder::set_item_type_eligibility(
        ItemTypeId item_type_id,
        EligibilityId eligibility_id)
{
    instance_.item_types_[item_type_id].eligibility_id = eligibility_id;
}

void InstanceBuilder::add_item_type(
        const ItemType& item_type,
        Profit profit,
        ItemPos copies)
{
    ItemTypeId item_type_id = add_item_type(
            item_type.length,
            profit,
            copies);
    set_item_type_weight(
            item_type_id,
            item_type.weight);
    set_item_type_nesting_length(
            item_type_id,
            item_type.nesting_length);
    set_item_type_maximum_stackability(
            item_type_id,
            item_type.maximum_stackability);
    set_item_type_maximum_weight_after(
            item_type_id,
            item_type.maximum_weight_after);
    set_item_type_eligibility(
            item_type_id,
            item_type.eligibility_id);
}

void InstanceBuilder::set_item_types_unweighted()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].profit = instance_.item_types_[item_type_id].length;
    }
}

Length InstanceBuilder::compute_bin_types_length_max() const
{
    Length bin_types_length_max = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        bin_types_length_max = std::max(
                bin_types_length_max,
                instance_.bin_type(bin_type_id).length);
    }
    return bin_types_length_max;
}

void InstanceBuilder::set_item_types_infinite_copies()
{
    Length bin_types_length_max = compute_bin_types_length_max();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        ItemPos c = (bin_types_length_max - 1) / item_type.length + 1;
        item_type.copies = c;
    }
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Read from files ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void InstanceBuilder::read_parameters(
        const std::string& parameters_path)
{
    if (parameters_path.empty())
        return;

    std::ifstream f(parameters_path);
    if (parameters_path != "" && !f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + parameters_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');
        std::string name;
        std::string value;
        for (Counter c = 0; c < (Counter)line.size(); ++c) {
            if (labels[c] == "NAME") {
                name = line[c];
            } else if (labels[c] == "VALUE") {
                value = line[c];
            }
        }

        if (name == "objective") {
            Objective objective;
            std::stringstream ss(value);
            ss >> objective;
            set_objective(objective);
        }
    }
}

void InstanceBuilder::read_bin_types(
        const std::string& bins_path)
{
    std::ifstream f(bins_path);
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + bins_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    // read bin file
    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        Length x = -1;
        Profit cost = -1;
        BinPos copies = 1;
        BinPos copies_min = 0;
        Weight maximum_weight = std::numeric_limits<Weight>::infinity();

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "COST") {
                cost = (Profit)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "COPIES_MIN") {
                copies_min = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "MAXIMUM_WEIGHT") {
                maximum_weight = (Weight)std::stod(line[i]);
            }
        }
        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"X\" column in \"" + bins_path + "\".");
        }

        BinTypeId bin_type_id = add_bin_type(
                x,
                cost,
                copies,
                copies_min);
        set_bin_type_maximum_weight(
                bin_type_id,
                maximum_weight);
    }
}

void InstanceBuilder::read_item_types(
        const std::string& items_path)
{
    std::ifstream f(items_path);
    if (!f.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + items_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        Length x = -1;
        Profit profit = -1;
        Weight weight = 0;
        ItemPos copies = 1;
        Length nesting_length = 0;
        ItemPos maximum_stackability = std::numeric_limits<ItemPos>::max();
        Weight maximum_weight_after = std::numeric_limits<Weight>::infinity();

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "PROFIT") {
                profit = (Profit)std::stod(line[i]);
            } else if (labels[i] == "WEIGHT") {
                weight = (Weight)std::stod(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "NESTING_LENGTH") {
                nesting_length = (Length)std::stol(line[i]);
            } else if (labels[i] == "MAXIMUM_STACKABILITY") {
                maximum_stackability = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "MAXIMUM_WEIGHT_AFTER") {
                maximum_weight_after = (Weight)std::stod(line[i]);
            }
        }

        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"X\" column in \"" + items_path + "\".");
        }

        if (profit == -1)
            profit = x;

        ItemTypeId item_type_id = add_item_type(
                x,
                profit,
                copies);
        set_item_type_weight(
                item_type_id,
                weight);
        set_item_type_nesting_length(
                item_type_id,
                nesting_length);
        set_item_type_maximum_stackability(
                item_type_id,
                maximum_stackability);
        set_item_type_maximum_weight_after(
                item_type_id,
                maximum_weight_after);
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Instance InstanceBuilder::build()
{
    // Compute item type attributes.
    Length bin_types_length_max = compute_bin_types_length_max();
    instance_.all_item_types_infinite_copies_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        // Update number_of_items_.
        instance_.number_of_items_ += item_type.copies;
        // Update item_profit_.
        instance_.item_profit_ += item_type.copies * item_type.profit;
        // Update item_length_.
        instance_.item_length_ += item_type.copies * item_type.length;
        // Update max_efficiency_item_type_.
        if (instance_.max_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.max_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.max_efficiency_item_type_id_).length
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).length) {
            instance_.max_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_length_max - 1) / item_type.length + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update maximum_item_copies_.
        if (instance_.maximum_item_copies_ < item_type.copies)
            instance_.maximum_item_copies_ = item_type.copies;
    }

    // Compute bin type attributes.
    instance_.bin_length_ = 0;
    Length previous_bins_length = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bin_type.copies.
        if (bin_type.copies == -1)
            instance_.bin_types_[bin_type_id].copies = instance_.number_of_items();
        // Update bins_length_sum_.
        instance_.bin_length_ += bin_type.copies * bin_type.length;
        // Update previous_bins_length_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_length_.push_back(previous_bins_length);
            previous_bins_length += bin_type.length;
        }
        // Update maximum_bin_cost_.
        if (instance_.maximum_bin_cost_ < bin_type.cost)
            instance_.maximum_bin_cost_ = bin_type.cost;
    }

    // Compute bin_type.item_type_ids_.
    for (BinTypeId bin_type_id = 0; bin_type_id < instance_.number_of_bin_types(); ++bin_type_id) {
        BinType& bin_type = instance_.bin_types_[bin_type_id];
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            if (item_type.eligibility_id != -1
                    && std::find(
                        bin_type.eligibility_ids.begin(),
                        bin_type.eligibility_ids.end(),
                        item_type.eligibility_id)
                    == bin_type.eligibility_ids.end()) {
                continue;
            }
            bin_type.item_type_ids.push_back(item_type_id);
        }
    }

    return std::move(instance_);
}
