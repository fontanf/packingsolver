#include "packingsolver/rectangleguillotine/instance.hpp"

#include "packingsolver/rectangleguillotine/solution.hpp"

#include <iostream>
#include <fstream>
#include <climits>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

/******************************************************************************/

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in, CutType1& cut_type_1)
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
        std::istream& in, CutType2& cut_type_2)
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
        std::istream& in, CutOrientation& o)
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
        std::ostream &os, CutType1 cut_type_1)
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
        std::ostream &os, CutType2 cut_type_2)
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
        std::ostream &os, CutOrientation o)
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

bool rectangleguillotine::rect_intersection(Coord c1, Rectangle r1, Coord c2, Rectangle r2)
{
    return c1.x + r1.w > c2.x
        && c2.x + r2.w > c1.x
        && c1.y + r1.h > c2.y
        && c2.y + r2.h > c1.y;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, Coord xy)
{
    os << xy.x << " " << xy.y;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, Rectangle r)
{
    os << r.w << " " << r.h;
    return os;
}

/******************************************************************************/

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const ItemType& item_type)
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
        std::ostream &os, const BinType& bin_type)
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
        std::ostream &os, const Defect& defect)
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

/********************************** Instance **********************************/

Area Instance::previous_bin_area(BinPos i_pos) const
{
    assert(i_pos < bin_number_);
    const BinType& b = bin(i_pos);
    return b.previous_bin_area + b.rect.area() * (i_pos - b.previous_bin_copies);
}

void Instance::add_item_type(Length w, Length h, Profit p, ItemPos copies, bool oriented, bool new_stack)
{
    ItemType item_type;
    item_type.id       = item_types_.size();
    item_type.rect.w   = w;
    item_type.rect.h   = h;
    item_type.profit   = (p == -1)? w * h: p;
    item_type.copies   = copies;
    item_type.stack    = (new_stack)? stack_number(): stack_number() - 1;
    item_type.oriented = oriented;
    item_types_.push_back(item_type);

    if (item_type.copies != 1)
        all_item_type_one_copy_ = false;
    item_number_ += copies; // Update item_number_
    length_sum_ += item_type.copies * std::max(item_type.rect.w, item_type.rect.h); // Update length_sum_

    // Update stacks_
    if (new_stack) {
        stacks_.push_back({item_type});
        stack_sizes_.push_back({copies});
    } else {
        stacks_.back().push_back(item_type);
        stack_sizes_.back() += copies;
    }

    // Compute item area and profit
    item_area_   += item_type.copies * item_type.rect.area();
    item_profit_ += item_type.copies * item_type.profit;
    if (max_efficiency_item_ == -1
            || (item_types_[max_efficiency_item_].profit * item_type.rect.area()
                < item_type.profit * item_types_[max_efficiency_item_].rect.area()))
        max_efficiency_item_ = item_type.id;
}

