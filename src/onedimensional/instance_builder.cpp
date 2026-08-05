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
        Length length)
{
    if (length <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'length' must be > 0; "
                "length: " + std::to_string(length) + ".");
    }

    BinType bin_type;
    bin_type.length = length;
    bin_type.cost = length;
    bin_type.copies = 1;
    bin_type.copies_min = 0;
    instance_.bin_types_.push_back(bin_type);
    return instance_.bin_types_.size() - 1;
}

void InstanceBuilder::set_bin_type_cost(
        BinTypeId bin_type_id,
        Profit cost)
{
    if (bin_type_id < 0 || bin_type_id >= (BinTypeId)instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }
    if (cost <= 0 && cost != -1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'cost' must be > 0 (or == -1); "
                "cost: " + std::to_string(cost) + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];
    bin_type.cost = (cost == -1)? bin_type.length: cost;
}

void InstanceBuilder::set_bin_type_maximum_weight(
        BinTypeId bin_type_id,
        Weight maximum_weight)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    instance_.bin_types_[bin_type_id].maximum_weight = maximum_weight;
}

void InstanceBuilder::add_bin_type_eligibility(
        BinTypeId bin_type_id,
        EligibilityId eligibility_id)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    instance_.bin_types_[bin_type_id].eligibility_ids.push_back(eligibility_id);
}

ResourceId InstanceBuilder::add_bin_type_resource(
        BinTypeId bin_type_id,
        double capacity)
{
    if (bin_type_id < 0 || bin_type_id >= (BinTypeId)instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];
    ResourceId resource_id = bin_type.resource_capacities.size();
    bin_type.resource_capacities.push_back(capacity);
    bin_type.item_resource_consumptions.push_back({});
    return resource_id;
}

void InstanceBuilder::add_resource_consumption(
        BinTypeId bin_type_id,
        ResourceId resource_id,
        ItemTypeId item_type_id,
        ItemPos item_copy,
        double consumption)
{
    if (bin_type_id < 0 || bin_type_id >= (BinTypeId)instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];
    if (resource_id < 0 || resource_id >= (ResourceId)bin_type.resource_capacities.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'resource_id'; "
                "resource_id: " + std::to_string(resource_id) + "; "
                "bin_type.resource_capacities.size(): " + std::to_string(bin_type.resource_capacities.size()) + ".");
    }
    if (item_copy < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_copy'; "
                "item_copy: " + std::to_string(item_copy) + ".");
    }

    std::vector<std::vector<double>>& item_consumptions = bin_type.item_resource_consumptions[resource_id];
    if (item_type_id >= (ItemTypeId)item_consumptions.size())
        item_consumptions.resize(item_type_id + 1);
    std::vector<double>& schedule = item_consumptions[item_type_id];
    if (item_copy >= (ItemPos)schedule.size())
        schedule.resize(item_copy + 1, 0.0);
    schedule[item_copy] = consumption;
}

BinTypeId InstanceBuilder::add_bin_type(
        const Instance& original_instance,
        BinTypeId original_bin_type_id)
{
    const BinType& bin_type = original_instance.bin_type(original_bin_type_id);
    BinTypeId bin_type_id = add_bin_type(bin_type.length);
    if ((BinTypeId)orig_to_sub_bin_type_ids_.size() <= original_bin_type_id)
        orig_to_sub_bin_type_ids_.resize(original_bin_type_id + 1, -1);
    orig_to_sub_bin_type_ids_[original_bin_type_id] = bin_type_id;
    set_bin_type_cost(bin_type_id, bin_type.cost);
    set_bin_type_copies(bin_type_id, bin_type.copies);
    set_bin_type_copies_min(bin_type_id, bin_type.copies_min);
    set_bin_type_maximum_weight(
            bin_type_id,
            bin_type.maximum_weight);
    for (EligibilityId eligibility_id: bin_type.eligibility_ids) {
        add_bin_type_eligibility(
                bin_type_id,
                eligibility_id);
    }
    // Copy resources (their consumptions are copied in 'add_item_type',
    // which assumes the corresponding bin types have already been added).
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        add_bin_type_resource(
                bin_type_id,
                bin_type.resource_capacities[resource_id]);
    }
    return bin_type_id;
}

