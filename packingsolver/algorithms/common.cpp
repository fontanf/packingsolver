#include "packingsolver/algorithms/common.hpp"

using namespace packingsolver;

std::istream& packingsolver::operator>>(std::istream& in, ProblemType& problem_type)
{
    std::string token;
    in >> token;
    if (token == "rectangleguillotine" || token == "RG") {
        problem_type = ProblemType::RectangleGuillotine;
    } else if (token == "rectangle" || token == "R") {
        problem_type = ProblemType::Rectangle;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(std::ostream &os, ProblemType problem_type)
{
    switch (problem_type) {
    case ProblemType::RectangleGuillotine: {
        os << "RectangleGuillotine";
        break;
    } case ProblemType::Rectangle: {
        os << "Rectangle";
        break;
    }
    }
    return os;
}

std::istream& packingsolver::operator>>(std::istream& in, Objective& objective)
{
    std::string token;
    in >> token;
    if (token == "default") {
        objective = Objective::Default;
    } else if (token == "bin-packing" || token == "BPP") {
        objective = Objective::BinPacking;
    } else if (token == "bin-packing-with-leftovers" || token == "BPPL") {
        objective = Objective::BinPackingWithLeftovers;
    } else if (token == "strip-packing-width" || token == "SPPW") {
        objective = Objective::StripPackingWidth;
    } else if (token == "strip-packing-height" || token == "SPPH") {
        objective = Objective::StripPackingHeight;
    } else if (token == "knapsack" || token == "KP") {
        objective = Objective::Knapsack;
    } else if (token == "variable-sized-bin-packing" || token == "VBPP") {
        objective = Objective::VariableSizedBinPacking;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(std::ostream &os, Objective objective)
{
    switch (objective) {
    case Objective::Default: {
        os << "Default";
        break;
    } case Objective::BinPacking: {
        os << "BinPacking";
        break;
    } case Objective::BinPackingWithLeftovers: {
        os << "BinPackingWithLeftovers";
        break;
    } case Objective::StripPackingWidth: {
        os << "StripPackingWidth";
        break;
    } case Objective::StripPackingHeight: {
        os << "StripPackingHeight";
        break;
    } case Objective::Knapsack: {
        os << "Knapsack";
        break;
    } case Objective::VariableSizedBinPacking: {
        os << "VariableSizedBinPacking";
        break;
    }
    }
    return os;
}

std::istream& packingsolver::operator>>(
        std::istream& in,
        Orientation& o)
{
    std::string token;
    in >> token;
    if (token == "horizontal") {
        o = Orientation::Horinzontal;
    } else if (token == "vertical") {
        o = Orientation::Vertical;
    } else if (token == "any") {
        o = Orientation::Any;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(
        std::ostream &os,
        Orientation o)
{
    switch (o) {
    case Orientation::Vertical: {
        os << "Vertical";
        break;
    } case Orientation::Horinzontal: {
        os << "Horinzontal";
        break;
    } case Orientation::Any: {
        os << "Any";
        break;
    }
    }
    return os;
}