void Instance::add_bin_type(Length w, Length h, Profit cost, BinPos copies)
{
    BinType bin_type;
    bin_type.id = bin_types_.size();
    bin_type.rect.w = w;
    bin_type.rect.h = h;
    bin_type.cost = (cost == -1)? w * h: cost;
    bin_type.copies = copies;
    bin_type.previous_bin_area = (bin_number() == 0)? 0:
        bin_types_.back().previous_bin_area + bin_types_.back().rect.area() * bin_types_.back().copies;
    bin_type.previous_bin_copies = (bin_number() == 0)? 0:
        bin_types_.back().previous_bin_copies + bin_types_.back().copies;
    bin_types_.push_back(bin_type);

    if (bin_type.copies != 1)
        all_bin_type_one_copy_ = false;
    bin_number_ += copies; // Update bin_number_
    packable_area_ += bin_types_.back().copies * bin_types_.back().rect.area(); // Update packable_area_;
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

void Instance::set_bin_infinite_width()
{
    for (BinTypeId i = 0; i < bin_type_number(); ++i)
        bin_types_[i].rect.w = length_sum_;
}

void Instance::set_bin_infinite_height()
{
    for (BinTypeId i = 0; i < bin_type_number(); ++i)
        bin_types_[i].rect.h = length_sum_;
}

void Instance::set_bin_infinite_copies()
{
    for (BinTypeId i = 0; i < bin_type_number(); ++i) {
        bin_number_ += item_number_ - bin_types_[i].copies;
        bin_types_[i].copies = item_number_;
    }
    all_bin_type_one_copy_ = false;
}

void Instance::set_item_infinite_copies()
{
    for (StackId s = 0; s < stack_number(); ++s) {
        for (ItemType& item: stacks_[s]) {
            item_number_    -= item.copies;
            item_area_      -= item.copies * item.rect.area();
            item_profit_    -= item.copies * item.profit;
            length_sum_     -= item.copies * std::max(item.rect.w, item.rect.h);
            stack_sizes_[s] -= item.copies;

            ItemPos c = (bin_types_[0].rect.area() - 1) / item.rect.area() + 1;
            item.copies = c;
            item_types_[item.id].copies = c;

            item_number_    += item.copies;
            length_sum_     += item.copies * std::max(item.rect.w, item.rect.h);
            item_area_      += item.copies * item.rect.area();
            item_profit_    += item.copies * item.profit;
            stack_sizes_[s] += item.copies;
        }
    }
    all_item_type_one_copy_ = false;
    all_item_type_infinite_copies = true;
}

void Instance::set_bin_unweighted()
{
    for (BinTypeId i = 0; i < bin_type_number(); ++i)
        bin_types_[i].cost = bin_types_[i].rect.area();
}

void Instance::set_unweighted()
{
    for (ItemTypeId j = 0; j < item_type_number(); ++j)
        item_types_[j].profit = item_types_[j].rect.area();
}

Instance::Instance(
        Objective objective,
        std::string items_filepath,
        std::string bins_filepath,
        std::string defects_filepath):
    objective_(objective)
{
    std::ifstream f_items(items_filepath);
    std::ifstream f_defects(defects_filepath);
    std::ifstream f_bins(bins_filepath);
    if (!f_items.good())
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << items_filepath << "\"" << "\033[0m" << std::endl;
    if (!f_bins.good())
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << bins_filepath << "\"" << "\033[0m" << std::endl;
    if (!f_items.good() || !f_bins.good() || (defects_filepath != "" && !f_defects.good()))
        return;

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    // read batch file
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
        if (w == -1)
            std::cerr << "\033[31m" << "ERROR, \"WIDTH\" not defined in \"" << items_filepath << "\"" << "\033[0m" << std::endl;
        if (h == -1)
            std::cerr << "\033[31m" << "ERROR, \"HEIGHT\" not defined in \"" << items_filepath << "\"" << "\033[0m" << std::endl;
        if (p == -1)
            p = w * h;
        add_item_type(w, h, p, c, oriented, new_stack);
    }

    // read bin file
    getline(f_bins, tmp);
    labels = optimizationtools::split(tmp, ',');
    while (getline(f_bins, tmp)) {
        line = optimizationtools::split(tmp, ',');
        Length w = -1;
        Length h = -1;
        Profit cost = -1;
        BinPos c = 1;
        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "WIDTH") {
                w = (Length)std::stol(line[i]);
            } else if (labels[i] == "HEIGHT") {
                h = (Length)std::stol(line[i]);
            } else if (labels[i] == "COST") {
                cost = (Profit)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                c = (BinPos)std::stol(line[i]);
            }
        }
        if (w == -1)
            std::cerr << "\033[31m" << "ERROR, \"WIDTH\" not defined in \"" << bins_filepath << "\"" << "\033[0m" << std::endl;
        if (h == -1)
            std::cerr << "\033[31m" << "ERROR, \"HEIGHT\" not defined in \"" << bins_filepath << "\"" << "\033[0m" << std::endl;
        add_bin_type(w, h, cost, c);
    }

    // read defects file
    if (defects_filepath != "") {
        getline(f_defects, tmp);
        labels = optimizationtools::split(tmp, ',');
        while (getline(f_defects, tmp)) {
            line = optimizationtools::split(tmp, ',');
            BinTypeId i = -1;
            Length    x = -1;
            Length    y = -1;
            Length    w = -1;
            Length    h = -1;
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
            if (i == -1)
                std::cerr << "\033[31m" << "ERROR, \"BIN\" not defined in \"" << defects_filepath << "\"" << "\033[0m" << std::endl;
            if (x == -1)
                std::cerr << "\033[31m" << "ERROR, \"X\" not defined in \"" << defects_filepath << "\"" << "\033[0m" << std::endl;
            if (y == -1)
                std::cerr << "\033[31m" << "ERROR, \"Y\" not defined in \"" << defects_filepath << "\"" << "\033[0m" << std::endl;
            if (w == -1)
                std::cerr << "\033[31m" << "ERROR, \"WIDTH\" not defined in \"" << defects_filepath << "\"" << "\033[0m" << std::endl;
            if (h == -1)
                std::cerr << "\033[31m" << "ERROR, \"HEIGHT\" not defined in \"" << defects_filepath << "\"" << "\033[0m" << std::endl;
            add_defect(i, x, y, w, h);
        }
    }
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const Instance& instance)
{
    os
        << "objective " << instance.objective() << std::endl
        << "item type number " << instance.item_type_number() << " - item number " << instance.item_number() << std::endl
        << "bin type number " << instance.bin_type_number() << " - bin number " << instance.bin_number() << std::endl
        << "defect number " << instance.defect_number() << std::endl;

    os << "Items" << std::endl;
    for (ItemTypeId j = 0; j < instance.item_type_number(); ++j)
        os << instance.item_type(j) << std::endl;

    //os << "Stacks" << std::endl;
    //for (StackId s = 0; s < instance.stack_number(); ++s) {
    //    os << "Stack " << s << std::endl;
    //    for (ItemPos pos = 0; pos < instance.stack_size(s); ++pos)
    //        os << instance.item(s, pos) << std::endl;
    //}

    os << "Bins" << std::endl;
    for (BinTypeId i = 0; i < instance.bin_type_number(); ++i)
        os << instance.bin_type(i) << std::endl;

    os << "Defects" << std::endl;
    for (DefectId k = 0; k < instance.defect_number(); ++k)
        os << instance.defect(k) << std::endl;

    os << "Defects by bins" << std::endl;
    for (BinTypeId i = 0; i < instance.bin_type_number(); ++i) {
        os << "Bin " << i << std::endl;
        for (const Defect& defect: instance.bin_type(i).defects)
            os << defect << std::endl;
    }

    return os;
}

