#include "packingsolver/rectangleguillotine/instance.hpp"

#include "packingsolver/rectangleguillotine/solution.hpp"

#include <iostream>
#include <fstream>
#include <climits>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in,
        CutType1& cut_type_1)
{
    std::string token;
    in >> token;
    if (token == "two-staged-guillotine") {
        cut_type_1 = CutType1::TwoStagedGuillotine;
    } else if (token == "three-staged-guillotine") {
        cut_type_1 = CutType1::ThreeStagedGuillotine;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in,
        CutType2& cut_type_2)
{
    std::string token;
    in >> token;
    if (token == "roadef2018") {
        cut_type_2 = CutType2::Roadef2018;
    } else if (token == "non-exact") {
        cut_type_2 = CutType2::NonExact;
    } else if (token == "exact") {
        cut_type_2 = CutType2::Exact;
    } else if (token == "homogenous") {
        cut_type_2 = CutType2::Homogenous;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in,
        CutOrientation& o)
{
    std::string token;
    in >> token;
    if (token == "horizontal") {
        o = CutOrientation::Horinzontal;
    } else if (token == "vertical") {
        o = CutOrientation::Vertical;
    } else if (token == "any") {
        o = CutOrientation::Any;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        CutType1 cut_type_1)
{
    switch (cut_type_1) {
    case CutType1::ThreeStagedGuillotine: {
        os << "ThreeStagedGuillotine";
        break;
    } case CutType1::TwoStagedGuillotine: {
        os << "TwoStagedGuillotine";
        break;
    }
    }
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        CutType2 cut_type_2)
{
    switch (cut_type_2) {
    case CutType2::Roadef2018: {
        os << "Roadef2018";
        break;
    } case CutType2::NonExact: {
        os << "NonExact";
        break;
    } case CutType2::Exact: {
        os << "Exact";
        break;
    } case CutType2::Homogenous: {
        os << "Homogenous";
        break;
    }
    }
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        CutOrientation o)
{
    switch (o) {
    case CutOrientation::Vertical: {
        os << "Vertical";
        break;
    } case CutOrientation::Horinzontal: {
        os << "Horinzontal";
        break;
    } case CutOrientation::Any: {
        os << "Any";
        break;
    }
    }
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        TrimType trim_type)
{
    switch (trim_type) {
    case TrimType::Hard: {
        os << "H";
        break;
    } case TrimType::Soft: {
        os << "S";
        break;
    }
    }
    return os;
}

bool rectangleguillotine::rect_intersection(
        Coord c1, Rectangle r1, Coord c2, Rectangle r2)
{
    return c1.x + r1.w > c2.x
        && c2.x + r2.w > c1.x
        && c1.y + r1.h > c2.y
        && c2.y + r2.h > c1.y;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        Coord xy)
{
    os << xy.x << " " << xy.y;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        Rectangle r)
{
    os << r.w << " " << r.h;
    return os;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////// Item type, Bin type, Defect //////////////////////////
////////////////////////////////////////////////////////////////////////////////

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        const ItemType& item_type)
{
    os
        << "j " << item_type.id
        << " w " << item_type.rect.w
        << " h " << item_type.rect.h
        << " p " << item_type.profit
        << " copies " << item_type.copies
        << " stack " << item_type.stack
        << " oriented " << item_type.oriented
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        const BinType& bin_type)
{
    os
        << "i " << bin_type.id
        << " w " << bin_type.rect.w
        << " h " << bin_type.rect.h
        << " copies " << bin_type.copies
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os,
        const Defect& defect)
{
    os
        << "k " << defect.id
        << " i " << defect.bin_id
        << " x " << defect.pos.x
        << " y " << defect.pos.y
        << " w " << defect.rect.w
        << " h " << defect.rect.h
        ;
    return os;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// Instance ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

Area Instance::previous_bin_area(BinPos i_pos) const
{
    assert(i_pos < number_of_bins());
    const BinType& b = bin(i_pos);
    return b.previous_bin_area + b.area() * (i_pos - b.previous_bin_copies);
}

ItemTypeId Instance::add_item_type(
        Length w,
        Length h,
        Profit p,
        ItemPos copies,
        bool oriented,
        bool new_stack)
{
    ItemType item_type;
    item_type.id = item_types_.size();
    item_type.rect.w = w;
    item_type.rect.h = h;
    item_type.profit = (p == -1)? w * h: p;
    item_type.copies = copies;
    item_type.stack = (new_stack)? number_of_stacks(): number_of_stacks() - 1;
    item_type.oriented = oriented;
    item_types_.push_back(item_type);

    // Update number_of_items_.
    number_of_items_ += copies;
    // Update length_sum_.
    length_sum_ += item_type.copies * std::max(item_type.rect.w, item_type.rect.h);

    // Update stacks_.
    if (new_stack) {
        stacks_.push_back({});
        items_pos2type_.push_back({});
    }
    stacks_.back().push_back(item_type);
    for (ItemPos pos = 0; pos < copies; ++pos)
        items_pos2type_.back().push_back(item_type.id);

    // Update item_area_ and item_profit_.
    item_area_ += item_type.copies * item_type.area();
    item_profit_ += item_type.copies * item_type.profit;
    if (max_efficiency_item_ == -1
            || (item_types_[max_efficiency_item_].profit * item_type.area()
                < item_type.profit * item_types_[max_efficiency_item_].area()))
        max_efficiency_item_ = item_type.id;

    return item_type.id;
}

BinTypeId Instance::add_bin_type(
        Length w,
        Length h,
        Profit cost,
        BinPos copies,
        BinPos copies_min)
{
    if (w <= 0) {
        throw std::runtime_error(
                "'rectangleguillotine::Instance::add_bin_type'"
                " requires 'w > 0'.");
    }
    if (h <= 0) {
        throw std::runtime_error(
                "'rectangleguillotine::Instance::add_bin_type'"
                " requires 'h > 0'.");
    }
    if (cost < 0 && cost != -1) {
        throw std::runtime_error(
                "'rectangleguillotine::Instance::add_bin_type'"
                " requires 'cost >= 0' or 'cost == -1'.");
    }
    if (copies <= 0) {
        throw std::runtime_error(
                "'rectangleguillotine::Instance::add_bin_type'"
                " requires 'copies > 0'.");
    }
    if (copies_min < 0) {
        throw std::runtime_error(
                "'rectangleguillotine::Instance::add_bin_type'"
                " requires 'copies_min >= 0'.");
    }
    if (copies_min > copies) {
        throw std::runtime_error(
                "'rectangleguillotine::Instance::add_bin_type'"
                " requires 'copies_min <= copies'.");
    }

    BinType bin_type;
    bin_type.id = bin_types_.size();
    bin_type.rect.w = w;
    bin_type.rect.h = h;
    bin_type.cost = (cost == -1)? w * h: cost;
    bin_type.copies = copies;
    bin_type.copies_min = copies_min;
    bin_type.previous_bin_area = (number_of_bins() == 0)? 0:
        bin_types_.back().previous_bin_area
        + bin_types_.back().area() * bin_types_.back().copies;
    bin_type.previous_bin_copies = (number_of_bins() == 0)? 0:
        bin_types_.back().previous_bin_copies + bin_types_.back().copies;
    bin_types_.push_back(bin_type);

    for (ItemPos pos = 0; pos < copies; ++pos)
        bins_pos2type_.push_back(bin_type.id);
    // Update packable_area_.
    packable_area_ += bin_types_.back().copies * bin_types_.back().area();

    return bin_type.id;
}

void Instance::add_trims(
        BinTypeId i,
        Length left_trim,
        TrimType left_trim_type,
        Length right_trim,
        TrimType right_trim_type,
        Length bottom_trim,
        TrimType bottom_trim_type,
        Length top_trim,
        TrimType top_trim_type)
{
    BinType& bin_type = bin_types_[i];

    if (bottom_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
                " requires 'bottom_trim >= 0'.");
    }
    if (bottom_trim >= bin_type.rect.h) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
                " requires 'bottom_trim < h'.");
    }

    if (top_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
                " requires 'top_trim >= 0'.");
    }
    if (top_trim >= bin_type.rect.h - bottom_trim) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
                " requires 'top_trim < h - bottom_trim'.");
    }

    if (left_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
                " requires 'left_trim >= 0'.");
    }
    if (left_trim >= bin_type.rect.w) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
                " requires 'left_trim < w'.");
    }

    if (right_trim < 0) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
                " requires 'right_trim >= 0'.");
    }
    if (right_trim > bin_type.rect.h - left_trim) {
        throw std::invalid_argument(
                "'rectangleguillotine::Instance::add_trims'"
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

void Instance::add_defect(BinTypeId i, Length x, Length y, Length w, Length h)
{
    Defect defect;
    defect.id = defects_.size();
    defect.bin_id = i;
    defect.pos.x = x;
    defect.pos.y = y;
    defect.rect.w = w;
    defect.rect.h = h;
    defects_.push_back(defect);

    bin_types_[i].defects.push_back(defect);

    // Update packable_area_ and defect_area_
    // TODO
}

void Instance::set_bin_infinite_x()
{
    for (BinTypeId i = 0; i < number_of_bin_types(); ++i)
        bin_types_[i].rect.w = length_sum_;
}

void Instance::set_bin_infinite_y()
{
    for (BinTypeId i = 0; i < number_of_bin_types(); ++i)
        bin_types_[i].rect.h = length_sum_;
}

void Instance::set_bin_infinite_copies()
{
    bins_pos2type_.clear();
    for (BinTypeId i = 0; i < number_of_bin_types(); ++i) {
        bin_types_[i].copies = number_of_items();
        bin_types_[i].previous_bin_area = (i == 0)?
            0:
            bin_types_[i - 1].previous_bin_area
            + bin_types_[i - 1].area() * bin_types_[i - 1].copies;
        bin_types_[i].previous_bin_copies = (i == 0)?
            0:
            bin_types_[i - 1].previous_bin_copies + bin_types_[i - 1].copies;
        for (ItemPos pos = 0; pos < bin_types_[i].copies; ++pos)
            bins_pos2type_.push_back(i);
        packable_area_ += bin_types_[i].copies * bin_types_[i].area();
    }
}

void Instance::set_item_infinite_copies()
{
    for (StackId s = 0; s < number_of_stacks(); ++s) {
        items_pos2type_[s].clear();
        for (ItemType& item: stacks_[s]) {
            number_of_items_ -= item.copies;
            item_area_ -= item.copies * item.area();
            item_profit_ -= item.copies * item.profit;
            length_sum_ -= item.copies * std::max(item.rect.w, item.rect.h);

            ItemPos c = (bin_types_[0].area() - 1) / item.area() + 1;
            item.copies = c;
            item_types_[item.id].copies = c;

            number_of_items_ += item.copies;
            length_sum_ += item.copies * std::max(item.rect.w, item.rect.h);
            item_area_ += item.copies * item.area();
            item_profit_ += item.copies * item.profit;

            for (ItemPos pos = 0; pos < item.copies; ++pos)
                items_pos2type_[s].push_back(item.id);
        }
    }
    all_item_type_infinite_copies_ = true;
}

void Instance::set_bin_unweighted()
{
    for (BinTypeId i = 0; i < number_of_bin_types(); ++i)
        bin_types_[i].cost = bin_types_[i].area();
}

void Instance::set_unweighted()
{
    for (ItemTypeId j = 0; j < number_of_item_types(); ++j)
        item_types_[j].profit = item_types_[j].area();
}

void Instance::read_item_types(std::string items_path)
{
    std::ifstream f_items(items_path);
    if (!f_items.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + items_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f_items, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f_items, tmp)) {
        line = optimizationtools::split(tmp, ',');
        Length w = -1;
        Length h = -1;
        Profit p = -1;
        ItemPos c = 1;
        bool oriented = false;
        bool new_stack = true;
        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "WIDTH") {
                w = (Length)std::stol(line[i]);
            } else if (labels[i] == "HEIGHT") {
                h = (Length)std::stol(line[i]);
            } else if (labels[i] == "PROFIT") {
                p = (Profit)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                c = (ItemPos)std::stol(line[i]);
            } else if (labels[i] == "ORIENTED") {
                oriented = (bool)std::stol(line[i]);
            } else if (labels[i] == "NEWSTACK") {
                new_stack = (bool)std::stol(line[i]);
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
        if (p == -1)
            p = w * h;
        add_item_type(w, h, p, c, oriented, new_stack);
    }
}

void Instance::read_bin_types(std::string bins_path)
{
    std::ifstream f_bins(bins_path);
    if (!f_bins.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + bins_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f_bins, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f_bins, tmp)) {
        line = optimizationtools::split(tmp, ',');
        Length w = -1;
        Length h = -1;
        Profit cost = -1;
        BinPos c = 1;
        BinPos c_min = 0;
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
                c = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "COPIES_MIN") {
                c = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "BOTTOM_TRIM") {
                bottom_trim = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "TOP_TRIM") {
                top_trim = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "LEFT_TRIM") {
                left_trim = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "RIGHT_TRIM") {
                right_trim = (BinPos)std::stol(line[i]);
            } else if (labels[i] == "BOTTOM_TRIM_TYPE") {
                bottom_trim_type = (TrimType)std::stol(line[i]);
            } else if (labels[i] == "TOP_TRIM_TYPE") {
                top_trim_type = (TrimType)std::stol(line[i]);
            } else if (labels[i] == "LEFT_TRIM_TYPE") {
                left_trim_type = (TrimType)std::stol(line[i]);
            } else if (labels[i] == "RIGHT_TRIM_TYPE") {
                right_trim_type = (TrimType)std::stol(line[i]);
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
        BinTypeId i = add_bin_type(w, h, cost, c, c_min);
        add_trims(
                i,
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

void Instance::read_defects(std::string defects_path)
{
    if (defects_path.empty())
        return;

    std::ifstream f_defects(defects_path);
    if (defects_path != "" && !f_defects.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + defects_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f_defects, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f_defects, tmp)) {
        line = optimizationtools::split(tmp, ',');
        BinTypeId i = -1;
        Length x = -1;
        Length y = -1;
        Length w = -1;
        Length h = -1;
        for (Counter c = 0; c < (Counter)line.size(); ++c) {
            if (labels[c] == "BIN") {
                i = (BinTypeId)std::stol(line[c]);
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
        if (i == -1) {
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
        add_defect(i, x, y, w, h);
    }
}

void Instance::read_parameters(std::string parameters_path)
{
    if (parameters_path.empty())
        return;

    std::ifstream f_parameterss(parameters_path);
    if (parameters_path != "" && !f_parameterss.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + parameters_path + "\".");
    }

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    getline(f_parameterss, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f_parameterss, tmp)) {
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
        if (name == "cut_type_1") {
            CutType1 cut_type_1;
            std::stringstream ss(value);
            ss >> cut_type_1;
            set_cut_type_1(cut_type_1);
        } else if (name == "cut_type_2") {
            CutType2 cut_type_2;
            std::stringstream ss(value);
            ss >> cut_type_2;
            set_cut_type_2(cut_type_2);
        } else if (name == "first_stage_orientation") {
            CutOrientation first_stage_orientation;
            std::stringstream ss(value);
            ss >> first_stage_orientation;
            set_first_stage_orientation(first_stage_orientation);
        } else if (name == "min1cut" || name == "min1Cut") {
            set_min1cut(std::stol(value));
        } else if (name == "max1cut" || name == "max1Cut") {
            set_max1cut(std::stol(value));
        } else if (name == "min2cut" || name == "min2Cut") {
            set_min2cut(std::stol(value));
        } else if (name == "max2cut" || name == "max2Cut") {
            set_max2cut(std::stol(value));
        } else if (name == "min_waste" || name == "minwaste" || name == "minWaste") {
            set_min_waste(std::stol(value));
        }
    }
}

void Instance::write(std::string instance_path) const
{
    std::string items_path = instance_path + "_items.csv";
    std::string bins_path = instance_path + "_bins.csv";
    std::string defects_path = instance_path + "_defects.csv";
    std::ofstream f_items(items_path);
    std::ofstream f_bins(bins_path);
    std::ofstream f_defects(defects_path);
    if (!f_items.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + items_path + "\".");
    }
    if (!f_bins.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + bins_path + "\".");
    }
    if (number_of_defects() > 0 && !f_defects.good()) {
        throw std::runtime_error(
                "Unable to open file \"" + defects_path + "\".");
    }

    // Export items.
    f_items << "ID,WIDTH,HEIGHT,PROFIT,COPIES,ORIENTED,NEWSTACK" << std::endl;
    for (ItemTypeId j = 0; j < number_of_item_types(); ++j) {
        const ItemType& it = item_type(j);
        f_items << j << "," << it.rect.w << "," << it.rect.h << "," << it.profit << "," << it.copies << "," << it.oriented << "," << (j == 0 || it.stack != item_type(j - 1).stack) << std::endl;
    }

    // Export bins.
    f_bins << "ID,WIDTH,HEIGHT" << std::endl;
    for (BinTypeId i = 0; i < number_of_bin_types(); ++i) {
        const BinType& bi = bin(i);
        f_bins << i << "," << bi.rect.w << "," << bi.rect.h << std::endl;
    }

    // Export defects.
    if (number_of_defects() > 0) {
        f_defects << "ID,BIN,X,Y,WIDTH,HEIGHT" << std::endl;
        for (DefectId k = 0; k < number_of_defects(); ++k) {
            const Defect& de = defect(k);
            f_defects << k << "," << de.bin_id << "," << de.pos.x << "," << de.pos.y << "," << de.rect.w << "," << de.rect.h << std::endl;
        }
    }
}

void Instance::set_predefined(std::string str)
{
    if (str == "roadef2018") {
        set_roadef2018();
        return;
    }

    if (str.length() != 4) {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter \"" << str << "\" should contain four characters." << "\033[0m" << std::endl;
        if (str.length() < 4)
            return;
    }
    switch (str[0]) {
    case '3': {
        parameters_.cut_type_1 = rectangleguillotine::CutType1::ThreeStagedGuillotine;
        break;
    } case '2': {
        parameters_.cut_type_1 = rectangleguillotine::CutType1::TwoStagedGuillotine;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 1st character \"" << str[0] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
    switch (str[1]) {
    case 'R': {
        parameters_.cut_type_2 = rectangleguillotine::CutType2::Roadef2018;
        break;
    } case 'N': {
        parameters_.cut_type_2 = rectangleguillotine::CutType2::NonExact;
        break;
    } case 'E': {
        parameters_.cut_type_2 = rectangleguillotine::CutType2::Exact;
        break;
    } case 'H': {
        parameters_.cut_type_2 = rectangleguillotine::CutType2::Homogenous;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 2nd character \"" << str[1] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
    switch (str[2]) {
    case 'V': {
        parameters_.first_stage_orientation = rectangleguillotine::CutOrientation::Vertical;
        break;
    } case 'H': {
        parameters_.first_stage_orientation = rectangleguillotine::CutOrientation::Horinzontal;
        break;
    } case 'A': {
        parameters_.first_stage_orientation = rectangleguillotine::CutOrientation::Any;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 3rd character \"" << str[2] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
    switch (str[3]) {
    case 'R': {
        parameters_.no_item_rotation = false;
        break;
    } case 'O': {
        parameters_.no_item_rotation = true;
        break;
    } default: {
        std::cerr << "\033[31m" << "ERROR, predefined branching scheme parameter 4th character \"" << str[3] << "\" invalid." << "\033[0m" << std::endl;
    }
    }
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
            << "Number of stacks:         " << number_of_stacks() << std::endl
            << "Number of defects:        " << number_of_defects() << std::endl
            << "Cut type 1:               " << cut_type_1() << std::endl
            << "Cut type 2:               " << cut_type_2() << std::endl
            << "First stage orientation:  " << first_stage_orientation() << std::endl
            << "min1cut:                  " << min1cut() << std::endl
            << "max1cut:                  " << max1cut() << std::endl
            << "min2cut:                  " << min2cut() << std::endl
            << "max2cut:                  " << max2cut() << std::endl
            << "Minimum waste:            " << min_waste() << std::endl
            << "one2cut:                  " << one2cut() << std::endl
            << "No item rotation:         " << no_item_rotation() << std::endl
            << "Cut through defects:      " << cut_through_defects() << std::endl;
    }

    if (verbose >= 2) {
        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Width"
            << std::setw(12) << "Height"
            << std::setw(12) << "COST"
            << std::setw(12) << "COPIES"
            << std::setw(12) << "COPIES_MIN"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "----"
            << std::setw(12) << "------"
            << std::setw(12) << "----------"
            << std::endl;
        for (BinTypeId i = 0; i < number_of_bin_types(); ++i) {
            os
                << std::setw(12) << i
                << std::setw(12) << bin_type(i).rect.w
                << std::setw(12) << bin_type(i).rect.h
                << std::setw(12) << bin_type(i).cost
                << std::setw(12) << bin_type(i).copies
                << std::setw(12) << bin_type(i).copies_min
                << std::endl;
        }

        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Left trim"
            << std::setw(12) << "Right trim"
            << std::setw(12) << "Bottom trim"
            << std::setw(12) << "Top trim"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "---------"
            << std::setw(12) << "----------"
            << std::setw(12) << "-----------"
            << std::setw(12) << "--------"
            << std::endl;
        for (BinTypeId i = 0; i < number_of_bin_types(); ++i) {
            os
                << std::setw(12) << i
                << std::setw(8) << bin_type(i).left_trim
                << std::setw(4) << bin_type(i).left_trim_type
                << std::setw(8) << bin_type(i).right_trim
                << std::setw(4) << bin_type(i).right_trim_type
                << std::setw(8) << bin_type(i).bottom_trim
                << std::setw(4) << bin_type(i).bottom_trim_type
                << std::setw(8) << bin_type(i).top_trim
                << std::setw(4) << bin_type(i).top_trim_type
                << std::endl;
        }

        if (number_of_defects() > 0) {
            os
                << std::endl
                << std::setw(12) << "Defect"
                << std::setw(12) << "BIN"
                << std::setw(12) << "X"
                << std::setw(12) << "Y"
                << std::setw(12) << "wIDTH"
                << std::setw(12) << "HEIGHT"
                << std::endl
                << std::setw(12) << "------"
                << std::setw(12) << "---"
                << std::setw(12) << "-"
                << std::setw(12) << "-"
                << std::setw(12) << "-----"
                << std::setw(12) << "------"
                << std::endl;
            for (DefectId k = 0; k < number_of_defects(); ++k) {
                os
                    << std::setw(12) << k
                    << std::setw(12) << defect(k).bin_id
                    << std::setw(12) << defect(k).pos.x
                    << std::setw(12) << defect(k).pos.y
                    << std::setw(12) << defect(k).rect.w
                    << std::setw(12) << defect(k).rect.h
                    << std::endl;
            }
        }

        os
            << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "WIDTH"
            << std::setw(12) << "HEIGHT"
            << std::setw(12) << "PROFIT"
            << std::setw(12) << "COPIES"
            << std::setw(12) << "ORIENTED"
            << std::setw(12) << "Stack"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "--------"
            << std::setw(12) << "-----"
            << std::endl;
        for (ItemTypeId j = 0; j < number_of_item_types(); ++j) {
            os
                << std::setw(12) << j
                << std::setw(12) << item_type(j).rect.w
                << std::setw(12) << item_type(j).rect.h
                << std::setw(12) << item_type(j).profit
                << std::setw(12) << item_type(j).copies
                << std::setw(12) << item_type(j).oriented
                << std::setw(12) << item_type(j).stack
                << std::endl;
        }
    }

    return os;
}

