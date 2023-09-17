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

std::ostream& packingsolver::operator<<(std::ostream &os, ProblemType problem_type)
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

std::istream& packingsolver::operator>>(std::istream& in, Algorithm& algorithm)
{
    std::string token;
    in >> token;
    if (token == "auto") {
        algorithm = Algorithm::Auto;
    } else if (token == "tree-search" || token == "TS") {
        algorithm = Algorithm::TreeSearch;
    } else if (token == "column-generation" || token == "CG") {
        algorithm = Algorithm::ColumnGeneration;
    } else if (token == "dichotomic-search" || token == "DS") {
        algorithm = Algorithm::DichotomicSearch;
    } else if (token == "sequential-value-correction" || token == "SVC") {
        algorithm = Algorithm::SequentialValueCorrection;
    } else if (token == "vsbpp2bpp" || token == "VSBPP2BPP") {
        algorithm = Algorithm::Vsbpp2Bpp;
    } else if (token == "sequential-onedimensional-rectangle" || token == "SOR") {
        algorithm = Algorithm::SequentialOneDimensionalRectangle;
    } else if (token == "irregular-to-rectangle" || token == "I2R") {
        algorithm = Algorithm::SequentialOneDimensionalRectangle;
    } else if (token == "nlp" || token == "NLP") {
        algorithm = Algorithm::Nlp;
    } else  {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

std::ostream& packingsolver::operator<<(std::ostream &os, Algorithm algorithm)
{
    switch (algorithm) {
    case Algorithm::Auto: {
        os << "Auto";
        break;
    } case Algorithm::TreeSearch: {
        os << "Tree search";
        break;
    } case Algorithm::ColumnGeneration: {
        os << "Column generation";
        break;
    } case Algorithm::DichotomicSearch: {
        os << "Dichotomic search";
        break;
    } case Algorithm::SequentialValueCorrection: {
        os << "Sequential Value Correction";
        break;
    } case Algorithm::Vsbpp2Bpp: {
        os << "VSBPP2BPP";
        break;
    } case Algorithm::SequentialOneDimensionalRectangle: {
        os << "Sequential onedimensional rectangle";
        break;
    } case Algorithm::IrregularToRectangle: {
        os << "Irregular to rectangle";
        break;
    } case Algorithm::Nlp: {
        os << "NLP";
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
        std::ostream &os,
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
