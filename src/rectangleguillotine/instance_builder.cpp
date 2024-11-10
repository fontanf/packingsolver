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

    if (str.length() != 4) {
        // TODO
        throw std::invalid_argument("");
    }

    Counter number_of_stages = (Counter)(str[0] - 48);
    if (number_of_stages <= 1) {
        // TODO
        throw std::invalid_argument(
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
                "predefined branching scheme parameter 3rd character");
    }
    }
    switch (str[3]) {
    case 'R': {
        break;
    } case 'O': {
        set_item_types_oriented();
        break;
    } default: {
        // TODO
        throw std::invalid_argument(
                "predefined branching scheme parameter 4th character");
    }
    }
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
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// Bin types ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

BinTypeId InstanceBuilder::add_bin_type(
        Length w,
        Length h,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (w <= 0) {
        throw std::runtime_error(
                "'rectangleguillotine::InstanceBuilder::add_bin_type'"
                " requires 'w > 0'.");
    }
    if (h <= 0) {
        throw std::runtime_error(
                "'rectangleguillotine::InstanceBuilder::add_bin_type'"
                " requires 'h > 0'.");
    }
    if (cost < 0 && cost != -1) {
        throw std::runtime_error(
                "'rectangleguillotine::InstanceBuilder::add_bin_type'"
                " requires 'cost >= 0' or 'cost == -1'.");
    }
    if (copies <= 0) {
        throw std::runtime_error(
                "'rectangleguillotine::InstanceBuilder::add_bin_type'"
                " requires 'copies > 0'.");
    }
    if (copies_min < 0) {
        throw std::runtime_error(
                "'rectangleguillotine::InstanceBuilder::add_bin_type'"
                " requires 'copies_min >= 0'.");
    }
    if (copies_min > copies) {
        throw std::runtime_error(
                "'rectangleguillotine::InstanceBuilder::add_bin_type'"
                " requires 'copies_min <= copies'.");
    }

    BinType bin_type;
    bin_type.id = instance_.bin_types_.size();
    bin_type.rect.w = w;
    bin_type.rect.h = h;
    bin_type.cost = (cost == -1)? w * h: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    instance_.bin_types_.push_back(bin_type);
    return bin_type.id;
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
    BinType& bin_type = instance_.bin_types_[bin_type_id];

    if (bottom_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'bottom_trim >= 0'.");
    }
    if (bottom_trim >= bin_type.rect.h) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'bottom_trim < h'.");
    }

    if (top_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'top_trim >= 0'.");
    }
    if (top_trim >= bin_type.rect.h - bottom_trim) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'top_trim < h - bottom_trim'.");
    }

    if (left_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'left_trim >= 0'.");
    }
    if (left_trim >= bin_type.rect.w) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'left_trim < w'.");
    }

    if (right_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'right_trim >= 0'.");
    }
    if (right_trim > bin_type.rect.h - left_trim) {
        throw std::invalid_argument(
                "'rectangleguillotine::InstanceBuilder::add_trims'"
                " requires 'right_trim < w - left_trim'.");
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
    Defect defect;
    defect.id = instance_.bin_types_[bin_type_id].defects.size();
    defect.bin_type_id = bin_type_id;
    defect.pos.x = x;
    defect.pos.y = y;
    defect.rect.w = w;
    defect.rect.h = h;
    instance_.bin_types_[bin_type_id].defects.push_back(defect);
}

void InstanceBuilder::add_bin_type(
        const BinType& bin_type,
        BinPos copies,
        BinPos copies_min)
{
    BinTypeId bin_type_id = add_bin_type(
            bin_type.rect.w,
            bin_type.rect.h,
            bin_type.cost,
            copies,
            copies_min);
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

ItemPos InstanceBuilder::compute_number_of_items() const
{
    ItemPos number_of_items = 0;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        number_of_items += item_type.copies;
    }
    return number_of_items;
}

void InstanceBuilder::set_bin_types_infinite_copies()
{
    ItemPos number_of_items = compute_number_of_items();
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        instance_.bin_types_[bin_type_id].copies = number_of_items;
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
        Length w,
        Length h,
        Profit p,
        ItemPos copies,
        bool oriented,
        StackId stack_id)
{
    ItemType item_type;
    item_type.id = instance_.item_types_.size();
    item_type.rect.w = w;
    item_type.rect.h = h;
    item_type.profit = (p == -1)? w * h: p;
    item_type.copies = copies;
    item_type.stack_id = stack_id;
    item_type.oriented = oriented;
    instance_.item_types_.push_back(item_type);
    return item_type.id;
}

