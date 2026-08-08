#include "packingsolver/box/instance_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::box;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Parameters //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Bin types ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinTypeId InstanceBuilder::add_bin_type(
        Length x,
        Length y,
        Length z)
{
    if (x <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'x' must be > 0; "
                "x: " + std::to_string(x) + ".");
    }
    if (y <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'y' must be > 0; "
                "y: " + std::to_string(y) + ".");
    }
    if (z <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'z' must be > 0; "
                "z: " + std::to_string(z) + ".");
    }

    BinType bin_type;
    bin_type.box.x = x;
    bin_type.box.y = y;
    bin_type.box.z = z;
    bin_type.cost = x * y;
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
    bin_type.cost = (cost == -1)? bin_type.box.x * bin_type.box.y: cost;
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

ResourceId InstanceBuilder::add_bin_type_resource(
        BinTypeId bin_type_id,
        double capacity,
        bool penalize,
        double penalty)
{
    if (bin_type_id < 0 || bin_type_id >= (BinTypeId)instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];
    ResourceId resource_id = bin_type.resources.size();
    Resource resource;
    resource.capacity = capacity;
    resource.penalize = penalize;
    resource.penalty = penalty;
    bin_type.resources.push_back(resource);
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
    if (resource_id < 0 || resource_id >= (ResourceId)bin_type.resources.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'resource_id'; "
                "resource_id: " + std::to_string(resource_id) + "; "
                "bin_type.resources.size(): " + std::to_string(bin_type.resources.size()) + ".");
    }
    if (item_copy < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_copy'; "
                "item_copy: " + std::to_string(item_copy) + ".");
    }

    std::vector<std::vector<double>>& item_consumptions = bin_type.resources[resource_id].item_consumptions;
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
    BinTypeId bin_type_id = add_bin_type(
            bin_type.box.x,
            bin_type.box.y,
            bin_type.box.z);
    if ((BinTypeId)orig_to_sub_bin_type_ids_.size() <= original_bin_type_id)
        orig_to_sub_bin_type_ids_.resize(original_bin_type_id + 1, -1);
    orig_to_sub_bin_type_ids_[original_bin_type_id] = bin_type_id;
    set_bin_type_cost(bin_type_id, bin_type.cost);
    set_bin_type_copies(bin_type_id, bin_type.copies);
    set_bin_type_copies_min(bin_type_id, bin_type.copies_min);
    set_bin_type_maximum_weight(
            bin_type_id,
            bin_type.maximum_weight);
    // Copy resources (their consumptions are copied in 'add_item_type',
    // which assumes the corresponding bin types have already been added).
    for (ResourceId resource_id = 0;
            resource_id < bin_type.number_of_resources();
            ++resource_id) {
        const Resource& resource = bin_type.resource(resource_id);
        add_bin_type_resource(
                bin_type_id,
                resource.capacity,
                resource.penalize,
                resource.penalty);
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

Length InstanceBuilder::compute_item_types_max_length_sum() const
{
    Length length = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        length += item_type.copies * item_type.box.max();
    }
    return length;
}

void InstanceBuilder::set_bin_types_infinite_x()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].box.x = length;
    }
}

void InstanceBuilder::set_bin_types_infinite_y()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].box.y = length;
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
        instance_.bin_types_[bin_type_id].cost = instance_.bin_types_[bin_type_id].volume();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Item types //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ItemTypeId InstanceBuilder::add_item_type(
        Length x,
        Length y,
        Length z)
{
    if (x < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'x' must be > 0; "
                "x: " + std::to_string(x) + ".");
    }
    if (y < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'y' must be > 0; "
                "y: " + std::to_string(y) + ".");
    }
    if (z < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'z' must be > 0; "
                "z: " + std::to_string(z) + ".");
    }

    ItemType item_type;
    item_type.box.x = x;
    item_type.box.y = y;
    item_type.box.z = z;
    item_type.profit = x * y * z;
    item_type.copies = 1;
    item_type.copies_min = -1;
    instance_.item_types_.push_back(item_type);
    return instance_.item_types_.size() - 1;
}

