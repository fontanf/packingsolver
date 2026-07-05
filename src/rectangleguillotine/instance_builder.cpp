#include "packingsolver/rectangleguillotine/instance_builder.hpp"

#include "optimizationtools/utils/utils.hpp"

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Parameters //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void InstanceBuilder::set_predefined(std::string str)
{
    if (str == "roadef2018") {
        set_roadef2018();
        return;
    }

    if (str[0] == 'U') {
        this->set_number_of_stages_unlimited();

        switch (str[1]) {
        case 'R': {
            set_item_types_oriented(false);
            break;
        } case 'O': {
            set_item_types_oriented(true);
            break;
        } default: {
            // TODO
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "predefined branching scheme parameter 4th character");
        }
        }
        return;
    }

    if (str.length() != 4) {
        // TODO
        throw std::invalid_argument(FUNC_SIGNATURE);
    }

    Counter number_of_stages = (Counter)(str[0] - 48);
    if (number_of_stages <= 1) {
        // TODO
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "predefined branching scheme parameter 1st character");
    }
    set_number_of_stages(number_of_stages);

    switch (str[1]) {
    case 'R': {
        set_cut_type(rectangleguillotine::CutType::Roadef2018);
        break;
    } case 'N': {
        set_cut_type(rectangleguillotine::CutType::NonExact);
        break;
    } case 'E': {
        set_cut_type(rectangleguillotine::CutType::Exact);
        break;
    } case 'H': {
        set_cut_type(rectangleguillotine::CutType::Homogenous);
        break;
    } default: {
        // TODO
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "predefined branching scheme parameter 2nd character");
    }
    }

    switch (str[2]) {
    case 'V': {
        set_first_stage_orientation(rectangleguillotine::CutOrientation::Vertical);
        break;
    } case 'H': {
        set_first_stage_orientation(rectangleguillotine::CutOrientation::Horizontal);
        break;
    } case 'A': {
        set_first_stage_orientation(rectangleguillotine::CutOrientation::Any);
        break;
    } default: {
        // TODO
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "predefined branching scheme parameter 3rd character");
    }
    }
    switch (str[3]) {
    case 'R': {
        set_item_types_oriented(false);
        break;
    } case 'O': {
        set_item_types_oriented(true);
        break;
    } default: {
        // TODO
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "predefined branching scheme parameter 4th character");
    }
    }
}

void InstanceBuilder::set_number_of_stages(Counter number_of_stages)
{
    instance_.parameters_.number_of_stages = number_of_stages;
    resize_cutting_costs();
}

void InstanceBuilder::set_cut_type(CutType cut_type)
{
    instance_.parameters_.cut_type = cut_type;
    resize_cutting_costs();
}

void InstanceBuilder::resize_cutting_costs()
{
    Counter number_of_required_cutting_costs = 1 + instance_.parameters_.number_of_stages
        + ((instance_.parameters_.cut_type == CutType::NonExact
                    || instance_.parameters_.cut_type == CutType::Roadef2018)? 1: 0);
    if ((Counter)instance_.parameters_.cutting_costs.size() < number_of_required_cutting_costs)
        instance_.parameters_.cutting_costs.resize(number_of_required_cutting_costs);
}

void InstanceBuilder::set_fixed_cutting_cost(
        Counter stage_id,
        Profit fixed_cost)
{
    if (stage_id < 0 || stage_id >= (Counter)instance_.parameters_.cutting_costs.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'stage_id'; "
                "stage_id: " + std::to_string(stage_id) + "; "
                "cutting_costs.size(): " + std::to_string(instance_.parameters_.cutting_costs.size()) + ".");
    }
    instance_.parameters_.cutting_costs[stage_id].fixed = fixed_cost;
}

