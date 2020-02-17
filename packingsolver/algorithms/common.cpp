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
    } else if (token == "strip-packing" || token == "SPP") {
        objective = Objective::StripPacking;
    } else if (token == "knapsack" || token == "KP") {
        objective = Objective::Knapsack;
    } else if (token == "bin-packing-with-leftovers" || token == "BPPL") {
        objective = Objective::BinPackingLeftovers;
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
    } case Objective::StripPacking: {
        os << "StripPacking";
        break;
    } case Objective::Knapsack: {
        os << "Knapsack";
        break;
    } case Objective::BinPackingLeftovers: {
        os << "BinPackingLeftovers";
        break;
    }
    }
    return os;
}

