#include "packingsolver/algorithms/common.hpp"

using namespace packingsolver;

std::istream& packingsolver::operator>>(
        std::istream& in,
        OptimizationMode& optimization_mode)
{
    std::string token;
    in >> token;
    if (token == "anytime"
            || token == "Anytime") {
        optimization_mode = OptimizationMode::Anytime;
    } else if (token == "not-anytime"
            || token == "NotAnytime") {
        optimization_mode = OptimizationMode::NotAnytime;
    } else if (token == "not-anytime-deterministic"
            || token == "NotAnytimeDeterministic") {
        optimization_mode = OptimizationMode::NotAnytimeDeterministic;
    } else if (token == "not-anytime-sequential"
            || token == "NotAnytimeSequential") {
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
    } case OptimizationMode::NotAnytimeDeterministic: {
        os << "Not anytime, deterministic";
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
    if (token == "rectangleguillotine"
            || token == "RectangleGuillotine"
            || token == "RG") {
        problem_type = ProblemType::RectangleGuillotine;
    } else if (token == "rectangle"
            || token == "Rectangle"
            || token == "R") {
        problem_type = ProblemType::Rectangle;
    } else if (token == "onedimensional"
            || token == "OneDimensional"
            || token == "1") {
        problem_type = ProblemType::OneDimensional;
    } else if (token == "box"
            || token == "Box"
            || token == "B") {
        problem_type = ProblemType::Box;
    } else if (token == "boxstacks"
            || token == "BoxStacks"
            || token == "BS") {
        problem_type = ProblemType::BoxStacks;
    } else if (token == "irregular"
            || token == "Irregular"
            || token == "I") {
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
    } case ProblemType::Box: {
        os << "Box";
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
    if (token == "default"
            || token == "Default") {
        objective = Objective::Default;
    } else if (token == "bin-packing"
            || token == "BinPacking"
            || token == "BPP") {
        objective = Objective::BinPacking;
    } else if (token == "bin-packing-with-leftovers"
            || token == "BinPackingWithLeftovers"
            || token == "BPPL") {
        objective = Objective::BinPackingWithLeftovers;
    } else if (token == "open-dimension-x"
            || token == "OpenDimensionX"
            || token == "ODX") {
        objective = Objective::OpenDimensionX;
    } else if (token == "open-dimension-y"
            || token == "OpenDimensionY"
            || token == "ODY") {
        objective = Objective::OpenDimensionY;
    } else if (token == "open-dimension-z"
            || token == "OpenDimensionZ"
            || token == "ODZ") {
        objective = Objective::OpenDimensionZ;
    } else if (token == "open-dimension-xy"
            || token == "OpenDimensionXY"
            || token == "ODXY") {
        objective = Objective::OpenDimensionXY;
    } else if (token == "knapsack"
            || token == "Knapsack"
            || token == "KP") {
        objective = Objective::Knapsack;
    } else if (token == "variable-sized-bin-packing"
            || token == "VariableSizedBinPacking"
            || token == "VBPP") {
        objective = Objective::VariableSizedBinPacking;
    } else if (token == "sequential-onedimensional-rectangle-subproblem"
            || token == "SequentialOneDimensionalRectangleSubproblem"
            || token == "BDRS") {
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
    } case Objective::OpenDimensionZ: {
        os << "OpenDimensionZ";
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