void InstanceBuilder::set_variable_cutting_cost(
        Counter stage_id,
        Profit variable_cost)
{
    if (stage_id < 0 || stage_id >= (Counter)instance_.parameters_.cutting_costs.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'stage_id'; "
                "stage_id: " + std::to_string(stage_id) + "; "
                "cutting_costs.size(): " + std::to_string(instance_.parameters_.cutting_costs.size()) + ".");
    }
    instance_.parameters_.cutting_costs[stage_id].variable = variable_cost;
}

void InstanceBuilder::set_number_of_stages_unlimited()
{
    Counter max_stages = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        max_stages = std::max(max_stages, bin_type.rect.w + bin_type.rect.h);
    }
    instance_.parameters_.number_of_stages = max_stages;
    instance_.parameters_.cut_type = CutType::Exact;
    instance_.parameters_.first_stage_orientation = CutOrientation::Any;
    resize_cutting_costs();
}

void InstanceBuilder::set_roadef2018()
{
    instance_.parameters_.number_of_stages = 3;
    instance_.parameters_.cut_type = rectangleguillotine::CutType::Roadef2018;
    instance_.parameters_.first_stage_orientation = rectangleguillotine::CutOrientation::Vertical;
    instance_.parameters_.minimum_distance_1_cuts = 100;
    instance_.parameters_.maximum_distance_1_cuts = 3500;
    instance_.parameters_.minimum_distance_2_cuts = 100;
    instance_.parameters_.minimum_waste_length = 20;
    instance_.parameters_.cut_through_defects = false;
    resize_cutting_costs();
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Bin types ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinTypeId InstanceBuilder::add_bin_type(
        Length width,
        Length height,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (width <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'width' must be > 0; "
                "width: " + std::to_string(width) + ".");
    }
    if (height <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'height' must be > 0; "
                "height: " + std::to_string(height) + ".");
    }
    if (cost <= 0 && cost != -1) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'cost' must be > 0 (or == -1); "
                "cost: " + std::to_string(cost) + ".");
    }
    if (copies_min < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "bin 'copies_min' must be >= 0; "
                "copies_min: " + std::to_string(copies_min) + ".");
    }
    if (copies != -1) {
        if (copies <= 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin 'copies' must be > 0 (or == -1); "
                    "copies: " + std::to_string(copies) + ".");
        }
        if (copies_min > copies) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "bin 'copies_min' must be <= 'copies'; "
                    "copies: " + std::to_string(copies) + "; "
                    "copies_min: " + std::to_string(copies_min) + ".");
        }
    }

    BinType bin_type;
    bin_type.rect.w = width;
    bin_type.rect.h = height;
    bin_type.cost = (cost == -1)? width * height: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return instance_.bin_types_.size() - 1;
}

void InstanceBuilder::add_trims(
        BinTypeId bin_type_id,
        Length left_trim,
        TrimType left_trim_type,
        Length right_trim,
        TrimType right_trim_type,
        Length bottom_trim,
        TrimType bottom_trim_type,
        Length top_trim,
        TrimType top_trim_type)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    if (bottom_trim < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'bottom_trim >= 0'.");
    }
    if (bottom_trim >= bin_type.rect.h) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'bottom_trim < h'.");
    }

    if (top_trim < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'top_trim >= 0'.");
    }
    if (top_trim >= bin_type.rect.h - bottom_trim) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'top_trim < h - bottom_trim'.");
    }

    if (left_trim < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'left_trim >= 0'.");
    }
    if (left_trim >= bin_type.rect.w) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'left_trim < w'.");
    }

    if (right_trim < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'right_trim >= 0'.");
    }
    if (right_trim > bin_type.rect.h - left_trim) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "requires 'right_trim < w - left_trim'.");
    }

    bin_type.left_trim = left_trim;
    bin_type.left_trim_type = left_trim_type;
    bin_type.right_trim = right_trim;
    bin_type.right_trim_type = right_trim_type;
    bin_type.bottom_trim = bottom_trim;
    bin_type.bottom_trim_type = bottom_trim_type;
    bin_type.top_trim = top_trim;
    bin_type.top_trim_type = top_trim_type;
}