void InstanceBuilder::set_bin_type_copies(
        BinTypeId bin_type_id,
        BinPos copies)
{
    if (bin_type_id < 0 || bin_type_id >= (BinTypeId)instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }
    if (copies != -1) {
        if (copies <= 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin 'copies' must be > 0 (or == -1); "
                    "copies: " + std::to_string(copies) + ".");
        }
        if (instance_.bin_types_[bin_type_id].copies_min > copies) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin 'copies_min' must be <= 'copies'; "
                    "copies: " + std::to_string(copies) + "; "
                    "copies_min: " + std::to_string(instance_.bin_types_[bin_type_id].copies_min) + ".");
        }
    }

    instance_.bin_types_[bin_type_id].copies = copies;
}

void InstanceBuilder::set_bin_type_copies_min(
        BinTypeId bin_type_id,
        BinPos copies_min)
{
    if (bin_type_id < 0 || bin_type_id >= (BinTypeId)instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }
    if (copies_min < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'copies_min' must be >= 0; "
                "copies_min: " + std::to_string(copies_min) + ".");
    }
    if (instance_.bin_types_[bin_type_id].copies != -1
            && copies_min > instance_.bin_types_[bin_type_id].copies) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'copies_min' must be <= 'copies'; "
                "copies: " + std::to_string(instance_.bin_types_[bin_type_id].copies) + "; "
                "copies_min: " + std::to_string(copies_min) + ".");
    }

    instance_.bin_types_[bin_type_id].copies_min = copies_min;
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
        Length length)
{
    ItemType item_type;
    item_type.length = length;
    item_type.profit = length;
    item_type.copies = 1;
    item_type.copies_min = -1;
    instance_.item_types_.push_back(item_type);
    return instance_.item_types_.size() - 1;
}

void InstanceBuilder::set_item_type_weight(
        ItemTypeId item_type_id,
        Weight weight)
{
    if (item_type_id < 0 || item_type_id >= instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].weight = weight;
}

void InstanceBuilder::set_item_type_nesting_length(
        ItemTypeId item_type_id,
        Length nesting_length)
{
    if (item_type_id < 0 || item_type_id >= instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].nesting_length = nesting_length;
}

void InstanceBuilder::set_item_type_maximum_stackability(
        ItemTypeId item_type_id,
        ItemPos maximum_stackability)
{
    if (item_type_id < 0 || item_type_id >= instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].maximum_stackability = maximum_stackability;
}

void InstanceBuilder::set_item_type_maximum_weight_after(
        ItemTypeId item_type_id,
        Weight maximum_weight_after)
{
    if (item_type_id < 0 || item_type_id >= instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].maximum_weight_after = maximum_weight_after;
}

void InstanceBuilder::set_item_type_eligibility(
        ItemTypeId item_type_id,
        EligibilityId eligibility_id)
{
    if (item_type_id < 0 || item_type_id >= instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].eligibility_id = eligibility_id;
}

