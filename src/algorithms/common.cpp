#include "packingsolver/algorithms/common.hpp"

using namespace packingsolver;

std::istream& packingsolver::operator>>(
        std::istream& in,
        OptimizationMode& optimization_mode)
{
    std::string token;
    in >> token;
    if (token == "anytime") {
        optimization_mode = OptimizationMode::Anytime;
    } else if (token == "not-anytime") {
        optimization_mode = OptimizationMode::NotAnytime;
    } else if (token == "not-anytime-sequential") {
        optimization_mode = OptimizationMode::NotAnytimeSequential;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(
        std::ostream& os,
        OptimizationMode optimization_mode)
{
    switch (optimization_mode) {
    case OptimizationMode::Anytime: {
        os << "Anytime";
        break;
    } case OptimizationMode::NotAnytime: {
        os << "Not anytime";
        break;
    } case OptimizationMode::NotAnytimeSequential: {
        os << "Not anytime, sequential";
        break;
    }
    }
    return os;
}

std::istream& packingsolver::operator>>(
        std::istream& in,
        ProblemType& problem_type)
{
    std::string token;
    in >> token;
    if (token == "rectangleguillotine" || token == "RG") {
        problem_type = ProblemType::RectangleGuillotine;
    } else if (token == "rectangle" || token == "R") {
        problem_type = ProblemType::Rectangle;
    } else if (token == "onedimensional" || token == "1") {
        problem_type = ProblemType::OneDimensional;
    } else if (token == "boxstacks" || token == "BS") {
        problem_type = ProblemType::BoxStacks;
    } else if (token == "irregular" || token == "I") {
        problem_type = ProblemType::Irregular;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(
        std::ostream &os,
        ProblemType problem_type)
{
    switch (problem_type) {
    case ProblemType::RectangleGuillotine: {
        os << "RectangleGuillotine";
        break;
    } case ProblemType::Rectangle: {
        os << "Rectangle";
        break;
    } case ProblemType::OneDimensional: {
        os << "OneDimensional";
        break;
    } case ProblemType::BoxStacks: {
        os << "BoxStacks";
        break;
    } case ProblemType::Irregular: {
        os << "Irregular";
        break;
    }
    }
    return os;
}

std::istream& packingsolver::operator>>(
        std::istream& in,
        Objective& objective)
{
    std::string token;
    in >> token;
    if (token == "default") {
        objective = Objective::Default;
    } else if (token == "bin-packing" || token == "BPP") {
        objective = Objective::BinPacking;
    } else if (token == "bin-packing-with-leftovers" || token == "BPPL") {
        objective = Objective::BinPackingWithLeftovers;
    } else if (token == "open-dimension-x" || token == "ODX") {
        objective = Objective::OpenDimensionX;
    } else if (token == "open-dimension-y" || token == "ODY") {
        objective = Objective::OpenDimensionY;
    } else if (token == "knapsack" || token == "KP") {
        objective = Objective::Knapsack;
    } else if (token == "variable-sized-bin-packing" || token == "VBPP") {
        objective = Objective::VariableSizedBinPacking;
    } else if (token == "sequential-onedimensional-rectangle-subproblem" || token == "BDRS") {
        objective = Objective::SequentialOneDimensionalRectangleSubproblem;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(
        std::ostream& os,
        Objective objective)
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
    } case Objective::OpenDimensionX: {
        os << "OpenDimensionX";
        break;
    } case Objective::OpenDimensionY: {
        os << "OpenDimensionY";
        break;
    } case Objective::OpenDimensionXY: {
        os << "OpenDimensionXY";
        break;
    } case Objective::Knapsack: {
        os << "Knapsack";
        break;
    } case Objective::VariableSizedBinPacking: {
        os << "VariableSizedBinPacking";
        break;
    } case Objective::SequentialOneDimensionalRectangleSubproblem: {
        os << "SequentialOneDimensionalRectangleSubproblem";
        break;
    }
    }
    return os;
}

std::istream& packingsolver::operator>>(
        std::istream& in,
        Direction& o)
{
    std::string token;
    in >> token;
    if (token == "x") {
        o = Direction::X;
    } else if (token == "y") {
        o = Direction::Y;
    } else if (token == "any") {
        o = Direction::Any;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(
        std::ostream& os,
        Direction o)
{
    switch (o) {
    case Direction::X: {
        os << "X";
        break;
    } case Direction::Y: {
        os << "Y";
        break;
    } case Direction::Any: {
        os << "Any";
        break;
    }
    }
    return os;
}