void InstanceBuilder::add_defect(
        BinTypeId bin_type_id,
        Length x,
        Length y,
        Length w,
        Length h)
{
    if (bin_type_id < 0 || bin_type_id >= instance_.bin_types_.size()) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "invalid 'bin_type_id'; "
                "bin_type_id: " + std::to_string(bin_type_id) + "; "
                "instance_.bin_types_.size(): " + std::to_string(instance_.bin_types_.size()) + ".");
    }

    BinType& bin_type = instance_.bin_types_[bin_type_id];

    Defect defect;
    defect.pos.x = x;
    defect.pos.y = y;
    defect.rect.w = w;
    defect.rect.h = h;
    bin_type.defects.push_back(defect);
}

void InstanceBuilder::add_bin_type(
        const Instance& original_instance,
        BinTypeId original_bin_type_id,
        BinPos copies,
        BinPos copies_min)
{
    const BinType& bin_type = original_instance.bin_type(original_bin_type_id);
    BinTypeId bin_type_id = add_bin_type(
            bin_type.rect.w,
            bin_type.rect.h,
            bin_type.cost,
            copies,
            copies_min);
    if ((BinTypeId)orig_to_sub_bin_type_ids_.size() <= original_bin_type_id)
        orig_to_sub_bin_type_ids_.resize(original_bin_type_id + 1, -1);
    orig_to_sub_bin_type_ids_[original_bin_type_id] = bin_type_id;
    add_trims(
            bin_type_id,
            bin_type.left_trim,
            bin_type.left_trim_type,
            bin_type.right_trim,
            bin_type.right_trim_type,
            bin_type.bottom_trim,
            bin_type.bottom_trim_type,
            bin_type.top_trim,
            bin_type.top_trim_type);
    for (const Defect& defect: bin_type.defects) {
        add_defect(
                bin_type_id,
                defect.pos.x,
                defect.pos.y,
                defect.rect.w,
                defect.rect.h);
    }
}

Length InstanceBuilder::compute_item_types_max_length_sum() const
{
    Length length = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        length += item_type.copies * item_type.rect.max();
    }
    return length;
}

void InstanceBuilder::set_bin_types_infinite_x()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].rect.w = length;
    }
}

void InstanceBuilder::set_bin_types_infinite_y()
{
    Length length = compute_item_types_max_length_sum();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].rect.h = length;
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
        instance_.bin_types_[bin_type_id].cost = instance_.bin_types_[bin_type_id].area();
    }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Item types //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

ItemTypeId InstanceBuilder::add_item_type(
        Length width,
        Length height,
        Profit profit,
        ItemPos copies,
        bool oriented,
        StackId stack_id)
{
    if (width < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'width' must be > 0; "
                "width: " + std::to_string(width) + ".");
    }
    if (height < 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'height' must be > 0; "
                "height: " + std::to_string(height) + ".");
    }
    if (copies <= 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "item 'copies' must be > 0; "
                "copies: " + std::to_string(copies) + ".");
    }

    ItemType item_type;
    item_type.rect.w = width;
    item_type.rect.h = height;
    item_type.profit = (profit == -1)? width * height: profit;
    item_type.copies = copies;
    item_type.stack_id = stack_id;
    item_type.oriented = oriented;
    instance_.item_types_.push_back(item_type);
    return instance_.item_types_.size() - 1;
}

void InstanceBuilder::add_item_type(
        const Instance& original_instance,
        ItemTypeId original_item_type_id,
        Profit profit,
        ItemPos copies)
{
    const ItemType& item_type = original_instance.item_type(original_item_type_id);
    ItemTypeId item_type_id = add_item_type(
            item_type.rect.w,
            item_type.rect.h,
            profit,
            copies,
            item_type.oriented,
            item_type.stack_id);
    if ((ItemTypeId)orig_to_sub_item_type_ids_.size() <= original_item_type_id)
        orig_to_sub_item_type_ids_.resize(original_item_type_id + 1, -1);
    orig_to_sub_item_type_ids_[original_item_type_id] = item_type_id;
}