void InstanceBuilder::add_item_type_rotation(
        ItemTypeId item_type_id,
        Rotation rotation)
{
    if (item_type_id < 0 || item_type_id >= (ItemTypeId)instance_.item_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'item_type_id'; "
                "item_type_id: " + std::to_string(item_type_id) + ".");
    }
    instance_.item_types_[item_type_id].rotations.push_back(rotation);
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

ItemTypeId InstanceBuilder::add_item_type(
        const Instance& original_instance,
        ItemTypeId original_item_type_id)
{
    const ItemType& item_type = original_instance.item_type(original_item_type_id);
    ItemTypeId item_type_id = add_item_type(
            item_type.box.x,
            item_type.box.y,
            item_type.box.z);
    for (Rotation rotation: item_type.rotations)
        add_item_type_rotation(item_type_id, rotation);
    if ((ItemTypeId)orig_to_sub_item_type_ids_.size() <= original_item_type_id)
        orig_to_sub_item_type_ids_.resize(original_item_type_id + 1, -1);
    orig_to_sub_item_type_ids_[original_item_type_id] = item_type_id;
    set_item_type_profit(item_type_id, item_type.profit);
    set_item_type_copies(item_type_id, item_type.copies);
    set_item_type_weight(
            item_type_id,
            item_type.weight);
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
                = original_bin_type.resource(resource_id).item_consumptions;
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

void InstanceBuilder::compute_bin_attributes()
{
    instance_.bin_volume_ = 0;
    instance_.bin_weight_ = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        instance_.bin_volume_ += bin_type.copies * bin_type.volume();
        instance_.bin_weight_ += bin_type.copies * bin_type.maximum_weight;
    }
}

void InstanceBuilder::set_item_types_profits_auto()
{
    compute_bin_attributes();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        Profit profit
            = (double)instance_.item_volume() / instance_.bin_volume()
            * (double)item_type.volume() / instance_.bin_volume()
            + (double)instance_.item_weight() / instance_.bin_weight()
            * (double)item_type.weight / instance_.bin_weight();
        instance_.item_types_[item_type_id].profit = profit;
    }
}

void InstanceBuilder::set_item_types_unweighted()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].profit = instance_.item_types_[item_type_id].volume();
    }
}

void InstanceBuilder::set_item_types_oriented()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].rotations = {Rotation::XYZ};
    }
}

Volume InstanceBuilder::compute_bin_types_volume_max() const
{
    Volume bin_types_volume_max = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        bin_types_volume_max = std::max(
                bin_types_volume_max,
                instance_.bin_type(bin_type_id).volume());
    }
    return bin_types_volume_max;
}

