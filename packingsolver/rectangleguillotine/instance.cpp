#include "packingsolver/rectangleguillotine/instance.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>

using namespace packingsolver;
using namespace packingsolver::rectangleguillotine;

std::istream& packingsolver::rectangleguillotine::operator>>(
        std::istream& in,
        CutType1& cut_type_1)
{
    std::string token;
    in >> token;
    if (token == "two-staged-guillotine"
            || token == "TWO_STAGED_GUILLOTINE"
            || token == "TwoStagedGuillotine") {
        cut_type_1 = CutType1::TwoStagedGuillotine;
    } else if (token == "three-staged-guillotine"
            || token == "THREE_STAGED_GUILLOTINE"
            || token == "ThreeStagedGuillotine") {
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
        std::ostream& os,
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
        std::ostream& os,
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
        std::ostream& os,
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
        std::ostream& os,
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
        Coord c1,
        Rectangle r1,
        Coord c2,
        Rectangle r2)
{
    return c1.x + r1.w > c2.x
        && c2.x + r2.w > c1.x
        && c1.y + r1.h > c2.y
        && c2.y + r2.h > c1.y;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        Coord xy)
{
    os << xy.x << " " << xy.y;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        Rectangle r)
{
    os << r.w << " " << r.h;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const ItemType& item_type)
{
    os
        << "item type id " << item_type.id
        << " w " << item_type.rect.w
        << " h " << item_type.rect.h
        << " profit " << item_type.profit
        << " copies " << item_type.copies
        << " stack_id " << item_type.stack_id
        << " oriented " << item_type.oriented
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const BinType& bin_type)
{
    os
        << "bin type id " << bin_type.id
        << " w " << bin_type.rect.w
        << " h " << bin_type.rect.h
        << " copies " << bin_type.copies
        ;
    return os;
}

std::ostream& packingsolver::rectangleguillotine::operator<<(
        std::ostream& os,
        const Defect& defect)
{
    os
        << "id " << defect.id
        << " bin_type_id " << defect.bin_type_id
        << " x " << defect.pos.x
        << " y " << defect.pos.y
        << " w " << defect.rect.w
        << " h " << defect.rect.h
        ;
    return os;
}

void Instance::write(
        const std::string& instance_path) const
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
    f_items << "ID,WIDTH,HEIGHT,PROFIT,COPIES,ORIENTED,STACK_ID" << std::endl;
    for (ItemTypeId item_type_id = 0;
            item_type_id < number_of_item_types();
            ++item_type_id) {
        const ItemType& item_type = this->item_type(item_type_id);
        f_items << item_type_id << ","
            << item_type.rect.w << ","
            << item_type.rect.h << ","
            << item_type.profit << ","
            << item_type.copies << ","
            << item_type.oriented << ","
            << item_type.stack_id << std::endl;
    }

    // Export bins.
    f_bins << "ID,WIDTH,HEIGHT" << std::endl;
    for (BinTypeId bin_type_id = 0;
            bin_type_id < number_of_bin_types();
            ++bin_type_id) {
        const BinType& bin_type = this->bin_type(bin_type_id);
        f_bins
            << bin_type_id << ","
            << bin_type.rect.w << ","
            << bin_type.rect.h << std::endl;
    }

    // Export defects.
    if (number_of_defects() > 0) {
        f_defects << "ID,BIN,X,Y,WIDTH,HEIGHT" << std::endl;
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            for (DefectId defect_id = 0;
                    defect_id < (DefectId)bin_type.defects.size();
                    ++defect_id) {
                const Defect& defect = bin_type.defects[defect_id];
                f_defects
                    << defect_id << ","
                    << defect.bin_type_id << ","
                    << defect.pos.x << ","
                    << defect.pos.y << ","
                    << defect.rect.w << ","
                    << defect.rect.h << std::endl;
            }
        }
    }
}

std::ostream& Instance::format(
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
            << "Cut through defects:      " << cut_through_defects() << std::endl
            << "Cut thickness:            " << cut_thickness() << std::endl;
    }

    if (verbose >= 2) {
        os
            << std::endl
            << std::setw(12) << "Bin type"
            << std::setw(12) << "Width"
            << std::setw(12) << "Height"
            << std::setw(12) << "Cost"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Copies min"
            << std::endl
            << std::setw(12) << "--------"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
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
                << std::setw(12) << bin_type.rect.w
                << std::setw(12) << bin_type.rect.h
                << std::setw(12) << bin_type.cost
                << std::setw(12) << bin_type.copies
                << std::setw(12) << bin_type.copies_min
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
        for (BinTypeId bin_type_id = 0;
                bin_type_id < number_of_bin_types();
                ++bin_type_id) {
            const BinType& bin_type = this->bin_type(bin_type_id);
            os
                << std::setw(12) << bin_type_id
                << std::setw(8) << bin_type.left_trim
                << std::setw(4) << bin_type.left_trim_type
                << std::setw(8) << bin_type.right_trim
                << std::setw(4) << bin_type.right_trim_type
                << std::setw(8) << bin_type.bottom_trim
                << std::setw(4) << bin_type.bottom_trim_type
                << std::setw(8) << bin_type.top_trim
                << std::setw(4) << bin_type.top_trim_type
                << std::endl;
        }

        if (number_of_defects() > 0) {
            os
                << std::endl
                << std::setw(12) << "Defect"
                << std::setw(12) << "Bin type"
                << std::setw(12) << "X"
                << std::setw(12) << "Y"
                << std::setw(12) << "width"
                << std::setw(12) << "Height"
                << std::endl
                << std::setw(12) << "------"
                << std::setw(12) << "--------"
                << std::setw(12) << "-"
                << std::setw(12) << "-"
                << std::setw(12) << "-----"
                << std::setw(12) << "------"
                << std::endl;
            for (BinTypeId bin_type_id = 0;
                    bin_type_id < number_of_bin_types();
                    ++bin_type_id) {
                const BinType& bin_type = this->bin_type(bin_type_id);
                for (DefectId defect_id = 0;
                        defect_id < (DefectId)bin_type.defects.size();
                        ++defect_id) {
                    const Defect& defect = bin_type.defects[defect_id];
                    os
                        << std::setw(12) << defect_id
                        << std::setw(12) << defect.bin_type_id
                        << std::setw(12) << defect.pos.x
                        << std::setw(12) << defect.pos.y
                        << std::setw(12) << defect.rect.w
                        << std::setw(12) << defect.rect.h
                        << std::endl;
                }
            }
        }

        os
            << std::endl
            << std::setw(12) << "Item type"
            << std::setw(12) << "Width"
            << std::setw(12) << "Height"
            << std::setw(12) << "Profit"
            << std::setw(12) << "Copies"
            << std::setw(12) << "Oriented"
            << std::setw(12) << "Stack id"
            << std::endl
            << std::setw(12) << "---------"
            << std::setw(12) << "-----"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "------"
            << std::setw(12) << "--------"
            << std::setw(12) << "-----"
            << std::endl;
        for (ItemTypeId item_type_id = 0;
                item_type_id < number_of_item_types();
                ++item_type_id) {
            const ItemType& item_type = this->item_type(item_type_id);
            os
                << std::setw(12) << item_type_id
                << std::setw(12) << item_type.rect.w
                << std::setw(12) << item_type.rect.h
                << std::setw(12) << item_type.profit
                << std::setw(12) << item_type.copies
                << std::setw(12) << item_type.oriented
                << std::setw(12) << item_type.stack_id
                << std::endl;
        }
    }

    return os;
}