Area InstanceBuilder::compute_bin_types_area_max() const
{
    Area bin_types_area_max = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        bin_types_area_max = std::max(
                bin_types_area_max,
                instance_.bin_type(bin_type_id).area());
    }
    return bin_types_area_max;
}

void InstanceBuilder::set_item_types_infinite_copies()
{
    Area bin_types_area_max = compute_bin_types_area_max();
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        ItemPos c = (bin_types_area_max - 1) / item_type.area() + 1;
        item_type.copies = c;
    }
}

void InstanceBuilder::multiply_item_types_copies(ItemPos factor)
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        ItemType& item_type = instance_.item_types_[item_type_id];
        item_type.copies *= factor;
    }
}

void InstanceBuilder::set_item_types_unweighted()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].profit = instance_.item_types_[item_type_id].area();
    }
}

void InstanceBuilder::set_item_types_oriented(bool oriented)
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].oriented = oriented;
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
        } else if (name == "number_of_stages") {
            if (value == "u"
                    || value == "U"
                    || value == "unlimited") {
                set_number_of_stages_unlimited();
            } else {
                set_number_of_stages(std::stol(value));
            }
        } else if (name == "cut_type") {
            CutType cut_type;
            std::stringstream ss(value);
            ss >> cut_type;
            set_cut_type(cut_type);
        } else if (name == "first_stage_orientation") {
            CutOrientation first_stage_orientation;
            std::stringstream ss(value);
            ss >> first_stage_orientation;
            set_first_stage_orientation(first_stage_orientation);
        } else if (name == "min1cut"
                || name == "min1Cut"
                || name == "minimum_distance_1_cuts") {
            set_minimum_distance_1_cuts(std::stol(value));
        } else if (name == "max1cut"
                || name == "max1Cut"
                || name == "maximum_distance_1_cuts") {
            set_maximum_distance_1_cuts(std::stol(value));
        } else if (name == "min2cut"
                || name == "min2Cut"
                || name == "minimum_distance_2_cuts") {
            set_minimum_distance_2_cuts(std::stol(value));
        } else if (name == "max2cut"
                || name == "max2Cut"
                || name == "maximum_distance_2_cuts") {
            set_maximum_distance_2_cuts(std::stol(value));
        } else if (name == "min_waste"
                || name == "minwaste"
                || name == "minWaste"
                || name == "minimum_waste_length") {
            set_minimum_waste_length(std::stol(value));
        } else if (name == "maximum_number_1_cuts") {
            set_maximum_number_1_cuts(std::stol(value));
        } else if (name == "maximum_number_2_cuts") {
            set_maximum_number_2_cuts(std::stol(value));
        } else if (name == "cut_through_defects") {
            set_cut_through_defects(std::stol(value));
        } else if (name == "cut_thickness") {
            set_cut_thickness(std::stol(value));
        } else if (name.rfind("cuts_fixed_cost_", 0) == 0) {
            Counter stage_id = std::stol(name.substr(16));
            set_fixed_cutting_cost(stage_id, std::stod(value));
        } else if (name.rfind("cuts_variable_cost_", 0) == 0) {
            Counter stage_id = std::stol(name.substr(19));
            set_variable_cutting_cost(stage_id, std::stod(value));
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

    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        Length w = -1;
        Length h = -1;
        Profit cost = -1;
        BinPos copies = 1;
        BinPos copies_min = 0;
        Length bottom_trim = 0;
        Length top_trim = 0;
        Length left_trim = 0;
        Length right_trim = 0;
        TrimType bottom_trim_type = TrimType::Hard;
        TrimType top_trim_type = TrimType::Soft;
        TrimType left_trim_type = TrimType::Hard;
        TrimType right_trim_type = TrimType::Soft;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "WIDTH") {
                w = (Length)std::stol(line[i]);
            } else if (labels[i] == "HEIGHT") {
                h = (Length)std::stol(line[i]);
            } else if (labels[i] == "COST") {
                cost = (Profit)std::stod(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "COPIES_MIN") {
                copies_min = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "BOTTOM_TRIM") {
                bottom_trim = (Length)std::stol(line[i]);
            } else if (labels[i] == "TOP_TRIM") {
                top_trim = (Length)std::stol(line[i]);
            } else if (labels[i] == "LEFT_TRIM") {
                left_trim = (Length)std::stol(line[i]);
            } else if (labels[i] == "RIGHT_TRIM") {
                right_trim = (Length)std::stol(line[i]);
            } else if (labels[i] == "BOTTOM_TRIM_TYPE") {
                std::stringstream ss(line[i]);
                ss >> bottom_trim_type;
            } else if (labels[i] == "TOP_TRIM_TYPE") {
                std::stringstream ss(line[i]);
                ss >> top_trim_type;
            } else if (labels[i] == "LEFT_TRIM_TYPE") {
                std::stringstream ss(line[i]);
                ss >> left_trim_type;
            } else if (labels[i] == "RIGHT_TRIM_TYPE") {
                std::stringstream ss(line[i]);
                ss >> right_trim_type;
            }
        }

        if (w == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"WIDTH\" column in \"" + bins_path + "\".");
        }
        if (h == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"HEIGHT\" column in \"" + bins_path + "\".");
        }

        BinTypeId bin_type_id = add_bin_type(
                w,
                h,
                cost,
                copies,
                copies_min);
        add_trims(
                bin_type_id,
                left_trim,
                left_trim_type,
                right_trim,
                right_trim_type,
                bottom_trim,
                bottom_trim_type,
                top_trim,
                top_trim_type);
    }
}