void InstanceBuilder::add_item_type(
        const ItemType& item_type,
        Profit profit,
        ItemPos copies)
{
    add_item_type(
            item_type.rect.w,
            item_type.rect.h,
            profit,
            copies,
            item_type.oriented,
            item_type.stack_id);
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

void InstanceBuilder::set_item_types_oriented()
{
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        instance_.item_types_[item_type_id].oriented = true;
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
        } else if (name == "number_of_stages") {
            set_number_of_stages(std::stol(value));
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
        } else if (name == "min_waste"
                || name == "minwaste"
                || name == "minWaste"
                || name == "minimum_waste_length") {
            set_minimum_waste_length(std::stol(value));
        } else if (name == "maximum_number_2_cuts") {
            set_maximum_number_2_cuts(std::stol(value));
        } else if (name == "cut_thickness") {
            set_cut_thickness(std::stol(value));
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
                cost = (Profit)std::stol(line[i]);
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
                    "Missing \"WIDTH\" column in \"" + bins_path + "\".");
        }
        if (h == -1) {
            throw std::runtime_error(
                    "Missing \"HEIGHT\" column in \"" + bins_path + "\".");
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
                "Unable to open file \"" + defects_path + "\".");
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
                    "Missing \"BIN\" column in \"" + defects_path + "\".");
        }
        if (x == -1) {
            throw std::runtime_error(
                    "Missing \"X\" column in \"" + defects_path + "\".");
        }
        if (y == -1) {
            throw std::runtime_error(
                    "Missing \"Y\" column in \"" + defects_path + "\".");
        }
        if (w == -1) {
            throw std::runtime_error(
                    "Missing \"WIDTH\" column in \"" + defects_path + "\".");
        }
        if (h == -1) {
            throw std::runtime_error(
                    "Missing \"HEIGHT\" column in \"" + defects_path + "\".");
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
                "Unable to open file \"" + items_path + "\".");
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
                profit = (Profit)std::stol(line[i]);
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
                    "Missing \"WIDTH\" column in \"" + items_path + "\".");
        }
        if (h == -1) {
            throw std::runtime_error(
                    "Missing \"HEIGHT\" column in \"" + items_path + "\".");
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

Instance InstanceBuilder::build()
{
    // Compute item_type_ids_.
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        if (item_type.stack_id == -1)
            continue;
        while ((StackId)instance_.item_type_ids_.size()
                <= item_type.stack_id) {
            instance_.item_type_ids_.push_back({});
        }
        for (ItemPos c = 0; c < item_type.copies; ++c)
            instance_.item_type_ids_[item_type.stack_id].push_back(item_type_id);
    }
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        if (item_type.stack_id != -1)
            continue;
        instance_.item_types_[item_type_id].stack_id = instance_.item_type_ids_.size();
        instance_.item_type_ids_.push_back({});
        for (ItemPos c = 0; c < item_type.copies; ++c)
            instance_.item_type_ids_[item_type.stack_id].push_back(item_type_id);
    }

    // Compute item type attributes.
    Area bin_types_area_max = compute_bin_types_area_max();
    instance_.all_item_types_infinite_copies_ = true;
    for (ItemTypeId item_type_id = 0;
            item_type_id < instance_.number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = instance_.item_type(item_type_id);
        // Update number_of_items_.
        instance_.number_of_items_ += item_type.copies;
        // Update item_profit_.
        instance_.item_profit_ += item_type.copies * item_type.profit;
        // Update item_area_.
        instance_.item_area_ += item_type.copies * item_type.area();
        // Update max_efficiency_item_type_.
        if (instance_.max_efficiency_item_type_id_ == -1
                || instance_.item_type(instance_.max_efficiency_item_type_id_).profit
                / instance_.item_type(instance_.max_efficiency_item_type_id_).area()
                < instance_.item_type(item_type_id).profit
                / instance_.item_type(item_type_id).area()) {
            instance_.max_efficiency_item_type_id_ = item_type_id;
        }
        // Update all_item_types_infinite_copies_.
        ItemPos c = (bin_types_area_max - 1) / item_type.area() + 1;
        if (item_type.copies < c)
            instance_.all_item_types_infinite_copies_ = false;
    }

    // Compute bin type attributes.
    instance_.bins_area_sum_ = 0;
    Area previous_bins_area = 0;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < instance_.number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = instance_.bin_type(bin_type_id);
        // Update bins_area_sum_.
        instance_.bins_area_sum_ += bin_type.copies * bin_type.area();
        // Update previous_bins_area_ and bin_type_ids_.
        for (BinPos copy = 0; copy < bin_type.copies; ++copy) {
            instance_.bin_type_ids_.push_back(bin_type_id);
            instance_.previous_bins_area_.push_back(previous_bins_area);
            previous_bins_area += bin_type.area();
        }
        // Update number_of_defects_.
        instance_.number_of_defects_ += bin_type.defects.size();
    }

    if (instance_.objective() == Objective::OpenDimensionX
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                "rectangleguillotine::InstanceBuilder::build."
                " The instance has objective OpenDimensionX and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionX must contain exactly one bin.");
    }
    if (instance_.objective() == Objective::OpenDimensionY
            && instance_.number_of_bins() != 1) {
        throw std::invalid_argument(
                "rectangleguillotine::InstanceBuilder::build."
                " The instance has objective OpenDimensionY and contains " + std::to_string(instance_.number_of_bins()) + " bins;"
                " an instance with objective OpenDimensionY must contain exactly one bin.");
    }

    return std::move(instance_);
}