void InstanceBuilder::set_item_types_infinite_copies()
{
    Volume bin_types_volume_max = compute_bin_types_volume_max();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        ItemPos c = (bin_types_volume_max - 1) / item_type.volume() + 1;
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
        //std::cout << "name " << name << std::endl;
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
        Length y = -1;
        Length z = -1;
        Profit cost = -1;
        BinPos copies = 1;
        BinPos copies_min = 0;
        Weight maximum_weight = std::numeric_limits<Weight>::infinity();

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "Y") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "Z") {
                z = (Length)std::stol(line[i]);
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
        if (y == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Y\" column in \"" + bins_path + "\".");
        }
        if (z == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Z\" column in \"" + bins_path + "\".");
        }

        BinTypeId bin_type_id = add_bin_type(
                x,
                y,
                z);
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
        Length y = -1;
        Length z = -1;
        Profit profit = -1;
        Weight weight = 0;
        ItemPos copies = 1;
        ItemPos copies_min = -1;
        std::vector<Rotation> rotations;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "X") {
                x = (Length)std::stol(line[i]);
            } else if (labels[i] == "Y") {
                y = (Length)std::stol(line[i]);
            } else if (labels[i] == "Z") {
                z = (Length)std::stol(line[i]);
            } else if (labels[i] == "PROFIT") {
                profit = (Profit)std::stod(line[i]);
            } else if (labels[i] == "WEIGHT") {
                weight = (Weight)std::stod(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "COPIES_MIN") {
                copies_min = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "ROTATION_XYZ" && std::stol(line[i])) {
                rotations.push_back(Rotation::XYZ);
            } else if (labels[i] == "ROTATION_YXZ" && std::stol(line[i])) {
                rotations.push_back(Rotation::YXZ);
            } else if (labels[i] == "ROTATION_ZYX" && std::stol(line[i])) {
                rotations.push_back(Rotation::ZYX);
            } else if (labels[i] == "ROTATION_YZX" && std::stol(line[i])) {
                rotations.push_back(Rotation::YZX);
            } else if (labels[i] == "ROTATION_XZY" && std::stol(line[i])) {
                rotations.push_back(Rotation::XZY);
            } else if (labels[i] == "ROTATION_ZXY" && std::stol(line[i])) {
                rotations.push_back(Rotation::ZXY);
            }
        }

        if (x == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"X\" column in \"" + items_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Y\" column in \"" + items_path + "\".");
        }
        if (z == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Z\" column in \"" + items_path + "\".");
        }

        if (profit == -1)
            profit = x * y * z;

        ItemTypeId item_type_id = add_item_type(
                x,
                y,
                z);
        set_item_type_profit(item_type_id, profit);
        set_item_type_copies(item_type_id, copies);
        set_item_type_copies_min(item_type_id, copies_min);
        for (Rotation rotation: rotations)
            add_item_type_rotation(item_type_id, rotation);
        set_item_type_weight(
                item_type_id,
                weight);
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
        Length x = json_bin_type["x"];
        Length y = json_bin_type["y"];
        Length z = json_bin_type["z"];
        BinTypeId bin_type_id = add_bin_type(x, y, z);
        if (json_bin_type.contains("cost"))
            set_bin_type_cost(bin_type_id, json_bin_type["cost"]);
        if (json_bin_type.contains("copies"))
            set_bin_type_copies(bin_type_id, json_bin_type["copies"]);
        if (json_bin_type.contains("copies_min"))
            set_bin_type_copies_min(bin_type_id, json_bin_type["copies_min"]);
        if (json_bin_type.contains("maximum_weight"))
            set_bin_type_maximum_weight(bin_type_id, json_bin_type["maximum_weight"]);

        // Read resources.
        if (json_bin_type.contains("resources")) {
            for (const auto& json_resource: json_bin_type["resources"]) {
                double capacity = json_resource["capacity"];
                bool penalize = json_resource.value("penalize", false);
                double penalty = json_resource.value("penalty", 0.0);
                ResourceId resource_id = add_bin_type_resource(bin_type_id, capacity, penalize, penalty);
                if (json_resource.contains("consumptions")) {
                    for (const auto& json_consumption: json_resource["consumptions"]) {
                        ItemTypeId item_type_id = json_consumption["item_type_id"];
                        if (json_consumption.contains("consumption_schedule")) {
                            // Per-copy consumption schedule (a copy past
                            // the end of the schedule repeats its last
                            // entry); see 'Resource::item_consumptions'.
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
        Length x = json_item_type["x"];
        Length y = json_item_type["y"];
        Length z = json_item_type["z"];
        ItemTypeId item_type_id = add_item_type(x, y, z);
        if (json_item_type.contains("profit"))
            set_item_type_profit(item_type_id, json_item_type["profit"]);
        if (json_item_type.contains("copies"))
            set_item_type_copies(item_type_id, json_item_type["copies"]);
        if (json_item_type.contains("copies_min"))
            set_item_type_copies_min(item_type_id, json_item_type["copies_min"]);
        if (json_item_type.contains("weight"))
            set_item_type_weight(item_type_id, json_item_type["weight"]);
        if (json_item_type.contains("rotations")) {
            for (const auto& json_rotation: json_item_type["rotations"]) {
                std::string rotation_string = json_rotation;
                add_item_type_rotation(item_type_id, rotation_from_string(rotation_string));
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Instance InstanceBuilder::build()
{
    // Default rotations to {XYZ} for item types with no rotation specified.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        if (instance_.item_types_[item_type_id].rotations.empty())
            instance_.item_types_[item_type_id].rotations = {Rotation::XYZ};
    }

    // Compute item type attributes.
    Volume bin_types_volume_max = compute_bin_types_volume_max();
    instance_.all_item_types_infinite_copies_ = true;
    instance_.smallest_item_x_ = std::numeric_limits<Length>::max();
    instance_.smallest_item_y_ = std::numeric_limits<Length>::max();
    instance_.smallest_item_z_ = std::numeric_limits<Length>::max();
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
        // Update item_volume_.
        instance_.item_volume_ += item_type.copies * item_type.volume();
        // Update item_weight_.
        instance_.item_weight_ += item_type.copies * item_type.weight;
        // Update largest_efficiency_item_type_.
        if (instance_.largest_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.largest_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.largest_efficiency_item_type_id_).volume()
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).volume()) {
            instance_.largest_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_volume_max - 1) / item_type.volume() + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update largest_item_copies_.
        if (instance_.largest_item_copies_ < item_type.copies)
            instance_.largest_item_copies_ = item_type.copies;
        // Update smallest_item_x_, smallest_item_y_ and smallest_item_z_.
        for (Rotation rotation: item_type.rotations) {
            Box rotated = item_type.box.rotate(rotation);
            instance_.smallest_item_x_ = std::min(instance_.smallest_item_x_, rotated.x);
            instance_.smallest_item_y_ = std::min(instance_.smallest_item_y_, rotated.y);
            instance_.smallest_item_z_ = std::min(instance_.smallest_item_z_, rotated.z);
        }
    }
    if (instance_.number_of_item_types() == 0) {
        instance_.smallest_item_x_ = 0;
        instance_.smallest_item_y_ = 0;
        instance_.smallest_item_z_ = 0;
    }

    // Compute bin type attributes.
    instance_.bin_volume_ = 0;
    instance_.bin_area_ = 0;
    instance_.bin_weight_ = 0;
    Volume previous_bins_volume = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bin_type.copies.
        if (bin_type.copies == -1)
            instance_.bin_types_[bin_type_id].copies = instance_.number_of_items();
        // Update bins_volume_.
        instance_.bin_volume_ += bin_type.copies * bin_type.volume();
        // Update bins_area_.
        instance_.bin_area_ += bin_type.copies * bin_type.area();
        // Update bin_weight_..
        instance_.bin_weight_ += bin_type.copies * bin_type.maximum_weight;
        // Update previous_bins_volume_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_volume_.push_back(previous_bins_volume);
            previous_bins_volume += bin_type.volume();
        }
        // Update largest_bin_cost_.
        if (instance_.largest_bin_cost_ < bin_type.cost)
            instance_.largest_bin_cost_ = bin_type.cost;
    }

    if (instance_.objective() == Objective::OpenDimensionX
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "the instance has objective OpenDimensionX and contains " + std::to_string(instance_.number_of_bins()) + " bins; "
                "an instance with objective OpenDimensionX must contain exactly one bin.");
    }
    if (instance_.objective() == Objective::OpenDimensionY
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "the instance has objective OpenDimensionY and contains " + std::to_string(instance_.number_of_bins()) + " bins; "
                "an instance with objective OpenDimensionY must contain exactly one bin.");
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

    // Compute item_type.resource_ids (see its own doc comment).
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].resource_ids.resize(instance_.number_of_bin_types());
    }
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_types_[bin_type_id];
        for (ResourceId resource_id = 0;
                resource_id < bin_type.number_of_resources();
                ++resource_id) {
            const Resource& resource = bin_type.resource(resource_id);
            for (ItemTypeId item_type_id = 0;
                    item_type_id < (ItemTypeId)resource.item_consumptions.size();
                    ++item_type_id) {
                if (!resource.item_consumptions[item_type_id].empty()) {
                    instance_.item_types_[item_type_id].resource_ids[bin_type_id]
                        .push_back(resource_id);
                }
            }
        }
    }

    return std::move(instance_);
}