void InstanceBuilder::read_defects(
        const std::string& defects_path)
{
    if (defects_path.empty())
        return;

    std::ifstream f(defects_path);
    if (defects_path != "" && !f.good()) {
        throw std::runtime_error(
                FUNC_SIGNATURE + ": "
                "unable to open file \"" + defects_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f, tmp)) {
        line = optimizationtools::split(tmp, ',');

        BinTypeId bin_type_id = -1;
        Length x = -1;
        Length y = -1;
        Length w = -1;
        Length h = -1;

        for (Counter c = 0; c < (Counter)line.size(); ++c) {
            if (labels[c] == "BIN") {
                bin_type_id = (BinTypeId)std::stol(line[c]);
            } else if (labels[c] == "X") {
                x = (Length)std::stol(line[c]);
            } else if (labels[c] == "Y") {
                y = (Length)std::stol(line[c]);
            } else if (labels[c] == "WIDTH") {
                w = (Length)std::stol(line[c]);
            } else if (labels[c] == "HEIGHT") {
                h = (Length)std::stol(line[c]);
            }
        }

        if (bin_type_id == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"BIN\" column in \"" + defects_path + "\".");
        }
        if (x == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"X\" column in \"" + defects_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"Y\" column in \"" + defects_path + "\".");
        }
        if (w == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"WIDTH\" column in \"" + defects_path + "\".");
        }
        if (h == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"HEIGHT\" column in \"" + defects_path + "\".");
        }

        add_defect(bin_type_id, x, y, w, h);
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

        Length w = -1;
        Length h = -1;
        Profit profit = -1;
        ItemPos copies = 1;
        bool oriented = false;
        StackId stack_id = -1;

        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "WIDTH") {
                w = (Length)std::stol(line[i]);
            } else if (labels[i] == "HEIGHT") {
                h = (Length)std::stol(line[i]);
            } else if (labels[i] == "PROFIT") {
                profit = (Profit)std::stod(line[i]);
            } else if (labels[i] == "COPIES") {
                copies = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "ORIENTED") {
                oriented = (bool)std::stol(line[i]);
            } else if (labels[i] == "STACK_ID") {
                stack_id = (StackId)std::stol(line[i]);
            }
        }

        if (w == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"WIDTH\" column in \"" + items_path + "\".");
        }
        if (h == -1) {
            throw std::runtime_error(
                    FUNC_SIGNATURE + ": "
                    "missing \"HEIGHT\" column in \"" + items_path + "\".");
        }

        if (profit == -1)
            profit = w * h;

        add_item_type(
                w,
                h,
                profit,
                copies,
                oriented,
                stack_id);
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////// Build /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void InstanceBuilder::build_stacks()
{
    // Either ALL item types have a named stack_id (>= 0), or NONE do.
    // Mixed assignments are not supported.
    bool any_named = false;
    bool any_unnamed = false;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        if (instance_.item_type(item_type_id).stack_id >= 0) {
            any_named = true;
        } else {
            any_unnamed = true;
        }
    }
    if (any_named && any_unnamed) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "mixed named (stack_id >= 0) and unnamed (stack_id == -1) "
                "item types are not supported.");
    }

    if (any_named) {
        // Named stacks: determine number of stacks and per-stack sizes.
        StackId n_stacks = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            n_stacks = std::max(n_stacks, instance_.item_type(item_type_id).stack_id + 1);
        }
        std::vector<ItemPos> stack_sizes(n_stacks, 0);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            stack_sizes[item_type.stack_id] += item_type.copies;
        }
        // Build prefix-sum offsets (size = n_stacks + 1).
        instance_.stack_offsets_.resize(n_stacks + 1);
        instance_.stack_offsets_[0] = 0;
        for (StackId stack_id = 0; stack_id < n_stacks; ++stack_id) {
            instance_.stack_offsets_[stack_id + 1] =
                instance_.stack_offsets_[stack_id] + stack_sizes[stack_id];
        }
        instance_.item_type_ids_.resize(instance_.stack_offsets_[n_stacks]);
        // Fill flat array and update stack_pos.
        std::vector<ItemPos> write_pos(n_stacks, 0);
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = instance_.item_type(item_type_id);
            StackId stack_id = item_type.stack_id;
            instance_.item_types_[item_type_id].stack_pos = write_pos[stack_id];
            ItemPos offset = instance_.stack_offsets_[stack_id] + write_pos[stack_id];
            for (ItemPos c = 0; c < item_type.copies; ++c)
                instance_.item_type_ids_[offset + c] = item_type_id;
            write_pos[stack_id] += item_type.copies;
        }
    } else {
        // Unnamed stacks: each item type gets its own stack.
        StackId n_stacks = instance_.number_of_item_types();
        instance_.stack_offsets_.resize(n_stacks + 1);
        ItemPos total_copies = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            instance_.stack_offsets_[item_type_id] = total_copies;
            instance_.item_types_[item_type_id].stack_id = item_type_id;
            instance_.item_types_[item_type_id].stack_pos = 0;
            total_copies += instance_.item_type(item_type_id).copies;
        }
        instance_.stack_offsets_[n_stacks] = total_copies;
        instance_.item_type_ids_.resize(total_copies);
        ItemPos offset = 0;
        for (ItemTypeId item_type_id = 0;
                item_type_id < instance_.number_of_item_types();
                ++item_type_id) {
            for (ItemPos c = 0; c < instance_.item_type(item_type_id).copies; ++c)
                instance_.item_type_ids_[offset + c] = item_type_id;
            offset += instance_.item_type(item_type_id).copies;
        }
    }
}

