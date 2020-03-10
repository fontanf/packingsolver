#include "packingsolver/rectangleguillotine/instance.hpp"

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
        std::ostream &os, const Item& item)
{
    os
        << "j " << item.id
        << " w " << item.rect.w
        << " h " << item.rect.h
        << " p " << item.profit
        << " copies " << item.copies
        << " stack " << item.stack
        << " oriented " << item.oriented
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const Bin& bin)
{
    os
        << "i " << bin.id
        << " w " << bin.rect.w
        << " h " << bin.rect.h
        << " copies " << bin.copies
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
    const Bin& b = bin(i_pos);
    return b.previous_bin_area + b.rect.area() * (i_pos - b.previous_bin_copies);
}

void Instance::add_item(Length w, Length h, Profit p, ItemPos copies, bool oriented, bool new_stack)
{
    Item item;
    item.id       = items_.size();
    item.rect.w   = w;
    item.rect.h   = h;
    item.profit   = (p == -1)? w * h: p;
    item.copies   = copies;
    item.stack    = (new_stack)? stack_number(): stack_number() - 1;
    item.oriented = oriented;
    items_.push_back(item);

    if (item.copies != 1)
        all_item_type_one_copy_ = false;
    item_number_ += copies; // Update item_number_
    length_sum_ += item.copies * std::max(item.rect.w, item.rect.h); // Update length_sum_

    // Update stacks_
    if (new_stack) {
        stacks_.push_back({item});
        stack_sizes_.push_back({copies});
    } else {
        stacks_.back().push_back(item);
        stack_sizes_.back() += copies;
    }

    // Compute item area and profit
    item_area_   += item.copies * item.rect.area();
    item_profit_ += item.copies * item.profit;
    if (max_efficiency_item_ == -1
            || (items_[max_efficiency_item_].profit * item.rect.area()
                < item.profit * items_[max_efficiency_item_].rect.area()))
        max_efficiency_item_ = item.id;
}

void Instance::add_bin(Length w, Length h, BinPos copies)
{
    Bin bin;
    bin.id = bins_.size();
    bin.rect.w = w;
    bin.rect.h = h;
    bin.copies = copies;
    bin.previous_bin_area = (bin_number() == 0)? 0:
        bins_.back().previous_bin_area + bins_.back().rect.area() * bins_.back().copies;
    bin.previous_bin_copies = (bin_number() == 0)? 0:
        bins_.back().previous_bin_copies + bins_.back().copies;
    bins_.push_back(bin);

    if (bin.copies != 1)
        all_bin_type_one_copy_ = false;
    bin_number_ += copies; // Update bin_number_
    packable_area_ += bins_.back().rect.area(); // Update packable_area_;
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

    bins_[i].defects.push_back(defect);

    // Update packable_area_ and defect_area_
    // TODO
}

void Instance::set_bin_infinite_width()
{
    for (BinTypeId i = 0; i < bin_type_number(); ++i)
        bins_[i].rect.w = length_sum_;
}

void Instance::set_bin_infinite_height()
{
    for (BinTypeId i = 0; i < bin_type_number(); ++i)
        bins_[i].rect.h = length_sum_;
}

void Instance::set_bin_infinite_copies()
{
    for (BinTypeId i = 0; i < bin_type_number(); ++i) {
        bin_number_ += item_number_ - bins_[i].copies;
        bins_[i].copies = item_number_;
    }
    all_bin_type_one_copy_ = false;
}

void Instance::set_unweighted()
{
    for (ItemTypeId j = 0; j < item_type_number(); ++j)
        items_[j].profit = items_[j].rect.area();
}

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

std::vector<std::string> split(std::string line)
{
    std::vector<std::string> v;
    std::stringstream ss(line);
    std::string tmp;
    while (getline(ss, tmp, ',')) {
        rtrim(tmp);
        v.push_back(tmp);
    }
    return v;
}