void InstanceBuilder::add_item_type_precedence(
        ItemTypeId dominated_item_type_id,
        ItemTypeId dominating_item_type_id)
{
    if (dominated_item_type_id < 0 || dominated_item_type_id >= (ItemTypeId)instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'dominated_item_type_id'; "
                "dominated_item_type_id: " + std::to_string(dominated_item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }
    if (dominating_item_type_id < 0 || dominating_item_type_id >= (ItemTypeId)instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'dominating_item_type_id'; "
                "dominating_item_type_id: " + std::to_string(dominating_item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    PrecedenceId precedence_id = instance_.precedences_.size();
    instance_.precedences_.push_back({dominated_item_type_id, dominating_item_type_id});
    instance_.item_types_[dominated_item_type_id].dominated_precedence_ids.push_back(precedence_id);
    instance_.item_types_[dominating_item_type_id].dominating_precedence_ids.push_back(precedence_id);
}

ItemTypeId InstanceBuilder::add_item_type(
        const Instance& original_instance,
        ItemTypeId original_item_type_id)
{
    const ItemType& item_type = original_instance.item_type(original_item_type_id);
    ItemTypeId item_type_id = add_item_type(item_type.length);
    if ((ItemTypeId)orig_to_sub_item_type_ids_.size() <= original_item_type_id)
        orig_to_sub_item_type_ids_.resize(original_item_type_id + 1, -1);
    orig_to_sub_item_type_ids_[original_item_type_id] = item_type_id;
    set_item_type_profit(item_type_id, item_type.profit);
    set_item_type_copies(item_type_id, item_type.copies);
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
    // Copy the consumption of this item type for the resources of
    // already-added bin types. Assumes the relevant bin types have already
    // been added via 'add_bin_type(original_instance, ...)'.
    for (BinTypeId original_bin_type_id = 0;
            original_bin_type_id < (BinTypeId)orig_to_sub_bin_type_ids_.size();
            ++original_bin_type_id) {
        BinTypeId sub_bin_type_id = orig_to_sub_bin_type_ids_[original_bin_type_id];
        if (sub_bin_type_id == -1)
            continue;
        const BinType& original_bin_type = original_instance.bin_type(original_bin_type_id);
        for (ResourceId resource_id = 0;
                resource_id < original_bin_type.number_of_resources();
                ++resource_id) {
            const std::vector<std::vector<double>>& item_consumptions
                = original_bin_type.item_resource_consumptions[resource_id];
            if (original_item_type_id >= (ItemTypeId)item_consumptions.size())
                continue;
            const std::vector<double>& schedule = item_consumptions[original_item_type_id];
            for (ItemPos item_copy = 0;
                    item_copy < (ItemPos)schedule.size();
                    ++item_copy) {
                add_resource_consumption(
                        sub_bin_type_id,
                        resource_id,
                        item_type_id,
                        item_copy,
                        schedule[item_copy]);
            }
        }
    }
    // Record this item type's side of every precedence it is involved in
    // (as dominated or dominating), keyed by the *original* precedence id;
    // 'build()' finalizes every precedence whose other side also ends up
    // recorded - see 'pending_precedences_by_original_id_''s doc comment.
    for (PrecedenceId original_precedence_id: item_type.dominated_precedence_ids) {
        if ((PrecedenceId)pending_precedences_by_original_id_.size() <= original_precedence_id)
            pending_precedences_by_original_id_.resize(original_precedence_id + 1, {-1, -1});
        pending_precedences_by_original_id_[original_precedence_id].first = item_type_id;
    }
    for (PrecedenceId original_precedence_id: item_type.dominating_precedence_ids) {
        if ((PrecedenceId)pending_precedences_by_original_id_.size() <= original_precedence_id)
            pending_precedences_by_original_id_.resize(original_precedence_id + 1, {-1, -1});
        pending_precedences_by_original_id_[original_precedence_id].second = item_type_id;
    }
    return item_type_id;
}

void InstanceBuilder::set_item_type_profit(
        ItemTypeId item_type_id,
        Profit profit)
{
    if (item_type_id < 0 || item_type_id >= (ItemTypeId)instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }

    instance_.item_types_[item_type_id].profit = profit;
}

void InstanceBuilder::set_item_type_copies(
        ItemTypeId item_type_id,
        ItemPos copies)
{
    if (item_type_id < 0 || item_type_id >= (ItemTypeId)instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }
    if (copies != -1 && copies <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'copies' must be > 0 (or == -1); "
                "copies: " + std::to_string(copies) + ".");
    }

    instance_.item_types_[item_type_id].copies = copies;
}

void InstanceBuilder::set_item_type_copies_min(
        ItemTypeId item_type_id,
        ItemPos copies_min)
{
    if (item_type_id < 0 || item_type_id >= (ItemTypeId)instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + "; "
                "instance_.item_types_.size(): " + std::to_string(instance_.item_types_.size()) + ".");
    }
    if (copies_min < -1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'copies_min' must be >= -1; "
                "copies_min: " + std::to_string(copies_min) + ".");
    }

    instance_.item_types_[item_type_id].copies_min = copies_min;
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
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + parameters_path + "\".");
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
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + bins_path + "\".");
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
                cost = (Profit)std::stod(line[i]);
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
                    FUNC_SIGNATURE + ": "
                    "missing \"X\" column in \"" + bins_path + "\".");
        }

        BinTypeId bin_type_id = add_bin_type(x);
        set_bin_type_cost(bin_type_id, cost);
        set_bin_type_copies(bin_type_id, copies);
        set_bin_type_copies_min(bin_type_id, copies_min);
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
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + items_path + "\".");
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
        ItemPos copies_min = -1;
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
            } else if (labels[i] == "COPIES_MIN") {
                copies_min = (ItemPos)std::stol(line[i]);
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
                    FUNC_SIGNATURE + ": "
                    "missing \"X\" column in \"" + items_path + "\".");
        }

        if (profit == -1)
            profit = x;

        ItemTypeId item_type_id = add_item_type(x);
        set_item_type_profit(item_type_id, profit);
        set_item_type_copies(item_type_id, copies);
        set_item_type_copies_min(item_type_id, copies_min);
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