Instance InstanceBuilder::build()
{
    // minimum_distance_2_cuts, maximum_distance_2_cuts and
    // maximum_number_2_cuts are not allowed for 2-staged instances.
    if (instance_.parameters().number_of_stages == 2) {
        if (instance_.parameters().minimum_distance_2_cuts != 0) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "minimum_distance_2_cuts is not allowed if number_of_stages == 2.");
        }
        if (instance_.parameters().maximum_distance_2_cuts != -1) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "maximum_distance_2_cuts is not allowed if number_of_stages == 2.");
        }
        if (instance_.parameters().maximum_number_2_cuts != -1) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "maximum_number_2_cuts is not allowed if number_of_stages == 2.");
        }
    }

    // maximum_number_1_cuts and maximum_number_2_cuts must not be 0.
    if (instance_.parameters().maximum_number_1_cuts == 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "maximum_number_1_cuts must not be 0.");
    }
    if (instance_.parameters().maximum_number_2_cuts == 0) {
        throw std::invalid_argument(
                FUNC_SIGNATURE + ": "
                "maximum_number_2_cuts must not be 0.");
    }

    // For the 'BinPackingCuttingCost' objective, 'cutting_costs' must contain
    // a valid (fixed >= 0, variable >= 0) entry for stage 0 (the bin) and for
    // stages 1 to number_of_stages, plus one extra stage if cut_type is
    // NonExact or Roadef2018.
    if (instance_.objective() == Objective::BinPackingCuttingCost) {
        Counter number_of_required_cutting_costs = 1 + instance_.parameters().number_of_stages
            + ((instance_.parameters().cut_type == CutType::NonExact
                        || instance_.parameters().cut_type == CutType::Roadef2018)? 1: 0);
        if ((Counter)instance_.parameters().cutting_costs.size() < number_of_required_cutting_costs) {
            throw std::invalid_argument(
                    FUNC_SIGNATURE + ": "
                    "missing cutting costs; "
                    "cutting_costs.size(): " + std::to_string(instance_.parameters().cutting_costs.size()) + "; "
                    "number_of_required_cutting_costs: " + std::to_string(number_of_required_cutting_costs) + ".");
        }
        for (Counter stage_id = 0;
                stage_id < number_of_required_cutting_costs;
                ++stage_id) {
            const CutCost& cutting_cost = instance_.parameters().cutting_costs[stage_id];
            if (cutting_cost.fixed == -1 || cutting_cost.variable == -1) {
                throw std::invalid_argument(
                        FUNC_SIGNATURE + ": "
                        "missing cutting cost for stage " + std::to_string(stage_id) + ".");
            }
        }
    }

    // Compute item_type_ids_ and stack_offsets_.
    build_stacks();

    // Compute item type attributes.
    Area bin_types_area_max = compute_bin_types_area_max();
    instance_.all_item_types_infinite_copies_ = true;
    instance_.all_item_types_oriented_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        // Update number_of_items_.
        instance_.number_of_items_ += item_type.copies;
        // Update item_profit_.
        instance_.item_profit_ += item_type.copies * item_type.profit;
        // Update largest_item_profit_.
        instance_.largest_item_profit_ = std::max(instance_.largest_item_profit(), item_type.profit);
        // Update item_area_.
        instance_.item_area_ += item_type.copies * item_type.area();
        // Update largest_efficiency_item_type_.
        if (instance_.largest_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.largest_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.largest_efficiency_item_type_id_).area()
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).area()) {
            instance_.largest_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_area_max - 1) / item_type.area() + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
        // Update largest_item_copies_.
        if (instance_.largest_item_copies_ < item_type.copies)
            instance_.largest_item_copies_ = item_type.copies;
        // Update all_item_types_oriented_.
        if (!item_type.oriented)
            instance_.all_item_types_oriented_ = false;
    }

    // Compute bin type attributes.
    instance_.bins_area_sum_ = 0;
    Area previous_bins_area = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bin_type.copies.
        if (bin_type.copies == -1)
            instance_.bin_types_[bin_type_id].copies = instance_.number_of_items();
        // Update bins_area_sum_.
        instance_.bins_area_sum_ += bin_type.copies * bin_type.area();
        // Update previous_bins_area_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_area_.push_back(previous_bins_area);
            previous_bins_area += bin_type.area();
        }
        // Update largest_bin_cost_.
        if (instance_.largest_bin_cost_ < bin_type.cost)
            instance_.largest_bin_cost_ = bin_type.cost;
        // Update number_of_defects_.
        instance_.number_of_defects_ += bin_type.defects.size();
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

    return std::move(instance_);
}