Counter Instance::state_number() const
{
    Counter val = 1;
    for (StackId s=0; s<stack_number(); ++s) {
        if (val > LLONG_MAX / (stack_size(s) + 1))
            return LLONG_MAX;
        val *= (stack_size(s) + 1);
    }
    return val;
}

void Instance::write(std::string filepath) const
{
    // Export items
    std::ofstream f_items(filepath + "_items.csv");
    if (!f_items.good()) {
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << filepath + "_items.csv" << "\"" << "\033[0m" << std::endl;
        return;
    }
    f_items << "ID,WIDTH,HEIGHT,PROFIT,COPIES,ORIENTED,NEWSTACK" << std::endl;
    for (ItemTypeId j = 0; j < item_type_number(); ++j) {
        const ItemType& it = item_type(j);
        f_items << j << "," << it.rect.w << "," << it.rect.h << "," << it.profit << "," << it.copies << "," << it.oriented << "," << (j == 0 || it.stack != item_type(j - 1).stack) << std::endl;
    }

    // Export bins
    std::ofstream f_bins(filepath + "_bins.csv");
    if (!f_bins.good()) {
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << filepath + "_bins.csv" << "\"" << "\033[0m" << std::endl;
        return;
    }
    f_bins << "ID,WIDTH,HEIGHT" << std::endl;
    for (BinTypeId i = 0; i < bin_type_number(); ++i) {
        const BinType& bi = bin(i);
        f_bins << i << "," << bi.rect.w << "," << bi.rect.h << std::endl;
    }

    // Export Defects
    if (defect_number() > 0) {
        std::ofstream f_defects(filepath + "_defects.csv");
        if (!f_defects.good()) {
            std::cerr << "\033[31m" << "ERROR, unable to open file \"" << filepath + "_defects.csv" << "\"" << "\033[0m" << std::endl;
            return;
        }
        f_defects << "ID,BIN,X,Y,WIDTH,HEIGHT" << std::endl;
        for (DefectId k = 0; k < defect_number(); ++k) {
            const Defect& de = defect(k);
            f_defects << k << "," << de.bin_id << "," << de.pos.x << "," << de.pos.y << "," << de.rect.w << "," << de.rect.h << std::endl;
        }
    }
}

/******************************************************************************/

DefectId Instance::rect_intersects_defect(
        Length l, Length r, Length b, Length t, BinTypeId i, CutOrientation o) const
{
    assert(l <= r);
    assert(b <= t);
    for (const Defect& defect: bin(i).defects) {
        if (left(defect, o) >= r)
            continue;
        if (l >= right(defect, o))
            continue;
        if (bottom(defect, o) >= t)
            continue;
        if (b >= top(defect, o))
            continue;
        return defect.id;
    }
    return -1;
}

DefectId Instance::item_intersects_defect(
        Length l, Length b, const ItemType& item_type, bool rotate, BinTypeId i, CutOrientation o) const
{
    return rect_intersects_defect(
            l, l + width(item_type, rotate, o),
            b, b + height(item_type, rotate, o),
            i, o);
}

DefectId Instance::y_intersects_defect(
        Length l, Length r, Length y, BinTypeId i, CutOrientation o) const
{
    DefectId k_min = -1;
    for (const Defect& k: bin(i).defects) {
        if (right(k, o) <= l || left(k, o) >= r)
            continue;
        if (bottom(k, o) >= y || top(k, o) <= y)
            continue;
        if (k_min == -1 || left(k, o) < left(defect(k_min), o))
            k_min = k.id;
    }
    return k_min;
}

DefectId Instance::x_intersects_defect(Length x, BinTypeId i, CutOrientation o) const
{
    for (const Defect& k: bin(i).defects)
        if (left(k, o) < x && right(k, o) > x)
            return k.id;
    return -1;
}