void InstanceBuilder::read(
        const std::string& instance_path)
{
    std::ifstream file(instance_path);
    if (!file.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + instance_path + "\".");
    }

    nlohmann::json j;
    file >> j;

    if (!j.contains("objective")) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "missing \"objective\" field.");
    }
    {
        std::string objective_string = j["objective"];
        std::stringstream objective_ss(objective_string);
        Objective objective;
        objective_ss >> objective;
        if (objective_ss.fail()) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "unrecognized \"objective\" value \""
                    + objective_string + "\".");
        }
        set_objective(objective);
    }

    // Read bin types.
    for (const auto& json_bin_type: j["bin_types"]) {
        Length length = json_bin_type["length"];
        BinTypeId bin_type_id = add_bin_type(length);
        if (json_bin_type.contains("cost"))
            set_bin_type_cost(bin_type_id, json_bin_type["cost"]);
        if (json_bin_type.contains("copies"))
            set_bin_type_copies(bin_type_id, json_bin_type["copies"]);
        if (json_bin_type.contains("copies_min"))
            set_bin_type_copies_min(bin_type_id, json_bin_type["copies_min"]);
        if (json_bin_type.contains("maximum_weight"))
            set_bin_type_maximum_weight(bin_type_id, json_bin_type["maximum_weight"]);
        if (json_bin_type.contains("eligibility_ids")) {
            for (const auto& json_eligibility_id: json_bin_type["eligibility_ids"])
                add_bin_type_eligibility(bin_type_id, json_eligibility_id);
        }

        // Read resources.
        if (json_bin_type.contains("resources")) {
            for (const auto& json_resource: json_bin_type["resources"]) {
                double capacity = json_resource["capacity"];
                ResourceId resource_id = add_bin_type_resource(bin_type_id, capacity);
                if (json_resource.contains("consumptions")) {
                    for (const auto& json_consumption: json_resource["consumptions"]) {
                        ItemTypeId item_type_id = json_consumption["item_type_id"];
                        if (json_consumption.contains("consumption_schedule")) {
                            // Per-copy consumption schedule (a copy past
                            // the end of the schedule repeats its last
                            // entry); see 'BinType::item_resource_consumptions'.
                            std::vector<double> schedule = json_consumption["consumption_schedule"];
                            for (ItemPos item_copy = 0;
                                    item_copy < (ItemPos)schedule.size();
                                    ++item_copy) {
                                add_resource_consumption(
                                        bin_type_id,
                                        resource_id,
                                        item_type_id,
                                        item_copy,
                                        schedule[item_copy]);
                            }
                        } else {
                            // Uniform consumption, regardless of how many
                            // copies are already packed.
                            double consumption = json_consumption["consumption"];
                            add_resource_consumption(
                                    bin_type_id,
                                    resource_id,
                                    item_type_id,
                                    0,
                                    consumption);
                        }
                    }
                }
            }
        }
    }

    // Read item types.
    for (const auto& json_item_type: j["item_types"]) {
        Length length = json_item_type["length"];
        ItemTypeId item_type_id = add_item_type(length);
        if (json_item_type.contains("profit"))
            set_item_type_profit(item_type_id, json_item_type["profit"]);
        if (json_item_type.contains("weight"))
            set_item_type_weight(item_type_id, json_item_type["weight"]);
        if (json_item_type.contains("copies"))
            set_item_type_copies(item_type_id, json_item_type["copies"]);
        if (json_item_type.contains("copies_min"))
            set_item_type_copies_min(item_type_id, json_item_type["copies_min"]);
        if (json_item_type.contains("nesting_length"))
            set_item_type_nesting_length(item_type_id, json_item_type["nesting_length"]);
        if (json_item_type.contains("maximum_stackability"))
            set_item_type_maximum_stackability(item_type_id, json_item_type["maximum_stackability"]);
        if (json_item_type.contains("maximum_weight_after"))
            set_item_type_maximum_weight_after(item_type_id, json_item_type["maximum_weight_after"]);
        if (json_item_type.contains("eligibility_id"))
            set_item_type_eligibility(item_type_id, json_item_type["eligibility_id"]);
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Instance InstanceBuilder::build()
{
    // Finalize item type precedences pending from 'add_item_type(original_
    // instance, ...)' calls, in a single pass now that every item type has
    // been added: only precedences whose dominated *and* dominating item
    // type were both copied into this sub-instance are kept (one whose
    // other item type was never added is simply dropped, since it would
    // otherwise reference an item type that doesn't exist here).
    for (const std::pair<ItemTypeId, ItemTypeId>& pending: pending_precedences_by_original_id_) {
        ItemTypeId dominated_item_type_id = pending.first;
        ItemTypeId dominating_item_type_id = pending.second;
        if (dominated_item_type_id == -1 || dominating_item_type_id == -1)
            continue;
        add_item_type_precedence(dominated_item_type_id, dominating_item_type_id);
    }

    // Compute item type attributes.
    Length bin_types_length_max = compute_bin_types_length_max();
    instance_.all_item_types_infinite_copies_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        // Resolve copies_min.
        if (item_type.copies_min == -1) {
            item_type.copies_min = (instance_.objective() == Objective::Knapsack)?
                0: item_type.copies;
        }
        if (item_type.copies_min > item_type.copies) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "item type " + std::to_string(item_type_id) + " has "
                    "'copies_min' (" + std::to_string(item_type.copies_min) + ") "
                    "> 'copies' (" + std::to_string(item_type.copies) + ").");
        }
        // Update number_of_items_.
        instance_.number_of_items_ += item_type.copies;
        // Update item_profit_.
        instance_.item_profit_ += item_type.copies * item_type.profit;
        // Update largest_item_profit_.
        instance_.largest_item_profit_ = std::max(instance_.largest_item_profit(), item_type.profit);
        // Update item_length_.
        instance_.item_length_ += item_type.copies * item_type.length;
        // Update largest_efficiency_item_type_.
        if (instance_.largest_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.largest_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.largest_efficiency_item_type_id_).length
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).length) {
            instance_.largest_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_length_max - 1) / item_type.length + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update largest_item_copies_.
        if (instance_.largest_item_copies_ < item_type.copies)
            instance_.largest_item_copies_ = item_type.copies;
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
        // Update largest_bin_cost_.
        if (instance_.largest_bin_cost_ < bin_type.cost)
            instance_.largest_bin_cost_ = bin_type.cost;
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

    // Check and compute copies_fixed for each item type.
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_types_[bin_type_id];
        if (!bin_type.fixed_items.empty()
                && bin_type.copies_min != bin_type.copies) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin type " + std::to_string(bin_type_id) + " has fixed items "
                    "but copies_min (" + std::to_string(bin_type.copies_min) + ") "
                    "!= copies (" + std::to_string(bin_type.copies) + "); "
                    "a bin type with fixed items must be mandatory (copies_min == copies).");
        }
        for (const FixedItem& fixed_item: bin_type.fixed_items)
            instance_.item_types_[fixed_item.item_type_id].copies_fixed += bin_type.copies;
    }

    return std::move(instance_);
}