Instance::Instance(
        Objective objective,
        std::string items_filename,
        std::string bins_filename,
        std::string defects_filename):
    objective_(objective)
{
    std::ifstream f_items(items_filename);
    std::ifstream f_defects(defects_filename);
    std::ifstream f_bins(bins_filename);
    if (!f_items.good())
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << items_filename << "\"" << "\033[0m" << std::endl;
    if (!f_bins.good())
        std::cerr << "\033[31m" << "ERROR, unable to open file \"" << bins_filename << "\"" << "\033[0m" << std::endl;
    if (!f_items.good() || !f_bins.good() || (defects_filename != "" && !f_defects.good()))
        return;

    std::string tmp;
    std::vector<std::string> line;
    std::vector<std::string> labels;

    // read batch file
    getline(f_items, tmp);
    labels = split(tmp);
    while (getline(f_items, tmp)) {
        line = split(tmp);
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
            std::cerr << "\033[31m" << "ERROR, \"WIDTH\" not defined in \"" << items_filename << "\"" << "\033[0m" << std::endl;
        if (h == -1)
            std::cerr << "\033[31m" << "ERROR, \"HEIGHT\" not defined in \"" << items_filename << "\"" << "\033[0m" << std::endl;
        if (p == -1)
            p = w * h;
        add_item(w, h, p, c, oriented, new_stack);
    }

    // read bin file
    getline(f_bins, tmp);
    labels = split(tmp);
    while (getline(f_bins, tmp)) {
        line = split(tmp);
        Length w = -1;
        Length h = -1;
        BinPos c = 1;
        for (Counter i = 0; i < (Counter)line.size(); ++i) {
            if (labels[i] == "WIDTH") {
                w = (Length)std::stol(line[i]);
            } else if (labels[i] == "HEIGHT") {
                h = (Length)std::stol(line[i]);
            } else if (labels[i] == "COPIES") {
                c = (BinPos)std::stol(line[i]);
            }
        }
        if (w == -1)
            std::cerr << "\033[31m" << "ERROR, \"WIDTH\" not defined in \"" << bins_filename << "\"" << "\033[0m" << std::endl;
        if (h == -1)
            std::cerr << "\033[31m" << "ERROR, \"HEIGHT\" not defined in \"" << bins_filename << "\"" << "\033[0m" << std::endl;
        add_bin(w, h, c);
    }

    // read defects file
    if (defects_filename != "") {
        getline(f_defects, tmp);
        labels = split(tmp);
        while (getline(f_defects, tmp)) {
            line = split(tmp);
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
                std::cerr << "\033[31m" << "ERROR, \"BIN\" not defined in \"" << defects_filename << "\"" << "\033[0m" << std::endl;
            if (x == -1)
                std::cerr << "\033[31m" << "ERROR, \"X\" not defined in \"" << defects_filename << "\"" << "\033[0m" << std::endl;
            if (y == -1)
                std::cerr << "\033[31m" << "ERROR, \"Y\" not defined in \"" << defects_filename << "\"" << "\033[0m" << std::endl;
            if (w == -1)
                std::cerr << "\033[31m" << "ERROR, \"WIDTH\" not defined in \"" << defects_filename << "\"" << "\033[0m" << std::endl;
            if (h == -1)
                std::cerr << "\033[31m" << "ERROR, \"HEIGHT\" not defined in \"" << defects_filename << "\"" << "\033[0m" << std::endl;
            add_defect(i, x, y, w, h);
        }
    }
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream &os, const Instance& ins)
{
    os
        << "objective " << ins.objective() << std::endl
        << "item type number " << ins.item_type_number() << " - item number " << ins.item_number() << std::endl
        << "bin type number " << ins.bin_type_number() << " - bin number " << ins.bin_number() << std::endl
        << "defect number " << ins.defect_number() << std::endl;

    os << "Items" << std::endl;
    for (ItemTypeId j = 0; j < ins.item_type_number(); ++j)
        os << ins.item(j) << std::endl;

    os << "Stacks" << std::endl;
    for (StackId s = 0; s < ins.stack_number(); ++s) {
        os << "Stack " << s << std::endl;
        for (ItemPos pos = 0; pos < ins.stack_size(s); ++pos)
            os << ins.item(s, pos) << std::endl;
    }

    os << "Bins" << std::endl;
    for (BinTypeId i = 0; i < ins.bin_type_number(); ++i)
        os << ins.bin_type(i) << std::endl;

    os << "Defects" << std::endl;
    for (DefectId k = 0; k < ins.defect_number(); ++k)
        os << ins.defect(k) << std::endl;

    os << "Defects by bins" << std::endl;
    for (BinTypeId i = 0; i < ins.bin_number(); ++i) {
        os << "Bin " << i << std::endl;
        for (const Defect& defect: ins.bin(i).defects)
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

/******************************************************************************/

DefectId Instance::rect_intersects_defect(
        Length l, Length r, Length b, Length t, BinTypeId i, CutOrientation o) const
{
    assert(l <= r);
    assert(b <= t);
    for (const Defect& defect: bin(i).defects) {
        Length lk = left(defect, o);
        Length rk = right(defect, o);
        assert(lk <= rk);
        if (lk >= r)
            continue;
        if (l >= rk)
            continue;
        Length bk = bottom(defect, o);
        Length tk = top(defect, o);
        assert(b <= t);
        if (bk >= t)
            continue;
        if (b >= tk)
            continue;
        return defect.id;
    }
    return -1;
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

